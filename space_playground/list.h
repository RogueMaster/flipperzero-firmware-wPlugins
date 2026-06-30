#ifndef LIST_H
#define LIST_H

#include <stddef.h>
#include <stdlib.h>

typedef struct ListNode {
    void* data;
    struct ListNode* next;
    struct ListNode* prev;
} ListNode;

typedef struct List {
    ListNode* head;
    ListNode* tail;
    size_t size;
} List;

List new_list() {
    List list;
    list.head = NULL;
    list.tail = NULL;
    list.size = 0;
    return list;
}

void list_free(List* list) {
    ListNode* current = list->head;
    while(current) {
        ListNode* next = current->next;
        free(current);
        current = next;
    }
    list->head = NULL;
    list->tail = NULL;
    list->size = 0;
}

void list_remove(List* list, ListNode* node) {
    if(!node || list->size == 0) return;

    if(node->prev) {
        node->prev->next = node->next;
    } else {
        list->head = node->next; // Node is head
    }

    if(node->next) {
        node->next->prev = node->prev;
    } else {
        list->tail = node->prev; // Node is tail
    }

    free(node);
    list->size--;
}

void list_push(List* list, void* data) {
    ListNode* node = (ListNode*)malloc(sizeof(ListNode));
    if(!node) return; // Handle memory allocation failure
    node->data = data;
    node->next = NULL;
    node->prev = list->tail;

    if(list->tail) {
        list->tail->next = node;
    } else {
        list->head = node; // First element
    }
    list->tail = node;
    list->size++;
}

void* list_pop_head(List* list) {
    if(list->head == NULL) return NULL; // List is empty

    ListNode* node = list->head;
    void* data = node->data;

    list->head = node->next;
    if(list->head) {
        list->head->prev = NULL;
    } else {
        list->tail = NULL; // List is now empty
    }
    
    free(node);
    list->size--;
    return data;
}

#endif
