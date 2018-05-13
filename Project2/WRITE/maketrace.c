#include <stdlib.h>
#include<stdio.h>
#include <math.h>
#include "global.h"


FILE *fp[MAX_NUM_THREADS];
struct tracerecord tracerec;

void init(int *p, int size) {
  int i;
  for (i=0; i  < size; i++)
    *p++ = i;
}

 
int  * record(int *q, int type, FILE *fp){
   tracerec.address = (long unsigned) q;
   tracerec.type = type;
   tracerec.data= *q;
   fwrite(&tracerec, sizeof(struct tracerecord), 1,fp);
   return q;
 }


dochunk(int *data, FILE *fp)
{
  int i;
  int *p;
  int temp;

  for (i=0,  p = data;  i < TOTALSIZE; i++)    {
    *(record(p,  WRITE, fp)) = temp;  // Change READ to WRITE for an all-write workload
    p++;
  }
}


main() {
    int  *a, *b;

// Create arrays "a" and "b"  of size 2*TOTALSIZE integers with starting address at a multiple of 128.
    posix_memalign((void **) &a, 128, 2*TOTALSIZE * sizeof(int)); 
    posix_memalign((void **) &b, 128, 2*TOTALSIZE * sizeof(int));


  fp[0] = fopen("memtrace0","w+");
  printf("Array start address: %p\n",a);
  
  init(a, TOTALSIZE);
  dochunk(a, fp[0]); 
  fclose(fp[0]);

  printf("Trace file %s  with size %d  records created\n", "memtrace0", TOTALSIZE);

  fp[1] = fopen("memtrace1","w+");
  
  printf("Array start address: %p\n",b);
  
  init(b, TOTALSIZE);
  dochunk(b+16, fp[1]);  //Change "b+16" to "b" for Part B
  fclose(fp[1]);

  printf("Trace file %s  with size %d  records created\n", "memtrace1", TOTALSIZE);
}
  

