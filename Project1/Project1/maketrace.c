#include <stdlib.h>
#include<stdio.h>
#include <math.h>
#include "global.h"




FILE *fp[MAX_NUM_THREADS];
struct tracerecord tracerec;
int a[TOTALSIZE];
int b[TOTALSIZE];
int c[TOTALSIZE];
int d[TOTALSIZE];



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
  int temp = -1;

  for (i=0,  p = data;  i < TOTALSIZE; i++)    {
    *(record(p,  WRITE, fp)) = temp;
    p++;
  }
}


main()
{
  fp[0] = fopen("memtrace0","w+");
    printf("Array start address: %p\n",a);
    init(a, TOTALSIZE);
    dochunk(a, fp[0]);
    fclose(fp[0]);
    
  printf("Trace file %s  with size %d  records created\n", "memtrace0", TOTALSIZE);

  if (MAX_NUM_THREADS > 1) {
  fp[1] = fopen("memtrace1","w+");  
  printf("Array start address: %p\n",b);
 init(b, TOTALSIZE);
  dochunk(b, fp[1]);
  fclose(fp[1]);
   printf("Trace file %s  with size %d  records created\n", "memtrace1", TOTALSIZE);
  }


  if (MAX_NUM_THREADS > 2) {
  fp[2] = fopen("memtrace2","w+");
  printf("Array start address: %p\n",c);
  init(c, TOTALSIZE);
  dochunk(c, fp[2]);
  fclose(fp[2]);
  printf("Trace file %s  with size %d  records created\n", "memtrace2", TOTALSIZE);
  }



  if (MAX_NUM_THREADS > 3) {
  fp[3] = fopen("memtrace3","w+");
  printf("Array start address: %p\n",d);
  init(d, TOTALSIZE);
  dochunk(d, fp[3]);
  fclose(fp[3]);
  printf("Trace file %s  with size %d  records created\n", "memtrace3", TOTALSIZE);
  }

    if (MAX_NUM_THREADS > 4) {
      printf("Only able to handle 4 threads at present\n");
      exit(1);
    }

}


  


