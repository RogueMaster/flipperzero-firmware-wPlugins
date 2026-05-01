#pragma once

#include <furi.h>
#include <flipper_application/flipper_application.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FLIPPASS_FILE_PLUGIN_APP_ID      "flippass_file_ops"
#define FLIPPASS_FILE_PLUGIN_API_VERSION 1u
#define FLIPPASS_FILE_HOST_API_VERSION   1u

typedef enum {
    FlipPassFilePluginItemUp = 0,
    FlipPassFilePluginItemDirectory,
    FlipPassFilePluginItemFile,
    FlipPassFilePluginItemNewObject,
    FlipPassFilePluginItemInfo,
} FlipPassFilePluginItemType;

typedef struct {
    uint32_t api_version;
    const char* root_path;
    const char* requested_directory;
    const char* fallback_file_path;
    uint32_t max_items;
    FuriString* resolved_directory;
    bool* has_parent;
} FlipPassFileListRequestV1;

typedef struct {
    uint32_t api_version;
    void* context;
    void (*add_item)(
        void* context,
        FlipPassFilePluginItemType type,
        const char* label,
        const char* name);
} FlipPassFileHostApiV1;

typedef struct {
    uint32_t api_version;
    bool (*list_directory)(
        const FlipPassFileListRequestV1* request,
        const FlipPassFileHostApiV1* host_api,
        FuriString* error);
    bool (*delete_path)(const char* path, FuriString* error);
} FlipPassFilePluginV1;

const FlipperAppPluginDescriptor* flippass_file_plugin_ep(void);

#ifdef __cplusplus
}
#endif
