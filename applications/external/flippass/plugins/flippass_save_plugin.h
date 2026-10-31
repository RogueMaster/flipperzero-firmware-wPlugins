#pragma once

#include <furi.h>
#include <flipper_application/flipper_application.h>

#include "../flippass_crypto.h"
#include "../kdbx/kdbx_data.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FLIPPASS_SAVE_HEADER_PLUGIN_APP_ID      "flippass_save_header"
#define FLIPPASS_SAVE_HEADER_PLUGIN_API_VERSION 2u
#define FLIPPASS_SAVE_PLUGIN_APP_ID             "flippass_save_writer"
#define FLIPPASS_SAVE_PLUGIN_API_VERSION        9u
#define FLIPPASS_SAVE_STAGE_HOST_API_VERSION    1u
#define FLIPPASS_SAVE_HOST_API_VERSION          9u

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
    const uint8_t* transformed_key;
    size_t transformed_key_size;
    const uint8_t* kdf_salt;
    size_t kdf_salt_size;
    FlipPassSaveCipher cipher;
    uint32_t compression;
    uint64_t kdf_rounds;
} FlipPassSaveHeaderRequestV1;

typedef struct {
    uint8_t cipher_key[32];
    uint8_t hmac_base[64];
    uint8_t transformed_key[32];
    uint8_t kdf_salt[32];
    uint8_t iv[16];
    size_t iv_size;
    bool transformed_key_ready;
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

/* Request for the payload writer — XML traversal moved to the serial plugin.
 * root_group and database_name are no longer needed here. */
typedef struct {
    uint32_t api_version;
    const char* file_path;
    const uint8_t* cipher_key;
    size_t cipher_key_size;
    const uint8_t* hmac_base;
    size_t hmac_base_size;
    const uint8_t* iv;
    size_t iv_size;
    FlipPassSaveCipher cipher;
    uint32_t compression;
} FlipPassSaveRequestV1;

typedef struct {
    uint32_t api_version;
    void* context;
    void (*progress)(void* context, const char* stage, const char* detail, uint8_t percent);
    void (*log)(void* context, const char* message);
    const FlipPassCryptoApiV1* crypto;
    /* Stream the serialized XML from the host's encrypted vault scratch.
     * The host calls callback(data, size, cb_context) for each decrypted chunk. */
    bool (*stream_xml)(
        void* context,
        FlipPassSaveChunkCallback callback,
        void* callback_context,
        FuriString* error);
    bool (*compute_block_hmac)(
        void* context,
        const uint8_t* hmac_base,
        size_t hmac_base_size,
        uint64_t block_index,
        const uint8_t* data,
        size_t data_size,
        uint8_t out[32]);
    /* HOST_PAYLOAD experimental path — default-off. */
    bool (*begin_payload)(void* context, const FlipPassSaveRequestV1* request, FuriString* error);
    bool (*write_payload_plain)(
        void* context,
        const uint8_t* data,
        size_t data_size,
        FuriString* error);
    bool (*finish_payload)(void* context, FuriString* error);
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
