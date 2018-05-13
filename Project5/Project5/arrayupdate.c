#include "global.h"


FILE *fp;


struct tracerecord tracerec;
int a[TOTALSIZE];
int numRecords = 0;


void init(int *p, int size) {
  int i;
  for (i=0; i  < size; i++)
    *p++ = i;
}

 
int  * record(FILE *fp, int *q, int type, int data){
   tracerec.address = (long unsigned) q;
   //    tracerec.delay= (int) (MINDELAY +  (drand48() * (MAXDELAY - MINDELAY)));
      tracerec.delay= (int) MAXDELAY;
   tracerec.type = type;
   tracerec.data = data;
   numRecords++;
   fwrite(&tracerec, sizeof(struct tracerecord), 1,fp);
   return q;
 }



// Processor i gets an inteleaved partition of the array.
dointerleave(FILE *fp, int id)
{
  int i;
  int *p;
  int temp;


  for (i=0, p = a + id; i < TOTALSIZE; i +=  NUM_PROCESSORS){
    temp =    *(record(fp, p,  1, *p));
    temp = temp + 1000;
    *(record(fp, p,  0, temp)) = temp;
    p = p+NUM_PROCESSORS;
  }
  
}


// Processor i gets the ith chunk of adjacent array elements.
dochunk(FILE *fp, int id)
{
  int i;
  int *p;
  int chunksize = TOTALSIZE/NUM_PROCESSORS;
  int temp;

  for (i=0,  p = a + id*chunksize; i < chunksize; i++)    {
    temp =    *(record(fp, p,  1, *p));
    temp = temp + 1000;
    *(record(fp, p,  0, temp)) = temp;
    p++;
  }
}


main()
{
  int i;
  int * base = a;
  char filename[20];

  fp = fopen("header", "w+");
  printf("Array start address: %p\n",a);
  fwrite(&base, sizeof(int *), 1,fp);

 
  init(a, TOTALSIZE);
  
  for (i=0; i < NUM_PROCESSORS; i++) {
    sprintf(filename, "memtrace%d\0",i);
    fp = fopen(filename,"w+");
    numRecords = 0;

    if (CHUNK)
      dochunk(fp,i);
    else
      dointerleave(fp,i);
    
    fclose(fp);
    printf("Created Trace File \"%s\"  with %d records\n", filename,numRecords);
  }


}

