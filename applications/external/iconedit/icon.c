#include "icon.h"

void delete_frames(Frame* frames) {
    Frame* f = frames;
    while(f) {
        free(f->data);
        Frame* nf = f->next;
        free(f);
        f = nf;
    }
}

IEIcon* ie_icon_alloc(bool default_size) {
    IEIcon* icon = (IEIcon*)malloc(sizeof(IEIcon));
    icon->width = 0;
    icon->height = 0;
    icon->frame_count = 1;
    icon->frame_rate = 4;
    icon->current_frame = 0;
    icon->frames = NULL;
    icon->tail = NULL;
    if(default_size) {
        ie_icon_reset(icon, 10, 10, NULL);
    }
    icon->name = furi_string_alloc_set_str(NEW_ICON_DEFAULT_NAME);
    return icon;
}

void ie_icon_free(IEIcon* icon) {
    delete_frames(icon->frames);
    furi_string_free(icon->name);
    free(icon);
}

// (Re)-allocates the image buffer and dimensions, resets name(?)
void ie_icon_reset(IEIcon* icon, size_t width, size_t height, uint8_t* data) {
    icon->width = width;
    icon->height = height;
    icon->frame_count = 1;
    icon->current_frame = 0;
    delete_frames(icon->frames);
    icon->frames = malloc(sizeof(Frame));
    icon->frames->data = data ? data : malloc(icon->width * icon->height * sizeof(uint8_t));
    icon->frames->next = NULL;
    icon->frames->prev = NULL;
    icon->tail = icon->frames;
}

Frame* ie_icon_get_frame(IEIcon* icon, size_t index) {
    if(index >= icon->frame_count || icon->frame_count == 0) {
        return NULL;
    }
    Frame* f = icon->frames;
    size_t i = 0;
    while(i < index) {
        f = f->next;
        i++;
    }
    return f;
}

Frame* ie_icon_get_current_frame(IEIcon* icon) {
    return ie_icon_get_frame(icon, icon->current_frame);
}

void ie_icon_duplicate_frame(IEIcon* icon, size_t index) {
    if(index >= icon->frame_count) {
        assert(false);
        return;
    }

    Frame* f = icon->frames;
    size_t i = 0;
    while(i < index) {
        f = f->next;
        i++;
    }
    Frame* new_frame = malloc(sizeof(Frame));
    new_frame->data = malloc(icon->width * icon->height * sizeof(uint8_t));
    memcpy(new_frame->data, f->data, icon->width * icon->height);
    new_frame->next = f->next;
    if(f->next) {
        f->next->prev = new_frame;
    }
    new_frame->prev = f;
    f->next = new_frame;
    icon->frame_count++;
    icon->current_frame++;
    if(new_frame->next == NULL) {
        icon->tail = new_frame;
    }
}

void ie_icon_delete_frame(IEIcon* icon, size_t index) {
    if(icon->frame_count == 1 || index >= icon->frame_count || !icon->frames) {
        return;
    }

    Frame* f = icon->frames;
    size_t i = 0;
    while(i < index) {
        f = f->next;
        i++;
    }

    if(f->prev == NULL) { // start of list
        icon->frames = f->next;
        if(icon->frames) {
            icon->frames->prev = NULL;
        }
    } else {
        f->prev->next = f->next;
        if(f->next) {
            f->next->prev = f->prev;
        } else {
            icon->current_frame--;
            icon->tail = f->prev;
        }
    }
    icon->frame_count--;
    free(f->data);
    free(f);
}

// IEIconAnimation
void ie_icon_animation_next_frame(IEIconAnimation* instance) {
    instance->frame = instance->frame->next;
    if(instance->frame == NULL) {
        instance->frame = instance->icon->frames;
    }
}
void ie_icon_animation_prev_frame(IEIconAnimation* instance) {
    instance->frame = instance->frame->prev;
    if(instance->frame == NULL) {
        instance->frame = instance->icon->tail;
    }
}

void ie_icon_animation_timer_callback(void* context) {
    IEIconAnimation* instance = context;
    if(!instance->animating) return;
    // advance frame
    ie_icon_animation_next_frame(instance);

    if(instance->callback) {
        instance->callback(instance, instance->callback_context);
    }
}

IEIconAnimation* ie_icon_animation_alloc(IEIcon* icon) {
    IEIconAnimation* instance = malloc(sizeof(IEIconAnimation));
    instance->icon = icon;
    instance->frame = instance->icon->frames;
    instance->timer =
        furi_timer_alloc(ie_icon_animation_timer_callback, FuriTimerTypePeriodic, instance);
    return instance;
}

void ie_icon_animation_free(IEIconAnimation* instance) {
    // ie_icon_animation_stop(instance);
    furi_timer_free(instance->timer);
    free(instance);
}

void ie_icon_animation_set_update_callback(
    IEIconAnimation* instance,
    IEIconAnimationCallback callback,
    void* context) {
    instance->callback = callback;
    instance->callback_context = context;
}

void ie_icon_animation_start(IEIconAnimation* instance) {
    if(!instance->animating) {
        instance->animating = true;
        furi_timer_start(
            instance->timer, (furi_kernel_get_tick_frequency() / instance->icon->frame_rate));
    }
}

void ie_icon_animation_stop(IEIconAnimation* instance) {
    instance->animating = false;
    furi_timer_stop(instance->timer);
    instance->frame = instance->icon->frames; // reset to beginning
}

Frame* ie_icon_animation_get_current_frame(IEIconAnimation* instance) {
    return instance->frame;
}
