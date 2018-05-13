#include "global.h"

extern struct cacheblock CACHE[][CACHESIZE];  // Cache

// Cache Statistics
extern int cache_write_hits[], cache_write_misses[];
extern int cache_upgrades[],cache_read_hits[], cache_read_misses[];
extern int cache_writebacks[], converted_cache_upgrades[];
extern int numCacheToCacheTransfer[], numMemToCacheTransfer[];

extern struct busrec BROADCAST_CMD;   // Bus Commands
extern int BUS_REQUEST[], BUS_GRANT[], BUS_RELEASE; // Bus Handshake Signals

extern SEMAPHORE *sem_bussnoop[]; // Signal each snooper that a bus command is being broadcast
extern SEMAPHORE *sem_bussnoopdone[]; // Signal that command has been handled at this snooper 
extern int CACHE_READY;
extern int CACHE_TRANSFER;

extern int map(int), f(int), g(int);
extern displayCacheBlock(int, int);

extern PFlag, PBit[];
extern OFlag, OBit[];
 
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

transferCache(int *blkDataPtrSrc, int *blkDataPtrDest) {  // Copy memory block to cache 
  int i;
  for (i=0; i < INTS_PER_BLOCK; i++)
    blkDataPtrDest[i] = blkDataPtrSrc[i];
}


int wireOR(int bits[]) {
  int i;
  int Flag;
  Flag = FALSE;
  for (i=0; i < NUM_PROCESSORS; i++)
    Flag  |=  bits[i];
  return Flag;
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
    printf("Processor %d --  Made BUS REQUEST Time %5.2f\n", procId, GetSimTime());

  while (BUS_GRANT[procId] == FALSE)  // Wait for BUS_GRANT
      ProcessDelay(CLOCK_CYCLE);

  BUS_REQUEST[procId] = FALSE;   // De-assert request


  // Create Bus Command based on type of request and current state
  blkNum = (req->address >> BLKSIZE) % CACHESIZE;
  op = req->type;


  BROADCAST_CMD.address  =  req->address;
  if  ((CACHE[procId][blkNum].STATE == SM) || (CACHE[procId][blkNum].STATE == OM)) 
    BROADCAST_CMD.reqtype  = INV;
  else  if  (op == LOAD)
    BROADCAST_CMD.reqtype  = BUS_RD;
  else 
    BROADCAST_CMD.reqtype  = BUS_RDX;
  

  if (TRACE)
    printf("Processor %d -- Received BUS GRANT.  Broadcast COMMAND: %s Address: %x Time %5.2f\n", procId, g(BROADCAST_CMD.reqtype), BROADCAST_CMD.address, GetSimTime());

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


receiveCacheTransfer() {
       CACHE_READY = TRUE; // ASSERT CACHE_READY
       while (CACHE_TRANSFER == FALSE)
	 ProcessDelay(CLOCK_CYCLE);
       CACHE_READY = FALSE;  // DE-ASSERT CACHE_READY
}

receiveMemTransfer(int procId, int blkNum, int address) {
    int BLKMASK = (-1 << BLKSIZE);

    ProcessDelay(MEM_CYCLE_TIME);
  readFromMemory(address & BLKMASK, CACHE[procId][blkNum].DATA);  // Read block from MEMORY  into cache
}

doWriteback(int procId, int blkNum) {
  struct genericQueueEntry writeback;
	writeback.address = (CACHE[procId][blkNum].TAG * TAGSHIFT) | (blkNum << BLKSIZE);
	cache_writebacks[procId]++; 
	writebackMemory(writeback.address, CACHE[procId][blkNum].DATA);  // Copy cache block contents to MEMORY
	ProcessDelay(MEM_CYCLE_TIME);
}

sendCacheTransfer(int procId, int requester, int blkNum) {
      if (DEBUG)
	printf("Snooper %d doing a cache-to-cache transfer for Block %d to Processor %d at time %5.2f\n", procId, blkNum, requester, GetSimTime()); 

      while (CACHE_READY == FALSE)
	ProcessDelay(CLOCK_CYCLE);
      transferCache(CACHE[procId][blkNum].DATA, CACHE[requester][blkNum].DATA);  // Allowing access to remote processor's cache
      ProcessDelay(CACHE_TRANSFER_TIME); 
      CACHE_TRANSFER = TRUE;
      while  (CACHE_READY == TRUE) {
	ProcessDelay(CLOCK_CYCLE);
      }
      CACHE_TRANSFER = FALSE;
}



getRequester() {
  int i;
  for (i = 0; i < NUM_PROCESSORS; i++) {
    if (BUS_GRANT[i] == TRUE) 
      break;
  }
  if (i == NUM_PROCESSORS) {
    printf("No requester when BusSnooper activated at time %5.2f\n", GetSimTime());
    exit(1);
  }
  return(i);
}
/* *********************************************************************************************
BusSnooper process for each processor/cache. Woken  with broadcast command (in struct BROADCAST_CMD).
********************************************************************************************* */

void BusSnooper() {
  int blkNum, broadcast_tag;
  unsigned  busreq_type;
  unsigned address;
  unsigned requester;
  int procId;
  int i;

  procId = ActivityArgSize(ME); // Id of this Snooper
  if (TRACE)
    printf("BusSnooper[%d]: Activated at time %5.2f\n", procId, GetSimTime());
 

  while (1) {
    SemaphoreWait(sem_bussnoop[procId]); // Wait for  a bus command
    if (BUSTRACE)
      printf("BusSnooper[%d] --  Woken with Bus COMMAND  at time %5.2f\n", procId, GetSimTime());


    // Parse  Bus Command 
  address = BROADCAST_CMD.address;
  busreq_type = BROADCAST_CMD.reqtype;
  blkNum =  (address >> BLKSIZE)% CACHESIZE;
  broadcast_tag = (address >> BLKSIZE)/ CACHESIZE;
  requester = getRequester();


    if (BUS_GRANT[procId] == TRUE)  { // My own Front End  initiated this request
  
    PBit[procId] = FALSE;
    OBit[procId] = FALSE;    // See comment below on the meaning and the need for an OBIT in the simulation 

    ProcessDelay(epsilon);

    PFlag = wireOR(PBit);
    OFlag = wireOR(OBit);

    // Note in addition to the Present bit PFlag  we are creating another global-OR signal OFlag.
    // This global signal should be set to TRUE if the block being request will be provided by a
    // cache-to-cache transfer  rather than by reading MEM.

    // In the real system the MEM will always try to respond but will be aborted if some other cache
    // is able to  do a  cache-to-cache transfer.  Since we don't model the memory controller or
    // MEM_ABORT signals we use the global OFlag  in the simulation.

    // By examining the OFlag value the receiving cache can decide whether to use 
    // receiveCacheTransfer() or receive MemTransafer() to read the cache block.
    
    // The appropriate handshaking protocol between caches is done within the sendCacheTransfer() and
    // receiveCacheTransfer() functions. The delays for both cache and memory transfers have also been 
    // already done within thos finctions.
 
    // As mentioned earlier, in  reality this signal is not needed since the receiving cache will just read the
    // data from the bus when a low-level data ready signal is asserted by the sender (be it MEM or another cache).
    // We are not modeling the hardware down to that level so we use OFlag.

    // Also note with MOESI  the cache block has an additional DIRTY bit that is set when the block is written.
    // Recall that due to cache-to-cache transfers a block in the S state can be DIRTY!!

    if (DEBUG)
      printf("PFlag : %s   OFLag: %s\n", PFlag?"TRUE":"FALSE", OFlag ? "TRUE" : "FALSE");
   
    //  Handle the three possible requests initiated by my own front end.

    switch (busreq_type) {  
    case (INV): {
     
      }
      break;
    
      

    case (BUS_RD): {

      /* 
	 1. If necessary write back the block using doWriteback() and set the DIRTY bit to FALSE.
	 2. Set cache tag for new block and read the value into cache. Update my "numCacheToCacheTransfer" or  
	    "numMemToCacheTransfer" counter  as appropriate.
	 3. Set the correct state of the new cache block using atomicUpdate()
      */
    }
      break;

      
    case BUS_RDX: { 
    
      /* 
	 1. If necessary write back the block using doWriteback() and set the DIRTY bit to FALSE.
	 2. Set the correct state of the new cache block using atomicUpdate()
	 3. Set cache tag for new block and read the value into cache. Update my  "numCacheToCacheTransfer" or  
	 "numMemToCacheTransfer" counter  as appropriate.
      */ 
    }
      break;
    }
    
    ProcessDelay(CLOCK_CYCLE);
    SemaphoreSignal(sem_bussnoopdone[procId]);  // Finished handling Bus Command from my own Front End
    if (BUSTRACE)
      printf("Snooper %d Done with my own %s request. Time: %5.2f\n", procId, g(busreq_type), GetSimTime());
    continue;
    }
  

  // If the Bus Command is from another processor
 

    /*  1. Set my PBit  and OBit appropriately */


  switch (busreq_type) { // Handling command from other processor for a block in my cache
    
  case (INV): // Provided in full
    switch (CACHE[procId][blkNum].STATE) {
    case S:
    case O:
      atomicUpdate(procId, blkNum, I);
      break;

    case SM:
    case OM:
      atomicUpdate(procId, blkNum,I);
      cache_write_misses[procId]++;  
      cache_upgrades[procId]--;
      converted_cache_upgrades[procId]++;
      break;
   }
    break;
  
    

  case (BUS_RD): 
    /* Handle BUS_RD based on the state of the cache block
     1. Update new cache state as needed using atomicUpdate()
     2. Use sendCacheTransfer() if you must do a cache-to-cache transfer
    */

 switch (CACHE[procId][blkNum].STATE) {
   
    }
    break;


   case (BUS_RDX): 

    /* Handle BUS_RDX based on the state of the cache block
     1. Update new cache state as needed using atomicUpdate()
     2. Use sendCacheTransfer() if you must do a cache-to-cache transfer 
     3. Update statistics of cache_write_misses,cache_upgrades, and converted_cache_upgrades  as needed.
    */

     switch (CACHE[procId][blkNum].STATE) {
    
     }

     // Set cache block to CLEAN
     break;
  }

      if (BUSTRACE)
	printf("Snooper %d finished %s at time %5.2f. CACHE[%d][%d].STATE: %s\n", procId, g(busreq_type), GetSimTime(),  procId, blkNum, f(CACHE[procId][blkNum].STATE));
     ProcessDelay(CLOCK_CYCLE);      
     SemaphoreSignal(sem_bussnoopdone[procId]);
  }
}






/* *********************************************************************************************
Bus Arbiter checks for an asserted  BUS_REQUEST signal every clock cycle.
*********************************************************************************************/
void BusArbiter(){
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

