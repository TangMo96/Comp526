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
  tracerec.delay= (int) MAXDELAY;
  tracerec.type = type;
  tracerec.data = data;
  numRecords++;
  fwrite(&tracerec, sizeof(struct tracerecord), 1,fp);
  return q;
}


maketrace(FILE *fp, int id) {
  int i;
  int *p;
  int  datasize = TOTALSIZE;
  int temp = 0;
  int mode;

  mode = 1;                  // mode = 1  implies WRITE; mode = 0 implies READ
 
   // Uncomment the floowing  line for demo3  and for the final part of Experiment II
  //  if (id == 2)  mode = 0;

  for (i=0,  p = a; i < datasize; i++)    {
    temp = *p + 1000;
   *(record(fp, p,  mode, temp)) = temp;
    p++;
  }
}


main()
{
  int i;
  int * base = a;
  char filename[20];

  fp = fopen("header","w+");
  printf("Array start address: %p\n",a);
  fwrite(&base, sizeof(int *), 1,fp);

  init(a, TOTALSIZE);

  for (i=0; i < NUM_PROCESSORS; i++) {
    sprintf(filename, "memtrace%d\0",i);
    fp = fopen(filename,"w+");
    numRecords = 0;
    maketrace(fp,i);
    fclose(fp);
    printf("Created Trace File \"%s\"  with %d records\n", filename,numRecords);
  } 
}

