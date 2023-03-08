// File:	mypthread.c

// List all group member's name: Paul Kim
// username of iLab: obawolo
// iLab Server: cp.csrutgers.edu

#include "mypthread.h"

// INITAILIZE ALL YOUR VARIABLES HERE
// YOUR CODE HERE

#define STACK_S 8192 
#define MAX_SIZE 100
#define INTERVAL 20000

tcb *currentThread, *prevThread;
list *runningQueue[MAX_SIZE];
list *allThreads[MAX_SIZE];
ucontext_t cleanup;
sigset_t signal_set;
mySig sig;

struct itimerval timer, currentTime;

int mainRetrieved;
int timeElapsed;
int threadCount;
int notFinished;

// add to queue
void enqueue(list** q, tcb* insert)
{
  list *queue = *q;

  if(queue == NULL)
  {
    queue = (list*)malloc(sizeof(list));
    queue->thread = insert;
    queue->next = queue;
    *q = queue;
    return;
  }

  list *front = queue->next;
  queue->next = (list*)malloc(sizeof(list));
  queue->next->thread = insert;
  queue->next->next = front;

  queue = queue->next;
  *q = queue;
  return;
}

// remove from queue
tcb* dequeue(list** q)
{
  list *queue = *q;
  if(queue == NULL)
  {
    return NULL;
  }
  //queue is the last element in a queue at level i
  //first get the thread control block to be returned
  list *front = queue->next;
  tcb *tgt = queue->next->thread;
  //check if there is only one element left in the queue
  //and assign null/free appropriately
  if(queue->next == queue)
  { 
    queue = NULL;
  }
  else
  {
    queue->next = front->next;
  }
  free(front);

  
  if(tgt == NULL)
  {printf("WE HAVE A PROBLEM IN DEQUEUE\n");}

  *q = queue;
  return tgt;
}

// insert to list
void l_insert(list** q, tcb* jThread) //Non-circular Linked List
{
  list *queue = *q;

  if(queue == NULL)
  {
    queue = (list*)malloc(sizeof(list));
    queue->thread = jThread;
    queue->next = NULL;
    *q = queue;
    return;
  }

  list *newNode = (list*)malloc(sizeof(list));
  newNode->thread = jThread;

  // append to front of LL
  newNode->next = queue;
  
  queue = newNode;
  *q = queue;
  return;
}

// remove from list
tcb* l_remove(list** q)
{
  list *queue = *q;

  if(queue == NULL)
  {
    return NULL;
  }

  list *temp = queue;
  tcb *ret = queue->thread;
  queue = queue->next;
  free(temp);
  *q = queue;
  return ret;
}

// Search table for a tcb given a threadID
tcb* thread_search(pthread_t tid)
{
  int key = tid % MAX_SIZE;
  tcb *ret = NULL;

  list *temp = allThreads[key];
  while(allThreads[key] != NULL)
  {
    if(allThreads[key]->thread->tid == tid)
    {
      ret = allThreads[key]->thread;
      break;
    }
    allThreads[key] = allThreads[key]->next;
  }

  allThreads[key] = temp;

  return ret;
}

// thread Queues startup
void initializeQueues(list** runQ) 
{
  int i;
  for(i = 0; i < MAX_SIZE; i++) 
  {
    runningQueue[i] = NULL;
    allThreads[i] = NULL;
  }
  
}

// thread priority boosting
void maintenance()
{
  int i;
  tcb *tgt;


  // template for priority inversion
  for(i = 1; i < MAX_SIZE; i++)
  {
    while(runningQueue[i] != NULL)
    {
      tgt = dequeue(&runningQueue[i]);
      tgt->priority = 0;
      enqueue(&runningQueue[0], tgt);
    }
  }

  return;
}

// handle exiting thread: supports invoked/non-invoked pthread_exit call
void garbage_collection()
{
  // Block signal here

  notFinished = 1;

  currentThread->status = EXIT;
  
  //if pthread_create isn't called yet
  if(!mainRetrieved)
  {
    exit(EXIT_SUCCESS);
  }

  tcb *jThread = NULL; //any threads waiting on the one being garbage collected

  // dequeue all threads waiting on this one to finish
  while(currentThread->joinQueue != NULL)
  {
    jThread = l_remove(&currentThread->joinQueue);
    jThread->retVal = currentThread->jVal;
    enqueue(&runningQueue[jThread->priority], jThread);
  }

  // free stored node in allThreads
  int key = currentThread->tid % MAX_SIZE;
  if(allThreads[key]->thread->tid == currentThread->tid)
  {
    list *removal = allThreads[key];
    allThreads[key] = allThreads[key]->next;
    free(removal); 
  }

  else
  {
    list *temp = allThreads[key];
    while(allThreads[key]->next != NULL)
    {
      if(allThreads[key]->next->thread->tid == currentThread->tid)
      {
	list *removal = allThreads[key]->next;
	allThreads[key]->next = removal->next;
	free(removal);
        break;
      }
      allThreads[key] = allThreads[key]->next;
    }

    allThreads[key] = temp;
  }

  notFinished = 0;

  raise(SIGVTALRM);
}

void initializeMainContext()
{
  tcb *mainThread = (tcb*)malloc(sizeof(tcb));
  ucontext_t *mText = (ucontext_t*)malloc(sizeof(ucontext_t));
  getcontext(mText);
  mText->uc_link = &cleanup;

  mainThread->context = mText;
  mainThread->tid = 0;
  mainThread->priority = 0;
  mainThread->joinQueue = NULL;
  mainThread->jVal = NULL;
  mainThread->retVal = NULL;
  mainThread->status = READY;

  mainRetrieved = 1;

  l_insert(&allThreads[0], mainThread);

  currentThread = mainThread;
}

void initializeGarbageContext()
{
  memset(&sig,0,sizeof(mySig));
  sig.sa_handler = &scheduler;
  sigaction(SIGVTALRM, &sig,NULL);
  initializeQueues(runningQueue); //set everything to NULL
    
  //Initialize garbage collector
  getcontext(&cleanup);
  cleanup.uc_link = NULL;
  cleanup.uc_stack.ss_sp = malloc(STACK_S);
  cleanup.uc_stack.ss_size = STACK_S;
  cleanup.uc_stack.ss_flags = 0;
  makecontext(&cleanup, (void*)&garbage_collection, 0);

  // set thread count
  threadCount = 1;
}

// Signal handler to reschedule upon VIRTUAL ALARM signal
void scheduler(int signum)
{
  if(notFinished)
  {
    //printf("caught in the handler! Get back!\n");
    return;
  }


  //Record remaining time
  getitimer(ITIMER_VIRTUAL, &currentTime);

  timer.it_value.tv_sec = 0;
  timer.it_value.tv_usec = 0;
  timer.it_interval.tv_sec = 0;
  timer.it_interval.tv_usec = 0;
  setitimer(ITIMER_VIRTUAL, &timer, NULL);

  if(signum != SIGVTALRM)
  {
    //printf("[Thread %d] Signal Received: %d.\nExiting...\n", currentThread->tid, signum);
    exit(signum);
  }


  // Time elapsed = difference between max interval size and time remaining in timer
  //if the time splice runs to completion the else body goes,
  //else the if body goes, and the actual amount of time that passed is added to timeelapsed
  int timeSpent = (int)currentTime.it_value.tv_usec;
  int expectedInterval = INTERVAL * (currentThread->priority + 1);
  //printf("timeSpent: %i, expectedInterva %i\n", timeSpent, expectedInterval);
  if(timeSpent < 0 || timeSpent > expectedInterval)
  {
    timeSpent = 0;
  }
  else
  {
    timeSpent = expectedInterval - timeSpent;
  }

  
  timeElapsed += timeSpent;
  //printf("total time spend so far before maintenance cycle %i and the amount of time spent just now %i\n", timeElapsed, timeSpent);
  //printf("[Thread %d] Total time: %d from time remaining: %d out of %d\n", currentThread->tid, timeElapsed, (int)currentTime.it_value.tv_usec, INTERVAL * (currentThread->priority + 1));

  // check for maintenance cycle
  if(timeElapsed >= 10000000)
  {
    //printf("\n[Thread %d] MAINTENANCE TRIGGERED\n\n",currentThread->tid);
    maintenance();

    // reset counter
    timeElapsed = 0;
  }

  prevThread = currentThread;
  
  int i;

  switch(currentThread->status)
  {
    case READY: //READY signifies that the current thread is in the running queue

      if(currentThread->priority < MAX_SIZE - 1)
      {
	currentThread->priority++;
      }

      //put back the thread that just finished back into the running queue
      enqueue(&runningQueue[currentThread->priority], currentThread);

      currentThread = NULL;

      for(i = 0; i < MAX_SIZE; i++) 
      {
        if (runningQueue[i] != NULL)
        { 
          //getting a new thread to run
          currentThread = dequeue(&runningQueue[i]);
	  break;
        }
	else
	{
	}
      }

      if(currentThread == NULL)
      {
        currentThread = prevThread;
      }

      break;
   
    case YIELD: //YIELD signifies pthread yield was called; don't update priority

      currentThread = NULL;

      for (i = 0; i < MAX_SIZE; i++) 
      {
        if (runningQueue[i] != NULL)
        { 
          currentThread = dequeue(&runningQueue[i]);
	        break;
        }
      }
      
      if(currentThread != NULL)
      {
	//later consider enqueuing it to the waiting queue instead
	enqueue(&runningQueue[prevThread->priority], prevThread);
      }
      else
      {
      	currentThread = prevThread;
      }

      break;

    case WAIT:
      break;

    case EXIT:

      currentThread = NULL;

      for (i = 0; i < MAX_SIZE; i++) 
      {
        if (runningQueue[i] != NULL)
        {
          currentThread = dequeue(&runningQueue[i]);
	        break;
        }
      }

      if(currentThread == NULL)
      {
        return;
      }

      // free the thread control block and ucontext
      free(prevThread->context->uc_stack.ss_sp);
      free(prevThread->context);
      free(prevThread);

      currentThread->status = READY;

      //printf("Switching to: TID %d Priority %d\n", currentThread->tid, currentThread->priority);

      // reset timer
      timer.it_value.tv_sec = 0;
      timer.it_value.tv_usec = INTERVAL * (currentThread->priority + 1);
      timer.it_interval.tv_sec = 0;
      timer.it_interval.tv_usec = 0;
      int ret = setitimer(ITIMER_VIRTUAL, &timer, NULL);
      if (ret < 0)
      {
        //printf("Timer Reset Failed. Exiting...\n");
        exit(0);
      }
      setcontext(currentThread->context);

      break;

    case JOIN: //JOIN corresponds with a call to pthread_join

      currentThread = NULL;

      for (i = 0; i < MAX_SIZE; i++) 
      {
        if (runningQueue[i] != NULL)
        { 
          currentThread = dequeue(&runningQueue[i]);
	        break;
        }
      }

      if(currentThread == NULL)
      {
        exit(EXIT_FAILURE);
      }
      
      break;
      
    case MUTEX_WAIT: //MUTEX_WAIT corresponds with a thread waiting for a mutex lock

      // Don't add current to queue: already in mutex queue
      currentThread = NULL;

      for (i = 0; i < MAX_SIZE; i++) 
      {
        if (runningQueue[i] != NULL)
        { 
          currentThread = dequeue(&runningQueue[i]);
	        break;
        }
      }

      if(currentThread == NULL)
      {
        //printf("DEADLOCK DETECTED\n");
	      exit(EXIT_FAILURE);
      }

      break;

    default:
      //printf("Thread Status Error: %d\n", currentThread->status);
      exit(-1);
      break;
  }
	
  currentThread->status = READY;

  // reset timer to 25ms times thread priority
  timer.it_value.tv_sec = 0;
  timer.it_value.tv_usec = INTERVAL * (currentThread->priority + 1) ;
  timer.it_interval.tv_sec = 0;
  timer.it_interval.tv_usec = 0;
  int ret = setitimer(ITIMER_VIRTUAL, &timer, NULL);

  if (ret < 0)
  {
     //printf("Timer Reset Failure. Exiting...\n");
     exit(0);
  }

  //printf("Switching to: TID %d Priority %d\n", currentThread->tid, currentThread->priority);
  //Switch to new context
  if(prevThread->tid == currentThread->tid)  
  {/*Assume switching to same context is bad. So don't do it.*/}
  else
  {swapcontext(prevThread->context, currentThread->context);}

  return;
}


/* create a new thread */
int mypthread_create(mypthread_t * thread, pthread_attr_t * attr,
                      void *(*function)(void*), void * arg) {
       // create Thread Control Block
       // create and initialize the context of this thread
       // allocate space of stack for this thread to run
       // after everything is all set, push this thread int

	if(!mainRetrieved)
	{
		initializeGarbageContext();
	}

	notFinished = 1;

	// Create a thread context to add to scheduler
	ucontext_t* task = (ucontext_t*)malloc(sizeof(ucontext_t));
	getcontext(task);
	task->uc_link = &cleanup;
	task->uc_stack.ss_sp = malloc(STACK_S);
	task->uc_stack.ss_size = STACK_S;
	task->uc_stack.ss_flags = 0;
	makecontext(task, (void*)function, 1, arg);

	tcb *newThread = (tcb*)malloc(sizeof(tcb));
	newThread->context = task;
	newThread->tid = threadCount;
	newThread->priority = 0;
	newThread->joinQueue = NULL;
	newThread->jVal = NULL;
	newThread->retVal = NULL;
	newThread->status = READY;

	*thread = threadCount;
	threadCount++;

	enqueue(&runningQueue[0], newThread);
	int key = newThread->tid % MAX_SIZE;
	l_insert(&allThreads[key], newThread);

	notFinished = 0;

	// store main context

	if (!mainRetrieved)
	{
		initializeMainContext();
		raise(SIGVTALRM);
	}
	//printf("New thread created: TID %d\n", newThread->tid);
	
	return 0;
};

/* give CPU possession to other user-level threads voluntarily */
int mypthread_yield() {

	// change thread state from Running to Ready
	// save context of this thread to its thread control block
	// wwitch from thread context to scheduler context

	if(!mainRetrieved)
	{
		initializeGarbageContext();
		initializeMainContext();
	}

	// return to signal handler/scheduler
	currentThread->status = YIELD;
	return raise(SIGVTALRM);
	//return 0;
};

/* terminate a thread */
void mypthread_exit(void *value_ptr) {
	// Deallocated any dynamic memory created when starting this thread

	if(!mainRetrieved)
	{
		initializeGarbageContext();
		initializeMainContext();
	}
	// call garbage collection
	currentThread->jVal = value_ptr;
	setcontext(&cleanup);
};


/* Wait for thread termination */
int mypthread_join(mypthread_t thread, void **value_ptr) {

	// wait for a specific thread to terminate
	// de-allocate any dynamic memory created by the joining thread

	if(!mainRetrieved)
	{
		initializeGarbageContext();
		initializeMainContext();
	}
	notFinished = 1;
	// make sure thread can't wait on self
	if(thread == currentThread->tid)
	{
		return -1;
	}

	tcb *tgt = thread_search(thread);

	if(tgt == NULL)
	{
		return -1;
	}

	//Priority Inversion Case
	tgt->priority = 0;

	l_insert(&tgt->joinQueue, currentThread);

	currentThread->status = JOIN;

	notFinished = 0;
	raise(SIGVTALRM);

	if(value_ptr == NULL)
	{
		return 0;
	}

	*value_ptr = currentThread->retVal;

	return 0;
};

/* initialize the mutex lock */
int mypthread_mutex_init(mypthread_mutex_t *mutex,
                          const pthread_mutexattr_t *mutexattr) {
	//initialize data structures for this mutex

	notFinished = 1;
	pthread_mutex_t m = *mutex;

	m.available = 1;
	m.locked = 0;
	m.holder = -1; //holder represents the tid of the thread that is currently holding the mutex
	m.queue = NULL;

	*mutex = m;
	notFinished = 0;
	return 0;
};

/* aquire the mutex lock */
int mypthread_mutex_lock(mypthread_mutex_t *mutex) {
        // use the built-in test-and-set atomic function to test the mutex
        // if the mutex is acquired successfully, enter the critical section
        // if acquiring mutex fails, push current thread into block list and //
        // context switch to the scheduler thread

	if(!mainRetrieved)
	{
		initializeGarbageContext();
		initializeMainContext();
	}
	notFinished = 1;

	//FOR NOW ASSUME MUTEX WAS INITIALIZED
	if(!mutex->available)
	{
		return -1;
	}

	while(__atomic_test_and_set((volatile void *)&mutex->locked,__ATOMIC_RELAXED))
	{
	//the reason why I reset notFinished to one here is that when coming back
	//from a swapcontext, notFinished may be zero and I can't let the operations
	//in the loop be interrupted
		notFinished = 1;
		enqueue(&mutex->queue, currentThread);
		currentThread->status = MUTEX_WAIT;
		//need notFinished to be zero before going to scheduler
		notFinished = 0;
		raise(SIGVTALRM);
	}

	if(!mutex->available)
	{
		mutex->locked = 0;
		return -1;
	}

	//Priority Inversion Case
	currentThread->priority = 0;
	mutex->holder = currentThread->tid;

	notFinished = 0;
	return 0;
};

/* release the mutex lock */
int mypthread_mutex_unlock(mypthread_mutex_t *mutex) {
	// Release mutex and make it available again.
	// Put threads in block list to run queue
	// so that they could compete for mutex later.

	if(!mainRetrieved)
	{
		initializeGarbageContext();
		initializeMainContext();
	}
	//NOTE: handle errors: unlocking an open mutex, unlocking mutex not owned by calling thread, or accessing unitialized mutex

	notFinished = 1;

	//ASSUMING mutex->available will be initialized to 0 by default without calling init
	//available in this case means that mutex has been initialized or destroyed (state variable)
	if(!mutex->available || !mutex->locked || mutex->holder != currentThread->tid)
	{
		return -1;
	}

	mutex->locked = 0;
	mutex->holder = -1;

	tcb* muThread = dequeue(&mutex->queue);

	if(muThread != NULL)
	{
	//Priority Inversion Case
		muThread->priority = 0;
		enqueue(&runningQueue[0], muThread);
	}

	notFinished = 0;

	return 0;
};


/* destroy the mutex */
int mypthread_mutex_destroy(mypthread_mutex_t *mutex) {
	// Deallocate dynamic memory created in mypthread_mutex_init

	return 0;
};

/* scheduler */
static void schedule() {
	// Every time when timer interrup happens, your thread library
	// should be contexted switched from thread context to this
	// schedule function

	// Invoke different actual scheduling algorithms
	// according to policy (STCF or MLFQ)

	// if (sched == STCF)
	//		sched_stcf();
	// else if (sched == MLFQ)
	// 		sched_mlfq();

	// YOUR CODE HERE

// schedule policy
#ifndef MLFQ
	// Choose STCF
#else
	// Choose MLFQ
#endif

}

/* Preemptive SJF (STCF) scheduling algorithm */
static void sched_stcf() {
	// Your own implementation of STCF
	// (feel free to modify arguments and return types)

	// YOUR CODE HERE
}

/* Preemptive MLFQ scheduling algorithm */
static void sched_mlfq() {
	// Your own implementation of MLFQ
	// (feel free to modify arguments and return types)

	// YOUR CODE HERE
}

// Feel free to add any other functions you need

// YOUR CODE HERE
