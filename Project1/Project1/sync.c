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
extern void initializeCache(struct cacheblock [][NUMWAYS]);


struct cacheblock CACHE[NUMSETS][NUMWAYS];  // Cache Structure

SEMAPHORE *sem_memrequest,  *sem_memdone[MAX_NUM_THREADS];
SEMAPHORE *sem_cpu, *sem_cache, *sem_writebackQueue;
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
int GetVictim(unsigned set_index) {
  int victim = 0;
  return victim;  // For Direct Mapped Cache there is no choice
}
/* ************************************************  */

void FlushDirtyBlock(struct genericQueueEntry * writeBlk) {
  if (TRACE)
    printf("Flushing Dirty Block  %x at time %5.2f\n", (writeBlk->reqAddress)/BLKSIZE, GetSimTime());

      totalMemoryIdleTime += GetSimTime() - lastMemoryIdleTime;
      ProcessDelay((double) MEMORY_WRITE_TIME);
      totalWritebacksCompleted++;
      totalMemoryWriteTime += MEMORY_WRITE_TIME;
      lastMemoryIdleTime = GetSimTime();
      if (TRACE)
	printf("\nCompleted Writeback at time %5.2f\n", GetSimTime());
      return;
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

int * MemoryRead(struct genericQueueEntry *memreq) {
  struct queueNode  *ptr;

  if (TRACE)
    printf("In function MemoryRead at time %5.2f\n", GetSimTime());
      totalMemoryIdleTime += GetSimTime() - lastMemoryIdleTime;
      ProcessDelay(MEMORY_READ_TIME);
      totalMemoryReadTime += MEMORY_READ_TIME;
      lastMemoryIdleTime = GetSimTime();
      return (int *) NULL;
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
    printf("In ServiceMemoryRequest: Thread: %d Address: %x Type: %s Time: %5.2f\n", req->threadId, req->reqAddress, (req->type==0) ? "WRITE": "READ", GetSimTime());
  blockdata = (unsigned *) MemoryRead(req);   // Read the missed block. Returns after memory read delay.

  if (TRACE)
    printf("\nCompleted Read Request for Address : %x at time %5.2f\n", req->reqAddress, GetSimTime());

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

  
  SemaphoreWait(sem_cache);  // Get exclusive access to the cache

  block_num =  address/ BLKSIZE;
  set_index = block_num% NUMSETS;
  my_tag = block_num/NUMSETS;
  

  for (way=0; way < NUMWAYS; way++)     {
    if ( (CACHE[set_index][way].V == TRUE) && (CACHE[set_index][way].TAG == my_tag)) {
		totalHits++; 
		if (type == WRITE) { 
		  CACHE[set_index][way].D = TRUE;
		  CACHE[set_index][way].BLKDATA = (int *) NULL; // We are not simulating the actual data in the cache
		}
		ProcessDelay(CACHE_LOOKUP_TIME);	// Delay for accessing cache
		 SemaphoreSignal(sem_cache);
		 return(TRUE);
    }
  }
  
  totalMisses++;
  ProcessDelay(CACHE_LOOKUP_TIME);

 if (TRACE)
      printf("\nCache Miss for Address %x at time %5.2f\n", address, GetSimTime());          
    
  way = GetVictim(set_index);
  if ( (CACHE[set_index][way].V == TRUE) && (CACHE[set_index][way].D == TRUE)) {  
       totalFlushes++;
       writeReq = (struct genericQueueEntry *) malloc(sizeof (struct genericQueueEntry));
       writeReq->reqAddress = (CACHE[set_index][way].TAG  * NUMSETS  + set_index) * BLKSIZE; 
       writeReq->type = WRITE;
       writeReq->data = CACHE[set_index][way].BLKDATA;  // This implementation does not actually write out data
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
    printf("\nThread: %d  Cache HIT  for Address %x at time %5.2f\n", thread_num, mem_ref->address, GetSimTime());          
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
	

  printf("Creating CPU thread %d at time %5.2f\n", thread_num, GetSimTime());
  
  SemaphoreWait(sem_cpu); // Contend for the processor
  if (TRACE)
    printf("Thread %d got CPU at time %5.2f\n", thread_num, GetSimTime());
  
  while(1){	  
    if (getNextRef(fp[thread_num], mem_ref) == FALSE)
	    break; // Get next trace record from tracefile; Quit if all records processed.	 
    
    if (TRACE)
      printf("Thread %d makes request for memory address %x at time %5.2f\n", thread_num, mem_ref->address, GetSimTime());
    numrecords[thread_num]++;
    doMemAccess(mem_ref, thread_num);

	if (TRACE)
	  printf("Thread %d completes request for memory address %x at time %5.2f\n", thread_num, mem_ref->address, GetSimTime());
	ProcessDelay(CPU_DELAY);  // Delay simulating processor computation time between memory accesses
  }

  if (TRACE)
	  printf("Thread %d completes at time %5.2f\n", thread_num, GetSimTime());
	SemaphoreSignal(sem_cpu);  // Relinquish CPU and quit 
}


//Memory Controller Model
void memorycontroller()
{
  int job_num;
  job_num = ActivityArgSize(ME) - 1;
  printf("Creating Memory Controller Process at time %5.2f\n", GetSimTime());

  while(1){
    SemaphoreWait(sem_memrequest);  // Wait to be woken up by the processor with a memory request on cache miss
    ServiceMemRequest();
        }
}


/* ******************************************************  */

void UserMain(int argc, char *argv[])
{
  void memorycontroller(), processor();
  int i;

  char *filename = malloc(20);

  sem_cpu = NewSemaphore("semcpu", 1); // Controls access to CPU
  sem_cache = NewSemaphore("semcache", 1); // Controls access to the cache
  sem_memrequest = NewSemaphore("memreq",0); //Used to signal  memory controller with request
  for (i=0; i < MAX_NUM_THREADS; i++) 
    sem_memdone[i] = NewSemaphore("memdone",0); // Used to signal thread that cache miss is complete


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
    
    
    
    // Initialize Global variables
    totalHits = 0; 
    totalMisses = 0;
    for (i=0; i < MAX_NUM_THREADS; i++)
      Hits[i] = Misses[i] = 0;

    makeQueue(MEMORY_REQUEST_QUEUE);  
    
    for (i=0; i < MAX_NUM_THREADS; i++) {
      sprintf(filename, "memtrace%d\0",i);
      printf("Opening Trace File: \"%s\" \n", filename);
      fp[i] = fopen(filename,"r");
    }

    initializeCache(CACHE);
    // Initialization is done, now start the simulation

    DriverRun(MAX_SIMULATION_TIME); // Maximum time of the simulation (in cycles).   
    printStatistics(TRUE);

}










