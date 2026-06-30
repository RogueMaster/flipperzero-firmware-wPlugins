#pragma once

#include <furi.h>

#define NEW_ICON_DEFAULT_NAME "new_icon"

typedef struct Frame {
    uint8_t* data;
    struct Frame* next;
    struct Frame* prev;
} Frame;

typedef struct {
    size_t width;
    size_t height;
    size_t frame_count;
    size_t frame_rate;

    size_t current_frame;
    Frame* frames; // head
    Frame* tail;
    FuriString* name;
} IEIcon;

typedef struct IEIconAnimation IEIconAnimation;
typedef void (*IEIconAnimationCallback)(IEIconAnimation* instance, void* context);
typedef struct IEIconAnimation {
    IEIcon* icon;
    Frame* frame;
    bool animating;
    FuriTimer* timer;
    IEIconAnimationCallback callback;
    void* callback_context;
} IEIconAnimation;

// IEIcon methods
IEIcon* ie_icon_alloc(bool default_size);
void ie_icon_free(IEIcon* icon);

// Clear all icon data, resetting to blank icon. Optionally supply data
void ie_icon_reset(IEIcon* icon, size_t width, size_t height, uint8_t* data);

Frame* ie_icon_get_frame(IEIcon* icon, size_t index);
Frame* ie_icon_get_current_frame(IEIcon* icon);
void ie_icon_duplicate_frame(IEIcon* icon, size_t index);
void ie_icon_delete_frame(IEIcon* icon, size_t index);

// IEIconAnimation methods
IEIconAnimation* ie_icon_animation_alloc(IEIcon* icon);
void ie_icon_animation_free(IEIconAnimation* instance);
void ie_icon_animation_set_update_callback(
    IEIconAnimation* instance,
    IEIconAnimationCallback callback,
    void* context);
void ie_icon_animation_start(IEIconAnimation* instance);
void ie_icon_animation_stop(IEIconAnimation* instance);
Frame* ie_icon_animation_get_current_frame(IEIconAnimation* instance);
void ie_icon_animation_next_frame(IEIconAnimation* instance);
void ie_icon_animation_prev_frame(IEIconAnimation* instance);
