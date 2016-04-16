#ifndef LIB_LIST_H
#define LIB_LIST_H

typedef struct list_node {
    void* data;
    struct list_node* prev;
    struct list_node* next;
} list_node_t;

typedef struct {
    list_node_t* head;
    list_node_t* tail;
} list_t;

list_t* list_create();
void list_destroy(list_t* list);
size_t list_size(list_t* list);
uint8_t list_empty(list_t* list);
void* list_front(list_t* list);
void* list_back(list_t* list);
void list_push_front(list_t* list, void* data);
void list_push_back(list_t* list, void* data);
void* list_pop_front(list_t* list);
void* list_pop_back(list_t* list);
uint8_t list_find(list_t* list, void* data);
void list_remove(list_t* list, void* data);

#endif
