#include <stdio.h>
#include <sys/time.h>
#include <signal.h>
#include <stdlib.h>
#include <ucontext.h>
#include <unistd.h>
#include "my_io.h"
#include "mythread.h"
#include "interrupt.h"
#include "queue.h"

TCB* scheduler();
void activator(TCB *next);
void timer_interrupt(int sig);


/* Array of state thread control blocks: the process allows a maximum of N threads */
static TCB t_state[N];

/* Current running thread */
static TCB* running;
static int current = 0;

/* Variable indicating if the library is initialized (init == 1) or not (init == 0) */
static int init=0;

//Queue where we will have the threads ready to execute
struct queue* ready;

void function_thread(int sec)
{
    //time_t end = time(NULL) + sec;
    while(running->remaining_ticks)
    {
      //do something
    }
    mythread_exit();
}


/* Initialize the thread library */
void init_mythreadlib()
{
  int i;

  t_state[0].state = INIT;
  t_state[0].priority = LOW_PRIORITY;
  t_state[0].ticks = QUANTUM_TICKS;

  if(getcontext(&t_state[0].run_env) == -1)
  {
    perror("*** ERROR: getcontext in init_thread_lib");
    exit(5);
  }

  for(i=1; i<N; i++)
  {
    t_state[i].state = FREE;
  }

  t_state[0].tid = 0;
  running = &t_state[0];
  ready = queue_new();

  /* Initialize disk & clock interrupts */
  init_disk_interrupt();
  init_interrupt();
}


/* Create and intialize a new thread with body fun_addr and one integer argument */
int mythread_create (void (*fun_addr)(),int priority,int seconds)
{
  int i;

  if(priority !=LOW_PRIORITY && priority!=HIGH_PRIORITY){
    //If the priority is invalid
    printf("The priority is invalid!!");
    exit(-1);

  }

  if (!init) { init_mythreadlib(); init=1;}

  for (i=0; i<N; i++)
    if (t_state[i].state == FREE) break;

  if (i == N) return(-1);

  if(getcontext(&t_state[i].run_env) == -1)
  {
    perror("*** ERROR: getcontext in my_thread_create");
    exit(-1);
  }

  t_state[i].state = INIT;
  t_state[i].priority = priority;
  t_state[i].function = fun_addr;
  t_state[i].execution_total_ticks = seconds_to_ticks(seconds);
  t_state[i].remaining_ticks = t_state[i].execution_total_ticks;
  t_state[i].run_env.uc_stack.ss_sp = (void *)(malloc(STACKSIZE));

  if(t_state[i].run_env.uc_stack.ss_sp == NULL)
  {
    printf("*** ERROR: thread failed to get stack space\n");
    exit(-1);
  }

  t_state[i].tid = i;
  //We add the ticks
  t_state[i].ticks = QUANTUM_TICKS;
  t_state[i].run_env.uc_stack.ss_size = STACKSIZE;
  t_state[i].run_env.uc_stack.ss_flags = 0;
  makecontext(&t_state[i].run_env, fun_addr,2,seconds);
  //We are going to enqueue the thread
  TCB *thread = &t_state[i];
  //First of all we disable interruptions
  disable_interrupt();
  //Now we enqueue the thread
  enqueue(ready, thread);
  //Finally we enable interruptions again
  enable_interrupt();
  return i;

}
/****** End my_thread_create() ******/

/* Read disk syscall */
int read_disk()
{
   return 1;
}

/* Disk interrupt  */
void disk_interrupt(int sig)
{

}

/* Free terminated thread and exits */
void mythread_exit() {

  printf("*** THREAD %d FINISHED\n", (mythread_gettid()));
  t_state[(mythread_gettid())].state = FREE;
  free(t_state[(mythread_gettid())].run_env.uc_stack.ss_sp);
  disable_interrupt();
  TCB* next = scheduler();
  enable_interrupt();
  current=next->tid;
  activator(next);

}


void mythread_timeout(int tid) {

    printf("*** THREAD %d EJECTED\n", tid);
    t_state[tid].state = FREE;
    free(t_state[tid].run_env.uc_stack.ss_sp);
    disable_interrupt();
    TCB* next = scheduler();
    enable_interrupt();
    current=next->tid;
    activator(next);

}


TCB* scheduler()
{

  if(queue_empty(ready)){

    //If we don´t have more threads on the queue we have finished
    enable_interrupt();
    printf("*** FINISH\n");
    exit(0);

  }

  //In another case we will pass the first element of the queue
  TCB * new = dequeue(ready);
  return new;

}

/* Sets the priority of the calling thread */
void mythread_setpriority(int priority)
{
  int tid = mythread_gettid();
  t_state[tid].priority = priority;
  if(priority ==  HIGH_PRIORITY){
    t_state[tid].remaining_ticks = 195;
  }
}

/* Returns the priority of the calling thread */
int mythread_getpriority(int priority) {
  int tid = mythread_gettid();
  return t_state[tid].priority;
}


/* Get the current thread id.  */
int mythread_gettid(){
  if (!init) { init_mythreadlib(); init=1;}
  return current;
}


/* Timer interrupt */
void timer_interrupt(int sig)
{

  (running->ticks)--;
  if((running->ticks)==0){

    //We re-establish the ticks to QUANTUM_TICKS
    running ->ticks = QUANTUM_TICKS;
    //We are going to call to enqueue the actual thread
    //So we need to disable interruptions
    disable_interrupt();
    //Aux to call in activator
    TCB *previous = running;
    //Enqueue the thread
    enqueue(ready, previous);
    //We call scheduler to get the new thread
    TCB * next = scheduler();
    //We can enable interruptions now
    enable_interrupt();
    //We stablish current to the thread scheduler returned
    current = next->tid;
    //We call activator to do the SWAPCONTEXT
    activator(next);

  }

}

/* Activator */
void activator(TCB* next)
{

  TCB * actual = running;
  if((actual->tid) != (next->tid)){

    //SWAPCONTEXT
    running = next;
    printf("*** SWAPCONTEXT FROM %d TO %d\n", (actual->tid), (next->tid));
    if(swapcontext (&(actual->run_env), &(next->run_env))==-1){

      printf("ERROR DURING THE SWAPCONTEXT!!");
      exit(-1);

    }

  }

  if((actual->tid)==FREE){

    running = next;
    printf("*** THREAD %d TERMINATED: SETCONTEXT OF %d\n", (actual->tid), (next->tid));
    if(setcontext (&(next->run_env))==-1){

      printf("ERROR DURING SETCONETXT!!");
      exit(-1);


    }


  }

}
