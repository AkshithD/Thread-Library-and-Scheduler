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

typedef enum {
    QUEUE_TYPE_READY,
    QUEUE_TYPE_MUTEX_BLOCK,
    QUEUE_TYPE_GENERAL_BLOCK,
} QueueType;

//Initializing the queue:
typedef struct node {
    tcb* TCB;        
    struct node* next;
    QueueType type;
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

typedef struct {
    tcb** array; // Pointer to an array of TCB pointers
    size_t used;   // Number of TCBs currently in use
    size_t size;   // Current allocated size of the array
}ArrayList;

typedef struct {
    Node* blocked_head; // Head of the blocked queue
    Node* blocked_tail; // Tail of the blocked queue, for efficient enqueue
} BlockedList;

// INITIALIZE ALL YOUR OTHER VARIABLES HERE
int init_scheduler_done = 0;
Scheduler *ready_queue;
Node *current_thread;
ArrayList All_threads;
BlockedList blocked_queue;

void init_all_threads_array() {
    All_threads.array = malloc(sizeof(tcb*) * 100);
    All_threads.used = 0;
    All_threads.size = 100;
    if (All_threads.array == NULL) {
        perror("Failed to allocate TCB array");
        exit(EXIT_FAILURE);
    }
}

void insert_tcb(tcb* tcb) {
    if (All_threads.used == All_threads.size) {
        size_t newSize = All_threads.size * 2;
        All_threads.array = realloc(All_threads.array, sizeof(tcb) * newSize);
        All_threads.size = newSize;
    }
    All_threads.array[All_threads.used] = tcb; // Insert the new TCB and increment the used count
    tcb->thread_id = All_threads.used++; // Assign the thread ID as the current used count
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
        init_all_threads_array();
    }

    Node *new_thread = (Node *)malloc(sizeof(Node));
    if (new_thread == NULL)
    {
        perror("Failed to allocate TCB");
        exit(1);
    }
    new_thread->TCB = (tcb *)malloc(sizeof(tcb));
    tcb* TCB_stuff = new_thread-> TCB;
    insert_tcb(TCB_stuff);
    *thread = TCB_stuff->thread_id;
    TCB_stuff->thread_status = READY;
    ucontext_t *new_context = &TCB_stuff->thread_context;
    if (getcontext(new_context) < 0) {
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
    new_thread->type = QUEUE_TYPE_READY;
    // Add the new thread to the queue
    if (ready_queue->ready_queue_head == NULL) {
        ready_queue->ready_queue_head = new_thread;
        ready_queue->ready_queue_tail = new_thread;
        current_thread = new_thread;
        current_thread->next = NULL;
    } else {
        ready_queue->ready_queue_tail->next = new_thread;
        ready_queue->ready_queue_tail = new_thread;
        new_thread->next = NULL;
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

void delete_thread(Node *thread) {
    free(thread->TCB->thread_stack);
    free(thread->TCB);
    free(thread);
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
    // If there is a thread waiting for this thread to terminate, unblock it
    if (current_thread->TCB->joiner != NULL && current_thread->TCB->joiner_return != NULL) {
        // Set the joiner's return value
        current_thread->TCB->joiner->thread_status = READY;
        *(current_thread->TCB->joiner_return) = value_ptr;
        // add the joiner back from the block list to the ready queue
        Node *search_ptr1 = NULL;
        Node *search_ptr2 = blocked_queue.blocked_head;
        while (search_ptr2 != NULL) {
            if (search_ptr2->TCB->thread_id == current_thread->TCB->joiner->thread_id) {
                if (search_ptr1 == NULL) {
                    blocked_queue.blocked_head = search_ptr2->next;
                } else {
                    search_ptr1->next = search_ptr2->next;
                }
                search_ptr2->next = NULL;
                search_ptr2->type = QUEUE_TYPE_READY;
                if (ready_queue->ready_queue_head == NULL) {
                    ready_queue->ready_queue_head = ready_queue->ready_queue_tail = search_ptr2;
                } else {
                    ready_queue->ready_queue_tail->next = search_ptr2;
                    ready_queue->ready_queue_tail = search_ptr2;
                }
                break;
            }
            search_ptr1 = search_ptr2;
            search_ptr2 = search_ptr2->next;
        }
    }
    // swap context to the scheduler
    if (swapcontext(&current_thread->TCB->thread_context, &ready_queue->context) < 0) {
        perror("swapcontext");
        exit(1);
    }
}

/* Wait for thread termination */
int worker_join(worker_t thread, void **value_ptr)
{
    // - wait for a specific thread to terminate
    // - if value_ptr is provided, retrieve return value from joining thread
    // - de-allocate any dynamic memory created by the joining thread
    Node *target_thread = All_threads.array[thread - 1];
    if (target_thread->TCB->thread_status != TERMINATED) {
        // Block the calling thread
        current_thread->TCB->thread_status = BLOCKED;
        // Add the calling thread to the blocked queue
        current_thread->next = NULL;
        current_thread->type = QUEUE_TYPE_GENERAL_BLOCK;
        if (blocked_queue.blocked_head == NULL) {
            blocked_queue.blocked_head = blocked_queue.blocked_tail = current_thread;
        } else {
            blocked_queue.blocked_tail->next = current_thread;
            blocked_queue.blocked_tail = current_thread;
        }

        // Record the joiner in the target thread's TCB
        target_thread->TCB->joiner = current_thread->TCB;
        target_thread->TCB->joiner_return = value_ptr; // Store the address of value_ptr
        // Switch to scheduler context to block the current thread and continue with another thread
        if (swapcontext(&current_thread->TCB->thread_context, &ready_queue->context) < 0) {
            perror("swapcontext");
            exit(1);
        }
    } else if (value_ptr != NULL) {
        // If the target thread has already terminated, directly pass the return value
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
    if (__sync_lock_test_and_set(&(mutex->lock), 1) == 0) {
        // Lock acquired successfully
        return;
    }
    // Lock is already held, so block the current thread
    current_thread->TCB->thread_status = BLOCKED;
    // Add the calling thread to the blocked queue
    current_thread->next = NULL;
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
        exit(1);
    }
    return 0;
};

/* release the mutex lock */
int worker_mutex_unlock(worker_mutex_t *mutex)
{
    // - release mutex and make it available again.
    // - put one or more threads in block list to run queue
    // so that they could compete for mutex later.
    __sync_lock_release(&(mutex->lock));
    // If there are threads waiting for the mutex, unblock one of them
    if (mutex->wait_queue_head != NULL) {
        Node *unblocked_thread = mutex->wait_queue_head;
        mutex->wait_queue_head = unblocked_thread->next;
        unblocked_thread->next = NULL;
        unblocked_thread->type = QUEUE_TYPE_READY;
        if (ready_queue->ready_queue_head == NULL) {
            ready_queue->ready_queue_head = ready_queue->ready_queue_tail = unblocked_thread;
        } else {
            ready_queue->ready_queue_tail->next = unblocked_thread;
            ready_queue->ready_queue_tail = unblocked_thread;
        }
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