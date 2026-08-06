#include "lldp_module.h"

void lldp_module_init(void) {
    neighbor_db_clear();
}

void lldp_module_reset(void) {
    neighbor_db_clear();
}

bool lldp_module_process_frame(uint8_t* frame, uint16_t length) {
    printf("LLDP FRAME %u\n", length);
    fflush(stdout);
    if(!frame) {
        FURI_LOG_I("LLDP", "NULL frame");
        return false;
    }

    if(frame[0] == 0x01 && frame[1] == 0x80 && frame[2] == 0xC2) {
        FURI_LOG_I(
            "RX",
            "802.1 multicast received: %02X:%02X:%02X:%02X:%02X:%02X",
            frame[0],
            frame[1],
            frame[2],
            frame[3],
            frame[4],
            frame[5]);
    }

    if(!is_lldp(frame)) {
        FURI_LOG_I(
            "LLDP",
            "Ether bytes: %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X",
            frame[0],
            frame[1],
            frame[2],
            frame[3],
            frame[4],
            frame[5],
            frame[6],
            frame[7],
            frame[8],
            frame[9],
            frame[10],
            frame[11],
            frame[12],
            frame[13]);

        return false;
    }

    lldp_info_t info;

    if(!lldp_parse(frame, length, &info)) {
        FURI_LOG_I("LLDP", "Parse failed");
        return false;
    }

    neighbor_t neighbor;

    if(!lldp_fill_neighbor(&info, &neighbor)) {
        return false;
    }

    neighbor_t* existing = neighbor_db_find(neighbor.mac);

    bool ok = false;

    if(existing) {
        FURI_LOG_I("LLDP", "Updating neighbor");
        ok = neighbor_db_update(&neighbor);
    } else {
        FURI_LOG_I("LLDP", "Adding neighbor");
        ok = neighbor_db_add(&neighbor);
    }

    return ok;
}

bool lldp_packet_predicate(const uint8_t* frame, uint16_t len, void* ctx) {
    UNUSED(ctx);

    return lldp_module_process_frame((uint8_t*)frame, len);
}

bool lldp_module_run(scanner_session_t* session, uint32_t timeout_ms) {
    printf("LLDP RUN\n");
    fflush(stdout);

    uint16_t length = 0;

    return scanner_wait_for_packet(
        session, lldp_packet_predicate, NULL, NULL, NULL, &length, timeout_ms);
}

size_t lldp_module_count(void) {
    return neighbor_db_count();
}

neighbor_t* lldp_module_get(size_t index) {
    return neighbor_db_get(index);
}

static const char* lldp_get_display_name(void) {
    return "LLDP";
}

static void lldp_init(App* app) {
    enable_multicast(app->ethernet);
    lldp_module_init();
}

static bool lldp_run(scanner_session_t* session, uint32_t timeout_ms) {
    return lldp_module_run(session, timeout_ms);
}

static void lldp_cleanup(App* app) {
    disable_multicast(app->ethernet);
}

static uint8_t lldp_get_details_page_count(neighbor_t* neighbor) {
    UNUSED(neighbor);
    return 3;
}

static void lldp_build_details_page(
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
    if(!neighbor) return;

    switch(page) {
    case 0:

        snprintf(line1, line1_size, "NAME");

        snprintf(line2, line2_size, "%s", neighbor->name[0] ? neighbor->name : "Unnamed");

        snprintf(line3, line3_size, "SOURCE");

        snprintf(line4, line4_size, "LLDP");

        break;

    case 1:

        snprintf(line1, line1_size, "PORT");

        snprintf(line2, line2_size, "%s", neighbor->port[0] ? neighbor->port : "N/A");

        snprintf(line3, line3_size, "IP");

        snprintf(
            line4,
            line4_size,
            "%s",
            neighbor->management_address[0] ? neighbor->management_address : "N/A");

        break;

    case 2:

        snprintf(line1, line1_size, "CAPABILITIES");

        snprintf(line2, line2_size, "0x%04X", neighbor->capabilities);

        snprintf(line3, line3_size, "TTL");

        snprintf(line4, line4_size, "%u", neighbor->ttl);

        break;

    default:

        line1[0] = '\0';
        line2[0] = '\0';
        line3[0] = '\0';
        line4[0] = '\0';

        break;
    }
}

const PassiveProtocolHandler lldp_protocol_handler = {
    .get_display_name = lldp_get_display_name,
    .init = lldp_init,
    .run = lldp_run,
    .cleanup = lldp_cleanup,
    .get_details_page_count = lldp_get_details_page_count,
    .build_details_page = lldp_build_details_page,
};
