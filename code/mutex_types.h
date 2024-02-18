#ifndef MTX_TYPES_H
#define MTX_TYPES_H

#include "thread_worker_types.h"

typedef enum {
    QUEUE_TYPE_READY,
    QUEUE_TYPE_MUTEX_BLOCK,
    QUEUE_TYPE_GENERAL_BLOCK,
} QueueType;

/* Node struct definition */
typedef struct node {
    tcb* TCB;        
    struct node* next;
    QueueType type;
} Node;

/* mutex struct definition */
typedef struct worker_mutex_t
{
    volatile int lock; // Indicates if the mutex is locked (1) or unlocked (0)
    worker_t owner; // The ID of the thread that currently holds the lock, if any
    Node* wait_queue_head; // Pointer to the head of the queue of threads waiting for this mutex
    Node* wait_queue_tail; // Pointer to the tail of the queue of threads waiting for this mutex
} worker_mutex_t;
#endif
