#include<stdlib.h>
#include<stdio.h>
#include <math.h>
#include "sim.h"

#define TRUE 1
#define FALSE 0

#define TRACE FALSE

#define READ 1
#define WRITE 0

#define READ_TIME  0.1
#define WRITE_TIME 0.4

#define FCFS_SCHED  TRUE
#define STRIPED    FALSE


/*
//we make a change to global.h to control the read response time.

#define READ_TAG_INCREMENT   0.5 * (1 - READ_FRACTION)
#define WRITE_TAG_INCREMENT  0.5 * READ_FRACTION

//This time, instead of making read/write tag increment a constant value, we make it related to read_fraction.
So when the read_fraction changes, read/write tag increment will also change to keep read response the same tendency.

*/
#define NUM_OUTSTANDING_REQUESTS 8
#define READ_FRACTION  0.5
#define MAX_STORAGEQ 1

#define READ_DELAY 0.0
#define WRITE_DELAY 0.0

#define READ_TAG_INCREMENT   0.5
#define WRITE_TAG_INCREMENT  0.5





#define MAX_NUM_THREADS 1
#define GROUP_SIZE  1
#define MAX_NUM_QUEUES 200
#define NUM_DEVICES 1

#define REQUEST_QUEUE NUM_DEVICES
#define ARRIVAL_RATE 500

#define CPU_DELAY epsilon

#define REQUEST_QUEUE_SIZE 500
#define DEVICE_REQUEST_QUEUE_SIZE 500


#define MIN_SERVICE_TIME 0.10
#define MAX_SERVICE_TIME 0.10
#define epsilon 0.00001

#define MAX_SIMULATION_TIME 10000.0 + epsilon

struct tracerecord {
  unsigned address;
  unsigned  data;
  int type;
};

struct genericQueueEntry {
  int threadId;
  unsigned  reqAddress;
  int type;
  unsigned data;
  double startTime;
  double tag;
};

struct queueNode {
  struct genericQueueEntry * data;
  struct queueNode * next;
};


