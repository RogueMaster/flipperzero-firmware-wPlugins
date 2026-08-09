/*
 * Net Calculator port for Flipper Zero
 * Copyright (C) 2026 Dominik Krzywański
 * Publisher: WolfRor
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include <furi.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <gui/modules/number_input.h>
#include <gui/modules/widget.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VLSM_MAX_REQUESTS        16
#define VLSM_EMPTY_REQUEST_INDEX UINT32_MAX

typedef enum {
    VlsmViewMain,
    VlsmViewRequests,
    VlsmViewNumberInput,
    VlsmViewResults,
} VlsmView;

typedef enum {
    VlsmNumberPrefix,
    VlsmNumberHosts,
    VlsmNumberIpOctet,
} VlsmNumberMode;

typedef enum {
    VlsmMenuIp,
    VlsmMenuPrefix,
    VlsmMenuAddHosts,
    VlsmMenuRequests,
    VlsmMenuCalculate,
    VlsmMenuReset,
} VlsmMenuItem;

typedef struct {
    ViewDispatcher* view_dispatcher;

    Submenu* submenu;
    Submenu* requests_submenu;
    NumberInput* number_input;
    Widget* widget;

    Gui* gui;

    uint8_t ip[4];
    uint8_t prefix;

    uint32_t host_requests[VLSM_MAX_REQUESTS];
    size_t host_count;

    VlsmView current_view;
    VlsmNumberMode number_mode;

    uint8_t ip_octet_index;

    FuriString* results;

    char ip_label[32];
    char prefix_label[20];
    char requests_label[32];

    char request_item_labels[VLSM_MAX_REQUESTS][32];
} VlsmApp;

static void vlsm_rebuild_menu(VlsmApp* app);
static void vlsm_rebuild_requests_menu(VlsmApp* app);

/*
 * Convert four IPv4 octets into one 32-bit integer.
 */
static uint32_t vlsm_ip_to_u32(const uint8_t ip[4]) {
    return ((uint32_t)ip[0] << 24) | ((uint32_t)ip[1] << 16) | ((uint32_t)ip[2] << 8) |
           (uint32_t)ip[3];
}

/*
 * Append a 32-bit IPv4 address to a FuriString.
 */
static void vlsm_append_ip(FuriString* text, uint32_t ip) {
    furi_string_cat_printf(
        text,
        "%lu.%lu.%lu.%lu",
        (unsigned long)((ip >> 24) & 0xFFU),
        (unsigned long)((ip >> 16) & 0xFFU),
        (unsigned long)((ip >> 8) & 0xFFU),
        (unsigned long)(ip & 0xFFU));
}

/*
 * Convert prefix length, for example /24, into a 32-bit mask.
 */
static uint32_t vlsm_prefix_mask(uint8_t prefix) {
    if(prefix == 0U) {
        return 0U;
    }

    return 0xFFFFFFFFUL << (32U - prefix);
}

/*
 * Sort host requests from largest to smallest.
 *
 * Only a temporary copy is sorted. The order displayed in the Requests
 * menu remains the same as the order in which values were added.
 */
static void vlsm_sort_hosts_desc(uint32_t* values, size_t count) {
    for(size_t i = 1U; i < count; i++) {
        uint32_t value = values[i];
        size_t j = i;

        while(j > 0U && values[j - 1U] < value) {
            values[j] = values[j - 1U];
            j--;
        }

        values[j] = value;
    }
}

/*
 * Determine the smallest subnet block that can contain the requested
 * number of usable IPv4 hosts.
 */
static bool vlsm_block_for_hosts(uint32_t hosts, uint32_t* block_size, uint8_t* prefix) {
    uint64_t required = (uint64_t)hosts + 2U;
    uint64_t block = 1U;

    uint8_t calculated_prefix = 32U;

    while(block < required) {
        block <<= 1U;

        if(calculated_prefix == 0U || block > (1ULL << 32U)) {
            return false;
        }

        calculated_prefix--;
    }

    if(block > 0xFFFFFFFFULL) {
        return false;
    }

    *block_size = (uint32_t)block;
    *prefix = calculated_prefix;

    return true;
}

/*
 * Calculate all VLSM networks and prepare text for the result view.
 */
static void vlsm_calculate(VlsmApp* app) {
    furi_string_reset(app->results);

    if(app->host_count == 0U) {
        furi_string_set(
            app->results,
            "No subnet requests.\n"
            "Add host counts first.");

        return;
    }

    uint32_t sorted[VLSM_MAX_REQUESTS];

    for(size_t i = 0U; i < app->host_count; i++) {
        sorted[i] = app->host_requests[i];
    }

    vlsm_sort_hosts_desc(sorted, app->host_count);

    const uint32_t parent_mask = vlsm_prefix_mask(app->prefix);

    const uint32_t entered_ip = vlsm_ip_to_u32(app->ip);

    const uint32_t parent_network = entered_ip & parent_mask;

    const uint32_t parent_broadcast = parent_network | ~parent_mask;

    uint64_t cursor = parent_network;

    furi_string_cat(app->results, "Parent: ");
    vlsm_append_ip(app->results, parent_network);

    furi_string_cat_printf(app->results, "/%u\n\n", app->prefix);

    for(size_t i = 0U; i < app->host_count; i++) {
        uint32_t block_size;
        uint8_t subnet_prefix;

        if(!vlsm_block_for_hosts(sorted[i], &block_size, &subnet_prefix)) {
            furi_string_cat_printf(
                app->results, "#%u: invalid host count\n", (unsigned int)(i + 1U));

            continue;
        }

        /*
         * Align the next subnet to the boundary required by its block size.
         */
        const uint64_t aligned = (cursor + block_size - 1U) & ~((uint64_t)block_size - 1U);

        const uint64_t broadcast64 = aligned + block_size - 1U;

        if(subnet_prefix < app->prefix || broadcast64 > parent_broadcast ||
           broadcast64 > 0xFFFFFFFFULL) {
            furi_string_cat_printf(
                app->results,
                "#%u: %lu hosts\n"
                "OVERFLOW\n",
                (unsigned int)(i + 1U),
                (unsigned long)sorted[i]);

            break;
        }

        const uint32_t network = (uint32_t)aligned;

        const uint32_t broadcast = (uint32_t)broadcast64;

        const uint32_t first_host = network + 1U;

        const uint32_t last_host = broadcast - 1U;

        furi_string_cat_printf(
            app->results,
            "#%u  hosts:%lu  /%u\n"
            "N: ",
            (unsigned int)(i + 1U),
            (unsigned long)sorted[i],
            subnet_prefix);

        vlsm_append_ip(app->results, network);

        furi_string_cat(app->results, "\nF: ");

        vlsm_append_ip(app->results, first_host);

        furi_string_cat(app->results, "\nL: ");

        vlsm_append_ip(app->results, last_host);

        furi_string_cat(app->results, "\nB: ");

        vlsm_append_ip(app->results, broadcast);

        furi_string_cat(app->results, "\n\n");

        cursor = broadcast64 + 1U;
    }
}

/*
 * Change the currently displayed application view.
 */
static void vlsm_switch_view(VlsmApp* app, VlsmView view) {
    app->current_view = view;

    view_dispatcher_switch_to_view(app->view_dispatcher, view);
}

/*
 * Callback called after accepting a number input.
 */
static void vlsm_number_saved(void* context, int32_t number) {
    VlsmApp* app = context;

    if(app->number_mode == VlsmNumberPrefix) {
        app->prefix = (uint8_t)number;

        vlsm_rebuild_menu(app);
        vlsm_switch_view(app, VlsmViewMain);

        return;
    }

    if(app->number_mode == VlsmNumberHosts) {
        if(app->host_count < VLSM_MAX_REQUESTS) {
            app->host_requests[app->host_count] = (uint32_t)number;

            app->host_count++;
        }

        vlsm_rebuild_menu(app);
        vlsm_rebuild_requests_menu(app);

        vlsm_switch_view(app, VlsmViewMain);

        return;
    }

    /*
     * IPv4 address input.
     *
     * Each octet is entered separately as a decimal number.
     */
    app->ip[app->ip_octet_index] = (uint8_t)number;

    if(app->ip_octet_index < 3U) {
        app->ip_octet_index++;

        char header[24];

        snprintf(header, sizeof(header), "IP octet %u/4", app->ip_octet_index + 1U);

        number_input_set_header_text(app->number_input, header);

        number_input_set_result_callback(
            app->number_input, vlsm_number_saved, app, app->ip[app->ip_octet_index], 0, 255);

        return;
    }

    vlsm_rebuild_menu(app);

    vlsm_switch_view(app, VlsmViewMain);
}

/*
 * Display calculated subnet results.
 */
static void vlsm_show_results(VlsmApp* app) {
    vlsm_calculate(app);

    widget_reset(app->widget);

    widget_add_text_scroll_element(app->widget, 0, 0, 128, 64, furi_string_get_cstr(app->results));

    vlsm_switch_view(app, VlsmViewResults);
}

/*
 * Called after selecting an item from the Requests list.
 *
 * Pressing OK removes the selected request.
 */
static void vlsm_request_callback(void* context, uint32_t index) {
    VlsmApp* app = context;

    if(index == VLSM_EMPTY_REQUEST_INDEX || index >= app->host_count) {
        return;
    }

    /*
     * Move all following elements one position to the left.
     */
    for(size_t i = index; i + 1U < app->host_count; i++) {
        app->host_requests[i] = app->host_requests[i + 1U];
    }

    app->host_count--;

    /*
     * Update both the main menu and the requests list.
     */
    vlsm_rebuild_menu(app);
    vlsm_rebuild_requests_menu(app);

    /*
     * Keep the selection close to the deleted item.
     */
    if(app->host_count > 0U) {
        const uint32_t next_selection = index < app->host_count ? index :
                                                                  (uint32_t)(app->host_count - 1U);

        submenu_set_selected_item(app->requests_submenu, next_selection);
    }
}

/*
 * Main menu callback.
 */
static void vlsm_menu_callback(void* context, uint32_t index) {
    VlsmApp* app = context;

    switch(index) {
    case VlsmMenuIp:
        app->number_mode = VlsmNumberIpOctet;

        app->ip_octet_index = 0U;

        number_input_set_header_text(app->number_input, "IP octet 1/4");

        number_input_set_result_callback(
            app->number_input, vlsm_number_saved, app, app->ip[0], 0, 255);

        vlsm_switch_view(app, VlsmViewNumberInput);

        break;

    case VlsmMenuPrefix:
        app->number_mode = VlsmNumberPrefix;

        number_input_set_header_text(app->number_input, "Parent prefix /8..30");

        number_input_set_result_callback(
            app->number_input, vlsm_number_saved, app, app->prefix, 8, 30);

        vlsm_switch_view(app, VlsmViewNumberInput);

        break;

    case VlsmMenuAddHosts:
        if(app->host_count >= VLSM_MAX_REQUESTS) {
            furi_string_set(
                app->results,
                "Limit reached:\n"
                "16 subnet requests.");

            widget_reset(app->widget);

            widget_add_text_scroll_element(
                app->widget, 0, 0, 128, 64, furi_string_get_cstr(app->results));

            vlsm_switch_view(app, VlsmViewResults);

            break;
        }

        app->number_mode = VlsmNumberHosts;

        number_input_set_header_text(app->number_input, "Required usable hosts");

        number_input_set_result_callback(
            app->number_input, vlsm_number_saved, app, 10, 1, 16777214);

        vlsm_switch_view(app, VlsmViewNumberInput);

        break;

    case VlsmMenuRequests:
        vlsm_rebuild_requests_menu(app);

        vlsm_switch_view(app, VlsmViewRequests);

        break;

    case VlsmMenuCalculate:
        vlsm_show_results(app);
        break;

    case VlsmMenuReset:
        app->ip[0] = 192;
        app->ip[1] = 168;
        app->ip[2] = 1;
        app->ip[3] = 0;

        app->prefix = 24;
        app->host_count = 0U;

        vlsm_rebuild_menu(app);
        vlsm_rebuild_requests_menu(app);

        break;

    default:
        break;
    }
}

/*
 * Build the menu containing all host requests.
 *
 * Selecting an entry with OK removes it.
 */
static void vlsm_rebuild_requests_menu(VlsmApp* app) {
    submenu_reset(app->requests_submenu);

    submenu_set_header(app->requests_submenu, "Requests: OK=delete");

    if(app->host_count == 0U) {
        submenu_add_item(
            app->requests_submenu,
            "No requests",
            VLSM_EMPTY_REQUEST_INDEX,
            vlsm_request_callback,
            app);

        return;
    }

    for(size_t i = 0U; i < app->host_count; i++) {
        snprintf(
            app->request_item_labels[i],
            sizeof(app->request_item_labels[i]),
            "%u: %lu hosts",
            (unsigned int)(i + 1U),
            (unsigned long)app->host_requests[i]);

        submenu_add_item(
            app->requests_submenu,
            app->request_item_labels[i],
            (uint32_t)i,
            vlsm_request_callback,
            app);
    }
}

/*
 * Build the main application menu.
 */
static void vlsm_rebuild_menu(VlsmApp* app) {
    submenu_reset(app->submenu);

    submenu_set_header(app->submenu, "Net Calculator");

    snprintf(
        app->ip_label,
        sizeof(app->ip_label),
        "IP: %u.%u.%u.%u",
        app->ip[0],
        app->ip[1],
        app->ip[2],
        app->ip[3]);

    snprintf(app->prefix_label, sizeof(app->prefix_label), "Prefix: /%u", app->prefix);

    snprintf(
        app->requests_label,
        sizeof(app->requests_label),
        "Requests: %u",
        (unsigned int)app->host_count);

    submenu_add_item(app->submenu, app->ip_label, VlsmMenuIp, vlsm_menu_callback, app);

    submenu_add_item(app->submenu, app->prefix_label, VlsmMenuPrefix, vlsm_menu_callback, app);

    submenu_add_item(app->submenu, "Add host request", VlsmMenuAddHosts, vlsm_menu_callback, app);

    submenu_add_item(app->submenu, app->requests_label, VlsmMenuRequests, vlsm_menu_callback, app);

    submenu_add_item(app->submenu, "Calculate", VlsmMenuCalculate, vlsm_menu_callback, app);

    submenu_add_item(app->submenu, "Reset", VlsmMenuReset, vlsm_menu_callback, app);
}

/*
 * Back button callback.
 *
 * Back exits the application from the main menu.
 * From any other view, Back returns to the main menu.
 */
static bool vlsm_navigation_callback(void* context) {
    VlsmApp* app = context;

    if(app->current_view == VlsmViewMain) {
        view_dispatcher_stop(app->view_dispatcher);
    } else {
        vlsm_rebuild_menu(app);

        vlsm_switch_view(app, VlsmViewMain);
    }

    return true;
}

/*
 * Allocate application structures and register all views.
 */
static VlsmApp* vlsm_app_alloc(void) {
    VlsmApp* app = malloc(sizeof(VlsmApp));

    furi_check(app);

    memset(app, 0, sizeof(VlsmApp));

    /*
     * Default values.
     */
    app->ip[0] = 192;
    app->ip[1] = 168;
    app->ip[2] = 1;
    app->ip[3] = 0;

    app->prefix = 24;

    app->results = furi_string_alloc();

    app->view_dispatcher = view_dispatcher_alloc();

    app->submenu = submenu_alloc();

    app->requests_submenu = submenu_alloc();

    app->number_input = number_input_alloc();

    app->widget = widget_alloc();

    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);

    view_dispatcher_set_navigation_event_callback(app->view_dispatcher, vlsm_navigation_callback);

    view_dispatcher_add_view(app->view_dispatcher, VlsmViewMain, submenu_get_view(app->submenu));

    view_dispatcher_add_view(
        app->view_dispatcher, VlsmViewRequests, submenu_get_view(app->requests_submenu));

    view_dispatcher_add_view(
        app->view_dispatcher, VlsmViewNumberInput, number_input_get_view(app->number_input));

    view_dispatcher_add_view(app->view_dispatcher, VlsmViewResults, widget_get_view(app->widget));

    app->gui = furi_record_open(RECORD_GUI);

    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    vlsm_rebuild_menu(app);
    vlsm_rebuild_requests_menu(app);

    return app;
}

/*
 * Free application resources.
 */
static void vlsm_app_free(VlsmApp* app) {
    view_dispatcher_remove_view(app->view_dispatcher, VlsmViewMain);

    view_dispatcher_remove_view(app->view_dispatcher, VlsmViewRequests);

    view_dispatcher_remove_view(app->view_dispatcher, VlsmViewNumberInput);

    view_dispatcher_remove_view(app->view_dispatcher, VlsmViewResults);

    widget_free(app->widget);

    number_input_free(app->number_input);

    submenu_free(app->requests_submenu);

    submenu_free(app->submenu);

    view_dispatcher_free(app->view_dispatcher);

    furi_record_close(RECORD_GUI);

    furi_string_free(app->results);

    free(app);
}

/*
 * Application entry point defined in application.fam.
 */
int32_t net_calculator_app(void* p) {
    UNUSED(p);

    VlsmApp* app = vlsm_app_alloc();

    vlsm_switch_view(app, VlsmViewMain);

    view_dispatcher_run(app->view_dispatcher);

    vlsm_app_free(app);

    return 0;
}
