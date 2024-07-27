#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <stdio.h>
#include <threads.h>
#include "queue.h"

/* ### Linked List ###*/
// We are using a linked list to implement the queue (this way the queue size is not bounded arbitrarily)
typedef struct Node
{
    void *data;
    struct Node *next;
} Node;

/* ### data queue ### */
// the data queue is implemented using a linked list.
// the size and visited fields are atomic to avoid undefined behaviour in case of multithreading.
typedef struct Queue
{
    Node *head;
    Node *tail;
    atomic_size_t size;
    atomic_size_t visited;
} Queue;

/* ### List Helper Functions ###*/
void append_item(Node *item, Queue *q)
{
    q->size++;
    if (q->size == 0)
    {
        q->head = item;
        q->tail = item;
        return;
    }

    q->tail->next = item;
    q->tail = item;
    return;
}

/* ############## -Code Start- ############## */

// this is lock for enqueue and dequeue.
// TODO consider using one queue for enqueue and one for dequeue
mtx_t queue_lock;
static Queue *data_queue;
// please note we used the same structure for read_queue even though its visited value is unused. this means the visited value of read_queue will not be maintained.
static Queue *read_queue;

void initQueue(void)
{
    data_queue = (Queue *)malloc(sizeof(Queue));
    read_queue = (Queue *)malloc(sizeof(Queue));
    // Initialize queues values.
    data_queue->head = NULL;
    data_queue->tail = NULL;
    data_queue->size = 0;
    data_queue->visited = 0;
    read_queue->head = NULL;
    read_queue->tail = NULL;
    read_queue->size = 0;
    read_queue->visited = 0;
    mtx_init(&queue_lock, mtx_plain);
}

void destroyQueue(void)
{
    // destory linked lists and free queues
}

void enqueue(void *data)
{
    // aquire lock.
    mtx_lock(&queue_lock);

    // write data to queue, increase data_queue->size by one.
    Node *tmp = (Node *)malloc(sizeof(Node));
    tmp->data = data;
    tmp->next = NULL;

    append_item(tmp, data_queue);

    // awake the oldest member of read_queue (if there is one).
    if (read_queue->size > 0)
    {
        cnd_t ticket = *((cnd_t *)read_queue->head->data);
        cnd_signal(&ticket);
        tmp = read_queue->head;
        read_queue->head = tmp->next;
        free(tmp);
        if (read_queue->head == NULL)
            read_queue->tail = NULL;
        read_queue->size--;
    }

    // release lock.
    mtx_unlock(&queue_lock);
    return;
}

void *dequeue(void)
{
    // aquire lock.
    mtx_lock(&queue_lock);

    cnd_t ticket;
    Node *tmp;
    void *data;
    if (data_queue->size == 0)
    {
        cnd_init(&ticket);
        tmp = (Node *)malloc(sizeof(Node));
        tmp->data = &ticket;
        tmp->next = NULL;
        append_item(tmp, read_queue);
        cnd_wait(&ticket, &queue_lock);
    }
    // This is for added sequrity (for example in case spurios wakeups ARE allowed or in case of bugs).
    while (data_queue->size == 0)
        cnd_wait(&ticket, &queue_lock);

    tmp = data_queue->head;
    data = tmp->data;
    data_queue->head = tmp->next;
    free(tmp);
    if (data_queue->head == NULL)
        data_queue->tail = NULL;

    return data;
}

bool tryDequeue(void **args)
{
    return false;
}

size_t size(void)
{
    return data_queue->size;
}
size_t waiting(void)
{
    return read_queue->size;
}
size_t visited(void)
{
    return data_queue->visited;
}