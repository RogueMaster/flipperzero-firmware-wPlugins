#include "passive_discovery_module.h"
#include "passive_discovery_handler.h"
#include "lldp_module.h"
#include <stdio.h>

// Forward declaration of the thread worker function
static int32_t passive_discovery_thread(void* context);

// --- Dummy Handlers for Future Protocols (CDP / EAPOL) ---

static const char* dummy_cdp_get_display_name(void) {
    return "CDP";
}
static void dummy_cdp_init(App* app) {
    UNUSED(app);
    neighbor_db_clear();
}
static bool dummy_cdp_run(scanner_session_t* session, uint32_t timeout_ms) {
    UNUSED(session);
    furi_delay_ms(timeout_ms);
    return false;
}
static void dummy_cdp_cleanup(App* app) {
    UNUSED(app);
}
static uint8_t dummy_cdp_get_details_page_count(neighbor_t* neighbor) {
    UNUSED(neighbor);
    return 1;
}
static void dummy_cdp_build_details_page(
    neighbor_t* neighbor,
    uint8_t page,
    char* line1,
    size_t line1_size,
    char* line2,
    size_t line2_size,
    char* line3,
    size_t line3_size,
    char* line4,
    size_t line4_size) {
    UNUSED(neighbor);
    UNUSED(page);
    UNUSED(line3_size);
    UNUSED(line4_size);
    snprintf(line1, line1_size, "CDP Neighbor");
    snprintf(line2, line2_size, "Not implemented");
    line3[0] = '\0';
    line4[0] = '\0';
}

static const PassiveProtocolHandler cdp_protocol_handler = {
    .get_display_name = dummy_cdp_get_display_name,
    .init = dummy_cdp_init,
    .run = dummy_cdp_run,
    .cleanup = dummy_cdp_cleanup,
    .get_details_page_count = dummy_cdp_get_details_page_count,
    .build_details_page = dummy_cdp_build_details_page,
};

static const char* dummy_eapol_get_display_name(void) {
    return "EAPOL";
}
static void dummy_eapol_init(App* app) {
    UNUSED(app);
    neighbor_db_clear();
}
static bool dummy_eapol_run(scanner_session_t* session, uint32_t timeout_ms) {
    UNUSED(session);
    furi_delay_ms(timeout_ms);
    return false;
}
static void dummy_eapol_cleanup(App* app) {
    UNUSED(app);
}
static uint8_t dummy_eapol_get_details_page_count(neighbor_t* neighbor) {
    UNUSED(neighbor);
    return 1;
}
static void dummy_eapol_build_details_page(
    neighbor_t* neighbor,
    uint8_t page,
    char* line1,
    size_t line1_size,
    char* line2,
    size_t line2_size,
    char* line3,
    size_t line3_size,
    char* line4,
    size_t line4_size) {
    UNUSED(neighbor);
    UNUSED(page);
    UNUSED(line3_size);
    UNUSED(line4_size);
    snprintf(line1, line1_size, "EAPOL Neighbor");
    snprintf(line2, line2_size, "Not implemented");
    line3[0] = '\0';
    line4[0] = '\0';
}

static const PassiveProtocolHandler eapol_protocol_handler = {
    .get_display_name = dummy_eapol_get_display_name,
    .init = dummy_eapol_init,
    .run = dummy_eapol_run,
    .cleanup = dummy_eapol_cleanup,
    .get_details_page_count = dummy_eapol_get_details_page_count,
    .build_details_page = dummy_eapol_build_details_page,
};

// --- Protocol Registry Lookup Table ---

static const PassiveProtocolHandler* const protocol_handlers[PassiveProtocolCount] = {
    [PassiveProtocolLLDP] = &lldp_protocol_handler,
    [PassiveProtocolEAPOL] = &eapol_protocol_handler,
    [PassiveProtocolCDP] = &cdp_protocol_handler,
};

static const PassiveProtocolHandler* get_handler(passive_protocol_t protocol) {
    if(protocol >= PassiveProtocolCount) {
        return NULL;
    }
    return protocol_handlers[protocol];
}

// --- Background Scanning Thread ---

static int32_t passive_discovery_thread(void* context) {
    printf("PASSIVE THREAD ENTERED\n");
    App* app = context;
    enc28j60_t* ethernet = app->ethernet;

    bool start = app->enc28j60_connected;

    if(!start) {
        start = enc28j60_start(ethernet) != 0xff;
        app->enc28j60_connected = start;
    }

    if(!start) {
        draw_device_no_connected(app);
        furi_delay_ms(300);
        return 0;
    }

    if(!is_link_up(ethernet)) {
        draw_network_not_connected(app);
        furi_delay_ms(300);
        return 0;
    }

    scanner_session_t session;
    scanner_session_init(&session, app);

    const PassiveProtocolHandler* handler = get_handler(app->passive_discovery.protocol);
    if(handler && handler->init) {
        handler->init(app);
    }

    while(!app->passive_discovery_stop) {
        if(handler && handler->run) {
            bool result = handler->run(&session, 500);
            if(result) {
                FURI_LOG_I("PASSIVE", "Packet processed by active handler");
            }
        } else {
            furi_delay_ms(100);
        }

        uint16_t count = neighbor_db_count();
        if(count != app->passive_neighbor_count) {
            app->passive_neighbor_count = count;
            view_dispatcher_send_custom_event(app->view_dispatcher, 1);
        }
    }

    if(handler && handler->cleanup) {
        handler->cleanup(app);
    }

    scanner_session_deinit(&session);

    return 0;
}

// --- Public APIs implementation ---

void passive_discovery_module_start(App* app) {
    if(app->thread_alternative) {
        return;
    }

    app->passive_discovery_stop = false;
    app->thread_alternative =
        furi_thread_alloc_ex("Passive Discovery", 4096, passive_discovery_thread, app);
    furi_thread_start(app->thread_alternative);
}

void passive_discovery_module_stop(App* app) {
    if(!app) {
        return;
    }

    app->passive_discovery_stop = true;

    if(app->thread_alternative != NULL) {
        furi_thread_join(app->thread_alternative);

        furi_thread_free(app->thread_alternative);

        app->thread_alternative = NULL;
    }
}

size_t passive_discovery_module_get_protocol_count(void) {
    return PassiveProtocolCount;
}

const char* passive_discovery_module_get_protocol_name(passive_protocol_t protocol) {
    const PassiveProtocolHandler* handler = get_handler(protocol);
    if(handler && handler->get_display_name) {
        return handler->get_display_name();
    }
    return "Unknown";
}

uint8_t passive_discovery_module_get_details_page_count(
    passive_protocol_t protocol,
    neighbor_t* neighbor) {
    const PassiveProtocolHandler* handler = get_handler(protocol);
    if(handler && handler->get_details_page_count) {
        return handler->get_details_page_count(neighbor);
    }
    return 1;
}

void passive_discovery_module_build_details_page(
    passive_protocol_t protocol,
    neighbor_t* neighbor,
    uint8_t page,
    char* line1,
    size_t line1_size,
    char* line2,
    size_t line2_size,
    char* line3,
    size_t line3_size,
    char* line4,
    size_t line4_size) {
    const PassiveProtocolHandler* handler = get_handler(protocol);
    if(handler && handler->build_details_page) {
        handler->build_details_page(
            neighbor,
            page,
            line1,
            line1_size,
            line2,
            line2_size,
            line3,
            line3_size,
            line4,
            line4_size);
    } else {
        snprintf(line1, line1_size, "No handler");
        line2[0] = '\0';
        line3[0] = '\0';
        line4[0] = '\0';
    }
}

size_t passive_discovery_module_get_neighbor_count(void) {
    return neighbor_db_count();
}

neighbor_t* passive_discovery_module_get_neighbor(size_t index) {
    return neighbor_db_get(index);
}
