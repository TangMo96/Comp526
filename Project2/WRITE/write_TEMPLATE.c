#include<stdlib.h>
#include<stdio.h>
#include <math.h>
#include "sim.h"
#include "global.h"

FILE *fp[MAX_NUM_THREADS];
int Hits[MAX_NUM_THREADS], Misses[MAX_NUM_THREADS];
int totalHits = 0, totalMisses = 0, totalDirtyEvictions = 0;
int numrecords[MAX_NUM_THREADS];
int totalWritebacksCompleted = 0;
int   numReadsFoundInWriteQ = 0;

double totalMemoryIdleTime = 0.0;
double totalMemoryReadTime = 0.0;
double totalMemoryWriteTime = 0.0;
double lastMemoryIdleTime = 0.0;

double    totalTimeCPUBusy[MAX_NUM_THREADS] = {0.0};
double  lastTimeCPUBusy[MAX_NUM_THREADS] = {0.0};

extern struct genericQueueEntry * getFromQueue(int);
extern struct genericQueueEntry * pokeQueue(int);
extern struct queueNode * getLastEntry(int, int);

struct cacheblock CACHE[NUMSETS][NUMWAYS];  // Cache Structure
struct tracerecord  tracerec;   // Trace record

SEMAPHORE *sem_memrequest,  *sem_memdone[MAX_NUM_THREADS];
SEMAPHORE *sem_cpu, *sem_cache, *sem_memory, *sem_writebackQueue;
SEMAPHORE *sem_writeQueueFull, *sem_writeRequest;

PROCESS *proccntrl[MAX_NUM_THREADS]; // Yacsim process modeling activities of a  thread
PROCESS  *memcntrl, *writecntrl; // Yacsim processes for memory controller and flushing thread
 
int printStatistics(int normalCompletion) {
  int i;
  int totalRecords = 0;
  int totalHits = 0;
  int totalMisses = 0;
  int totalCPUBusy = 0;
  
  if (normalCompletion)
    printf("\nAll Threads Completed\n");
  else
    printf("Simulation ended without completing all traces\n");
  
  totalMemoryIdleTime += GetSimTime() - lastMemoryIdleTime;
  printf("\nSimulation ended  at %3.0f\n",GetSimTime());
  printf("\nCache Size (blocks): %d\tBlock Size (bytes): %d \n", CACHESIZE, BLKSIZE);
  printf("CPU Delay (cycles): %3.0f\n",CPU_DELAY);  
  printf("Memory READ Time (cycles): %3.0f  Memory WRITE Time (cycles): %3.0f\n", MEMORY_READ_TIME, MEMORY_WRITE_TIME);
  for (i=0; i < MAX_NUM_THREADS; i++) {
    printf("********************************************\n");
    printf("Number of records processed from trace file %d: %d\n", i, numrecords[i]);
    printf("Thread: %d  Hits: %d  Misses: %d\n", i, Hits[i], Misses[i]);
    printf("Thread: %d Total CPU Time Busy: %5.2f\n", i, totalTimeCPUBusy[i]);
    totalRecords += numrecords[i];
    totalHits += Hits[i];
    totalMisses += Misses[i];
    totalCPUBusy += totalTimeCPUBusy[i];
  }
  
  printf("\n********************************************\n");
  printf("Total number of Records read: %d\n", totalRecords);
  printf("Total number of Hits: %d\n", totalHits);
  printf("Total number of Misses: %d\n", totalMisses);
  printf("Total time CPU Busy: %d\n", totalCPUBusy);
  printf("Total number of Evictions of dirty blocks: %d\n", totalDirtyEvictions);  
  printf("Total number Read Misses Found In Write Buffer: %d\n",   numReadsFoundInWriteQ);  
  printf("Total number of Writebacks Completed: %d\n", totalWritebacksCompleted);
  printf("Memory Statistics:\nTotal Memory Read Time: %5.2f\nTotal Memory Write Time: %5.2f\nTotal Memory Idle Time: %5.2f\n", totalMemoryReadTime, totalMemoryWriteTime, totalMemoryIdleTime);
}
/* ************************************************  */

int GetVictim(unsigned set_index) {
  int victim = 0;
  return victim;  // For Direct Mapped Cache there is no choice
}
/* ************************************************  */

//Memory Write Thread
void memorywriter(){
  int job_num;
  job_num = ActivityArgSize(ME) - 1;
  struct genericQueueEntry *req;
  
  if (TRACE)
    printf("Memory Write Thread starts  at time %5.2f\n", GetSimTime());
  
  while(1){
    if (TRACE)
      printf("Memory WRITER: Waiting for writeback request at time %5.2f\n", GetSimTime());
    
    SemaphoreWait(sem_writeRequest);   // Wait to be woken up by the cache controller on dirty block eviction
    ProcessDelay(epsilon);
    
    SemaphoreWait(sem_writebackQueue); 
    req =   pokeQueue(WRITEBACK_QUEUE);
    SemaphoreSignal(sem_writebackQueue);
    
    if (TRACE)
      printf("Memory WRITER: Contend for memory semaphore at time %5.2f\n", GetSimTime());
    
    SemaphoreWait(sem_memory);
    if (TRACE)
      printf("Memory WRITER: Granted  memory semaphore at time %5.2f\n", GetSimTime());
    totalMemoryIdleTime += GetSimTime() - lastMemoryIdleTime;
    ProcessDelay((double) MEMORY_WRITE_TIME);
    SemaphoreSignal(sem_memory);
    
    totalWritebacksCompleted++;
    totalMemoryWriteTime += MEMORY_WRITE_TIME;
    lastMemoryIdleTime = GetSimTime();
    
    if (TRACE)
      printf("Memory WRITER: Completed write of  block %x with timestamp %5.2f to memory at Time:%5.2f\n", (req->reqAddress)/BLKSIZE,  req->startTime, GetSimTime());
    
    SemaphoreWait(sem_writebackQueue); 
    req =   getFromQueue(WRITEBACK_QUEUE);
    SemaphoreSignal(sem_writebackQueue);  
    SemaphoreSignal(sem_writeQueueFull);
  }  
}
/* ************************************************  */

void FlushDirtyBlock(struct genericQueueEntry * writeBlk) {
  if (TRACE)
    printf("Flushing Dirty Block  %x at time %5.2f\n", (writeBlk->reqAddress)/BLKSIZE, GetSimTime());
 
  SemaphoreWait(sem_writeQueueFull);
  SemaphoreWait(sem_writebackQueue); 
  insertQueue(WRITEBACK_QUEUE, writeBlk);
  SemaphoreSignal(sem_writebackQueue);
  SemaphoreSignal(sem_writeRequest);
}
/* ************************************************  */

void checkAndFlush(struct genericQueueEntry * req) {
  struct genericQueueEntry * writeReq;
  unsigned set_index;
  //   1.Compute block address  of cache block to be evicted
  unsigned add;
  SemaphoreWait(sem_cache);  
  set_index = (req->reqAddress /  BLKSIZE)% NUMSETS;
  add = GetVictim(set_index);
  //   2. Evict the current cache occupant. 
  CACHE[set_index][add].V = FALSE;
  //   3. Determine if the evicted block needs to be written to memory
  if(CACHE[set_index][add].D == FALSE){
    if (TRACE) 
      printf("CheckAndFlush: Evicted block at index %d is CLEAN. Time: %5.2f\n", set_index, GetSimTime());
    }
  //   4. If write back to memory is required: 
  else {
    if (TRACE) 
      printf("\nCheckAndFlush: Evicted block at index %d is DIRTY -- flush to memory. Time: %5.2f\n", set_index, GetSimTime());
    //      (a) Increment "totalDirtyEvictions".
    totalDirtyEvictions++;
    //  (a) Allocate space for a write request (struct genericQueueEntry) 
    writeRequest = (struct genericQueueEntry *) malloc(sizeof (struct genericQueueEntry));
    //      (b) Compute the memory address of the  block being written back
    writeRequest->reqAddress = BLKSIZE * (set_index + CACHE[set_index][add].TAG  * NUMSETS); 
    //      (c) Write back the evicted block to memory
    //  (b) Fill in  the fields of the struct
    writeRequest->startTime = GetSimTime();
    writeRequest->type = WRITE;
    writeRequest->data = CACHE[set_index][add].BLKDATA;
    //  (c) Call FlushDirtyBlock( ) to perform the write
    FlushDirtyBlock(writeRequest);
  }
  //if need to be written to memory
  SemaphoreSignal(sem_cache);  
}

/* ************************************************  */

int * MemoryRead(struct genericQueueEntry *memreq) {
  struct queueNode  *ptr;
  
  if (TRACE)
    printf("MemoryRead: Search Writeback Buffer for address %x Time %5.2f\n", memreq->reqAddress, GetSimTime());
  
  SemaphoreWait(sem_writebackQueue);
  ptr =  getLastEntry(WRITEBACK_QUEUE, memreq->reqAddress);
  ProcessDelay(WRITE_BUFFER_SEARCH_DELAY);
  SemaphoreSignal(sem_writebackQueue);
 
  if (ptr != (struct queueNode *) NULL) {
    if (TRACE)
      printf("Memory Read: Block %x found in Write Buffer: Time: %5.2f\n", ((ptr->data)->reqAddress)/BLKSIZE, GetSimTime());   
    numReadsFoundInWriteQ++;  
    return (int *) NULL; // Data is not modeled
  }
  
  if (TRACE)
    printf("Memory Read: Block %x NOT found in Write Buffer: Contend for memory access semaphore. Time %5.2f\n", (memreq->reqAddress)/BLKSIZE,GetSimTime());

  SemaphoreWait(sem_memory); // Contend for memory
  if (TRACE)
    printf("Memory Read:  Got memory access semaphore for Block %x.  Time %5.2f\n", (memreq->reqAddress)/BLKSIZE, GetSimTime());
      totalMemoryIdleTime += GetSimTime() - lastMemoryIdleTime;
      ProcessDelay(MEMORY_READ_TIME);
      totalMemoryReadTime += MEMORY_READ_TIME;
      lastMemoryIdleTime = GetSimTime();
      SemaphoreSignal(sem_memory);
      
      return (int *) NULL;
}
/* ************************************************  */

void   installBlockInCache(struct genericQueueEntry *req) {
  unsigned set_index, my_tag;
  int type, way;
  int  *blockdata;
  
  SemaphoreWait(sem_cache);
  set_index = (req->reqAddress /  BLKSIZE)% NUMSETS;
  my_tag = (req->reqAddress / BLKSIZE)/NUMSETS;
  way = GetVictim(set_index);
  
  if (TRACE)
    printf("installBlockInCache: Installing request in cache index %d. Time: %5.2f\n", set_index, GetSimTime());
  
  CACHE[set_index][way].V = TRUE;
  CACHE[set_index][way].TAG = my_tag;
  CACHE[set_index][way].BLKDATA = blockdata;   // In the  present implementation this will be a null pointer.
  CACHE[set_index][way].D = FALSE;
  SemaphoreSignal(sem_cache);
}
/* ************************************************  */

int ServiceMemRequest() {
  struct genericQueueEntry  *req;
  unsigned * blockdata ;
  
  req =  getFromQueue(MEMORY_REQUEST_QUEUE);  
   if (req == NULL){
     printf("ERROR: No Memory Request To Service\n");
     exit(1);
   }
   
  if (TRACE)
    printf("\nIn ServiceMemoryRequest: Thread: %d Address: %x Type: %s Time: %5.2f\n", req->threadId, req->reqAddress, (req->type==0) ? "WRITE": "READ", GetSimTime());

  blockdata = (unsigned *) MemoryRead(req);   // Read the missed block. Returns after memory read delay.
  if (TRACE)
    printf("\nCompleted Memory Read for Thread: %d Block %x at Time %5.2f\n", req->threadId, (req->reqAddress)/BLKSIZE, GetSimTime()); 
  installBlockInCache(req);
  SemaphoreSignal(sem_memdone[req->threadId]);  // Wake up processor and go back to waiting for next request.
}

/* ************************************************  */
/* Called with a memory address. Checks for presence in the cache and returns status.
Updates  count of hits and misses. */

int LookupCache(unsigned address, unsigned type) {
  unsigned block_num, set_index, my_tag;
  unsigned way;
  struct genericQueueEntry *writeReq;

  SemaphoreWait(sem_cache);
  block_num =  address/ BLKSIZE;
  set_index = block_num% NUMSETS;
  my_tag = block_num/NUMSETS;
  
  for (way=0; way < NUMWAYS; way++)     {
	      if ( (CACHE[set_index][way].V == TRUE) && (CACHE[set_index][way].TAG == my_tag)) {
		totalHits++; 
		if (type == WRITE) { 
		  CACHE[set_index][way].D = TRUE;
		  CACHE[set_index][way].BLKDATA = (int *) NULL;
		}
		ProcessDelay(CACHE_LOOKUP_TIME);	// Delay for accessing cache
		SemaphoreSignal(sem_cache);
		return(TRUE);
	      }
  }
  
  ProcessDelay(CACHE_LOOKUP_TIME);
  totalMisses++;
  SemaphoreSignal(sem_cache);
  return(FALSE);
}


void    doMemAccess(struct tracerecord *mem_ref,int thread_num) {
  struct genericQueueEntry   *mem_req = malloc(sizeof(struct genericQueueEntry));    // Memory Request record for Memory Controller
  
  while  (!LookupCache(mem_ref->address, mem_ref->type))  {
    Misses[thread_num]++;
    if (TRACE)
      printf("Thread %d:  Cache Miss for Memory Address %x at time %5.2f\n", thread_num, mem_ref->address, GetSimTime());          
    
    // Create a memory request record and insert into Memory request queue
    mem_req->threadId = thread_num;
    mem_req->reqAddress = mem_ref->address; 
    mem_req->type = mem_ref->type;	  
    mem_req->startTime = GetSimTime();
    insertQueue(MEMORY_REQUEST_QUEUE, mem_req);
    
    SemaphoreSignal(sem_memrequest);  // Signal  memory controller to handle this request. 
    totalTimeCPUBusy[thread_num] += GetSimTime() - lastTimeCPUBusy[thread_num];
    SemaphoreSignal(sem_cpu);  // Release CPU
    
    SemaphoreWait(sem_memdone[thread_num]);   // Wait for the memory request to complete.  
    SemaphoreWait(sem_cpu); // Contend for CPU
    lastTimeCPUBusy[thread_num] = GetSimTime();
  }
  
  // Cache Hit
  Hits[thread_num]++;	     
  if (TRACE)
    printf("Thread %d:  Cache Hit  for Memory Address %x at time %5.2f\n", thread_num, mem_ref->address, GetSimTime());
}
/* ***********n *************************************  */

// Thread  model
void processor()
{
  struct tracerecord  *mem_ref = malloc(sizeof(struct tracerecord));  // Record from trace file

  // Yacsim specific commands
  int thread_num;   // ID for this thread
  thread_num = ActivityArgSize(ME) - 1;
  
  printf("\nStarting CPU thread %d at time %5.2f\n", thread_num, GetSimTime());
  SemaphoreWait(sem_cpu); // Contend for the processor
  lastTimeCPUBusy[thread_num] = GetSimTime();
  if (TRACE)
    printf("\nThread %d got CPU at time %5.2f\n", thread_num, GetSimTime());
  
  while(1){	  
    if (getNextRef(fp[thread_num], mem_ref) == FALSE)
      break; // Get next trace record from tracefile; Quit if all records processed.	 
    
    if (TRACE)
      printf("\nThread %d makes request for Memory Address %x Block %x at time %5.2f\n", thread_num, mem_ref->address, mem_ref->address/BLKSIZE, GetSimTime());
    numrecords[thread_num]++; // COunts number of accesses made by thread
    
    doMemAccess(mem_ref, thread_num); // Function returns after request is complete
    
    if (TRACE)
      printf("Thread %d completes request for memory address %x at time %5.2f\n", thread_num, mem_ref->address, GetSimTime());
    ProcessDelay(CPU_DELAY);  // Delay simulating processor computation time between memory accesses
  }
  
  if (TRACE)
    printf("Thread %d completes at time %5.2f\n", thread_num, GetSimTime());
  SemaphoreSignal(sem_cpu);  // Relinquish CPU and quit 
  
  totalTimeCPUBusy[thread_num] += GetSimTime() - lastTimeCPUBusy[thread_num];
}


//Memory Controller Model
void memorycontroller()
{
  int job_num;
  job_num = ActivityArgSize(ME) - 1;
  
  while(1){
    SemaphoreWait(sem_memrequest);  // Wait to be woken up by the processor with a memory request on cache miss
    ServiceMemRequest();
  }
}

/* ******************************************************  */

void UserMain(int argc, char *argv[]){
  void memorycontroller(), processor(), memorywriter();
  int i;
  int x;
  
  char *filename = malloc(20);
  
 sem_cpu = NewSemaphore("semcpu", 1); // Controls access to CPU
 sem_cache = NewSemaphore("semcache", 1); // Controls access to the cache
 sem_memory = NewSemaphore("semmemory", 1); // Controls access to the backend memory
 sem_writebackQueue = NewSemaphore("semwriteq", 1); // Controls access to write queue
 
 sem_memrequest = NewSemaphore("memreq",0); //Used to signal  memory controller with request
 sem_writeRequest = NewSemaphore("writereq",0); //Used to signal  writeback controller with request
 
 sem_writeQueueFull = NewSemaphore("writeQFull",MAX_WRITE_BUFFER_SIZE); //Used to limit number of outstanding writeback requests
 
  for (i=0; i < MAX_NUM_THREADS; i++) 
    sem_memdone[i] = NewSemaphore("memdone",0); // Used to signal thread that cache miss complete
  
  // Create a process to model the activities of the processor
  for (i=0; i < MAX_NUM_THREADS; i++) {
    proccntrl[i] = NewProcess("proccntrl",processor,0);
    ActivitySetArg(proccntrl[i],NULL,i+1);
    ActivitySchedTime(proccntrl[i],0.00001,INDEPENDENT);
  }
  
  
  // Create a process to model the activities of the memory controller 
    memcntrl = NewProcess("memcntrl",memorycontroller,0);
    ActivitySetArg(memcntrl,NULL,1);
    ActivitySchedTime(memcntrl,0.00002,INDEPENDENT);
    
    // Create a process to model the BACKGROUND WRITE (Flush) process
    writecntrl = NewProcess("writecntrl",memorywriter,0);
    ActivitySetArg(writecntrl,NULL,1);
    ActivitySchedTime(writecntrl,0.00002,INDEPENDENT);
    
    
    // Initialize Global variables
    totalHits = 0; 
    totalMisses = 0;
    for (i=0; i < MAX_NUM_THREADS; i++)
      Hits[i] = Misses[i] = 0;

    makeQueue(MEMORY_REQUEST_QUEUE);  
    makeQueue(WRITEBACK_QUEUE); 
    
    for (i=0; i < MAX_NUM_THREADS; i++) {
      sprintf(filename, "memtrace%d\0",i);
      printf("Opening File %s\n", filename);
      fp[i] = fopen(filename,"r");
    }
    
    

    // Initialization is done, now start the simulation
    
    DriverRun(MAX_SIMULATION_TIME); // Maximum time of the simulation (in cycles).   
    printStatistics(TRUE);

}










