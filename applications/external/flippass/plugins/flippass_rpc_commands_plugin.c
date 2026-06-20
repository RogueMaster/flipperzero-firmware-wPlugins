#include "flippass_rpc_commands_plugin.h"

#include <stdlib.h>
#include <string.h>

#ifndef COUNT_OF
#define COUNT_OF(x) (sizeof(x) / sizeof((x)[0]))
#endif

static void fp_memzero(void* data, size_t size) {
    volatile uint8_t* bytes = data;
    while(size-- > 0U) {
        *bytes++ = 0U;
    }
}

static bool fp_eq(const char* a, const char* b) {
    if(a == NULL || b == NULL) return false;
    while(*a != '\0' && *b != '\0') {
        char ca = *a++;
        char cb = *b++;
        if(ca >= 'A' && ca <= 'Z') ca = (char)(ca - 'A' + 'a');
        if(cb >= 'A' && cb <= 'Z') cb = (char)(cb - 'A' + 'a');
        if(ca != cb) return false;
    }
    return (*a == '\0') && (*b == '\0');
}

static void fp_trim(char* text) {
    if(text == NULL) return;
    size_t len = strlen(text);
    while(len > 0U &&
          (text[len - 1U] == '\r' || text[len - 1U] == '\n' || text[len - 1U] == ' ')) {
        text[--len] = '\0';
    }
}

static void fp_json(FuriString* out, const char* value) {
    furi_string_cat(out, "\"");
    if(value != NULL) {
        for(const unsigned char* p = (const unsigned char*)value; *p != '\0'; ++p) {
            if(*p == '\\') {
                furi_string_cat(out, "\\\\");
            } else if(*p == '"') {
                furi_string_cat(out, "\\\"");
            } else if(*p == '\n') {
                furi_string_cat(out, "\\n");
            } else if(*p == '\r') {
                furi_string_cat(out, "\\r");
            } else if(*p == '\t') {
                furi_string_cat(out, "\\t");
            } else {
                furi_string_cat_printf(out, "%c", *p);
            }
        }
    }
    furi_string_cat(out, "\"");
}

static void fp_fail(
    FuriString* response,
    uint32_t code,
    const char* message,
    uint32_t* error_code,
    FuriString* error_text) {
    if(response != NULL) {
        furi_string_set_str(response, "{\"ok\":false,\"error\":");
        fp_json(response, message);
        furi_string_cat(response, "}");
    }
    if(error_code != NULL) *error_code = code;
    if(error_text != NULL) furi_string_set_str(error_text, message);
}

static void fp_fail_from_response(
    FuriString* response,
    uint32_t code,
    const char* fallback,
    uint32_t* error_code,
    FuriString* error_text) {
    const char* message = fallback;
    FuriString* message_copy = NULL;

    if(response != NULL && !furi_string_empty(response)) {
        message_copy = furi_string_alloc_set(response);
        message = furi_string_get_cstr(message_copy);
    }

    fp_fail(response, code, message, error_code, error_text);

    if(message_copy != NULL) {
        furi_string_free(message_copy);
    }
}

static bool fp_backend_hint(const char* text, const char** backend_out) {
    if(backend_out == NULL) return false;
    if(text == NULL || text[0] == '\0' || fp_eq(text, "ram")) {
        *backend_out = "ram";
        return true;
    }
    if(fp_eq(text, "int") || fp_eq(text, "internal")) {
        *backend_out = "int";
        return true;
    }
    if(fp_eq(text, "ext") || fp_eq(text, "external")) {
        *backend_out = "ext";
        return true;
    }
    return false;
}

static bool fp_transport(const char* text, FlipPassRpcTransport* transport) {
    if(transport == NULL) return false;
    if(text == NULL || text[0] == '\0' || fp_eq(text, "usb")) {
        *transport = FlipPassRpcTransportUsb;
        return true;
    }
    if(fp_eq(text, "bt") || fp_eq(text, "ble") || fp_eq(text, "bluetooth")) {
        *transport = FlipPassRpcTransportBluetooth;
        return true;
    }
    return false;
}

bool flip_pass_rpc_commands_request_parse(
    const uint8_t* data,
    size_t data_size,
    FlipPassRpcCommandsRequestV1* request) {
    if(data == NULL || data_size == 0U || request == NULL) return false;

    memset(request, 0, sizeof(*request));
    request->raw = malloc(data_size + 1U);
    if(request->raw == NULL) return false;

    request->raw_size = data_size + 1U;
    memcpy(request->raw, data, data_size);
    request->raw[data_size] = '\0';

    char* cursor = request->raw;
    while(cursor != NULL && *cursor != '\0' && request->count < COUNT_OF(request->part)) {
        char* token = cursor;
        char* newline = strchr(cursor, '\n');
        if(newline != NULL) {
            *newline = '\0';
            cursor = newline + 1;
        } else {
            cursor = NULL;
        }
        fp_trim(token);
        request->part[request->count++] = token;
    }

    if(request->count == 0U || request->part[0][0] == '\0') {
        flip_pass_rpc_commands_request_free(request);
        return false;
    }

    return true;
}

void flip_pass_rpc_commands_request_free(FlipPassRpcCommandsRequestV1* request) {
    if(request == NULL) return;
    if(request->raw != NULL) {
        fp_memzero(request->raw, request->raw_size);
        free(request->raw);
    }
    memset(request, 0, sizeof(*request));
}

static bool fp_require_host(
    const FlipPassRpcCommandsHostApiV1* host_api,
    uint32_t* error_code,
    FuriString* error_text) {
    const bool api_ok = host_api != NULL &&
                        host_api->api_version == FLIPPASS_RPC_COMMANDS_HOST_API_VERSION &&
                        host_api->context != NULL;
    if(!api_ok) {
        if(error_code != NULL) *error_code = FlipPassRpcCommandsErrorInvalidState;
        if(error_text != NULL) {
            furi_string_set_str(error_text, "RPC host API is unavailable or incompatible.");
        }
    }
    return api_ok;
}

static void fp_prepare_response(FuriString* response) {
    if(response != NULL) furi_string_reset(response);
}

static bool fp_exec_help(FuriString* response) {
    if(response != NULL) {
        furi_string_set_str(
            response,
            "{\"ok\":true,\"commands\":[\"status\",\"load\",\"unlock\",\"ls\",\"cd\",\"entry\",\"show\",\"type\"]}");
    }
    return true;
}

static bool fp_call_command(
    bool (*command_fn)(void* context, FuriString* response),
    void* context,
    FuriString* response,
    uint32_t* error_code,
    FuriString* error_text) {
    if(command_fn == NULL) {
        fp_fail(
            response,
            FlipPassRpcCommandsErrorInvalidState,
            "RPC host command is unavailable.",
            error_code,
            error_text);
        return false;
    }

    fp_prepare_response(response);
    const bool ok = command_fn(context, response);
    if(ok) {
        if(response != NULL && furi_string_empty(response)) {
            furi_string_set_str(response, "{\"ok\":true}");
        }
        if(error_code != NULL) *error_code = 0U;
        if(error_text != NULL) furi_string_reset(error_text);
        return true;
    }

    if(response == NULL || furi_string_empty(response)) {
        fp_fail(
            response,
            FlipPassRpcCommandsErrorOperationFailed,
            "RPC command failed.",
            error_code,
            error_text);
        return false;
    }

    fp_fail_from_response(
        response,
        FlipPassRpcCommandsErrorOperationFailed,
        "RPC command failed.",
        error_code,
        error_text);
    return false;
}

static bool fp_execute_request(
    const FlipPassRpcCommandsHostApiV1* host_api,
    const FlipPassRpcCommandsRequestV1* request,
    FuriString* response,
    uint32_t* error_code,
    FuriString* error_text) {
    if(request == NULL || request->count == 0U || request->part[0] == NULL) {
        fp_fail(
            response,
            FlipPassRpcCommandsErrorBadCommand,
            "Empty RPC command.",
            error_code,
            error_text);
        return false;
    }

    if(fp_eq(request->part[0], "help")) {
        if(error_code != NULL) *error_code = 0U;
        if(error_text != NULL) furi_string_reset(error_text);
        return fp_exec_help(response);
    }

    if(fp_eq(request->part[0], "status")) {
        if(!fp_require_host(host_api, error_code, error_text)) return false;
        return fp_call_command(
            host_api->status, host_api->context, response, error_code, error_text);
    }

    if(fp_eq(request->part[0], "load")) {
        if(request->count < 2U) {
            fp_fail(
                response,
                FlipPassRpcCommandsErrorMissingArgument,
                "The load command requires a file path.",
                error_code,
                error_text);
            return false;
        }
        if(!fp_require_host(host_api, error_code, error_text)) return false;
        if(host_api->load_file == NULL) {
            fp_fail(
                response,
                FlipPassRpcCommandsErrorInvalidState,
                "RPC host command is unavailable.",
                error_code,
                error_text);
            return false;
        }
        fp_prepare_response(response);
        const bool ok = host_api->load_file(host_api->context, request->part[1], response);
        if(ok) {
            if(response != NULL && furi_string_empty(response)) {
                furi_string_set_str(response, "{\"ok\":true}");
            }
            if(error_code != NULL) *error_code = 0U;
            if(error_text != NULL) furi_string_reset(error_text);
            return true;
        }
        if(response == NULL || furi_string_empty(response)) {
            fp_fail(
                response,
                FlipPassRpcCommandsErrorOperationFailed,
                "The requested KDBX file does not exist.",
                error_code,
                error_text);
            return false;
        }
        fp_fail_from_response(
            response,
            FlipPassRpcCommandsErrorOperationFailed,
            "The requested KDBX file does not exist.",
            error_code,
            error_text);
        return false;
    }

    if(fp_eq(request->part[0], "unlock")) {
        if(request->count < 2U) {
            fp_fail(
                response,
                FlipPassRpcCommandsErrorMissingArgument,
                "The unlock command requires a password.",
                error_code,
                error_text);
            return false;
        }
        const char* backend_hint = NULL;
        if(request->count >= 3U) {
            if(!fp_backend_hint(request->part[2], &backend_hint)) {
                fp_fail(
                    response,
                    FlipPassRpcCommandsErrorBadCommand,
                    "Unknown backend. Use ram, int, or ext.",
                    error_code,
                    error_text);
                return false;
            }
        }
        if(!fp_require_host(host_api, error_code, error_text)) return false;
        if(host_api->unlock == NULL) {
            fp_fail(
                response,
                FlipPassRpcCommandsErrorInvalidState,
                "RPC host command is unavailable.",
                error_code,
                error_text);
            return false;
        }
        fp_prepare_response(response);
        const bool ok =
            host_api->unlock(host_api->context, request->part[1], backend_hint, response);
        if(ok) {
            if(response != NULL && furi_string_empty(response)) {
                furi_string_set_str(response, "{\"ok\":true}");
            }
            if(error_code != NULL) *error_code = 0U;
            if(error_text != NULL) furi_string_reset(error_text);
            return true;
        }
        if(response == NULL || furi_string_empty(response)) {
            fp_fail(
                response,
                FlipPassRpcCommandsErrorOperationFailed,
                "Unlock failed.",
                error_code,
                error_text);
            return false;
        }
        fp_fail_from_response(
            response,
            FlipPassRpcCommandsErrorOperationFailed,
            "Unlock failed.",
            error_code,
            error_text);
        return false;
    }

    if(fp_eq(request->part[0], "ls")) {
        if(!fp_require_host(host_api, error_code, error_text)) return false;
        return fp_call_command(
            host_api->list, host_api->context, response, error_code, error_text);
    }

    if(fp_eq(request->part[0], "cd")) {
        if(request->count < 2U) {
            fp_fail(
                response,
                FlipPassRpcCommandsErrorMissingArgument,
                "The cd command requires a group index or '..'.",
                error_code,
                error_text);
            return false;
        }
        if(!fp_require_host(host_api, error_code, error_text)) return false;
        if(host_api->cd_parent == NULL || host_api->cd_index == NULL) {
            fp_fail(
                response,
                FlipPassRpcCommandsErrorInvalidState,
                "RPC host command is unavailable.",
                error_code,
                error_text);
            return false;
        }
        fp_prepare_response(response);
        bool ok = false;
        if(fp_eq(request->part[1], "..")) {
            ok = host_api->cd_parent(host_api->context, response);
        } else {
            char* end = NULL;
            unsigned long index = strtoul(request->part[1], &end, 10);
            if(end == NULL || *end != '\0') {
                fp_fail(
                    response,
                    FlipPassRpcCommandsErrorInvalidIndex,
                    "Group index must be numeric.",
                    error_code,
                    error_text);
                return false;
            }
            ok = host_api->cd_index(host_api->context, (uint32_t)index, response);
        }
        if(ok) {
            if(response != NULL && furi_string_empty(response)) {
                furi_string_set_str(response, "{\"ok\":true}");
            }
            if(error_code != NULL) *error_code = 0U;
            if(error_text != NULL) furi_string_reset(error_text);
            return true;
        }
        if(response == NULL || furi_string_empty(response)) {
            fp_fail(
                response,
                FlipPassRpcCommandsErrorOperationFailed,
                "The requested group could not be opened.",
                error_code,
                error_text);
            return false;
        }
        fp_fail_from_response(
            response,
            FlipPassRpcCommandsErrorOperationFailed,
            "The requested group could not be opened.",
            error_code,
            error_text);
        return false;
    }

    if(fp_eq(request->part[0], "entry")) {
        if(request->count < 2U) {
            fp_fail(
                response,
                FlipPassRpcCommandsErrorMissingArgument,
                "The entry command requires an entry index.",
                error_code,
                error_text);
            return false;
        }
        if(!fp_require_host(host_api, error_code, error_text)) return false;
        if(host_api->entry_index == NULL) {
            fp_fail(
                response,
                FlipPassRpcCommandsErrorInvalidState,
                "RPC host command is unavailable.",
                error_code,
                error_text);
            return false;
        }
        char* end = NULL;
        unsigned long index = strtoul(request->part[1], &end, 10);
        if(end == NULL || *end != '\0') {
            fp_fail(
                response,
                FlipPassRpcCommandsErrorInvalidIndex,
                "Entry index must be numeric.",
                error_code,
                error_text);
            return false;
        }
        fp_prepare_response(response);
        const bool ok = host_api->entry_index(host_api->context, (uint32_t)index, response);
        if(ok) {
            if(response != NULL && furi_string_empty(response)) {
                furi_string_set_str(response, "{\"ok\":true}");
            }
            if(error_code != NULL) *error_code = 0U;
            if(error_text != NULL) furi_string_reset(error_text);
            return true;
        }
        if(response == NULL || furi_string_empty(response)) {
            fp_fail(
                response,
                FlipPassRpcCommandsErrorOperationFailed,
                "The requested entry could not be opened.",
                error_code,
                error_text);
            return false;
        }
        fp_fail_from_response(
            response,
            FlipPassRpcCommandsErrorOperationFailed,
            "The requested entry could not be opened.",
            error_code,
            error_text);
        return false;
    }

    if(fp_eq(request->part[0], "show")) {
        if(request->count < 2U) {
            fp_fail(
                response,
                FlipPassRpcCommandsErrorMissingArgument,
                "The show command requires a field name.",
                error_code,
                error_text);
            return false;
        }
        if(!fp_require_host(host_api, error_code, error_text)) return false;
        if(host_api->show_entry == NULL || host_api->show_field == NULL) {
            fp_fail(
                response,
                FlipPassRpcCommandsErrorInvalidState,
                "RPC host command is unavailable.",
                error_code,
                error_text);
            return false;
        }
        fp_prepare_response(response);
        bool ok = false;
        if(fp_eq(request->part[1], "entry")) {
            ok = host_api->show_entry(host_api->context, response);
        } else {
            ok = host_api->show_field(host_api->context, request->part[1], response);
            if(!ok && response != NULL && furi_string_empty(response)) {
                fp_fail(
                    response,
                    FlipPassRpcCommandsErrorBadCommand,
                    "Unknown field name.",
                    error_code,
                    error_text);
                return false;
            }
        }
        if(ok) {
            if(response != NULL && furi_string_empty(response)) {
                furi_string_set_str(response, "{\"ok\":true}");
            }
            if(error_code != NULL) *error_code = 0U;
            if(error_text != NULL) furi_string_reset(error_text);
            return true;
        }
        if(response == NULL || furi_string_empty(response)) {
            fp_fail(
                response,
                FlipPassRpcCommandsErrorOperationFailed,
                "The requested value could not be read.",
                error_code,
                error_text);
            return false;
        }
        fp_fail_from_response(
            response,
            FlipPassRpcCommandsErrorOperationFailed,
            "The requested value could not be read.",
            error_code,
            error_text);
        return false;
    }

    if(fp_eq(request->part[0], "type")) {
        if(request->count < 2U) {
            fp_fail(
                response,
                FlipPassRpcCommandsErrorMissingArgument,
                "The type command requires a field name.",
                error_code,
                error_text);
            return false;
        }
        if(!fp_require_host(host_api, error_code, error_text)) return false;
        if(host_api->type_field == NULL) {
            fp_fail(
                response,
                FlipPassRpcCommandsErrorInvalidState,
                "RPC host command is unavailable.",
                error_code,
                error_text);
            return false;
        }
        FlipPassRpcTransport transport = FlipPassRpcTransportUsb;
        if(request->count >= 3U && !fp_transport(request->part[2], &transport)) {
            fp_fail(
                response,
                FlipPassRpcCommandsErrorUnsupportedTransport,
                "Unknown transport. Use usb, bt, ble, or bluetooth.",
                error_code,
                error_text);
            return false;
        }
        fp_prepare_response(response);
        const bool ok =
            host_api->type_field(host_api->context, request->part[1], transport, response);
        if(ok) {
            if(response != NULL && furi_string_empty(response)) {
                furi_string_set_str(response, "{\"ok\":true}");
            }
            if(error_code != NULL) *error_code = 0U;
            if(error_text != NULL) furi_string_reset(error_text);
            return true;
        }
        if(response == NULL || furi_string_empty(response)) {
            fp_fail(
                response,
                FlipPassRpcCommandsErrorOperationFailed,
                "Typing failed because the transport was unavailable, not connected, or unsupported by the selected entry.",
                error_code,
                error_text);
            return false;
        }
        fp_fail_from_response(
            response,
            FlipPassRpcCommandsErrorOperationFailed,
            "Typing failed because the transport was unavailable, not connected, or unsupported by the selected entry.",
            error_code,
            error_text);
        return false;
    }

    fp_fail(
        response,
        FlipPassRpcCommandsErrorBadCommand,
        "Unknown RPC command.",
        error_code,
        error_text);
    return false;
}

static bool fp_execute_bytes(
    const FlipPassRpcCommandsHostApiV1* host_api,
    const uint8_t* data,
    size_t data_size,
    FuriString* response,
    uint32_t* error_code,
    FuriString* error_text) {
    FlipPassRpcCommandsRequestV1 request = {0};
    const bool parsed = flip_pass_rpc_commands_request_parse(data, data_size, &request);
    const bool ok = parsed &&
                    fp_execute_request(host_api, &request, response, error_code, error_text);
    if(!parsed) {
        fp_fail(
            response,
            FlipPassRpcCommandsErrorBadCommand,
            "Empty RPC command.",
            error_code,
            error_text);
    }
    flip_pass_rpc_commands_request_free(&request);
    return ok;
}

static const FlipPassRpcCommandsPluginV1 flippass_rpc_commands_plugin_v1 = {
    .api_version = FLIPPASS_RPC_COMMANDS_PLUGIN_API_VERSION,
    .execute_bytes = fp_execute_bytes,
    .execute_request = fp_execute_request,
};

static const FlipperAppPluginDescriptor flippass_rpc_commands_plugin_descriptor = {
    .appid = FLIPPASS_RPC_COMMANDS_PLUGIN_APPID,
    .ep_api_version = FLIPPASS_RPC_COMMANDS_PLUGIN_API_VERSION,
    .entry_point = &flippass_rpc_commands_plugin_v1,
};

const FlipperAppPluginDescriptor* flippass_rpc_commands_plugin_ep(void) {
    return &flippass_rpc_commands_plugin_descriptor;
}
