#include <stdio.h>
#include <sys/time.h>
#include <signal.h>
#include <stdlib.h>
#include <ucontext.h>
#include <unistd.h>

#include "mythread.h"


//Each thread executes this function
extern void function_thread(int sec);



int main(int argc, char *argv[])
{
  int a, b;
  int f, g, h;


  mythread_setpriority(LOW_PRIORITY);
  // First thread should be swapping with the main for a while
  if((f = mythread_create(function_thread,LOW_PRIORITY,3)) == -1){
    printf("thread failed to initialize\n");
    exit(-1);
  }

  // After some swaps, introduce a HPT that kicks out both other threads till its finished. 
  for (a=0; a<20; ++a) {
    for (b=0; b<30000000; ++b);
  }
  if((g = mythread_create(function_thread,HIGH_PRIORITY,2)) == -1){
    printf("thread failed to initialize\n");
    exit(-1);
  }

  // Once its finished, set the main in high priority.
  // Introduce a new, shorter HPT (H->H) that swaps the main.
  // In order to swap the new thread will be so short that it ends instantly.
  printf(" - SET MAIN TO HIGH PRIORITY - \n");
  mythread_setpriority(HIGH_PRIORITY);
  if((h = mythread_create(function_thread,HIGH_PRIORITY, 0)) == -1){
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


