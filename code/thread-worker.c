// File:	thread-worker.c

// List all group member's name:
/*
 */
// username of iLab: 
// iLab Server:


#include "thread-worker.h"
#include "thread_worker_types.h"

#define STACK_SIZE 16 * 1024
#define QUANTUM 10 * 1000
#define SCHED_STACK_SIZE 16 * 1024

//Initializing the queue:
typedef struct node {
    tcb* TCB;        
    struct node* next; 
} Node;

typedef struct {
    Node* ready_queue_head; // Head of the ready queue
    Node* ready_queue_tail; // Tail of the ready queue, for efficient enqueue
    ucontext_t context;      // Scheduler context
} Scheduler;

typedef enum {
    READY,
    RUNNING,
    BLOCKED,
    TERMINATED
} ThreadState;


// INITIALIZE ALL YOUR OTHER VARIABLES HERE
int init_scheduler_done = 0;
Scheduler *ready_queue;
Node *current_thread;

void initialize_scheduler() {
    ready_queue = (Scheduler *)malloc(sizeof(Scheduler));
    if (ready_queue == NULL) {
        // Handle memory allocation failure
        perror("Failed to allocate scheduler");
        exit(1); // Or another appropriate error handling mechanism
    }
    ready_queue->ready_queue_head = NULL;
    ready_queue->ready_queue_tail = NULL;
    // Get the current context to modify it into the scheduler's context
    getcontext(&ready_queue->context);

    // Set up the scheduler context
    ready_queue->context.uc_stack.ss_sp = malloc(SCHED_STACK_SIZE);
    ready_queue->context.uc_stack.ss_size = SCHED_STACK_SIZE;
    ready_queue->context.uc_link = NULL; // Typically no successor for the scheduler

    // Assign a function that the scheduler will execute
    makecontext(&ready_queue->context, (void (*)())schedule, 0);
}

/* create a new thread */
int worker_create(worker_t *thread, pthread_attr_t *attr,
                  void *(*function)(void *), void *arg)
{
    // - create Thread Control Block (TCB)
    // - create and initialize the context of this worker thread
    // - allocate space of stack for this thread to run
    // after everything is set, push this thread into run queue and
    // - make it ready for the execution.

    if (init_scheduler_done == 0) {
        initialize_scheduler();
        init_scheduler_done = 1;
    }

    Node *new_thread = (Node *)malloc(sizeof(Node));
    if (new_thread == NULL)
    {
        perror("Failed to allocate TCB");
        exit(1);
    }
    tcb* TCB_stuff = new_thread-> TCB;
    TCB_stuff->thread_id = *thread;
    TCB_stuff->thread_status = READY;
    ucontext_t *new_context = (ucontext_t *)malloc(sizeof(ucontext_t));
    if (getcontext(&TCB_stuff->thread_context) < 0){
            perror("getcontext");
            exit(1);
        }
    // Allocate space for stack
	void *stack=malloc(STACK_SIZE);
	
	if (stack == NULL){
		perror("Failed to allocate stack");
		exit(1);
	}

    new_context->uc_link = NULL;
    new_context->uc_stack.ss_sp = stack;
    new_context->uc_stack.ss_size = STACK_SIZE;
    new_context->uc_stack.ss_flags = 0;

    makecontext(new_context, (void *)&function, 1, arg);

    TCB_stuff->thread_context = *new_context;
    TCB_stuff->thread_stack = stack;

    new_thread->next = NULL;
    // Add the new thread to the queue
    if (ready_queue->ready_queue_head == NULL) {
        ready_queue->ready_queue_head = new_thread;
        ready_queue->ready_queue_tail = new_thread;
    } else {
        ready_queue->ready_queue_tail->next = new_thread;
        ready_queue->ready_queue_tail = new_thread;
    }
    current_thread->next = NULL; // Ensure the queue is properly terminated
    return 0;
}

/* give CPU possession to other user-level worker threads voluntarily */
int worker_yield()
{
    // - change worker thread's state from Running to Ready
    // - save context of this thread to its thread control block
    // - switch from thread context to scheduler context
    current_thread->TCB->thread_status = READY;
    if (ready_queue->ready_queue_head == NULL) {
        ready_queue->ready_queue_head = ready_queue-> ready_queue_tail = current_thread;
    } else {
        ready_queue->ready_queue_tail->next = current_thread;
        ready_queue->ready_queue_tail = current_thread;
    }
    current_thread->next = NULL; // Ensure the queue is properly terminated

    // Swap context to the scheduler, saving the current context for later
    if (swapcontext(&current_thread->TCB->thread_context, &ready_queue->context) < 0) {
        perror("swapcontext");
        exit(1);
    }

    return 0;
};

/* terminate a thread */
void worker_exit(void *value_ptr)
{
    // - if value_ptr is provided, save return value
    // - de-allocate any dynamic memory created when starting this thread (could be done here or elsewhere)
}

/* Wait for thread termination */
int worker_join(worker_t thread, void **value_ptr)
{

    // - wait for a specific thread to terminate
    // - if value_ptr is provided, retrieve return value from joining thread
    // - de-allocate any dynamic memory created by the joining thread
    return 0;

};

/* initialize the mutex lock */
int worker_mutex_init(worker_mutex_t *mutex,
                      const pthread_mutexattr_t *mutexattr)
{
    //- initialize data structures for this mutex
    return 0;

};

/* aquire the mutex lock */
int worker_mutex_lock(worker_mutex_t *mutex)
{

    // - use the built-in test-and-set atomic function to test the mutex
    // - if the mutex is acquired successfully, enter the critical section
    // - if acquiring mutex fails, push current thread into block list and
    // context switch to the scheduler thread
    return 0;

};

/* release the mutex lock */
int worker_mutex_unlock(worker_mutex_t *mutex)
{
    // - release mutex and make it available again.
    // - put one or more threads in block list to run queue
    // so that they could compete for mutex later.

    return 0;
};

/* destroy the mutex */
int worker_mutex_destroy(worker_mutex_t *mutex)
{
    // - make sure mutex is not being used
    // - de-allocate dynamic memory created in worker_mutex_init

    return 0;
};

/* scheduler */
static void schedule()
{
// - every time a timer interrupt occurs, your worker thread library
// should be contexted switched from a thread context to this
// schedule() function

// - invoke scheduling algorithms according to the policy (RR or MLFQ)

// - schedule policy
#ifndef MLFQ
    // Choose RR
    
#else
    // Choose MLFQ
    
#endif
}

static void sched_rr()
{
    // - your own implementation of RR
    // (feel free to modify arguments and return types)

}

/* Preemptive MLFQ scheduling algorithm */
static void sched_mlfq()
{
    // - your own implementation of MLFQ
    // (feel free to modify arguments and return types)

}

// Feel free to add any other functions you need.
// You can also create separate files for helper functions, structures, etc.
// But make sure that the Makefile is updated to account for the same.