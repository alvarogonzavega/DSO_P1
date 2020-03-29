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


  mythread_setpriority(HIGH_PRIORITY);
  if((f = mythread_create(function_thread,HIGH_PRIORITY,2)) == -1){
    printf("thread failed to initialize\n");
    exit(-1);
  }
  if((g = mythread_create(function_thread,HIGH_PRIORITY, 2)) == -1){
    printf("thread failed to initialize\n");
    exit(-1);
  }
  // Wait a while so it swaps to another (shorter) thread before creating the last one
  for (a=0; a<20; ++a) {
    for (b=0; b<30000000; ++b);
  }
  if((h = mythread_create(function_thread,HIGH_PRIORITY, 1)) == -1){
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


