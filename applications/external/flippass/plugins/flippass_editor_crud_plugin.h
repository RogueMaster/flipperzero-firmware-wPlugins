#pragma once

#include <furi.h>
#include <flipper_application/flipper_application.h>

#include "../flippass.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FLIPPASS_EDITOR_CRUD_PLUGIN_APP_ID      "flippass_editor_crud"
#define FLIPPASS_EDITOR_CRUD_PLUGIN_API_VERSION 2u
#define FLIPPASS_EDITOR_CRUD_HOST_API_VERSION   2u

typedef enum {
    FlipPassEditorCrudDeleteNone = 0,
    FlipPassEditorCrudDeleteGroup,
    FlipPassEditorCrudDeleteEntry,
    FlipPassEditorCrudDeleteField,
} FlipPassEditorCrudDeleteTarget;

typedef struct {
    uint32_t api_version;
    void* context;
    bool (*create_group)(
        void* context,
        KDBXGroup* parent,
        const char* name,
        KDBXGroup** out_group,
        FuriString* error);
    bool (*update_group)(void* context, KDBXGroup* group, const char* name, FuriString* error);
    bool (*delete_group)(void* context, KDBXGroup* group, FuriString* error);
    bool (*create_entry)(
        void* context,
        KDBXGroup* group,
        const char* title,
        const char* username,
        const char* password,
        const char* url,
        const char* notes,
        const char* autotype,
        KDBXEntry** out_entry,
        FuriString* error);
    bool (*update_entry)(
        void* context,
        KDBXEntry* entry,
        const char* title,
        const char* username,
        const char* password,
        const char* url,
        const char* notes,
        const char* autotype,
        FuriString* error);
    bool (*delete_entry)(void* context, KDBXEntry* entry, FuriString* error);
    bool (*create_custom_field)(
        void* context,
        KDBXEntry* entry,
        const char* name,
        const char* value,
        bool protected_value,
        KDBXCustomField** out_field,
        FuriString* error);
    bool (*update_custom_field)(
        void* context,
        KDBXEntry* entry,
        KDBXCustomField* field,
        const char* name,
        const char* value,
        bool protected_value,
        FuriString* error);
    bool (*delete_custom_field)(
        void* context,
        KDBXEntry* entry,
        KDBXCustomField* field,
        FuriString* error);
    void (*save_settings)(void* context);
    void (
        *show_status)(void* context, const char* title, const char* message, uint32_t return_scene);
} FlipPassEditorCrudHostApiV1;

typedef struct {
    uint32_t api_version;
    bool (*execute_commit)(App* app, const FlipPassEditorCrudHostApiV1* host_api);
    bool (*execute_delete)(
        App* app,
        FlipPassEditorCrudDeleteTarget target,
        const FlipPassEditorCrudHostApiV1* host_api);
} FlipPassEditorCrudPluginV1;

const FlipperAppPluginDescriptor* flippass_editor_crud_plugin_ep(void);

#ifdef __cplusplus
}
#endif
