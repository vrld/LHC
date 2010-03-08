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

    lhc_mutex_lock( &q->lock );
    {
        if (q->head == NULL)
            q->head = tmp;

        if (q->tail != NULL)
            q->tail->next = tmp;

        q->tail = tmp;
    }
    lhc_mutex_unlock( &q->lock );
}

void queue_pop(Queue *q)
{
    if (queue_empty(q))
        return;

    struct QueueItem *tmp;

    lhc_mutex_lock( &q->lock );
    {
        tmp = q->head;
        q->head = tmp->next;
        if (q->head == NULL)
            q->tail = NULL;
    }
    lhc_mutex_unlock( &q->lock );

    free(tmp->val);
    free(tmp);
    tmp = NULL;
}

char* queue_front(Queue *q)
{
    char* val;
    lhc_mutex_lock( &q->lock );
    {
        val = malloc(strlen(q->head->val) + 1);
        strcpy(val, q->head->val);
    }
    lhc_mutex_unlock( &q->lock );

    return val;
}

