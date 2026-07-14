#include "../flippass.h"
#include "../flippass_db.h"
#include "../kdbx/memzero.h"
#include "../plugins/flippass_output_action_plugin.h"
#include "flippass_output_transport.h"

#include <storage/storage.h>
#include <toolbox/path.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FLIPPASS_OUTPUT_VAULT_STREAM_CHUNK_MAX (4U * KDBX_VAULT_RECORD_PLAIN_MAX)

typedef struct {
    App* app;
    const FlipPassOutputRequest* request;
} FlipPassOutputActionHostContext;

typedef struct {
    FuriString* group_path;
    FuriString* db_dir;
    FuriString* db_name;
    FuriString* db_basename;
    FuriString* db_ext;
    FuriString* entry_uuid;
    FuriString* resolved_autotype;
} FlipPassOutputActionStrings;

static FlipPassOutputActionPluginTransport
    flippass_output_action_transport(FlipPassOutputTransport transport) {
    return (transport == FlipPassOutputTransportBluetooth) ?
               FlipPassOutputActionPluginTransportBluetooth :
               FlipPassOutputActionPluginTransportUsb;
}

static FlipPassOutputTransport
    flippass_output_action_host_transport(FlipPassOutputActionPluginTransport transport) {
    return (transport == FlipPassOutputActionPluginTransportBluetooth) ?
               FlipPassOutputTransportBluetooth :
               FlipPassOutputTransportUsb;
}

static FlipPassOutputActionPluginKind flippass_output_action_kind(FlipPassOutputAction action) {
    switch(action) {
    case FlipPassOutputActionString:
        return FlipPassOutputActionPluginKindString;
    case FlipPassOutputActionLogin:
        return FlipPassOutputActionPluginKindLogin;
    case FlipPassOutputActionVaultRef:
        return FlipPassOutputActionPluginKindVaultRef;
    case FlipPassOutputActionLoginRefs:
        return FlipPassOutputActionPluginKindLoginRefs;
    case FlipPassOutputActionAutotype:
        return FlipPassOutputActionPluginKindAutotype;
    default:
        return FlipPassOutputActionPluginKindString;
    }
}

const char* flippass_output_transport_name(FlipPassOutputTransport transport) {
    switch(transport) {
    case FlipPassOutputTransportBluetooth:
        return "Bluetooth HID";
    case FlipPassOutputTransportUsb:
    default:
        return "USB HID";
    }
}

static void flippass_output_action_strings_alloc(FlipPassOutputActionStrings* strings) {
    furi_assert(strings);

    strings->group_path = furi_string_alloc();
    strings->db_dir = furi_string_alloc();
    strings->db_name = furi_string_alloc();
    strings->db_basename = furi_string_alloc();
    strings->db_ext = furi_string_alloc();
    strings->entry_uuid = furi_string_alloc();
    strings->resolved_autotype = furi_string_alloc();
}

static void flippass_output_action_strings_free(FlipPassOutputActionStrings* strings) {
    if(strings == NULL) {
        return;
    }

    if(strings->group_path != NULL) {
        furi_string_free(strings->group_path);
    }
    if(strings->db_dir != NULL) {
        furi_string_free(strings->db_dir);
    }
    if(strings->db_name != NULL) {
        furi_string_free(strings->db_name);
    }
    if(strings->db_basename != NULL) {
        furi_string_free(strings->db_basename);
    }
    if(strings->db_ext != NULL) {
        furi_string_free(strings->db_ext);
    }
    if(strings->entry_uuid != NULL) {
        furi_string_free(strings->entry_uuid);
    }
    if(strings->resolved_autotype != NULL) {
        furi_string_free(strings->resolved_autotype);
    }
    memset(strings, 0, sizeof(*strings));
}

static void flippass_output_cleanup_after_request(App* app) {
    if(app == NULL) {
        return;
    }

    flippass_module_unload(app, FlipPassModuleSlotOutputAction);
    flippass_output_transport_cleanup(app, FlipPassOutputTransportUsb);
    flippass_output_transport_cleanup(app, FlipPassOutputTransportBluetooth);
}

static const KDBXGroup* flippass_output_current_group(const App* app) {
    if(app == NULL) {
        return NULL;
    }

    return (app->current_group != NULL) ? app->current_group : app->active_group;
}

static void flippass_output_append_group_path(FuriString* out, const KDBXGroup* group) {
    if(out == NULL || group == NULL) {
        return;
    }

    if(group->parent != NULL && group->parent->parent != NULL) {
        flippass_output_append_group_path(out, group->parent);
        if(!furi_string_empty(out)) {
            furi_string_cat_str(out, ".");
        }
    }

    if(group->name != NULL) {
        furi_string_cat_str(out, group->name);
    }
}

static void flippass_output_set_db_ext(FuriString* out, const FuriString* path_string) {
    furi_assert(out);

    furi_string_reset(out);
    if(path_string == NULL) {
        return;
    }

    const char* path = furi_string_get_cstr(path_string);
    const char* slash = strrchr(path, '/');
    const char* dot = strrchr(path, '.');
    if(dot != NULL && (slash == NULL || dot > slash) && dot[1] != '\0') {
        furi_string_set_str(out, dot + 1);
    }
}

static bool flippass_output_prepare_autotype_request(
    App* app,
    const FlipPassOutputRequest* source,
    FlipPassOutputActionRequestV1* request,
    FlipPassOutputActionStrings* strings) {
    KDBXEntry* entry = (KDBXEntry*)source->entry;
    FuriString* error = furi_string_alloc();
    bool ok = true;

    if(entry == NULL) {
        furi_string_free(error);
        return false;
    }

    if(flippass_db_entry_has_field(entry, KDBXEntryFieldUsername) && entry->username == NULL) {
        ok = flippass_db_ensure_entry_field(app, entry, KDBXEntryFieldUsername, error);
    }
    if(ok && flippass_db_entry_has_field(entry, KDBXEntryFieldPassword) &&
       entry->password == NULL) {
        ok = flippass_db_ensure_entry_field(app, entry, KDBXEntryFieldPassword, error);
    }
    if(ok && flippass_db_entry_has_field(entry, KDBXEntryFieldUrl) && entry->url == NULL) {
        ok = flippass_db_ensure_entry_field(app, entry, KDBXEntryFieldUrl, error);
    }
    if(ok && flippass_db_entry_has_field(entry, KDBXEntryFieldNotes) && entry->notes == NULL) {
        ok = flippass_db_ensure_entry_field(app, entry, KDBXEntryFieldNotes, error);
    }
    if(ok && flippass_db_entry_has_field(entry, KDBXEntryFieldAutotype) &&
       entry->autotype_sequence == NULL) {
        ok = flippass_db_ensure_entry_field(app, entry, KDBXEntryFieldAutotype, error);
    }

    if(ok) {
        ok = flippass_db_copy_entry_uuid(app, entry, strings->entry_uuid, error);
    }

    if(ok) {
        const KDBXGroup* group = flippass_output_current_group(app);
        furi_string_reset(strings->group_path);
        flippass_output_append_group_path(strings->group_path, group);

        if(app->file_path != NULL) {
            path_extract_dirname(furi_string_get_cstr(app->file_path), strings->db_dir);
            path_extract_basename(furi_string_get_cstr(app->file_path), strings->db_name);
            path_extract_filename_no_ext(
                furi_string_get_cstr(app->file_path), strings->db_basename);
            flippass_output_set_db_ext(strings->db_ext, app->file_path);
        }

        if(entry->autotype_sequence != NULL && entry->autotype_sequence[0] != '\0') {
            ok = flippass_otp_resolve_autotype_sequence(
                app, entry, entry->autotype_sequence, strings->resolved_autotype, error);
        }

        request->autotype_sequence = (ok && !furi_string_empty(strings->resolved_autotype)) ?
                                         furi_string_get_cstr(strings->resolved_autotype) :
                                         entry->autotype_sequence;
        request->entry_title = entry->title;
        request->entry_username = entry->username;
        request->entry_password = entry->password;
        request->entry_url = entry->url;
        request->entry_notes = entry->notes;
        request->entry_uuid = furi_string_get_cstr(strings->entry_uuid);
        request->group_name = (group != NULL) ? group->name : NULL;
        request->group_path = furi_string_get_cstr(strings->group_path);
        request->db_path = (app->file_path != NULL) ? furi_string_get_cstr(app->file_path) : NULL;
        request->db_dir = furi_string_get_cstr(strings->db_dir);
        request->db_name = furi_string_get_cstr(strings->db_name);
        request->db_basename = furi_string_get_cstr(strings->db_basename);
        request->db_ext = furi_string_get_cstr(strings->db_ext);
    } else {
        FLIPPASS_LOG_EVENT(
            app, "OUTPUT_AUTOTYPE_PREP_FAIL reason=%s", furi_string_get_cstr(error));
    }

    furi_string_free(error);
    return ok;
}

static uint8_t* flippass_output_alloc_vault_stream_buffer(size_t* out_capacity) {
    static const size_t capacities[] = {
        FLIPPASS_OUTPUT_VAULT_STREAM_CHUNK_MAX,
        2U * KDBX_VAULT_RECORD_PLAIN_MAX,
        KDBX_VAULT_RECORD_PLAIN_MAX,
    };

    if(out_capacity == NULL) {
        return NULL;
    }

    for(size_t index = 0U; index < COUNT_OF(capacities); index++) {
        uint8_t* buffer = malloc(capacities[index]);
        if(buffer != NULL) {
            *out_capacity = capacities[index];
            return buffer;
        }
    }

    *out_capacity = 0U;
    return NULL;
}

static void flippass_output_action_progress(
    void* context,
    const char* stage,
    const char* detail,
    uint8_t percent) {
    FlipPassOutputActionHostContext* host = context;
    if(host == NULL || host->app == NULL) {
        return;
    }

    flippass_progress_update(
        host->app, (stage != NULL) ? stage : "Typing", (detail != NULL) ? detail : "", percent);
}

static bool flippass_output_action_should_cancel(void* context) {
    FlipPassOutputActionHostContext* host = context;
    return host != NULL && flippass_typing_should_cancel(host->app);
}

static bool flippass_output_action_begin_transport(
    void* context,
    FlipPassOutputActionPluginTransport transport) {
    FlipPassOutputActionHostContext* host = context;
    return host != NULL && host->app != NULL &&
           flippass_output_transport_begin(
               host->app, flippass_output_action_host_transport(transport));
}

static void flippass_output_action_end_transport(
    void* context,
    FlipPassOutputActionPluginTransport transport) {
    FlipPassOutputActionHostContext* host = context;
    if(host != NULL && host->app != NULL) {
        flippass_output_transport_end(host->app, flippass_output_action_host_transport(transport));
    }
}

static bool flippass_output_action_press_key(
    void* context,
    FlipPassOutputActionPluginTransport transport,
    uint16_t hid_key) {
    FlipPassOutputActionHostContext* host = context;
    return host != NULL && host->app != NULL &&
           flippass_output_transport_press_prepared(
               host->app, flippass_output_action_host_transport(transport), hid_key);
}

static bool flippass_output_action_release_key(
    void* context,
    FlipPassOutputActionPluginTransport transport,
    uint16_t hid_key) {
    FlipPassOutputActionHostContext* host = context;
    return host != NULL && host->app != NULL &&
           flippass_output_transport_release_prepared(
               host->app, flippass_output_action_host_transport(transport), hid_key);
}

static void flippass_output_action_release_all(
    void* context,
    FlipPassOutputActionPluginTransport transport) {
    FlipPassOutputActionHostContext* host = context;
    if(host != NULL && host->app != NULL) {
        flippass_output_transport_release_all_prepared(
            host->app, flippass_output_action_host_transport(transport));
    }
}

static bool flippass_output_action_usb_numlock_on(void* context) {
    UNUSED(context);
    return (furi_hal_hid_get_led_state() & HID_KB_LED_NUM) != 0U;
}

static const KDBXFieldRef* flippass_output_action_select_ref(
    const FlipPassOutputRequest* request,
    FlipPassOutputActionPluginRef ref) {
    if(request == NULL) {
        return NULL;
    }

    switch(ref) {
    case FlipPassOutputActionPluginRefPrimary:
        return request->ref;
    case FlipPassOutputActionPluginRefUsername:
        return request->username_ref;
    case FlipPassOutputActionPluginRefPassword:
        return request->password_ref;
    default:
        return NULL;
    }
}

static bool flippass_output_action_stream_ref(
    void* context,
    FlipPassOutputActionPluginRef ref,
    FlipPassOutputActionChunkCallback callback,
    void* callback_context) {
    FlipPassOutputActionHostContext* host = context;
    KDBXVaultReader reader;
    uint8_t* buffer = NULL;
    size_t buffer_capacity = 0U;
    bool ok = true;

    if(host == NULL || host->request == NULL || host->request->vault == NULL || callback == NULL) {
        return false;
    }

    const KDBXFieldRef* field_ref = flippass_output_action_select_ref(host->request, ref);
    if(field_ref == NULL) {
        return false;
    }
    if(kdbx_vault_ref_is_empty(field_ref)) {
        return true;
    }

    buffer = flippass_output_alloc_vault_stream_buffer(&buffer_capacity);
    if(buffer == NULL) {
        return false;
    }

    kdbx_vault_reader_reset(&reader, host->request->vault, field_ref);
    while(ok) {
        size_t chunk_size = 0U;
        if(!kdbx_vault_reader_read(&reader, buffer, buffer_capacity, &chunk_size)) {
            ok = false;
            break;
        }
        if(chunk_size == 0U) {
            break;
        }
        ok = callback(buffer, chunk_size, callback_context);
    }

    memzero(buffer, buffer_capacity);
    free(buffer);
    return ok;
}

static const FlipPassOutputActionPluginV1*
    flippass_output_action_plugin_get(App* app, FuriString* error) {
    const FlipperAppPluginDescriptor* descriptor = flippass_module_ensure(
        app,
        FlipPassModuleSlotOutputAction,
        NULL,
        FLIPPASS_OUTPUT_ACTION_PLUGIN_APP_ID,
        FLIPPASS_OUTPUT_ACTION_PLUGIN_API_VERSION,
        error);
    if(descriptor == NULL || descriptor->entry_point == NULL) {
        if(error != NULL && furi_string_empty(error)) {
            furi_string_set_str(error, "FlipPass typing engine plugin is unavailable.");
        }
        return NULL;
    }

    const FlipPassOutputActionPluginV1* plugin = descriptor->entry_point;
    if(plugin->api_version != FLIPPASS_OUTPUT_ACTION_PLUGIN_API_VERSION || plugin->run == NULL) {
        if(error != NULL) {
            furi_string_set_str(error, "FlipPass typing engine plugin has an incompatible API.");
        }
        flippass_module_unload(app, FlipPassModuleSlotOutputAction);
        return NULL;
    }

    return plugin;
}

bool flippass_output_execute_request(App* app, const FlipPassOutputRequest* source) {
    furi_assert(app);
    furi_assert(source);

    if(flippass_typing_should_cancel(app)) {
        return false;
    }

    FlipPassOutputActionStrings strings = {0};
    flippass_output_action_strings_alloc(&strings);

    FlipPassOutputActionRequestV1 request = {
        .api_version = FLIPPASS_OUTPUT_ACTION_PLUGIN_API_VERSION,
        .transport = flippass_output_action_transport(source->transport),
        .action = flippass_output_action_kind(source->action),
        .text = source->text,
        .username = source->username,
        .password = source->password,
        .keyboard_layout_path = (app->keyboard_layout_path != NULL) ?
                                    furi_string_get_cstr(app->keyboard_layout_path) :
                                    NULL,
        .primary_ref_plain_len = (source->ref != NULL) ? source->ref->plain_len : 0U,
        .username_ref_plain_len =
            (source->username_ref != NULL) ? source->username_ref->plain_len : 0U,
        .password_ref_plain_len =
            (source->password_ref != NULL) ? source->password_ref->plain_len : 0U,
    };

    bool ok = true;
    switch(source->action) {
    case FlipPassOutputActionString:
        ok = source->text != NULL;
        break;
    case FlipPassOutputActionLogin:
        ok = source->username != NULL && source->password != NULL;
        break;
    case FlipPassOutputActionVaultRef:
        ok = source->vault != NULL && source->ref != NULL;
        break;
    case FlipPassOutputActionLoginRefs:
        ok = source->vault != NULL && source->username_ref != NULL && source->password_ref != NULL;
        break;
    case FlipPassOutputActionAutotype:
        ok = flippass_output_prepare_autotype_request(app, source, &request, &strings);
        break;
    default:
        ok = false;
        break;
    }

    if(!ok) {
        flippass_output_cleanup_after_request(app);
        flippass_output_action_strings_free(&strings);
        return false;
    }

    FuriString* error = furi_string_alloc();
    const FlipPassOutputActionPluginV1* plugin = flippass_output_action_plugin_get(app, error);
    if(plugin == NULL) {
        FLIPPASS_LOG_EVENT(
            app, "OUTPUT_ACTION_PLUGIN_LOAD_FAIL reason=%s", furi_string_get_cstr(error));
        furi_string_free(error);
        flippass_output_cleanup_after_request(app);
        flippass_output_action_strings_free(&strings);
        return false;
    }

    FlipPassOutputActionHostContext context = {
        .app = app,
        .request = source,
    };
    const FlipPassOutputActionHostApiV1 host_api = {
        .api_version = FLIPPASS_OUTPUT_ACTION_HOST_API_VERSION,
        .context = &context,
        .progress = flippass_output_action_progress,
        .should_cancel = flippass_output_action_should_cancel,
        .begin_transport = flippass_output_action_begin_transport,
        .end_transport = flippass_output_action_end_transport,
        .press_key = flippass_output_action_press_key,
        .release_key = flippass_output_action_release_key,
        .release_all = flippass_output_action_release_all,
        .usb_numlock_on = flippass_output_action_usb_numlock_on,
        .stream_ref = flippass_output_action_stream_ref,
    };

    ok = plugin->run(&request, &host_api, error);
    if(!ok && !furi_string_empty(error)) {
        FLIPPASS_LOG_EVENT(app, "OUTPUT_ACTION_FAIL reason=%s", furi_string_get_cstr(error));
    }

    flippass_output_cleanup_after_request(app);
    furi_string_free(error);
    flippass_output_action_strings_free(&strings);
    return ok;
}

bool flippass_output_type_string(App* app, FlipPassOutputTransport transport, const char* text) {
    const FlipPassOutputRequest request = {
        .transport = transport,
        .action = FlipPassOutputActionString,
        .text = text,
    };
    return flippass_output_execute_request(app, &request);
}

bool flippass_output_type_login(
    App* app,
    FlipPassOutputTransport transport,
    const char* username,
    const char* password) {
    const FlipPassOutputRequest request = {
        .transport = transport,
        .action = FlipPassOutputActionLogin,
        .username = username,
        .password = password,
    };
    return flippass_output_execute_request(app, &request);
}

bool flippass_output_type_vault_ref(
    App* app,
    FlipPassOutputTransport transport,
    KDBXVault* vault,
    const KDBXFieldRef* ref) {
    const FlipPassOutputRequest request = {
        .transport = transport,
        .action = FlipPassOutputActionVaultRef,
        .vault = vault,
        .ref = ref,
    };
    return flippass_output_execute_request(app, &request);
}

bool flippass_output_type_login_refs(
    App* app,
    FlipPassOutputTransport transport,
    KDBXVault* vault,
    const KDBXFieldRef* username_ref,
    const KDBXFieldRef* password_ref) {
    const FlipPassOutputRequest request = {
        .transport = transport,
        .action = FlipPassOutputActionLoginRefs,
        .vault = vault,
        .username_ref = username_ref,
        .password_ref = password_ref,
    };
    return flippass_output_execute_request(app, &request);
}

bool flippass_output_type_autotype(
    App* app,
    FlipPassOutputTransport transport,
    const KDBXEntry* entry) {
    const FlipPassOutputRequest request = {
        .transport = transport,
        .action = FlipPassOutputActionAutotype,
        .entry = entry,
    };
    return flippass_output_execute_request(app, &request);
}

bool flippass_usb_begin(App* app) {
    return flippass_output_transport_begin(app, FlipPassOutputTransportUsb);
}

void flippass_usb_restore(App* app) {
    flippass_output_transport_cleanup(app, FlipPassOutputTransportUsb);
}

static bool flippass_usb_send_key(App* app, uint16_t hid_key) {
    if(hid_key == HID_KEYBOARD_NONE) {
        return false;
    }

    if(!flippass_output_transport_press_prepared(app, FlipPassOutputTransportUsb, hid_key)) {
        flippass_output_transport_release_all_prepared(app, FlipPassOutputTransportUsb);
        return false;
    }
    furi_delay_ms(FLIPPASS_USB_PRESS_DELAY_MS);
    if(!flippass_output_transport_release_prepared(app, FlipPassOutputTransportUsb, hid_key)) {
        flippass_output_transport_release_all_prepared(app, FlipPassOutputTransportUsb);
        return false;
    }
    furi_delay_ms(FLIPPASS_USB_RELEASE_DELAY_MS);
    return true;
}

bool flippass_usb_type_string(App* app, const char* text) {
    return flippass_output_type_string(app, FlipPassOutputTransportUsb, text);
}

bool flippass_usb_type_login(App* app, const char* username, const char* password) {
    return flippass_output_type_login(app, FlipPassOutputTransportUsb, username, password);
}

bool flippass_usb_type_key(App* app, uint16_t hid_key) {
    furi_assert(app);

    if(!flippass_usb_begin(app)) {
        flippass_usb_restore(app);
        return false;
    }

    const bool ok = flippass_usb_send_key(app, hid_key);
    flippass_usb_restore(app);
    return ok;
}

bool flippass_usb_type_autotype(App* app, const KDBXEntry* entry) {
    furi_assert(app);
    furi_assert(entry);

    return flippass_output_type_autotype(app, FlipPassOutputTransportUsb, entry);
}

bool flippass_output_bluetooth_is_connected(const App* app) {
    return flippass_output_transport_is_connected(app, FlipPassOutputTransportBluetooth);
}

bool flippass_output_usb_is_connected(const App* app) {
    return flippass_output_transport_is_connected(app, FlipPassOutputTransportUsb);
}

bool flippass_output_bluetooth_is_advertising(const App* app) {
    return flippass_output_transport_is_advertising(app, FlipPassOutputTransportBluetooth);
}

bool flippass_output_bluetooth_advertise(App* app) {
    return flippass_output_transport_advertise(app, FlipPassOutputTransportBluetooth);
}

bool flippass_output_prewarm_transport(App* app, FlipPassOutputTransport transport) {
    return flippass_output_transport_prewarm(app, transport);
}

void flippass_output_bluetooth_get_name(char* buffer, size_t size) {
    if(buffer == NULL || size == 0U) {
        return;
    }

    snprintf(buffer, size, "BadUSB %s", furi_hal_version_get_name_ptr());
}

void flippass_output_release_all(App* app) {
    furi_assert(app);

    flippass_output_transport_release_all_prepared(app, FlipPassOutputTransportUsb);
    flippass_output_transport_release_all_prepared(app, FlipPassOutputTransportBluetooth);
}

void flippass_output_cleanup_transport(App* app, FlipPassOutputTransport transport) {
    furi_assert(app);

    flippass_output_transport_cleanup(app, transport);
}

void flippass_output_cleanup(App* app) {
    furi_assert(app);

    flippass_output_cleanup_after_request(app);
}
