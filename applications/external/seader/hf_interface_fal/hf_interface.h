#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "../protocol/picopass_poller.h"
#include "../seader_credential_type.h"
#include <lib/nfc/nfc.h>
#include <nfc/nfc_device.h>

#define HF_PLUGIN_APP_ID      "plugin_hf"
#define HF_PLUGIN_API_VERSION 1

typedef enum {
    PluginHfStageCardDetect = 0,
    PluginHfStageConversation,
    PluginHfStageComplete,
    PluginHfStageSuccess,
    PluginHfStageFail,
} PluginHfStage;

typedef enum {
    PluginHfActionTypeIso14443Tx,
    PluginHfActionTypeMfClassicTx,
    PluginHfActionTypePicopassTx,
} PluginHfActionType;

typedef struct {
    PluginHfActionType type;
    uint8_t* data;
    size_t len;
    uint16_t timeout;
    uint8_t format[3];
} PluginHfAction;

typedef struct {
    /* Required runtime callbacks. A successful plugin alloc assumes these remain valid until free. */
    void (*notify_card_detected)(void* host_ctx);
    void (*notify_worker_exit)(void* host_ctx);
    bool (*sam_can_accept_card)(void* host_ctx);
    void (*send_card_detected)(
        void* host_ctx,
        uint8_t sak,
        const uint8_t* uid,
        uint8_t uid_len,
        const uint8_t* ats,
        uint8_t ats_len);
    void (*send_nfc_rx)(void* host_ctx, uint8_t* buffer, size_t len);
    void (*run_conversation)(void* host_ctx);
    void (*set_stage)(void* host_ctx, PluginHfStage stage);
    PluginHfStage (*get_stage)(void* host_ctx);
    void (*set_credential_type)(void* host_ctx, SeaderCredentialType type);
    SeaderCredentialType (*get_credential_type)(void* host_ctx);
    bool (*get_desfire_ev2)(void* host_ctx);
    void (*set_desfire_ev2)(void* host_ctx, bool is_desfire_ev2);
    void (*append_picopass_sio)(void* host_ctx, uint8_t block_num, const uint8_t* data, size_t len);
    void (*set_14a_sio)(void* host_ctx, const uint8_t* data, size_t len);
    Nfc* (*get_nfc)(void* host_ctx);
    NfcDevice* (*get_nfc_device)(void* host_ctx);

    /* Required Picopass hooks. All Flippers expose Picopass through the HF host API. */
    bool (*picopass_detect)(void* host_ctx);
    bool (*picopass_start)(void* host_ctx, PicopassPollerCallback callback, void* callback_ctx);
    void (*picopass_stop)(void* host_ctx);
    uint8_t* (*picopass_get_csn)(void* host_ctx);
    bool (*picopass_transmit)(
        void* host_ctx,
        const uint8_t* tx_data,
        size_t tx_len,
        uint8_t* rx_data,
        size_t rx_capacity,
        size_t* rx_len,
        uint32_t fwt_fc);

    /* Optional UX hook for richer read failure text. */
    void (*set_read_error)(void* host_ctx, const char* text);
} PluginHfHostApi;

typedef struct {
    const char* name;
    void* (*alloc)(const PluginHfHostApi* api, void* host_ctx);
    void (*free)(void* plugin_ctx);
    size_t (*detect_supported_types)(
        void* plugin_ctx,
        SeaderCredentialType* detected_types,
        size_t detected_capacity);
    bool (*start_read_for_type)(void* plugin_ctx, SeaderCredentialType type);
    void (*stop)(void* plugin_ctx);
    bool (*handle_action)(void* plugin_ctx, const PluginHfAction* action);
} PluginHf;
