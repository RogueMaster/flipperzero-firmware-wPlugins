// utils.h
#pragma once
#include "app_state.h"
#include "confirmation_view.h"

// Function declarations
void show_confirmation_dialog_ex(
    void* context,
    const char* header,
    const char* text,
    ConfirmationViewCallback ok_callback,
    ConfirmationViewCallback cancel_callback);

void show_confirmation_view_wrapper(void* context, ConfirmationView* view); // Added declaration

void clear_pcap_files(void* context);
void clear_wardrive_files(void* context);
