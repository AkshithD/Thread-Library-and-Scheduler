#ifndef TW_TYPES_H
#define TW_TYPES_H

#include <ucontext.h>

typedef unsigned int worker_t;

typedef struct TCB
{
    /* add important states in a thread control block */
    worker_t thread_id;       // Thread ID
    int thread_status;        // Thread status (e.g., running, waiting, terminated)
    ucontext_t thread_context; // Thread context (e.g., register state)
    void *thread_stack;       // Thread stack pointer
    // Thread priority (e.g., for scheduling)
    // Add more states as needed...

    // YOUR CODE HERE

} tcb;

#endif
