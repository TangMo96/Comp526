#include<stdlib.h>
#include<stdio.h>
#include <math.h>
#include "sim.h"
#include "global.h"

FILE *fp[MAX_NUM_THREADS];
int Hits[MAX_NUM_THREADS], Misses[MAX_NUM_THREADS];
int totalHits = 0, totalMisses = 0, totalFlushes = 0;
int numrecords[MAX_NUM_THREADS];
int totalWritebacksCompleted = 0;

int   numFoundInWriteQ = 0;

double totalMemoryIdleTime = 0.0;
double totalMemoryReadTime = 0.0;
double totalMemoryWriteTime = 0.0;
double lastMemoryIdleTime = 0.0;


extern struct genericQueueEntry * getFromQueue(int);
extern struct queueNode * getLastEntry(int, int);

double totalTimeCPUBusy[MAX_NUM_THREADS] = {0.0};
double  lastTimeCPUBusy[MAX_NUM_THREADS] = {0.0};


struct cacheblock CACHE[NUMSETS][NUMWAYS];  // Cache Structure

SEMAPHORE *sem_memrequest,  *sem_memdone[MAX_NUM_THREADS];
SEMAPHORE *sem_cpu, *sem_cache, *sem_memory, *sem_writebackQueue;
SEMAPHORE *sem_writeQueueFull, *sem_writeRequest;


PROCESS *proccntrl[MAX_NUM_THREADS]; // Yacsim process modeling activities of a  thread
PROCESS  *memcntrl, *writecntrl; // Yacsim processes for memory controller and flushing thread
 

int printStatistics(int normalCompletion) {
  int i;
  
  if (normalCompletion)
    printf("\nAll Threads Completed\n");
  else
    printf("Simulation ended without completing all traces\n");

  printf("Cache Size (blocks): %d\tBlock Size (bytes): %d \n", CACHESIZE, BLKSIZE);
  printf("CPU Delay: %5.2f\n",CPU_DELAY);  

  totalMemoryIdleTime += GetSimTime() - lastMemoryIdleTime;
  
  printf("\nSimulation ended  at %3.0f\n",GetSimTime());  

  for (i=0; i < MAX_NUM_THREADS; i++) {
    printf("Number of records processed from trace file: %d\n", numrecords[i]);
    printf("Thread: %d  Hits: %d  Misses: %d\n", i, Hits[i], Misses[i]);
  }

  printf("Total number of flushes of dirty blocks: %d\n\n", totalFlushes);   
  printf("Total Number of Writebacks completed: %d\n", totalWritebacksCompleted);
  printf("Memory Statistics:\n\tTotal Memory Read Time: %5.2f\n\tTotal Memory Write Time: %5.2f\n\tTotal Memory Idle Time: %5.2f\n", totalMemoryReadTime, totalMemoryWriteTime, totalMemoryIdleTime);
}


/* ************************************************  */
struct tracerecord  tracerec;   // Trace record



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
      
      SemaphoreWait(sem_writeRequest);   // Wait to be woken up by the cache controller on dirty block eviction
      ProcessDelay(epsilon);
  
      if (TRACE)
	printf("Memory Write Thread: Get write request from queue  at time %5.2f\n", GetSimTime());
  
      SemaphoreWait(sem_writebackQueue); 
      req =   getFromQueue(WRITEBACK_QUEUE);
      SemaphoreSignal(sem_writebackQueue);
    
            if (TRACE)
      	printf("Memory Write Thread: Contend for memory semaphore at time %5.2f\n", GetSimTime());
      
      SemaphoreWait(sem_memory);
       if (TRACE)
      	printf("Memory Write Thread: Granted  memory semaphore at time %5.2f\n", GetSimTime());
    
      totalMemoryIdleTime += GetSimTime() - lastMemoryIdleTime;

      ProcessDelay((double) MEMORY_WRITE_TIME);
      totalWritebacksCompleted++;
      totalMemoryWriteTime += MEMORY_WRITE_TIME;
      lastMemoryIdleTime = GetSimTime();
      SemaphoreSignal(sem_memory);

     
   SemaphoreSignal(sem_writeQueueFull);
   if (TRACE)
	printf("Memory Write Thread completed Writeback. Time %5.2f\n", GetSimTime());


    }

}

/* ************************************************  */
void FlushDirtyBlock(struct genericQueueEntry * writeBlk) {
   if (TRACE)
    printf("Flushing  Dirty Block  %x at time %5.2f\n", (writeBlk->reqAddress)/BLKSIZE, GetSimTime());


    SemaphoreWait(sem_writeQueueFull);

    SemaphoreWait(sem_writebackQueue);

    insertQueue(WRITEBACK_QUEUE,writeBlk);

    SemaphoreSignal(sem_writebackQueue);

    SemaphoreSignal(sem_writeRequest);
/*
Note:
 Make sure to get exlusive access to the Writeback Queue when adding a request to it.
Be careful not to create possibilities for deadlock by locking access to the Writeback queue while waiting for space.
 */

}
/* ************************************************  */

 
/* ************************************************  */
int * MemoryRead(struct genericQueueEntry *memreq) {
  struct queueNode  *ptr;

  if (TRACE)
    printf("In function MemoryRead at time %5.2f\n", GetSimTime());

  SemaphoreWait(sem_writebackQueue);

  ProcessDelay(WRITE_BUFFER_SEARCH_DELAY);

  p = getLastEntry(memreq->threadId,memreq->reqAddress);

  SemaphoreSignal(sem_writebackQueue);
  if(p!=null){
    if (TRACE)
      printf("Found request in Write Queue: Time: %5.2f\n",  GetSimTime());   
  }
  else {
    if (TRACE)
      printf("Memory Reader  Contend for memory semaphore at time %5.2f\n", GetSimTime());

    if (TRACE)
      printf("Memory Reader  got memory semaphore at time %5.2f\n", GetSimTime());
  
    totalMemoryIdleTime += GetSimTime() - lastMemoryIdleTime;
       // memory no longer idle
    ProcessDelay(MEMORY_READ_TIME);

    totalMemoryReadTime += MEMORY_READ_TIME;  // Accumulate total time spent reading memory
    lastMemoryIdleTime = GetSimTime();  //Memory idle unless other request reactivates it
    
    SemaphoreSignal(sem_memory);
  }

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
    printf("In ServiceMemoryRequest: Thread: %d Address: %x  Time: %5.2f\n", req->threadId, req->reqAddress, GetSimTime());
  blockdata = (unsigned *) MemoryRead(req);   // Read the missed block. Returns after memory read delay.

  if (TRACE)
    printf("\nCompleted Memory Read  for Address : %x at time %5.2f\n", req->reqAddress, GetSimTime());

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
		//		if (TRACE) printf("\nCache HIT for address %x at time %5.2f\n", address, GetSimTime());  
		
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

  if (TRACE) printf("\nCache MISS for address %x at time %5.2f\n",address, GetSimTime());

  way = GetVictim(set_index);
  CACHE[set_index][way].V = FALSE; 
  if (CACHE[set_index][way].D == TRUE) {      // This implementation does not actually write the dirty block.
       totalFlushes++;
       writeReq = (struct genericQueueEntry *) malloc(sizeof (struct genericQueueEntry));
       writeReq->reqAddress = (CACHE[set_index][way].TAG  * NUMSETS  + set_index) * BLKSIZE; 
       writeReq->type = WRITE;
       writeReq->data = CACHE[set_index][way].BLKDATA;;
       writeReq->startTime = GetSimTime();
       FlushDirtyBlock(writeReq);
  }

  SemaphoreSignal(sem_cache);
  return(FALSE);
}


void    doMemAccess(struct tracerecord *mem_ref,int thread_num) {
  struct genericQueueEntry   *mem_req = malloc(sizeof(struct genericQueueEntry));    // Memory Request for Memory Controller

  
  while  (!LookupCache(mem_ref->address, mem_ref->type))  {
    Misses[thread_num]++;
    if (TRACE)
      printf("Thread: %d  Creating Memory Request for Address %x at time %5.2f\n", thread_num, mem_ref->address, GetSimTime());          

      // Create a memory request record and insert into Memory request queue
    mem_req->threadId = thread_num;
    mem_req->reqAddress = mem_ref->address; 
    mem_req->type = mem_ref->type;	  
    mem_req->startTime = GetSimTime();
    insertQueue(MEMORY_REQUEST_QUEUE, mem_req);
    SemaphoreSignal(sem_memrequest);  // Wake up the memory controller to handle this request. 
    SemaphoreWait(sem_memdone[thread_num]);   // Wait for the memory request to complete.  
  }
 if (TRACE)
   printf("Thread: %d  Cache HIT  for Address %x at time %5.2f\n", thread_num, mem_ref->address, GetSimTime());          
 Hits[thread_num]++;	     
}

/* ***********n *************************************  */
// Thread  model
void processor()
{
  struct tracerecord  *mem_ref = malloc(sizeof(struct tracerecord));  // Record from trace file 

  // Yacsim specific commands
  int thread_num;   // ID for this thread
  thread_num = ActivityArgSize(ME) - 1;
	
    printf("CPU thread  %d starts at time %5.2f\n", thread_num, GetSimTime());
  
    SemaphoreWait(sem_cpu); // Contend for the processor
    lastTimeCPUBusy[thread_num]= GetSimTime();

  while(1){	  
    if (getNextRef(fp[thread_num], mem_ref) == FALSE)
	    break; // Get next trace record from tracefile; Quit if all records processed.	 
    
	if (TRACE)
	  printf("\nThread %d makes request for memory address %x at time %5.2f\n", thread_num, mem_ref->address, GetSimTime());
	numrecords[thread_num]++;
	doMemAccess(mem_ref, thread_num);

	if (TRACE)
	  printf("Thread %d completes request for memory address %x at time %5.2f\n", thread_num, mem_ref->address, GetSimTime());
	ProcessDelay(CPU_DELAY);  // Delay simulating processor computation time between memory accesses
  }

  if (TRACE)
	  printf("Thread %d completes at time %5.2f\n", thread_num, GetSimTime());
	SemaphoreSignal(sem_cpu);  // Relinquish CPU and quit 

	totalTimeCPUBusy[thread_num] += GetSimTime() -lastTimeCPUBusy[thread_num];
	lastTimeCPUBusy[thread_num] = GetSimTime();
}


//Memory Controller Model
void memorycontroller()
{
  int job_num;
  job_num = ActivityArgSize(ME) - 1;
    printf("Memory Controller  starts at time %5.2f\n", GetSimTime());
  while(1){
    SemaphoreWait(sem_memrequest);  // Wait to be woken up by the processor with a memory request on cache miss
    ServiceMemRequest();
    }
}


/* ******************************************************  */

void UserMain(int argc, char *argv[])
{
  void memorycontroller(), processor(), cachecontroller();
  int i;

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

    makeQueue(MEMORY_REQUEST_QUEUE);  // New
    makeQueue(WRITEBACK_QUEUE); // New

    for (i=0; i < MAX_NUM_THREADS; i++) {
      sprintf(filename, "memtrace%d\0",i);
      printf("Opening File %s\n", filename);
      fp[i] = fopen(filename,"r");
    }



    // Initialization is done, now start the simulation

    DriverRun(MAX_SIMULATION_TIME); // Maximum time of the simulation (in cycles).   
    printStatistics(TRUE);

}










