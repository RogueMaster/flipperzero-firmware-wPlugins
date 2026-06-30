#pragma once
#include <gui/view_dispatcher.h>

typedef struct {
    View* view;
    ViewNavigationCallback orig_previous_callback;
    ViewCallback orig_enter_callback;
    ViewCallback orig_exit_callback;    
    ViewInputCallback orig_input_callback;
    ViewCustomCallback orig_custom_callback;
    void* orig_context;

    ViewNavigationCallback new_previous_callback;
    ViewCallback new_enter_callback;
    ViewCallback new_exit_callback;    
    ViewInputCallback new_input_callback;
    ViewCustomCallback new_custom_callback;
    void* new_previous_callback_context;
    void* new_enter_callback_context;
    void* new_exit_callback_context;
    void* new_input_callback_context;
    void* new_custom_callback_context;

} ViewHijacker;

ViewHijacker* alloc_view_hijacker();
void free_view_hijacker(ViewHijacker* view_hijacker);
void view_hijacker_attach_to_view(ViewHijacker* view_hijacker, View* view);
void view_hijacker_detach_from_view(ViewHijacker* view_hijacker);
void view_hijacker_attach_to_view_dispacher_current(ViewHijacker* view_hijacker, ViewDispatcher* view_dispatcher);
void view_hijacker_hijack_previous_callback(ViewHijacker* view_hijacker, ViewNavigationCallback previous_callback, void* context);
void view_hijacker_hijack_enter_callback(ViewHijacker* view_hijacker, ViewCallback enter_callback, void* context);
void view_hijacker_hijack_exit_callback(ViewHijacker* view_hijacker, ViewCallback exit_callback, void* context);
void view_hijacker_hijack_input_callback(ViewHijacker* view_hijacker, ViewInputCallback input_callback, void* context);
void view_hijacker_hijack_custom_callback(ViewHijacker* view_hijacker, ViewCustomCallback custom_callback, void* context);
void view_hijacker_restore_previous_callback(ViewHijacker* view_hijacker);
void view_hijacker_restore_enter_callback(ViewHijacker* view_hijacker);
void view_hijacker_restore_exit_callback(ViewHijacker* view_hijacker);
void view_hijacker_restore_input_callback(ViewHijacker* view_hijacker);
void view_hijacker_restore_custom_callback(ViewHijacker* view_hijacker);
