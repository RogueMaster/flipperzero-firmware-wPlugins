#pragma once
#include <furi.h>
#include <stdint.h>
#include <stddef.h>
#include <gui/view_dispatcher.h>
#include "url_links.h"

#define GTA_NFC_ID          "gta_nfc_link"
#define GTA_NFC_PATH        APP_ASSETS_PATH("plugins/gta_nfc_link.fal")
#define GTA_NFC_API_VERSION 2u
#define GTA_NFC_VIEW_ID     0x1000u
#define GTA_NFC_BACK_EVENT  0x47544E01u
#define GTA_NFC_EXIT_EVENT  0x47544E02u
#define GTA_NFC_LINKS_EVENT 0x47544E03u
#define GTA_URL_MAX         258u

typedef struct {
    uint32_t size;
    /* All calls occur on the host dispatcher thread. open copies the URL. */
    void* (*open)(
        ViewDispatcher* dispatcher,
        const char* url,
        uint16_t id,
        GtaUrlKind kind,
        bool has_alternates);
    void (*stop)(void* session);
    /* Host must switch away from GTA_NFC_VIEW_ID before close, except on shutdown. */
    void (*close)(void* session);
} GtaNfcPluginApi;
