#include <stdlib.h>
#include<stdio.h>
#include <math.h>
#include "global.h"


#define TOTALSIZE 100000
#define READ 1
#define WRITE 0


FILE *fp;
struct tracerecord tracerec;
int a[TOTALSIZE];
int numRead = 0;
int numWrite = 0;


void init(int *p, int size) {
  int i;
  for (i=0; i  < size; i++)
    *p++ = i;
  for (i=0; i  < size; i++)
    drand48();
}

 
 int  * record(int *q, int type){
   tracerec.address = (long unsigned) q;
   tracerec.type = type;
   tracerec.data= *q;
   fwrite(&tracerec, sizeof(struct tracerecord), 1,fp);
   return q;
 }


dochunk1() {
  int i;
  int *p;
  int temp;
  static turn = READ;
  static readCount = 1;
  static writeCount = 1;

  for (i=0,  p = a;  i < TOTALSIZE; i++)    {
    if (turn == READ) {
      *(record(p,  READ)) = temp;
      numRead++;
      readCount--;
      if (readCount == 0) {
	turn = WRITE;
	readCount = 1;
      }
      }
    else {
      *(record(p,  WRITE)) = temp;
      numWrite++;
      writeCount--;
      if (writeCount == 0) {
	turn = READ;
	writeCount = 1;
      }
    }
      p++;
  }
}


dochunk2() {
  int i;
  int *p;
  int temp;
  static turn = READ;
  static readCount = 3;
  static writeCount = 1;

  for (i=0,  p = a;  i < TOTALSIZE; i++)    {
    if (turn == READ) {
      *(record(p,  READ)) = temp;
      numRead++;
      readCount--;
      if (readCount == 0) {
	turn = WRITE;
	readCount = 3;
      }
      }
    else {
      *(record(p,  WRITE)) = temp;
      numWrite++;
      writeCount--;
      if (writeCount == 0) {
	turn = READ;
	writeCount = 1;
      }
    }
      p++;
  }
}


dochunk3() {
  int i;
  int *p;
  int temp;
  static turn = READ;
  static readCount = 1;
  static writeCount = 3;

  for (i=0,  p = a;  i < TOTALSIZE; i++)    {
    if (turn == READ) {
      *(record(p,  READ)) = temp;
      numRead++;
      readCount--;
      if (readCount == 0) {
	turn = WRITE;
	readCount = 1;
      }
      }
    else {
      *(record(p,  WRITE)) = temp;
      numWrite++;
      writeCount--;
      if (writeCount == 0) {
	turn = READ;
	writeCount = 3;
      }
    }
      p++;
  }
}


main()
{
  fp = fopen("memtrace","w+");
  
  printf("Array start address: %p\n",a);
 printf("READ FRACTION: %5.2f\n",READ_FRACTION);
  
  init(a, TOTALSIZE);

  if (READ_FRACTION == 0.5)
    dochunk1();
  else if (READ_FRACTION == 0.75)
    dochunk2();
  else if (READ_FRACTION == 0.25)
    dochunk3();

  fclose(fp);

  printf("Trace file %s  with size %d  records created\n", "memtrace", TOTALSIZE);
  printf("numRead records: %d   numWrite records: %d\n", numRead, numWrite);

}
  


