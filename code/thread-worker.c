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

// INITIALIZE ALL YOUR OTHER VARIABLES HERE
int init_scheduler_done = 0;
Scheduler *ready_queue;
Node *current_thread;
ArrayList All_threads;
struct itimerval timer;
struct sigaction sa;
int num_active_threads = 0;

void init_all_threads_map() {
    All_threads.array = malloc(sizeof(Node*) * 100);
    All_threads.used = 0;
    All_threads.size = 100;
    if (All_threads.array == NULL) {
        perror("Failed to allocate TCB array");
        exit(EXIT_FAILURE);
    }
}

void insert_thread_to_map(Node *inserting_thread) {
    if (All_threads.used == All_threads.size) {
        size_t newSize = All_threads.size * 2;
        All_threads.array = realloc(All_threads.array, sizeof(Node*) * newSize);
        All_threads.size = newSize;
    }
    All_threads.array[All_threads.used] = inserting_thread; // Insert the new thread and increment the used count
    inserting_thread->TCB->thread_id = All_threads.used++; // Assign the thread ID as the current used count
    num_active_threads++; // Increment the number of active threads
}

/* Timer interrupt handler */
static void timer_interrupt_handler(int sig)
{
    // - save the context of the current thread
    // - context switch to the scheduler
    if (current_thread != NULL) {
        current_thread->TCB->thread_status = READY;
        enqueue_to_ready_queue(current_thread);
    }
    // Switch to the scheduler context to shedule()
    if (swapcontext(&current_thread->TCB->thread_context, &ready_queue->context) < 0) {
        perror("swapcontext");
        exit(EXIT_FAILURE);
    }
}

/* Timer functions */
void init_timer() {
    // Set up the signal handler
    sa.sa_handler = &timer_interrupt_handler;
    sigaction(SIGPROF, &sa, NULL);
    // Configure the timer
    timer.it_value.tv_sec = 0;
    timer.it_value.tv_usec = QUANTUM;
    timer.it_interval.tv_sec = 0;
    timer.it_interval.tv_usec = 0;
}

void reset_timer() {
    setitimer(ITIMER_PROF, &timer, NULL);
}

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

    // Initialize the timer
    init_timer();
}

/* Queue helper functions */
void enqueue_to_ready_queue(Node *new_thread) {
    new_thread->next = NULL;
    new_thread->type = QUEUE_TYPE_READY;
    if (ready_queue->ready_queue_head == NULL) {
        ready_queue->ready_queue_head = ready_queue->ready_queue_tail = new_thread;
    } else {
        ready_queue->ready_queue_tail->next = new_thread;
        ready_queue->ready_queue_tail = new_thread;
    }
}

void dequeue_from_ready_queue() {
    if (ready_queue->ready_queue_head == NULL) {
        return;
    }
    current_thread = ready_queue->ready_queue_head;
    ready_queue->ready_queue_head = current_thread->next;
    if (ready_queue->ready_queue_head == NULL) {
        ready_queue->ready_queue_tail = NULL;
    }
    current_thread->type = QUEUE_TYPE_READY;
    current_thread->next = NULL;
    current_thread->TCB->thread_status = RUNNING;
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

    if (All_threads.used == 0) {
        init_all_threads_map();
    }

    Node *new_thread = (Node *)malloc(sizeof(Node));
    if (new_thread == NULL)
    {
        return -1;
    }
    new_thread->TCB = (tcb *)malloc(sizeof(tcb));
    tcb* TCB_stuff = new_thread-> TCB;
    TCB_stuff->thread_status = READY;
    ucontext_t *new_context = &TCB_stuff->thread_context;
    if (getcontext(new_context) < 0) {
        return -1;
    }
    // Allocate space for stack
	void *stack=malloc(STACK_SIZE);
	
	if (stack == NULL){
		return -1;
	}
    new_context->uc_link = NULL;
    new_context->uc_stack.ss_sp = stack;
    new_context->uc_stack.ss_size = STACK_SIZE;
    new_context->uc_stack.ss_flags = 0;

    makecontext(new_context, (void *)&function, 1, arg);

    TCB_stuff->thread_context = *new_context;
    TCB_stuff->thread_stack = stack;

    new_thread->next = NULL;
    new_thread->type = QUEUE_TYPE_READY;
    insert_thread_to_map(new_thread);
    *thread = TCB_stuff->thread_id;
    // Add the new thread to the queue
    enqueue_to_ready_queue(new_thread);
    // If the current thread is NULL, then we need to start the scheduler
    if (current_thread == NULL){
        if (swapcontext(&ready_queue->context, &current_thread->TCB->thread_context) < 0) {
            perror("swapcontext");
            return -1;
        }
    }
    return 0;
}

/* give CPU possession to other user-level worker threads voluntarily */
int worker_yield()
{
    // - change worker thread's state from Running to Ready
    // - save context of this thread to its thread control block
    // - switch from thread context to scheduler context
    current_thread->TCB->thread_status = READY;
    enqueue_to_ready_queue(current_thread);
    // Swap context to the scheduler, saving the current context for later
    if (swapcontext(&current_thread->TCB->thread_context, &ready_queue->context) < 0) {
        perror("swapcontext");
        return -1;
    }
    return 0;
};

void delete_TCB(Node *thread) {          
    free(thread->TCB->thread_stack);
    free(thread->TCB);
}

/* terminate a thread */
void worker_exit(void *value_ptr)
{
    // - if value_ptr is provided, save return value
    // - de-allocate any dynamic memory created when starting this thread (could be done here or elsewhere)
    current_thread->TCB->thread_status = TERMINATED;
    current_thread->TCB->thread_return = value_ptr;
    current_thread->next = NULL;
    current_thread->type = NULL;

    if (current_thread->TCB->waiting_thread != NULL) {
        current_thread->freed = false;
    }else{
        current_thread->freed = true;
        num_active_threads--;
        delete_TCB(current_thread); // If the thread is not waiting for another thread, free it now
    }
    // Switch to the scheduler context to continue with another thread
    if (swapcontext(&current_thread->TCB->thread_context, &ready_queue->context) < 0) {
        perror("swapcontext");
        exit(EXIT_FAILURE);
    }
}

void free_scheduler() {
    free(ready_queue->context.uc_stack.ss_sp);
    free(ready_queue);
}

/* Wait for thread termination */
int worker_join(worker_t thread, void **value_ptr)
{
    // - wait for a specific thread to terminate
    // - if value_ptr is provided, retrieve return value from joining thread
    // - de-allocate any dynamic memory created by the joining thread
    if (thread == NULL || thread < 1 || thread > All_threads.used) {
        return -1; // invalid thread ID
    }
    if (current_thread->TCB->thread_id == thread) {
        return -1; // Indicate that the calling thread cannot join itself
    }
    Node *target_thread = All_threads.array[thread - 1];
    if (target_thread == NULL) {
        return -1; // Indicate that the thread does not exist
    }
    target_thread->TCB->waiting_thread = current_thread;
    while (target_thread->TCB->thread_status != TERMINATED) { //potential infinite loop
        current_thread->TCB->thread_status = READY;
        // Switch to the scheduler context to block the current thread and continue with another thread
        if (swapcontext(&current_thread->TCB->thread_context, &ready_queue->context) < 0) {
            perror("swapcontext");
            return -1;
        }
    }
    if (value_ptr != NULL) {
        *value_ptr = target_thread->TCB->thread_return;
    }
    if (!target_thread->freed) { // If the thread has not been freed, free it now
        target_thread->freed = true;
        num_active_threads--;
        delete_TCB(target_thread);
    }
    return 0; // Success
};

/* initialize the mutex lock */
int worker_mutex_init(worker_mutex_t *mutex,
                      const pthread_mutexattr_t *mutexattr)
{
    //- initialize data structures for this mutex
    if (mutex == NULL) {
        perror("Mutex pointer is NULL");
        return -1; // Return an error code to indicate failure
    }
    mutex->lock = 0; // Mutex is initially unlocked
    mutex->owner = -1;  
    mutex->wait_queue_head = NULL; 
    mutex->wait_queue_tail = NULL; 
    return 0;
};

/* aquire the mutex lock */
int worker_mutex_lock(worker_mutex_t *mutex)
{
    // - use the built-in test-and-set atomic function to test the mutex
    // - if the mutex is acquired successfully, enter the critical section
    // - if acquiring mutex fails, push current thread into block list and
    // context switch to the scheduler thread
    // Attempt to acquire the lock atomically
    if (mutex->owner == current_thread->TCB->thread_id) {
        return 0; // Indicate that the calling thread already owns the mutex
    }
    if (__sync_lock_test_and_set(&(mutex->lock), 1) == 0 && mutex->owner == -1) {
        mutex->owner = current_thread->TCB->thread_id;
        // Lock acquired successfully
        return 0;
    }
    // Lock is already held, so block the current thread
    current_thread->TCB->thread_status = BLOCKED;
    // Add the calling thread to the blocked queue
    current_thread->next = NULL; //WARNING: this is a potential bug
    current_thread->type = QUEUE_TYPE_MUTEX_BLOCK;
    if (mutex->wait_queue_head == NULL) {
        mutex->wait_queue_head = mutex->wait_queue_tail = current_thread;
    } else {
        mutex->wait_queue_tail->next = current_thread;
        mutex->wait_queue_tail = current_thread;
    }
    // Switch to scheduler context to block the current thread and continue with another thread
    if (swapcontext(&current_thread->TCB->thread_context, &ready_queue->context) < 0) {
        perror("swapcontext");
        return -1;
    }
    return 0;
};

/* release the mutex lock */
int worker_mutex_unlock(worker_mutex_t *mutex)
{
    // - release mutex and make it available again.
    // - put one or more threads in block list to run queue
    // so that they could compete for mutex later.
    if (mutex->owner != current_thread->TCB->thread_id) {
        return -1; // Indicate that the calling thread does not own the mutex
    }
    __sync_lock_release(&(mutex->lock));
    mutex->owner = -1;
    // If there are threads waiting for the mutex, unblock one of them
    if (mutex->wait_queue_head != NULL) {
        Node *unblocked_thread = mutex->wait_queue_head;
        mutex->wait_queue_head = unblocked_thread->next;
        if (mutex->wait_queue_head == NULL) {
            mutex->wait_queue_tail = NULL;
        }
        enqueue_to_ready_queue(unblocked_thread);
    }
    return 0;
};

/* destroy the mutex */
int worker_mutex_destroy(worker_mutex_t *mutex)
{
    // - make sure mutex is not being used
    // - de-allocate dynamic memory created in worker_mutex_init
    if (mutex->wait_queue_head != NULL) {
        return -1; // Indicate that the mutex cannot be destroyed because it has waiting threads
    }
    if (mutex->owner != -1) {
        return -1; // Indicate that the mutex cannot be destroyed because it is locked
    }
    if (mutex->lock != 0) {
        return -1; // Indicate that the mutex cannot be destroyed because it is locked
    }
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
    sched_rr();
    if (current_thread == NULL) {
        if (num_active_threads == 0) {
            free_scheduler();
            for (int i = 0; i < All_threads.used; i++) {
                free(All_threads.array[i]);
            }
            free(All_threads.array);
            exit(EXIT_SUCCESS);
        }else{

        }
    }
#else
    // Choose MLFQ
    
#endif
    reset_timer();
// save the context of the current thread
    getcontext(&ready_queue->context);
    makecontext(&ready_queue->context, (void (*)())schedule, 0);
    // context switch to the next thread
    setcontext(&current_thread->TCB->thread_context);
}

static void sched_rr()
{
    // - your own implementation of RR
    // (feel free to modify arguments and return types)
    if (ready_queue->ready_queue_head == NULL) {
        return NULL;
    }
    current_thread = ready_queue->ready_queue_head;
    ready_queue->ready_queue_head = current_thread->next;
    if (ready_queue->ready_queue_head == NULL) {
        ready_queue->ready_queue_tail = NULL;
    }
    current_thread->next = NULL;
    current_thread->TCB->thread_status = RUNNING;
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