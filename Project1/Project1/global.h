#include<stdlib.h>
#include<stdio.h>
#include <math.h>
#include "sim.h"


#define TOTALSIZE 8192   // Controls size of the memory trace

#define CPU_DELAY 0.0

#define TRUE 1
#define FALSE 0

#define TRACE FALSE

#define READ 1
#define WRITE 0



#define MAX_NUM_THREADS 1
#define MAX_NUM_QUEUES 10

#define MEMORY_REQUEST_QUEUE 0
#define WRITEBACK_QUEUE 1


#define epsilon 0.00001

#define MAX_SIMULATION_TIME 1000000.0 + epsilon


#define CACHE_LOOKUP_TIME 1.0


#define MEMORY_READ_TIME 50.0
#define MEMORY_WRITE_TIME 50.0
#define WRITE_BUFFER_SEARCH_DELAY 1.0

#define CACHESIZE 2 // Size in Blocks
#define NUMWAYS 1  // Cache Associativity. Handles only this value
#define NUMSETS  CACHESIZE/NUMWAYS
#define BLKSIZE (0x1 << 5)  //   2^5 bytes per block

#define MAX_WRITE_BUFFER_SIZE 10

struct tracerecord {
  unsigned address;
  unsigned  data;
  int type;
};

struct genericQueueEntry {
  int threadId;
  unsigned  reqAddress;
  int type;
  unsigned * data;
  double startTime;
};

struct queueNode {
  struct genericQueueEntry * data;
  struct queueNode * next;
};

struct cacheblock {
  int V;
  int D;
  int TAG;
  unsigned * BLKDATA;
};
