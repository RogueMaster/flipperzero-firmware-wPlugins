#include "pcap_dump.h"

#include <furi.h>
#include <furi_hal_rtc.h>
#include <furi_hal_cortex.h>
#include <storage/storage.h>
#include <string.h>
#include <stdio.h>

#define TAG "PCAP_DUMP"

/* PCAP file format constants */
#define PCAP_MAGIC        0xA1B2C3D4
#define PCAP_VERSION_MAJ  2
#define PCAP_VERSION_MIN  4
#define PCAP_SNAPLEN      1518
#define PCAP_LINKTYPE_ETH 1

/* Directory for pcap files */
#define PCAP_DIR APP_DATA_PATH("pcap")

/* PCAP global header (24 bytes) */
typedef struct __attribute__((packed)) {
    uint32_t magic_number;
    uint16_t version_major;
    uint16_t version_minor;
    int32_t thiszone;
    uint32_t sigfigs;
    uint32_t snaplen;
    uint32_t network;
} PcapGlobalHeader;

/* PCAP packet header (16 bytes) */
typedef struct __attribute__((packed)) {
    uint32_t ts_sec;
    uint32_t ts_usec;
    uint32_t incl_len;
    uint32_t orig_len;
} PcapPacketHeader;

/* File handle kept internally (one capture at a time) */
static Storage* pcap_storage = NULL;
static File* pcap_file = NULL;

/* Base timestamp (seconds since epoch approximation from RTC) */
static uint32_t pcap_base_sec = 0;
static uint32_t pcap_start_tick = 0;

static uint32_t pcap_rtc_to_epoch(void) {
    DateTime dt;
    furi_hal_rtc_get_datetime(&dt);

    /* Simplified epoch calculation (good enough for PCAP timestamps) */
    uint32_t y = dt.year;
    uint32_t m = dt.month;
    uint32_t d = dt.day;

    /* Days from year */
    uint32_t days = 0;
    for(uint32_t i = 1970; i < y; i++) {
        bool leap = (i % 4 == 0 && (i % 100 != 0 || i % 400 == 0));
        days += leap ? 366 : 365;
    }

    /* Days from month */
    static const uint16_t mdays[12] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
    if(m >= 1 && m <= 12) {
        days += mdays[m - 1];
        /* Leap day adjustment */
        if(m > 2) {
            bool leap = (y % 4 == 0 && (y % 100 != 0 || y % 400 == 0));
            if(leap) days++;
        }
    }
    days += (d - 1);

    return days * 86400 + dt.hour * 3600 + dt.minute * 60 + dt.second;
}

bool pcap_dump_start(PcapDumpState* state) {
    memset(state, 0, sizeof(PcapDumpState));

    pcap_storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(pcap_storage, PCAP_DIR);

    /* Generate timestamped filename */
    DateTime dt;
    furi_hal_rtc_get_datetime(&dt);
    /* Static to avoid 128B stack usage; worker is single-threaded */
    static char filepath[128];
    snprintf(
        filepath,
        sizeof(filepath),
        PCAP_DIR "/%04d%02d%02d_%02d%02d%02d.pcap",
        (int)dt.year,
        (int)dt.month,
        (int)dt.day,
        (int)dt.hour,
        (int)dt.minute,
        (int)dt.second);

    pcap_file = storage_file_alloc(pcap_storage);
    if(!storage_file_open(pcap_file, filepath, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        FURI_LOG_E(TAG, "Failed to create: %s", filepath);
        storage_file_free(pcap_file);
        pcap_file = NULL;
        furi_record_close(RECORD_STORAGE);
        pcap_storage = NULL;
        return false;
    }

    /* Write PCAP global header */
    PcapGlobalHeader ghdr = {
        .magic_number = PCAP_MAGIC,
        .version_major = PCAP_VERSION_MAJ,
        .version_minor = PCAP_VERSION_MIN,
        .thiszone = 0,
        .sigfigs = 0,
        .snaplen = PCAP_SNAPLEN,
        .network = PCAP_LINKTYPE_ETH,
    };

    uint16_t written = storage_file_write(pcap_file, &ghdr, sizeof(ghdr));
    if(written != sizeof(ghdr)) {
        FURI_LOG_E(TAG, "Failed to write PCAP header");
        storage_file_close(pcap_file);
        storage_file_free(pcap_file);
        pcap_file = NULL;
        furi_record_close(RECORD_STORAGE);
        pcap_storage = NULL;
        return false;
    }

    /* Initialize timestamp base */
    pcap_base_sec = pcap_rtc_to_epoch();
    pcap_start_tick = furi_get_tick();

    state->active = true;
    FURI_LOG_I(TAG, "PCAP capture started: %s", filepath);
    return true;
}

void pcap_dump_frame(PcapDumpState* state, const uint8_t* frame, uint16_t len) {
    if(!state->active || !pcap_file || len == 0) return;

    /* Calculate timestamp from tick delta */
    uint32_t elapsed_ms = furi_get_tick() - pcap_start_tick;
    uint32_t ts_sec = pcap_base_sec + (elapsed_ms / 1000);
    uint32_t ts_usec = (elapsed_ms % 1000) * 1000;

    uint16_t capture_len = len;
    if(capture_len > PCAP_SNAPLEN) capture_len = PCAP_SNAPLEN;

    PcapPacketHeader phdr = {
        .ts_sec = ts_sec,
        .ts_usec = ts_usec,
        .incl_len = capture_len,
        .orig_len = len,
    };

    /* Write packet header + frame data */
    uint16_t w1 = storage_file_write(pcap_file, &phdr, sizeof(phdr));
    uint16_t w2 = storage_file_write(pcap_file, frame, capture_len);

    if(w1 == sizeof(phdr) && w2 == capture_len) {
        state->frames_written++;
        state->bytes_written += sizeof(phdr) + capture_len;
    } else {
        state->frames_dropped++;
        FURI_LOG_W(TAG, "Frame write failed (dropped: %lu)", (unsigned long)state->frames_dropped);
    }
}

void pcap_dump_stop(PcapDumpState* state) {
    if(pcap_file) {
        storage_file_close(pcap_file);
        storage_file_free(pcap_file);
        pcap_file = NULL;
    }
    if(pcap_storage) {
        furi_record_close(RECORD_STORAGE);
        pcap_storage = NULL;
    }

    if(state->active) {
        FURI_LOG_I(
            TAG,
            "PCAP stopped: %lu frames, %lu bytes, %lu dropped",
            (unsigned long)state->frames_written,
            (unsigned long)state->bytes_written,
            (unsigned long)state->frames_dropped);
    }
    state->active = false;
}
