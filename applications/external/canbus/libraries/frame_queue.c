#include "frame_queue.h"

FrameCANQueueNode* frame_can_queue_node_alloc() {
    FrameCANQueueNode* node = malloc(sizeof(FrameCANQueueNode));
    return node;
}

void frame_can_queue_node_free(FrameCANQueueNode* node) {
    free(node);
}

FrameCANQueue* frame_can_queue_alloc() {
    FrameCANQueue* frame_queue = malloc(sizeof(FrameCANQueue));

    frame_queue->first_node = NULL;
    frame_queue->last_node = NULL;

    return frame_queue;
}

void frame_can_queue_free(FrameCANQueue* frame_queue) {
    free(frame_queue);
}

void frame_can_queue_push(FrameCANQueue* queue, CANFRAME frame) {
    FrameCANQueueNode* new_node = frame_can_queue_node_alloc();
    new_node->frame = frame;

    new_node->next_node = queue->last_node;
    queue->last_node = new_node;

    if(queue->first_node == NULL) queue->first_node = new_node;
}

void frame_can_queue_pop(FrameCANQueue* queue) {
    FrameCANQueueNode* node_ptr = queue->last_node;
    if(node_ptr != NULL) {
        if(node_ptr->next_node != NULL) {
            while(true) {
                if(node_ptr->next_node == queue->first_node) {
                    break;
                } else {
                    node_ptr = node_ptr->next_node;
                }
            }

            frame_can_queue_node_free(node_ptr->next_node);
            queue->first_node = node_ptr;
            queue->first_node->next_node = NULL;
        } else {
            frame_can_queue_node_free(node_ptr);
            queue->last_node = NULL;
            queue->first_node = NULL;
        }
    }
}

CANFRAME* frame_can_queue_get(FrameCANQueue* queue) {
    if(queue->first_node == NULL) return NULL;
    return &queue->first_node->frame;
}
