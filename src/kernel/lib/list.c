/*
 * List - implementation of a doubly linked list
 */

#include <common.h>
#include <lib/list.h>
#include <mem/vmm.h>

list_t* list_create() {
    // TODO: use a proper heap (malloc)
    list_t* list = vmm_alloc(sizeof(list_t), VMM_KERNEL);
    list->head = list->tail = 0;
    return list;
}

void list_destroy(list_t* list) {
    while (!list_empty(list))
        list_pop_front(list);
    vmm_free(list, sizeof(list_t));
}

static list_node_t* list_create_node(void* data,
        list_node_t* prev, list_node_t* next) {
    // TODO: use a proper heap (malloc)
    list_node_t* node = vmm_alloc(sizeof(list_node_t), VMM_KERNEL);
    node->data = data;
    node->prev = prev;
    node->next = next;
    return node;
}

size_t list_size(list_t* list) {
    size_t num = 0;
    for (list_node_t* node = list->head; node; num++)
        node = node->next;
    return num;
}

uint8_t list_empty(list_t* list) {
    return !list->head;
}

static list_node_t* list_check(list_node_t* node) {
    if (!node)
        println("%4aNode not found in list%a");
    return node;
}

void* list_front(list_t* list) {
    return list_check(list->head)->data;
}

void* list_back(list_t* list) {
    return list_check(list->tail)->data;
}

void list_push_front(list_t* list, void* data) {
    list->head = list_create_node(data, 0, list->head);
    if (list->head->next)
        list->head->next->prev = list->head;
    if (!list->tail)
        list->tail = list->head;
}

void list_push_back(list_t* list, void* data) {
    list->tail = list_create_node(data, list->tail, 0);
    if (list->tail->prev)
        list->tail->prev->next = list->tail;
    if (!list->head)
        list->head = list->tail;
}

void* list_pop_front(list_t* list) {
    list_node_t* node = list_check(list->head);
    void* data = node->data;
    list->head = node->next;
    vmm_free(node, sizeof(list_node_t));
    if (!list->head)
        list->tail = 0;
    else
        list->head->prev = 0;
    return data;
}

void* list_pop_back(list_t* list) {
    list_node_t* node = list_check(list->tail);
    void* data = node->data;
    list->tail = node->prev;
    vmm_free(node, sizeof(list_node_t));
    if (!list->tail)
        list->head = 0;
    else
        list->tail->next = 0;
    return data;
}

uint8_t list_find(list_t* list, void* data) {
    list_node_t* node = list->head;
    for (int i = 0; node && node->data != data; i++)
        node = node->next;
    return !!node;
}

void list_remove(list_t* list, void* data) {
    list_node_t* node = list->head;
    for (int i = 0; node && node->data != data; i++)
        node = node->next;
    if (!list_check(node))
        return;
    if (node->prev)
        node->prev->next = node->next;
    else
        list->head = node->next;
    if (node->next)
        node->next->prev = node->prev;
    else
        list->tail = node->prev;
}
