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

//Queues where we will have the threads ready to execute
struct queue* readyLOW;
struct queue* readyHIGH;

//We add a third queue for blocked threads
struct queue* waiting;

// Next thread's id will have this value in order to not repeat ids.
// Its assumed that there will never be 2^31 created threads during an execution.
static int next_tid = 1;

/* Thread control block for the idle thread */
static TCB idle;

static void idle_function()
{
  while(1);
}

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

  /* Create context for the idle thread */
  if(getcontext(&idle.run_env) == -1)
  {
    perror("*** ERROR: getcontext in init_thread_lib");
    exit(-1);
  }

  idle.state = IDLE;
  idle.priority = SYSTEM;
  idle.function = idle_function;
  idle.run_env.uc_stack.ss_sp = (void *)(malloc(STACKSIZE));
  idle.tid = -1;

  if(idle.run_env.uc_stack.ss_sp == NULL)
  {
    printf("*** ERROR: thread failed to get stack space\n");
    exit(-1);
  }

  idle.run_env.uc_stack.ss_size = STACKSIZE;
  idle.run_env.uc_stack.ss_flags = 0;
  idle.ticks = QUANTUM_TICKS;
  makecontext(&idle.run_env, idle_function, 1);

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
  readyLOW = queue_new();
  readyHIGH = queue_new();
  waiting = queue_new();

  /* Initialize disk & clock interrupts */
  init_disk_interrupt();
  init_interrupt();
}


/* Create and intialize a new thread with body fun_addr and one integer argument */
int mythread_create (void (*fun_addr)(),int priority,int seconds)
{
  int i;

  if(priority !=LOW_PRIORITY && priority != HIGH_PRIORITY){
    //Invalid priority
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

  t_state[i].tid = next_tid++;
  //We add the ticks
  t_state[i].ticks = QUANTUM_TICKS;
  t_state[i].run_env.uc_stack.ss_size = STACKSIZE;
  t_state[i].run_env.uc_stack.ss_flags = 0;
  makecontext(&t_state[i].run_env, fun_addr,2,seconds);
  if(priority==LOW_PRIORITY){ //Low priority thread

    //We are going to enqueue the thread
    TCB *low = &t_state[i];
    //First of all we disable interruptions
    disable_interrupt();
    //Now we enqueue the thread
    enqueue(readyLOW, low);
    //Finally we enable interruptions again
    enable_interrupt();

  }else{ //High priority thread

    //We are going to enqueue the thread
    TCB *high = &t_state[i];
    //We are going to check if the actual thread is Low priority
    //In which case we should change it right away
    if(running->priority==LOW_PRIORITY){

      //We are going to re-establish the QUANTUM_TICKS
      running->ticks = QUANTUM_TICKS;
      //First of all we disable interruptions
      disable_interrupt();
      //Now we enqueue the running thread
      enqueue(readyLOW, running);
      //Finally we enable interruptions again
      enable_interrupt();
      //Print the message
      printf("*** THREAD %i PREEMTED: SET CONTEXT OF %i\n", (running->tid), high->tid);
      //Then we call the Activator
      activator(high);


    }else{ //The actual thread is High Priority

      //First of all we disable interruptions
      disable_interrupt();
      //Now we enqueue the thread
      enqueue(readyHIGH, high);
      //Finally we enable interruptions again
      enable_interrupt();

    }


  }

  return next_tid-1;

}
/****** End my_thread_create() ******/

/* Read disk syscall */
int read_disk()
{

  //We must free all the info on the cache
  if(data_in_page_cache()!=0){

    //The actual thread request a red from the disk
    //We print the information
    printf("*** THREAD %d READ FROM DISK\n", (running->tid));
    //We need to enqueue it on the waiting list
    //First we disable all interruptions
    disable_interrupt();
    //Now we enqueue it
    enqueue(waiting, running);
    //We call the scheduler
    TCB* new = scheduler();
    //Now we can enable interruptions
    enable_interrupt();
    //We save the current thread
    current = new->tid;
    //Lastly we call the Activator
    activator(new);

  }

  return 1;

}

/* Disk interrupt  */
void disk_interrupt(int sig)
{

  //We check if the waiting threads queue is empty
  if(!(queue_empty(waiting))){

    //We have a thread waiting to the disk interruption
    //First we disable the interruptions
    disable_interrupt();
    //Then we dequeue the thread
    TCB* new = dequeue(waiting);
    //If it is low priority we enqueue it on the low priority list
    if((new->priority)==LOW_PRIORITY) enqueue(readyLOW, new);
    //Otherwise we enqueue it on the high priority list
    else enqueue(readyHIGH, new);
    //Now we can enable interruptions
    enable_interrupt();
    //And print the information
    printf("*** THREAD %d READY\n", (new->tid));

    // If the running thread is idle, change execution to the thread that just got ready
    if (running->tid == -1)
    {
      disable_interrupt();
      TCB* new = scheduler();
      enable_interrupt();
      activator(new);
    }

  }


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

  if(queue_empty(readyLOW) && queue_empty(readyHIGH) && !queue_empty(waiting)) {

    //If we don´t have more threads on the queues of the priority lists
    current = idle.tid;
    return &idle;

  }
  else if(queue_empty(readyHIGH) && queue_empty(readyLOW) && queue_empty(waiting)) {

    //If we do not have any thread
    enable_interrupt();
    printf("*** FINISH\n");
    exit(1);

  }
  else if(queue_empty(readyHIGH)) {

    //If we do not have any High Priority threads we are in Round Robin
    //We simply pass the first element
    TCB * new = dequeue(readyLOW);
    return new;

  }
  else{

    //We have a High Priority thread
    //Find the next lowest REMAINING execution time thread in high priority
    struct my_struct* seeker = readyHIGH->head;
    TCB* new = NULL;
    int lowest_ticks = 2000000;
    while(seeker != NULL)
    {
      TCB* thr = seeker->data;
      if(thr->remaining_ticks < lowest_ticks) {
        new = thr;
      }
    }

    if (new == NULL || new->priority != HIGH_PRIORITY) {
      // It shouldnt get here
      printf("ERROR: something went wrong at scheduler\n");
      exit(-1);
    }

    return new;
  }
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

  // Lower the remaining ticks of any thread
  running->remaining_ticks--;
  if((running->remaining_ticks)==0) mythread_exit();

  if(running->priority == LOW_PRIORITY){

    running->ticks--;

    if(queue_empty(readyHIGH)) { //If we only have Low Priority threads (as supposed)

      if((running->ticks)==0)
      {
        TCB* previous;
        //We re-establish the ticks to QUANTUM_TICKS
        running ->ticks = QUANTUM_TICKS;
        //We are going to call to enqueue the actual thread
        //So we need to disable interruptions
        disable_interrupt();
        //Aux to call in activator
        previous = running;
        //Enqueue the thread
        enqueue(readyLOW, previous);
        //We call scheduler to get the new thread
        TCB * next = scheduler();
        //We can enable interruptions now
        enable_interrupt();
        //We call activator to do the SWAPCONTEXT
        activator(next);

      }
    }
    else {
      // Shouldn't reach here. Its for depuration.
      printf("ERROR: Found a high priotity thread while a low one was running. This shouldn't happen.\n");
      exit(-1);
    }
  }
  
  // Nothing else to do for high priority threads
}


/* Activator */
void activator(TCB* next)
{

  TCB * actual = running;
  //SWAPCONTEXT
  running = next;
  current = running->tid;
  if(actual->tid==-1) printf("*** THREAD READY: SET CONTEXT TO %d\n", next->tid);
  else if(actual->remaining_ticks==0) printf("*** THREAD %d TERMINATED: SETCONTEXT OF %d\n", actual->tid, next->tid);
  else if(actual->tid != next->tid) printf("*** SWAPCONTEXT FROM %d TO %d\n", (actual->tid), (next->tid));

  if(swapcontext (&(actual->run_env), &(next->run_env))==-1){
    printf("ERROR DURING THE SWAPCONTEXT!!");
    exit(-1);
  }

}
