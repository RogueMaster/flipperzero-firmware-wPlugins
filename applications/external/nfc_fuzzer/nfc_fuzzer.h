#pragma once

#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/submenu.h>
#include <gui/modules/text_box.h>
#include <gui/modules/variable_item_list.h>
#include <gui/view.h>
#include <nfc/nfc.h>
#include <nfc/nfc_device.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>
#include <storage/storage.h>
#include <toolbox/bit_buffer.h>
#include <bit_lib/bit_lib.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define NFC_FUZZER_LOG_TAG                 "NfcFuzzer"
#define NFC_FUZZER_LOG_DIR                 "/ext/nfc_fuzzer"
#define NFC_FUZZER_CUSTOM_DIR              "/ext/nfc_fuzzer/custom"
#define NFC_FUZZER_MAX_PAYLOAD_LEN         255
#define NFC_FUZZER_MAX_RESULTS             64 /* 256×520 B = 133 KB OOM; cap at 64×520 B = 33 KB */
#define NFC_FUZZER_INITIAL_RESULT_CAPACITY 8
#define NFC_FUZZER_HEX_STR_LEN             (NFC_FUZZER_MAX_PAYLOAD_LEN * 3 + 1)
#define NFC_FUZZER_RESULT_DETAIL_LEN       2048

/* ───────────────────── Enums ───────────────────── */

typedef enum {
    NfcFuzzerProfileUid,
    NfcFuzzerProfileAtqaSak,
    NfcFuzzerProfileFrame,
    NfcFuzzerProfileNtag,
    NfcFuzzerProfileIso15693,
    NfcFuzzerProfileReaderCommands,
    NfcFuzzerProfileMifareAuth,
    NfcFuzzerProfileMifareRead,
    NfcFuzzerProfileRats,
    NfcFuzzerProfileNfcB, /* ISO 14443-B PUPI fuzzer (listener mode) */
    NfcFuzzerProfileFelica, /* FeliCa IDm fuzzer (listener mode) */
    NfcFuzzerProfileCOUNT,
} NfcFuzzerProfile;

typedef enum {
    NfcFuzzerStrategySequential,
    NfcFuzzerStrategyRandom,
    NfcFuzzerStrategyBitflip,
    NfcFuzzerStrategyBoundary,
    NfcFuzzerStrategyCOUNT,
} NfcFuzzerStrategy;

typedef enum {
    NfcFuzzerAnomalyNone,
    NfcFuzzerAnomalyTimeout,
    NfcFuzzerAnomalyUnexpectedResponse,
    NfcFuzzerAnomalyTimingAnomaly,
} NfcFuzzerAnomalyType;

/* ───────────────────── View IDs ───────────────────── */

typedef enum {
    NfcFuzzerViewProfileSelect,
    NfcFuzzerViewStrategySelect,
    NfcFuzzerViewFuzzRun,
    NfcFuzzerViewResultsList,
    NfcFuzzerViewResultDetail,
    NfcFuzzerViewSettings,
} NfcFuzzerViewId;

/* ───────────────────── Result ───────────────────── */

typedef struct {
    uint32_t test_num;
    uint8_t payload[NFC_FUZZER_MAX_PAYLOAD_LEN];
    uint8_t payload_len;
    NfcFuzzerAnomalyType anomaly;
    uint8_t response[NFC_FUZZER_MAX_PAYLOAD_LEN];
    uint8_t response_len;
} NfcFuzzerResult;

/* ───────────────────── Settings ───────────────────── */

typedef enum {
    NfcFuzzerTimeout50ms = 0,
    NfcFuzzerTimeout100ms,
    NfcFuzzerTimeout250ms,
    NfcFuzzerTimeout500ms,
    NfcFuzzerTimeoutCOUNT,
} NfcFuzzerTimeoutIndex;

typedef enum {
    NfcFuzzerDelay0ms = 0,
    NfcFuzzerDelay10ms,
    NfcFuzzerDelay50ms,
    NfcFuzzerDelay100ms,
    NfcFuzzerDelayCOUNT,
} NfcFuzzerDelayIndex;

typedef enum {
    NfcFuzzerMaxCases100 = 0,
    NfcFuzzerMaxCases1000,
    NfcFuzzerMaxCases10000,
    NfcFuzzerMaxCasesUnlimited,
    NfcFuzzerMaxCasesCOUNT,
} NfcFuzzerMaxCasesIndex;

typedef struct {
    NfcFuzzerTimeoutIndex timeout_index;
    NfcFuzzerDelayIndex delay_index;
    bool auto_stop;
    NfcFuzzerMaxCasesIndex max_cases_index;
} NfcFuzzerSettings;

/* ───────────────────── Forward declarations ───────────────────── */

typedef struct NfcFuzzerWorker NfcFuzzerWorker;

/* ───────────────────── App state ───────────────────── */

typedef struct {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    NotificationApp* notifications;
    Storage* storage;

    /* Views / modules */
    Submenu* submenu_profile;
    Submenu* submenu_strategy;
    View* view_fuzz_run;
    Submenu* submenu_results;
    TextBox* text_box_detail;
    VariableItemList* variable_item_list_settings;

    /* State */
    NfcFuzzerProfile selected_profile;
    NfcFuzzerStrategy selected_strategy;
    NfcFuzzerSettings settings;

    /* Worker */
    NfcFuzzerWorker* worker;

    /* Results (dynamically allocated) */
    NfcFuzzerResult* results;
    uint32_t result_count;
    uint32_t result_capacity;

    /* Fuzz run view model data (shared with view via model) */
    uint32_t current_test;
    uint32_t total_tests;
    uint32_t anomaly_count;
    uint8_t current_payload[NFC_FUZZER_MAX_PAYLOAD_LEN];
    uint8_t current_payload_len;
    volatile bool worker_running;

    /* Mutex for shared state between main and worker threads */
    FuriMutex* mutex;

    /* Heap-allocated label strings for submenu items (to avoid dangling stack pointers) */
    char** result_labels;
    uint32_t result_labels_count;

    /* Detail text buffer */
    char detail_text[NFC_FUZZER_RESULT_DETAIL_LEN];

    /* Log file path */
    FuriString* log_path;
} NfcFuzzerApp;

/* ───────────────────── Helpers ───────────────────── */

static inline uint32_t nfc_fuzzer_timeout_ms(NfcFuzzerTimeoutIndex idx) {
    static const uint32_t vals[] = {50, 100, 250, 500};
    return (idx < NfcFuzzerTimeoutCOUNT) ? vals[idx] : 100;
}

static inline uint32_t nfc_fuzzer_delay_ms(NfcFuzzerDelayIndex idx) {
    static const uint32_t vals[] = {0, 10, 50, 100};
    return (idx < NfcFuzzerDelayCOUNT) ? vals[idx] : 0;
}

static inline uint32_t nfc_fuzzer_max_cases(NfcFuzzerMaxCasesIndex idx) {
    static const uint32_t vals[] = {100, 1000, 10000, 0xFFFFFFFF};
    return (idx < NfcFuzzerMaxCasesCOUNT) ? vals[idx] : 1000;
}

static inline const char* nfc_fuzzer_profile_name(NfcFuzzerProfile p) {
    static const char* names[] = {
        "UID Fuzzing",
        "ATQA/SAK Fuzzing",
        "Frame Fuzzing",
        "NTAG Fuzzing",
        "ISO15693 Fuzzing",
        "Reader Commands",
        "MIFARE Auth",
        "MIFARE Read/Write",
        "RATS/ATS Fuzzing",
        "NFC-B PUPI",
        "FeliCa IDm",
    };
    return (p < NfcFuzzerProfileCOUNT) ? names[p] : "Unknown";
}

static inline const char* nfc_fuzzer_strategy_name(NfcFuzzerStrategy s) {
    static const char* names[] = {
        "Sequential",
        "Random",
        "Bitflip",
        "Boundary",
    };
    return (s < NfcFuzzerStrategyCOUNT) ? names[s] : "Unknown";
}

static inline const char* nfc_fuzzer_anomaly_name(NfcFuzzerAnomalyType a) {
    switch(a) {
    case NfcFuzzerAnomalyTimeout:
        return "Timeout";
    case NfcFuzzerAnomalyUnexpectedResponse:
        return "Unexpected";
    case NfcFuzzerAnomalyTimingAnomaly:
        return "Timing";
    default:
        return "None";
    }
}

/** Convert byte array to hex string. dst must be at least len*3+1 bytes. */
static inline void nfc_fuzzer_bytes_to_hex(const uint8_t* src, size_t len, char* dst) {
    for(size_t i = 0; i < len; i++) {
        snprintf(dst + i * 3, 4, "%02X ", src[i]);
    }
    if(len > 0) {
        dst[len * 3 - 1] = '\0'; /* trim trailing space */
    } else {
        dst[0] = '\0';
    }
}
