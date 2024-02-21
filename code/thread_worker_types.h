#ifndef TW_TYPES_H
#define TW_TYPES_H

#include <ucontext.h>

typedef unsigned int worker_t;

typedef enum {
    QUEUE_TYPE_READY,
    QUEUE_TYPE_MUTEX_BLOCK
} QueueType;

typedef enum {
    READY,
    RUNNING,
    BLOCKED,
    TERMINATED
} ThreadState;

typedef struct TCB
{
    /* add important states in a thread control block */
    worker_t thread_id;       // Thread ID
    ThreadState thread_status;       // Thread status (e.g., running, waiting, terminated)
    ucontext_t thread_context; // Thread context (e.g., register state)
    void *thread_stack;       // Thread stack pointer
    void *thread_return;      // Thread return value
    worker_t waiting_thread;
    // Thread priority (e.g., for scheduling)
    // Add more states as needed...

    // YOUR CODE HERE

} tcb;

/* Node struct definition */
typedef struct node {
    tcb* TCB;        
    struct node* next;
    QueueType type;
    bool freed;
} Node;

typedef struct {
    Node* ready_queue_head; // Head of the ready queue
    Node* ready_queue_tail; // Tail of the ready queue, for efficient enqueue
    ucontext_t context;      // Scheduler context
} Scheduler;

typedef struct {
    Node** array; // Array of threads
    size_t used;   // Number of threads currently in use
    size_t size;   // Current allocated size of the array
}ArrayList;

#endif
