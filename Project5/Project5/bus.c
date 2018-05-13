#include "global.h"

extern struct cacheblock CACHE[][CACHESIZE];  // Cache

// Cache Statistics
extern int cache_write_hits[], cache_write_misses[];
extern int cache_upgrades[],cache_read_hits[], cache_read_misses[];
extern int cache_writebacks[];
extern int numUtoX[];

extern struct busrec BROADCAST_CMD;   // Bus Commands
extern int BUS_REQUEST[], BUS_GRANT[], BUS_RELEASE; // Bus Handshake Signals

extern SEMAPHORE *sem_bussnoop[]; // Signal each snooper that a bus command is being broadcast
extern SEMAPHORE *sem_bussnoopdone[]; // Signal that command has been handled at this snooper 


extern int map(int), f(int), g(int);
extern displayCacheBlock(int, int);


atomicUpdate(int procId, int blkNum, int state) {
  CACHE[procId][blkNum].STATE = state;
}

writebackMemory(int address, int * blkDataPtr) {  // Copy cache block to memory
  int i;
  int  *memptr = (int *) (long int) map(address);
  
  for (i=0; i < INTS_PER_BLOCK; i++){
    *memptr++  =  blkDataPtr[i];  
  }
}

readFromMemory(int address, int *blkDataPtr) {  // Copy memory block to cache 
  int i;
  int *memptr = (int *) (long int) map(address);

  for (i=0; i < INTS_PER_BLOCK; i++){
     blkDataPtr[i] = *memptr;
     memptr++;
  }
}




/* *********************************************************************************************
 Called by  HandleCacheMiss() and LookupCache() to create a bus transaction.
 It waits till granted bus access by the  Bus Arbiter, then broadcasts the command, and waits till 
 all snoopers acknowledge completion. 
 ********************************************************************************************* */

int  MakeBusRequest(int procId, struct genericQueueEntry * req){    
   
  int blkNum,  op;
  int i;

  BUS_REQUEST[procId] = TRUE;   // Assert BUS_REQUEST signal
  if (TRACE)
    printf("Processor %d --  BUS REQUEST Time %5.2f\n", procId, GetSimTime());

  while (BUS_GRANT[procId] == FALSE)  // Wait for BUS_GRANT 
      ProcessDelay(CLOCK_CYCLE);

  BUS_REQUEST[procId] = FALSE;   // Got bus! De-assert request


  // Create Bus Command based on type of request and current state
  blkNum = (req->address >> BLKSIZE) % CACHESIZE;
  op = req->type;

  BROADCAST_CMD.address  =  req->address;
  if  (CACHE[procId][blkNum].STATE == SM)
         BROADCAST_CMD.reqtype  = INV;
  else  if  (op == LOAD)
    BROADCAST_CMD.reqtype  = BUS_RD;
  else 
    BROADCAST_CMD.reqtype  = BUS_RDX;


  if (TRACE)
    printf("Processor %d -- BUS GRANT  COMMAND: %s Time %5.2f\n", procId, g(BROADCAST_CMD.reqtype), GetSimTime());
  if (TRACE) {
    printf("Processor %d -- BROADCAST Command %s ", procId,  g(BROADCAST_CMD.reqtype));
    printf(" Address %x  Time %5.2f\n", BROADCAST_CMD.address, GetSimTime());
  }

   for (i=0; i < NUM_PROCESSORS; i++) 
     SemaphoreSignal(sem_bussnoop[i]);  // Wake up each BusSnooper 
   
   for (i=0; i < NUM_PROCESSORS; i++)
     SemaphoreWait(sem_bussnoopdone[i]);  // Wait for all Bus Snoopers to complete
}


/* Release the bus for use by another processor */
releaseBus(int procId) {  
	BUS_GRANT[procId] = FALSE;
	BUS_RELEASE = TRUE;
	ProcessDelay(epsilon);
}

/* *********************************************************************************************
BusSnooper process for each processor/cache. Woken  with broadcast command (in struct BROADCAST_CMD).
********************************************************************************************* */

void BusSnooper() {
  int blkNum, broadcast_tag;
  unsigned  busreq_type;
  unsigned address;
  unsigned requester;
  int BLKMASK = (-1 << BLKSIZE);
  int procId;
  struct genericQueueEntry writeback;

  procId = ActivityArgSize(ME); // Id of this Snooper
  if (TRACE)
    printf("BusSnooper[%d]: Activated at time %5.2f\n", procId, GetSimTime());
 
  printf("Im a skeleton Snooper that needs fattening!\nI will simply terminate after the number of simulation cycles set expire.\n");

  while (1) {

      SemaphoreWait(sem_bussnoop[procId]); // Wait for  a bus command
  
    /* Parse  Bus Command 
       Actions depend on whether this command is from my own front end or from another processor
    */
      address = BROADCAST_CMD.address;

      busreq_type = BROADCAST_CMD.reqtype;

      broadcast_tag = (address >> BLKSIZE)/CACHESIZE;

      blkNum = (address >> BLKSIZE)%CACHESIZE;

/*  ************************************************************************************ 

Handle Bus Commands from my Front End. 
  
1.  Before handling the cache miss, I may need to write back the cache block being evicted if it is in M state. 
Use the provided function "writebackMemory()" to write the values to memory.
Then simulate the memory access time by calling "ProcessDelay(MEM_CYCLE_TIME)". 
My statistics counter "cache_writebacks[ ]" counter should be incremented. 

2. Simulate the memory access time on a read by calling "ProcessDelay(MEM_CYCLE_TIME)".  
To read the values of the  block from memory on a cache miss use the function "readFromMemory()"; 

3. After the missed block is read into cache the tag needs to be updated.

4. In the  case that the cache block that we are reading is actually being flushed by some other cache (that has the 
block in the M state) we should make sure that before the "readFromMemory()" of step 2, the memory has been updated 
by the other cache. The "ProcessDelay(epsilon)" command orders the concurrent memory write and read so we read memory 
only after the memory write is completed. Note we  delay for only one  for MEM_CYCLE_TIME; the delays by the writer
process and the reader process overlap.

4. After handling  the command the snooper should delay by one cycle using "ProcessDelay(CLOCK_CYCLE)" before 
it signals  "sem_busnoopdone[ ]" and returning. This simulates the time required for the Bus Snoopers  execution.
  
************************************************************************************ */

    if (BUS_GRANT[procId] == TRUE)  { // My own Front End  initiated this request


    if(CACHE[procId][blkNum].STATE == M){
      if(TRACE){
        
        printf("CACHE[%d][%d] is M STATE\n",proId,blkNum);
      }

      ProcessDelay(MEM_CYCLE_TIME);

      writebackMemory(((blkNum*CACHESIZE+CACHE[procId][blkNum].TAG)<<BLKSIZE),CACHE[procId][blkNum].DATA);     
      
      cache_writebacks[procId]++;
    }

    switch (busreq_type) {  
   
    case (INV): {

      break;
    }

    case (BUS_RD): {
      ProcessDelay(epsilon);

      readFromMemory(address & BLKMASK,CACHE[proId][blkNum].DATA);

      ProcessDelay(MEM_CYCLE_TIME);

      if(DEBUG){
        displayCacheBlock(proId,blkNum);
      }

      CACHE[proId][blkNum].TAG = broadcast_tag;

      break;
    }

    case (BUS_RDX): {
      ProcessDelay(epsilon);

      readFromMemory(address & BLKMASK,CACHE[proId][blkNum].DATA);

      ProcessDelay(MEM_CYCLE_TIME);

      if(DEBUG){
        displayCacheBlock(proId,blkNum);
      }

      CACHE[proId][blkNum].TAG = broadcast_tag;

      break;
    }
    }
	
    if(TRACE){
      printf("%d is done at %5.2f\n",proId, GetSimTime() );
    }
    ProcessDelay(CLOCK_CYCLE);


    SemaphoreSignal(sem_bussnoopdone[procId]);  //  Uncomment Signal when coding
    continue;
    }

  
/*  ************************************************************************************ 


   Handle  Bus Command  from another processor
  
1.  Ignore command if the block is INVALID in my cache.

2.  To flush a requested cache block use the provided function "writebackMemory()".
After copying to memory simulate the memory access time by calling "ProcessDelay(MEM_CYCLE_TIME)".       

3.  To change the state of a cache block call  the provided function "atomicUpdate()".

4. After handling the command, delay by one cycle using "ProcessDelay(CLOCK_CYCLE)" before performing the signal 
on "sem_bussnoopdone[ ]" and returning. This simulates the time required for this execution.

5. If you receive an INV or BUS_RDX signal from some other processor when in the transient state SM,
 you must update the statistics counters: decrement my "cache_upgrades[ ]" by 1 and increment my "cache_write_misses[ ]" by 1.
Also increment your "numUtoX[]"counter by 1. 
  

************************************************************************************ */

// Check for valid block in the cache; else ignore command.
    if ((CACHE[procId][blkNum].STATE == I)|| (CACHE[procId][blkNum].TAG != broadcast_tag))
    {
      if (TRACE)
        printf("%d ignore at %5.2f\n", procId, GetSimTime());

      ProcessDelay(CLOCK_CYCLE);

      SemaphoreSignal(sem_bussnoopdone[procId]);

      continue;
    }

    switch (busreq_type) { // Handling command from other processor for a block in my cache
    
    case (INV): {
      if (CACHE[procId][blkNum].STATE == SM)
        {
          cache_write_misses[procId]++;

          cache_upgrades[procId]--;

          CACHE[procId][blkNum].STATE = IM;
          
          numUtoX[procId]++;
        }

        else{
          CACHE[procId][blkNum].STATE = I;
        }

      break;
    }
    case (BUS_RD): {
      if (CACHE[procId][blkNum].STATE == M)
        {
          atomicUpdate(procId, blkNum, S);

          writebackMemory(address & BLKMASK, CACHE[procId][blkNum].DATA);

          ProcessDelay(MEM_CYCLE_TIME);

          if (DEBUG)
          {
            displayCacheBlock(procId, blkNum);
          }
        }

      break;
    }  
    case (BUS_RDX): {
      if (CACHE[procId][blkNum].STATE == M||CACHE[procId][blkNum].STATE == S)
        {
          atomicUpdate(procId, blkNum, I); 

          writebackMemory(address & BLKMASK, CACHE[procId][blkNum].DATA);

          ProcessDelay(MEM_CYCLE_TIME);

          if (DEBUG){
            displayCacheBlock(procId, blkNum); 
          }
        }
        else if (CACHE[procId][blkNum].STATE == SM)
        {

          cache_write_misses[procId]++;

          cache_upgrades[procId]--;

          CACHE[procId][blkNum].STATE = IM; 
      
          numUtoX[procId]++;
        }

      break;
    }
    }

    if(TRACE){
      printf("%d is done at %5.2f\n",proId, GetSimTime() );
    }
    ProcessDelay(CLOCK_CYCLE);   
    SemaphoreSignal(sem_bussnoopdone[procId]);   // Uncomment Signal while coding
  }
}






/* *********************************************************************************************
Bus Arbiter checks for an asserted  BUS_REQUEST signal every clock cycle.
*********************************************************************************************/
void BusArbiter()
{
  int job_num, i;
static int procId = 0;

job_num = ActivityArgSize(ME) - 1;

 if (TRACE)
   printf("Bus Arbiter Activated at time %3.0f\n", GetSimTime());
 
 while(1){

       ProcessDelay(CLOCK_CYCLE);
       if (BUS_RELEASE == FALSE)   // Current bus-owner will release the bus when done
	 continue;


     for (i=0; i < NUM_PROCESSORS; i++)
       if (BUS_REQUEST[(i + procId)%NUM_PROCESSORS]) 
	 break;

     if (i == NUM_PROCESSORS) 
       continue;
     
     BUS_RELEASE = FALSE;     
     BUS_GRANT[(i+procId)%NUM_PROCESSORS] = TRUE;
     
       if (TRACE)
	 printf("BUS GRANT: Time %5.2f.  BUS_GRANT[%d]: %s\n", GetSimTime(), procId, BUS_GRANT[procId] ? "TRUE" : "FALSE");

     procId = (procId+1) % NUM_PROCESSORS;
    
 }
}

