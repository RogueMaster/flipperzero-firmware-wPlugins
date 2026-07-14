#include "mcp_can_2515.h"

typedef struct FrameCANQueueNode {
    CANFRAME frame;
    struct FrameCANQueueNode* next_node;
} FrameCANQueueNode;

typedef struct {
    FrameCANQueueNode* last_node;
    FrameCANQueueNode* first_node;
} FrameCANQueue;

FrameCANQueue* frame_can_queue_alloc();
void frame_can_queue_free(FrameCANQueue* frame_queue);

CANFRAME* frame_can_queue_get(FrameCANQueue* queue);
void frame_can_queue_push(FrameCANQueue* queue, CANFRAME frame);
void frame_can_queue_pop(FrameCANQueue* queue);
