#include "list.h"
#include "helpers.h"

List* list_make() {
    List* list = malloc(sizeof(List));
    if(list != NULL) {
        list->head = NULL;
        list->tail = NULL;
        list->count = 0;
    } else {
        FURI_LOG_W("LIST", "Failed to create list");
    }
    return list;
}

void list_free(List* list) {
    if(list == NULL) return;

    ListItem* start = list->head;
    while(start) {
        ListItem* next = start->next;
        free(start->data);
        free(start);
        start = next;
    }
    free(list);
}

void list_free_data(List* list) {
    if(list == NULL) return;

    ListItem* start = list->head;
    while(start) {
        ListItem* next = start->next;
        free(start->data);
        free(start);
        start = next;
    }

    list->head = NULL;
    list->tail = NULL;
    list->count = 0;
}

void list_clear(List* list) {
    if(list == NULL) return;
    ListItem* start = list->head;
    while(start) {
        ListItem* next = start->next;
        free(start);
        start = next;
    }
    list->head = NULL;
    list->tail = NULL;
    list->count = 0;
}

void list_push_back(void* data, List* list) {
    if(list == NULL) {
        FURI_LOG_W("LIST", "List not initialized, cannot push data");
        return;
    }
    ListItem* newItem = malloc(sizeof(ListItem));
    if(newItem != NULL) {
        newItem->data = data;
        newItem->next = NULL;
        newItem->prev = list->tail;

        if(list->tail == NULL) {
            list->head = newItem;
            list->tail = newItem;
        } else {
            list->tail->next = newItem;
            list->tail = newItem;
        }
        list->count++;
    }
}

void list_push_front(void* data, List* list) {
    if(list == NULL) {
        FURI_LOG_W("LIST", "List not initialized, cannot push data");
        return;
    }
    ListItem* newItem = malloc(sizeof(ListItem));
    if(newItem != NULL) {
        newItem->data = data;
        newItem->next = list->head;
        if(list->head == NULL) {
            list->head = newItem;
            list->tail = newItem;
        } else {
            list->head->prev = newItem;
            list->head = newItem;
        }
        list->count++;
    }
}

void* list_pop_back(List* list) {
    if(!check_pointer(list)) {
        FURI_LOG_W("LIST", "List not initialized, cannot pop data");
        return NULL;
    }

    if(!check_pointer(list->tail)) {
        FURI_LOG_W("LIST", "List empty, cannot pop");
        return NULL;
    }
    void* data = list->tail->data;
    check_pointer(data);
    ListItem* prev = list->tail->prev;
    check_pointer(prev);
    if(prev) {
        prev->next = NULL;
    } else {
        list->head = NULL;
    }
    free(list->tail);
    list->tail = prev;
    list->count--;
    return data;
}

void* list_pop_front(List* list) {
    if(!check_pointer(list)) {
        FURI_LOG_W("LIST", "List not initialized, cannot pop data");
        return NULL;
    }
    if(!check_pointer(list->head)) {
        FURI_LOG_W("LIST", "List empty, cannot pop");
        return NULL;
    }
    void* data = list->head->data;
    check_pointer(data);
    ListItem* next = list->head->next;
    if(next) {
        next->prev = NULL;
    } else {
        list->tail = NULL;
    }
    free(list->head);
    list->head = next;
    list->count--;
    return data;
}

void* list_pop_at(size_t index, List* list) {
    if(!check_pointer(list)) {
        FURI_LOG_W("LIST", "List not initialized, cannot pop data");
        return NULL;
    }
    if(index >= list->count) {
        FURI_LOG_W("LIST", "Out of range, cannot pop");
        return NULL;
    }
    if(index == 0) {
        return list_pop_front(list);
    }
    if(index == list->count - 1) {
        return list_pop_back(list);
    }
    ListItem* current = list->head;
    check_pointer(current);
    for(size_t i = 0; i < index; i++) {
        current = current->next;
    }
    check_pointer(current);
    void* data = current->data;
    check_pointer(data);
    current->prev->next = current->next;
    current->next->prev = current->prev;
    free(current);
    list->count--;
    return data;
}

void list_remove_item(void* data, List* list) {
    if(list == NULL) {
        return;
    }
    ListItem* current = list->head;
    while(current != NULL) {
        if(current->data == data) {
            if(current->prev != NULL) {
                current->prev->next = current->next;
            } else {
                list->head = current->next;
            }
            if(current->next != NULL) {
                current->next->prev = current->prev;
            } else {
                list->tail = current->prev;
            }
            free(current);
            list->count--;
            break;
        }
        current = current->next;
    }
}

void list_remove_at(size_t index, List* list) {
    if(list == NULL) {
        return;
    }
    void* d = list_pop_at(index, list);
    free(d);
}

List* list_splice(size_t index, size_t count, List* list) {
    if(list == NULL) {
        FURI_LOG_W("LIST", "List not initialized, cannot splice");
        return NULL;
    }
    List* newList = list_make();
    if(index >= list->count || count == 0) {
        return newList;
    }
    if(index == 0 && count >= list->count) {
        newList->head = list->head;
        newList->tail = list->tail;
        newList->count = list->count;
        list->head = NULL;
        list->tail = NULL;
        list->count = 0;
        return newList;
    }

    ListItem* start = list->head;
    for(size_t i = 0; i < index; i++) {
        start = start->next;
    }

    size_t c = count;
    if(c > list->count) c = list->count - index;

    ListItem* end = start;
    for(size_t i = 1; i < c && end->next; i++) {
        end = end->next;
        if(end->next == NULL) c = i;
    }

    newList->head = start;
    newList->tail = end;
    newList->count = c;

    if(end->next != NULL) {
        end->next->prev = start->prev;
    } else {
        list->tail = start->prev;
    }

    if(start->prev != NULL) {
        start->prev->next = end->next;
    } else {
        list->head = end->next;
    }

    start->prev = NULL;
    end->next = NULL;
    list->count -= c;
    return newList;
}

void* list_peek_front(List* list) {
    if(list == NULL || list->head == NULL) {
        return NULL;
    }
    return list->head->data;
}

ListItem* list_get_index(List* list, size_t index) {
    check_pointer(list);
    ListItem* curr = list->head;
    check_pointer(curr);
    if(index > list->count || !curr) return NULL;
    for(size_t i = 0; i < index; i++) {
        if(!curr) return NULL;
        curr = curr->next;
    }
    return curr;
}

void* list_peek_index(List* list, size_t index) {
    ListItem* curr = list_get_index(list, index);
    check_pointer(curr);
    if(curr) {
        check_pointer(curr->data);
        return curr->data;
    }
    return NULL;
}

void* list_peek_back(List* list) {
    if(!list || !list->tail) {
        return NULL;
    }
    check_pointer(list->tail->data);
    return list->tail->data;
}
