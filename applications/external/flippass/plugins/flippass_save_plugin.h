#pragma once

#include <furi.h>
#include <flipper_application/flipper_application.h>

#include "../kdbx/kdbx_data.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FLIPPASS_SAVE_HEADER_PLUGIN_APP_ID      "flippass_save_header"
#define FLIPPASS_SAVE_HEADER_PLUGIN_API_VERSION 1u
#define FLIPPASS_SAVE_PLUGIN_APP_ID             "flippass_save_writer"
#define FLIPPASS_SAVE_PLUGIN_API_VERSION        3u
#define FLIPPASS_SAVE_STAGE_HOST_API_VERSION    1u
#define FLIPPASS_SAVE_HOST_API_VERSION          3u

typedef enum {
    FlipPassSaveCipherAes256 = 0,
    FlipPassSaveCipherChaCha20,
} FlipPassSaveCipher;

typedef bool (*FlipPassSaveChunkCallback)(const uint8_t* data, size_t data_size, void* context);

typedef struct {
    uint32_t api_version;
    const char* file_path;
    const uint8_t* composite_key;
    size_t composite_key_size;
    FlipPassSaveCipher cipher;
    uint32_t compression;
    uint64_t kdf_rounds;
} FlipPassSaveHeaderRequestV1;

typedef struct {
    uint8_t cipher_key[32];
    uint8_t hmac_base[64];
    uint8_t iv[16];
    size_t iv_size;
} FlipPassSaveHeaderResultV1;

typedef struct {
    uint32_t api_version;
    void* context;
    void (*progress)(void* context, const char* stage, const char* detail, uint8_t percent);
    void (*log)(void* context, const char* message);
} FlipPassSaveStageHostApiV1;

typedef struct {
    uint32_t api_version;
    bool (*run)(
        const FlipPassSaveHeaderRequestV1* request,
        const FlipPassSaveStageHostApiV1* host_api,
        FlipPassSaveHeaderResultV1* result,
        FuriString* error);
} FlipPassSaveHeaderPluginV1;

typedef struct {
    uint32_t api_version;
    const char* file_path;
    const uint8_t* cipher_key;
    size_t cipher_key_size;
    const uint8_t* hmac_base;
    size_t hmac_base_size;
    const uint8_t* iv;
    size_t iv_size;
    KDBXGroup* root_group;
    const char* database_name;
    FlipPassSaveCipher cipher;
    uint32_t compression;
} FlipPassSaveRequestV1;

typedef struct {
    uint32_t api_version;
    void* context;
    void (*progress)(void* context, const char* stage, const char* detail, uint8_t percent);
    void (*log)(void* context, const char* message);
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
    bool (*activate_entry)(void* context, KDBXEntry* entry, bool load_notes, FuriString* error);
    void (*deactivate_entry)(void* context);
    bool (*ensure_custom_field)(
        void* context,
        KDBXEntry* entry,
        KDBXCustomField* field,
        FuriString* error);
    bool (*entry_has_field)(void* context, const KDBXEntry* entry, uint32_t field_mask);
} FlipPassSaveHostApiV1;

typedef struct {
    uint32_t api_version;
    bool (*run)(
        const FlipPassSaveRequestV1* request,
        const FlipPassSaveHostApiV1* host_api,
        FuriString* error);
} FlipPassSavePluginV1;

const FlipperAppPluginDescriptor* flippass_save_header_plugin_ep(void);
const FlipperAppPluginDescriptor* flippass_save_plugin_ep(void);

#ifdef __cplusplus
}
#endif
