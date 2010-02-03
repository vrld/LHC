#include "queue.h"
#include <stdlib.h>
#include <string.h>

Queue* queue_init()
{
    Queue* q = (Queue*)malloc(sizeof(Queue));
    q->head = q->tail = NULL;
    lhc_mutex_init( &q->lock );
    return q;
}

void queue_free(Queue *q)
{
    while (!queue_empty(q))
        queue_pop(q);

    lhc_mutex_destroy( &q->lock );
    free(q);
}

void queue_push(Queue* q, const char* val)
{
    struct QueueItem *tmp;
    tmp = (struct QueueItem*)malloc(sizeof(struct QueueItem));

    tmp->next = NULL;
    tmp->val = (char*)malloc(strlen(val) + 1);
    strcpy(tmp->val, val);

    CRITICAL_SECTION( &q->lock )
    {
        if (q->head == NULL)
            q->head = tmp;

        if (q->tail != NULL)
            q->tail->next = tmp;

        q->tail = tmp;
    }
}

void queue_pop(Queue *q)
{
    if (queue_empty(q))
        return;

    struct QueueItem *tmp;

    CRITICAL_SECTION( &q->lock )
    {
        tmp = q->head;
        q->head = tmp->next;
        if (q->head == NULL)
            q->tail = NULL;
    }

    free(tmp->val);
    free(tmp);
    tmp = NULL;
}

char* queue_front(Queue *q)
{
    char* val;
    CRITICAL_SECTION( &q->lock )
    {
        val = malloc(strlen(q->head->val) + 1);
        strcpy(val, q->head->val);
    }

    return val;
}

