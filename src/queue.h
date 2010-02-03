#ifndef QUEUE_H
#define QUEUE_H

#include "thread.h"

struct QueueItem {
    char* val;
    struct QueueItem* next;
};

typedef struct {
    lhc_mutex lock;
    struct QueueItem* head;
    struct QueueItem* tail;
} Queue;

#define queue_empty(q) ((q)->head == NULL)

Queue* queue_init();
void queue_free(Queue* q);
void queue_push(Queue* q, const char* val);
void queue_pop(Queue* q);

/* WARNING WARNING WARNING:
 * the returned value has to be freed by YOU!
 */
char* queue_front(Queue *q);

/* WARNING WARNING WARNING:
 * the returned value is only valid until the next queue_pop!
 * (this also explains the rather verbose name)
 */
#define queue_front_nocopy_dont_use_this_if_not_sure(q) (q)->head->val

#endif /* QUEUE_H */
