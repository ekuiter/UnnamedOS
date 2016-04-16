/*
 * Queue - a queue class (implemented with a doubly linked list)
 */

#include <common.h>
#include <lib/queue.h>

queue_t* queue_create() { return list_create(); }
void queue_destroy(queue_t* queue) { list_destroy(queue); }
uint8_t queue_empty(queue_t* queue) { return list_empty(queue); }
size_t queue_size(queue_t* queue) { return list_size(queue); }
void* queue_front(queue_t* queue) { return list_front(queue); }
void queue_enqueue(queue_t* queue, void* data) { list_push_back(queue, data); }
void* queue_dequeue(queue_t* queue) { return list_pop_front(queue); }
uint8_t queue_find(queue_t* queue, void* data) { return list_find(queue, data); }
void queue_remove(queue_t* queue, void* data) { list_remove(queue, data); }
