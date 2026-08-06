#pragma once

/**
 * This files works to include a new view for the IP assignament
 * This is to set the values on decimal and not in hexadecimal
 *
 */

#include <furi.h>
#include <furi_hal.h>

#include <gui/gui.h>
#include <gui/view_dispatcher.h>

/**
 * Struct for the View
 */

typedef struct ip_assigner_t ip_assigner_t;
typedef void (*ip_assigner_callback_t)(void* context);

/**
 * Functions to work with this
 */

ip_assigner_t* ip_assigner_alloc();

void ip_assigner_free(ip_assigner_t* instance);

View* ip_assigner_get_view(ip_assigner_t* instance);

void ip_assigner_set_header(ip_assigner_t* instance, const char* text);

void ip_assigner_set_ip_array(ip_assigner_t* instance, uint8_t* ip_array);

void ip_assigner_callback(ip_assigner_t* instance, ip_assigner_callback_t callback, void* context);

void ip_assigner_reset(ip_assigner_t* instance);
