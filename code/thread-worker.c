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
Node *main_thread;// store the main thread
ucontext_t finished_context;
int creating_thread_started = 0;
int creating_thread_done = 0;

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
}
void enqueue_to_ready_queue();
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

void stop_timer() {
    timer.it_value.tv_sec = 0;
    timer.it_value.tv_usec = 0;
    timer.it_interval.tv_sec = 0;
    timer.it_interval.tv_usec = 0;
    setitimer(ITIMER_PROF, &timer, NULL); // Stop the timer
}

// declare the scheduler function
static void schedule();

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

void free_scheduler();
void main_thread_exit() {
    free_scheduler();
    for (int i = 0; i < All_threads.used; i++) {
        free(All_threads.array[i]->TCB->thread_stack);
        free(All_threads.array[i]->TCB);
        free(All_threads.array[i]);
    }
    free(All_threads.array);
    free(main_thread->TCB);
    free(main_thread);
    free(finished_context.uc_stack.ss_sp);
    exit(EXIT_SUCCESS);
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
        //initialize the main thread
        init_all_threads_map();
        main_thread = (Node *)malloc(sizeof(Node));
        if (main_thread == NULL) {
            perror("Failed to allocate main thread");
            exit(EXIT_FAILURE);
        }
        main_thread->TCB = (tcb *)malloc(sizeof(tcb));
        if (main_thread->TCB == NULL) {
            perror("Failed to allocate main thread TCB");
            exit(EXIT_FAILURE);
        }
        if(getcontext(&main_thread->TCB->thread_context) < 0) {
            perror("getcontext");
            exit(EXIT_FAILURE);
        }
        main_thread->TCB->thread_status = READY;
        main_thread->TCB->thread_stack = main_thread ->TCB->thread_context.uc_stack.ss_sp;
        main_thread->TCB->thread_return = NULL;
        main_thread->next = NULL;
        main_thread->type = QUEUE_TYPE_READY;
        current_thread = main_thread;
        insert_thread_to_map(main_thread);
        initialize_scheduler();
        // set up finished context
        getcontext(&finished_context);
        finished_context.uc_stack.ss_sp = malloc(STACK_SIZE);
        finished_context.uc_stack.ss_size = STACK_SIZE;
        finished_context.uc_link = NULL;
        makecontext(&finished_context, (void *) &main_thread_exit, 0);
        init_scheduler_done = 1;
    }
    stop_timer();
    Node *new_thread = (Node *)malloc(sizeof(Node));
    if (new_thread == NULL)
    {
        return -1;
    }
    new_thread->TCB = (tcb *)malloc(sizeof(tcb));
    new_thread->TCB->thread_status = READY;
    if (getcontext(&new_thread->TCB->thread_context) < 0) {
        return -1;
    }
    // Allocate space for stack
	void *stack=malloc(STACK_SIZE);
	
	if (stack == NULL){
		return -1;
	}
    
    new_thread->TCB->thread_context.uc_link = &finished_context;
    new_thread->TCB->thread_context.uc_stack.ss_sp = stack;
    new_thread->TCB->thread_context.uc_stack.ss_size = STACK_SIZE;
    new_thread->TCB->thread_context.uc_stack.ss_flags = 0;
    //setup the context for the TCB_stuff
    makecontext(&new_thread->TCB->thread_context, (void *)(*function), 1, arg);

    new_thread->TCB->thread_stack = stack;
    new_thread->next = NULL;
    new_thread->type = QUEUE_TYPE_READY;
    insert_thread_to_map(new_thread);
    *thread = new_thread->TCB->thread_id;
    // Add the new thread to the queue
    enqueue_to_ready_queue(new_thread);
    creating_thread_done = 1;
    //swap context from main context to ready queue
    swapcontext(&current_thread->TCB->thread_context ,&ready_queue->context);
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
    stop_timer();
    if (swapcontext(&current_thread->TCB->thread_context, &ready_queue->context) < 0) {
        perror("swapcontext");
        return -1;
    }
    return 0;
};

void delete_TCB(Node *thread) {
    thread->TCB->thread_id = -1;       
    free(thread->TCB->thread_stack);
    free(thread->TCB);
}

/* terminate a thread */
void worker_exit(void *value_ptr)
{
    // - if value_ptr is provided, save return value
    // - de-allocate any dynamic memory created when starting this thread (could be done here or elsewhere)
    current_thread->TCB->thread_status = TERMINATED;
    //change the status of the thread as terminated in the map array that we created
    All_threads.array[current_thread->TCB->thread_id - 1]->TCB->thread_status = TERMINATED;
    current_thread->TCB->thread_return = value_ptr;
    All_threads.array[current_thread->TCB->thread_id - 1]->TCB->thread_return = value_ptr;
    current_thread->next = NULL;
    All_threads.array[current_thread->TCB->thread_id - 1]->next = NULL;
    stop_timer();
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
    if (thread == -1 || thread < 1 || thread > All_threads.used) {
        return -1; // invalid thread ID
    }
    if (current_thread->TCB->thread_id == thread) {
        return -1; // Indicate that the calling thread cannot join itself
    }
    Node *target_thread = All_threads.array[thread];
    if (target_thread == NULL) {
        return -1; // Indicate that the thread does not exist
    }
    while (target_thread->TCB->thread_status != TERMINATED) { 
        current_thread->TCB->thread_status = READY;
        // Switch to the scheduler context to block the current thread and continue with another thread
        stop_timer();
        enqueue_to_ready_queue(current_thread);
        if (swapcontext(&current_thread->TCB->thread_context, &ready_queue->context) < 0) {
            perror("swapcontext");
            return -1;
        }
    }
    if (value_ptr != NULL) {
        *value_ptr = target_thread->TCB->thread_return;
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
    stop_timer();
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
static void sched_rr();
/* scheduler */
static void schedule()
{
// - every time a timer interrupt occurs, your worker thread library
// should be contexted switched from a thread context to this
// schedule() function
// - invoke scheduling algorithms according to the policy (RR or MLFQ)
// - schedule policy
    Node *prev = current_thread;
#ifndef MLFQ
    sched_rr();
    if (current_thread == NULL) {
        printf("No more threads to schedule\n");
        main_thread_exit();
    }
#else
    // Choose MLFQ
    
#endif
    if (creating_thread_done == 1) {
        enqueue_to_ready_queue(prev);
        creating_thread_done = 0;
    }

    reset_timer();
// save the context of the current thread
    getcontext(&ready_queue->context);
    makecontext(&ready_queue->context, (void (*)())schedule, 0);
    // context switch to the next thread
    // see if the set context function actually sets the context to the thing we are setting it to
    setcontext(&current_thread->TCB->thread_context);
}

static void sched_rr()
{
    // - your own implementation of RR
    // (feel free to modify arguments and return types)
    if (ready_queue->ready_queue_head == NULL) {
        printf("blaaaaaaa\n");
        current_thread = NULL;
        return;
    }
    current_thread = ready_queue->ready_queue_head;
    ready_queue->ready_queue_head = current_thread->next;
    if (ready_queue->ready_queue_head == NULL) {
        ready_queue->ready_queue_tail = NULL;
    }else{
        ready_queue->ready_queue_head = current_thread->next;
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