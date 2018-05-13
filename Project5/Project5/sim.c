#include "global.h"

extern displayCacheBlock(int, int);
extern h(int);

struct cacheblock CACHE[NUM_PROCESSORS][CACHESIZE];  // Cache
int MEM[TOTALSIZE]; // Physical Memory
int VABaseAddress;  // Addfresses in Trace File

// Statistics
int cache_write_hits[NUM_PROCESSORS], cache_write_misses[NUM_PROCESSORS];
int cache_read_hits[NUM_PROCESSORS], cache_read_misses[NUM_PROCESSORS];
int cache_upgrades[NUM_PROCESSORS], cache_writebacks[NUM_PROCESSORS];
int numThreadsCompleted = 0;
int numUtoX[NUM_PROCESSORS];

// Input
struct tracerecord  tracerec;   // Trace record read from trace file
char *traceFile[NUM_PROCESSORS];
FILE *fp[NUM_PROCESSORS];

// Synchronization Variables
SEMAPHORE *sem_memreq[NUM_PROCESSORS];
SEMAPHORE *sem_memdone[NUM_PROCESSORS];
SEMAPHORE *sem_bussnoop[NUM_PROCESSORS], *sem_bussnoopdone[NUM_PROCESSORS];

//   Bus Arbitration Signals
int BUS_REQUEST[NUM_PROCESSORS];  // Bus Request
int BUS_GRANT[NUM_PROCESSORS];  // Bus Grant
int BUS_RELEASE; // Bus Released
struct busrec BROADCAST_CMD;   // Broadcast request

PROCESS *proccntrl, *memcntrl, *buscntrl, *bussnooper, *cachecntrl, *memwritecntrl;






extern int  getNextRef(FILE *, struct tracerecord *);
extern void insertQueue(int,  struct genericQueueEntry *);
extern void * getFromQueue(int);



void cleanUp()  {
 int i;
     // Simulation complete. All records in tracefile have been processed
 int tcache_read_hits =0, tcache_read_misses=0, tcache_write_hits=0, tcache_upgrades=0, tcache_write_misses=0, tcache_writebacks=0, tnumUtoX = 0;

 for (i=0; i < NUM_PROCESSORS; i++) {
   printf("\nProcessor: %d\tREAD HITS: %d\tREAD MISSES: %d\tSILENT WRITE HITS: %d\tUPGRADES: %d\tWRITE MISSES: %d\tWRITEBACKS: %d\tNum U converted: %d\tTime: %5.2f\n", i, cache_read_hits[i], cache_read_misses[i], cache_write_hits[i], cache_upgrades[i], cache_write_misses[i],cache_writebacks[i],numUtoX[i],GetSimTime());
      printf("Simulation ended  at %5.2f\n",GetSimTime());
      tcache_read_hits += cache_read_hits[i];
      tcache_read_misses += cache_read_misses[i];
     tcache_write_hits  += cache_write_hits[i];
     tcache_upgrades  += cache_upgrades[i];
     tcache_write_misses += cache_write_misses[i];
     tcache_writebacks += cache_writebacks[i];
     tnumUtoX += numUtoX[i];
 }
 printf("\n\nREAD HITS: %d\tREAD MISSES: %d\tSILENT WRITE HITS: %d\tUPGRADES: %d\tWRITE MISSES: %d\tWRITEBACKS: %d\tTime: %5.2f\tNumber Upgrades Converted to BUSRDX: %d\n", tcache_read_hits, tcache_read_misses, tcache_write_hits, tcache_upgrades, tcache_write_misses,tcache_writebacks,tnumUtoX,GetSimTime());
    
 for (i=0; i < TOTALSIZE; i++)
   if (TRACE) {
     printf("Memory Dump\n");
     printf("MEM[%d] : %d\n", i, MEM[i]);
   }
      exit(1);
}







void UserMain(int argc, char *argv[])
{
  int i,j, k;
  void Processor(), FrontEndCacheController(), BusSnooper(), BusArbiter();
  FILE *fpH;

  fpH = fopen("header", "r");
  if (fread(&VABaseAddress, sizeof(int), 1,fpH)  == 0) {
    printf("ERROR reading Header File\n");
    exit(1);
  }



  // Initialize Memory to match the initial values in the trace 
  for (i=0; i  < TOTALSIZE; i++)
    MEM[i] = i;

  BUS_RELEASE = TRUE;  // No bus owner at start

  if (TRACE)
    printf("Base Physical Address: %p\n",  MEM);
  
  for (i=0; i < NUM_PROCESSORS; i++) {
    sem_memreq[i] = NewSemaphore("memreq",0);          
    sem_memdone[i] = NewSemaphore("memdone",0);
    sem_bussnoop[i] = NewSemaphore("bussnoop", 0);    // BusSnooper i  waits on sem_bussnoop[i]
    sem_bussnoopdone[i] = NewSemaphore("bussnoopdone", 0);    // Must wait for all Snoopers to be done
  }

  
// Initialize all cache blocks  to the Invalid State
  for (i=0; i < NUM_PROCESSORS; i++) {
    for (j=0; j < CACHESIZE; j++) {
      CACHE[i][j].STATE = I;   
      for (k=0; k < INTS_PER_BLOCK; k++) {
	CACHE[i][j].DATA[k] = 0;
      }
      if (DEBUG) 
	displayCacheBlock(i, j);
    }
  }
  
  
  // Initialize Cache Statistics
  for (i=0; i < NUM_PROCESSORS; i++) {
    char filename[20];
    cache_write_hits[i] = 0;
    cache_write_misses[i]= 0;
    cache_upgrades[i] = 0;
    cache_read_hits[i] = 0;
    cache_read_misses[i] = 0;
    cache_writebacks[i] = 0;
    sprintf(filename, "memtrace%d\0",i);
    fp[i] = fopen(filename,"r");
  }

  for (i=0; i < NUM_PROCESSORS; i++) 
    makeQueue(MEM_CONTROLLER_QUEUE+i);
  
  // Create a Front End Cache  Controller  for each processor */
  for (i=0; i < NUM_PROCESSORS; i++) {
    memcntrl = NewProcess("memcntrl",FrontEndCacheController,0);
    ActivitySetArg(memcntrl,NULL,i);
    ActivitySchedTime(memcntrl,0.00000,INDEPENDENT);
  }
  printf("Done Creating FrontEndCacheControllers \n");


// Create a Bus Snooper for each processor
  for (i=0; i < NUM_PROCESSORS; i++) {
    bussnooper = NewProcess("bussnooper",BusSnooper,0);
    ActivitySetArg(bussnooper,NULL,i);
    ActivitySchedTime(bussnooper,0.000005,INDEPENDENT);
  }
  printf("Done Creating Bus Snoopers\n");


  // Create a Bus Arbiter
  buscntrl = NewProcess("buscntrl",BusArbiter,0);
  ActivitySetArg(buscntrl,NULL,1);
  ActivitySchedTime(buscntrl,0.00000,INDEPENDENT);
  printf("Done Creating Bus Arbiter  Process\n");
  
  
// Create a process to model activities of  each processor 
  for (i=0; i < NUM_PROCESSORS; i++){
    proccntrl = NewProcess("proccntrl",Processor,0);
    ActivitySetArg(proccntrl,NULL,i);
    ActivitySchedTime(proccntrl,0.00000,INDEPENDENT);
  }
  printf("Done Creating Processors\n");
    
  // Initialization is done, now start the simulation
  DriverRun(MAX_SIMULATION_TIME); // Maximum time of the simulation (in cycles). 

  printf("Simulation ended without completing the trace  at %5.2f\n",GetSimTime());
}



// Processor model
void Processor()
{
  struct tracerecord  *traceref = malloc(sizeof(struct tracerecord));
  struct genericQueueEntry   *memreq;
  int proc_num;

  proc_num = ActivityArgSize(ME) ;
  if (TRACE)
    printf("Processor[%d]: Activated at time %5.2f\n", proc_num, GetSimTime());
  
  while(1) {	  
    if (getNextRef(fp[proc_num], traceref) == 0) 
      break;  // Get next trace record; quit if done

    // Create a memory request and  insert in MEM_CONTROLLER_QUEUE for this processor
    memreq = malloc(sizeof(struct genericQueueEntry));  
    memreq->address = traceref->address; 
    memreq->type = traceref->type;
    memreq->delay = traceref->delay;
    memreq->data = traceref->data;
    
       insertQueue(MEM_CONTROLLER_QUEUE + proc_num, memreq);
       
       if (TRACE) {
	 printf("\nProcessor %d makes %s request:", proc_num, h(memreq->type));
	 printf(" Address: %x Time:%5.2f\n", memreq->address, GetSimTime());
       }

       SemaphoreSignal(sem_memreq[proc_num]);  // Notify memory controller of request
       SemaphoreWait(sem_memdone[proc_num]);   // Wait for request completion 
       ProcessDelay((double) traceref->delay);  //  Delay between requests 
  }

  numThreadsCompleted++;
  if (numThreadsCompleted == NUM_PROCESSORS)
    cleanUp();

}


