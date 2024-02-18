#ifndef TW_TYPES_H
#define TW_TYPES_H

#include <ucontext.h>

typedef unsigned int worker_t;

typedef enum {
    QUEUE_TYPE_READY,
    QUEUE_TYPE_MUTEX_BLOCK,
    QUEUE_TYPE_GENERAL_BLOCK
} QueueType;

/* Node struct definition */
typedef struct node {
    tcb* TCB;        
    struct node* next;
    QueueType type;
} Node;

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
    int thread_status;        // Thread status (e.g., running, waiting, terminated)
    ucontext_t thread_context; // Thread context (e.g., register state)
    void *thread_stack;       // Thread stack pointer
    void *thread_return;      // Thread return value
    Node *joiner;           // Thread that is waiting for this thread to terminate
    void **joiner_return;      // Return value of the thread that is waiting for this thread to terminate
    // Thread priority (e.g., for scheduling)
    // Add more states as needed...

    // YOUR CODE HERE

} tcb;

#endif
