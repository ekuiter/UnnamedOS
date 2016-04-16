#ifndef LIB_QUEUE_H
#define LIB_QUEUE_H

#include <lib/list.h>

typedef list_t queue_t;

queue_t* queue_create();
void queue_destroy(queue_t* queue);
uint8_t queue_empty(queue_t* queue);
size_t queue_size(queue_t* queue);
void* queue_front(queue_t* queue);
void queue_enqueue(queue_t* queue, void* data);
void* queue_dequeue(queue_t* queue);
uint8_t queue_find(queue_t* queue, void* data);
void queue_remove(queue_t* queue, void* data);

#endif
