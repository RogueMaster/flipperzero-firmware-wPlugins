#pragma once

#include <furi.h>
#include <flipper_application/flipper_application.h>

#include "../flippass_crypto.h"
#include "../kdbx/kdbx_data.h"
#include "../kdbx/kdbx_vault.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FLIPPASS_SAVE_SERIAL_PLUGIN_APP_ID      "flippass_save_serial"
#define FLIPPASS_SAVE_SERIAL_PLUGIN_API_VERSION 2u
#define FLIPPASS_SAVE_SERIAL_HOST_API_VERSION   2u

typedef bool (
    *FlipPassSaveSerialChunkCallback)(const uint8_t* data, size_t data_size, void* context);

typedef struct {
    uint32_t api_version;
    KDBXGroup* root_group;
    const char* database_name;
} FlipPassSaveSerialRequestV1;

typedef struct {
    uint32_t api_version;
    void* context;
    void (*progress)(void* context, const char* stage, const char* detail, uint8_t percent);
    void (*log)(void* context, const char* message);
    const FlipPassCryptoApiV1* crypto;
    /* Write a plaintext XML chunk to the host's encrypted vault scratch. */
    bool (
        *write_xml_chunk)(void* context, const uint8_t* data, size_t data_size, FuriString* error);
    /* Stream a vault field value (session vault read). */
    bool (*stream_ref)(
        void* context,
        const KDBXFieldRef* ref,
        KDBXVaultChunkCallback callback,
        void* callback_context,
        FuriString* error);
    bool (*copy_group_uuid)(
        void* context,
        const KDBXGroup* group,
        FuriString* out,
        FuriString* error);
    bool (*copy_entry_uuid)(
        void* context,
        const KDBXEntry* entry,
        FuriString* out,
        FuriString* error);
    bool (*entry_has_field)(void* context, const KDBXEntry* entry, uint32_t field_mask);
    /* SHA-512 helper — host-owned to avoid linking sha2 twice. */
    bool (*sha512_raw)(void* context, const uint8_t* data, size_t data_size, uint8_t out[64]);
} FlipPassSaveSerialHostApiV1;

typedef struct {
    uint32_t api_version;
    bool (*run)(
        const FlipPassSaveSerialRequestV1* request,
        const FlipPassSaveSerialHostApiV1* host_api,
        FuriString* error);
} FlipPassSaveSerialPluginV1;

const FlipperAppPluginDescriptor* flippass_save_serial_plugin_ep(void);

#ifdef __cplusplus
}
#endif
