#include <stdio.h>
#include <sys/time.h>
#include <signal.h>
#include <stdlib.h>
#include <ucontext.h>
#include <unistd.h>

#include "mythread.h"


extern void function_thread(int sec);

int read_disk_thread()
{
  read_disk();
  read_disk();
  read_disk();
  mythread_exit();
}


int main(int argc, char *argv[])
{
  int a, b;
  int f, g, h, p;


  mythread_setpriority(HIGH_PRIORITY);

  // At the start, interrupt the main so the idle thread has to be called.
  // But first create a thread that ends instantaneusly to call the init.
  if((mythread_create(function_thread,HIGH_PRIORITY,0)) == -1){
    printf("thread failed to initialize\n");
    exit(-1);
  }
  read_disk();
  read_disk();
  read_disk();
  printf("\n\n");



  // Then add two LPT that work only when the main interrupts itself
  if((f = mythread_create(function_thread,LOW_PRIORITY,3)) == -1){
    printf("thread failed to initialize\n");
    exit(-1);
  }
  if((g = mythread_create(function_thread,LOW_PRIORITY,3)) == -1){
    printf("thread failed to initialize\n");
    exit(-1);
  }
  // Interrumpt the main some times so both LPTs have time to swap at least once
  read_disk();
  read_disk();
  read_disk();
  read_disk();
  for (a=0; a<10; ++a) {
    for (b=0; b<30000000; ++b);
  }


  printf("\n\n");
  printf(" - SET MAIN TO LOW PRIORITY - \n");
  mythread_setpriority(LOW_PRIORITY);

  // Introduce 2 threads that will be waiting simultaneously while LPTs run
  if((h = mythread_create(read_disk_thread,HIGH_PRIORITY,1)) == -1){
    printf("thread failed to initialize\n");
    exit(-1);
  }
  if((p = mythread_create(read_disk_thread,HIGH_PRIORITY,1)) == -1){
    printf("thread failed to initialize\n");
    exit(-1);
  }


     
  for (a=0; a<10; ++a) {
    for (b=0; b<30000000; ++b);
  }	

  mythread_exit();	
  
  printf("This program should never come here\n");
  
  return 0;
} /****** End main() ******/


