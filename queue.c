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
    size_t size;
    size_t visited;
} Queue;

/* ### List Helper Functions ###*/
void append_item(Node *item, Queue *q)
{
    q->size++;

    if (q->head == NULL)
    {
        q->head = item;
        q->tail = item;
        return;
    }

    q->tail->next = item;
    q->tail = item;
    return;
}

void *remove_head(Queue *q)
{
    Node *tmp = q->head;
    void *data = tmp->data;
    q->head = tmp->next;
    free(tmp);
    if (q->head == NULL)
        q->tail = NULL;
    q->size--;
    q->visited++;
    return data;
}

void destroy_list(Node *head)
{
    Node *tmp;
    while (head != NULL)
    {
        tmp = head;
        head = head->next;
        free(tmp);
    }
    return;
}

/* ############## -Code Start- ############## */

// this is lock for enqueue and dequeue.
// TODO consider using one queue for enqueue and one for dequeue
mtx_t read_lock, write_lock;
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
    destroy_list(data_queue->head);
    destroy_list(read_queue->head);
    free(data_queue);
    free(read_queue);
    mtx_destroy(&queue_lock);
    return;
}

void enqueue(void *data)
{
    // aquire lock.
    mtx_lock(&write_lock);
    bool is_read_lock_used = false;
    // In case the queue size is smaller then 2 simultanious appends and removes from the list can cause problems. otherwise head and tail are pointing to different locations and this is ok.
    if (data_queue->size <= 1)
    {
        is_read_lock_used = true;
        mtx_lock(&read_lock);
    }

    // write data to queue, increase data_queue->size by one.
    Node *tmp = (Node *)malloc(sizeof(Node));
    tmp->data = data;
    tmp->next = NULL;

    append_item(tmp, data_queue);

    // awake the oldest member of read_queue (if there is one).
    if (read_queue->size > 0)
    {
        cnd_signal(read_queue->head->data);
    }

    // release lock.
    mtx_unlock(&write_lock);
    if (is_read_lock_used)
    {
        mtx_unlock(&read_lock);
    }
    return;
}

void *dequeue(void)
{
    // aquire lock.
    mtx_lock(&read_lock);
    bool is_write_lock_used = false;

    if (data_queue->size <= 1)
    {
        is_write_lock_used = true;
        mtx_lock(&write_lock);
    }

    // cnd_t ticket;
    Node *tmp;
    void *data;
    if (data_queue->size == 0 || read_queue->size > 0)
    {
        tmp = (Node *)malloc(sizeof(Node));
        tmp->next = NULL;
        tmp->data = (cnd_t *)malloc(sizeof(cnd_t));
        append_item(tmp, read_queue);
        cnd_init(tmp->data);
        cnd_wait(tmp->data, &queue_lock);
    }
    // Note: we assume there can not be spurios wake ups of threads. in general we should add here a while loop to make sure there are items to deque. (it would be wise to add it either way to save us in case of bugs).

    // Nots: we can not assume that the head of read_queue is the CV this thread waited for.
    // Because in specific scheduler choices a different thread can run first.
    // Either way all the CV of threads that finished dequeing from data_queue will be dequeued from read_queue and this is the important part. (if we move this line to enqueue we can guruantee that but it may cause issues with the read_lock and write_lock)
    remove_head(read_queue);

    data = remove_head(data_queue);
    // release lock.
    mtx_unlock(&read_lock);
    if (is_write_lock_used)
    {
        mtx_unlock(&write_lock);
    }
    return data;
}

bool tryDequeue(void **item)
{
    // aquire lock.
    mtx_lock(&read_lock);
    bool is_write_lock_used = false;

    if (data_queue->size <= 1)
    {
        is_write_lock_used = true;
        mtx_lock(&write_lock);
    }

    if (data_queue->size == 0)
    {
        return false;
    }

    *item = remove_head(data_queue);
    mtx_unlock(&read_lock);
    if (is_write_lock_used)
    {
        mtx_unlock(&write_lock);
    }
    return true;
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