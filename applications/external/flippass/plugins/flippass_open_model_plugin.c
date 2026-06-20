#include "flippass_open_model_plugin.h"

#include "../kdbx/kdbx_protected.h"
#include "../kdbx/xml_parser.h"
#include "../kdbx/memzero.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

#define FLIPPASS_OPEN_MAX_XML_STREAM_BYTES (2U * 1024U * 1024U)
#define FLIPPASS_OPEN_MAX_XML_DEPTH        64U
#define FLIPPASS_OPEN_ERROR_SIZE           256U

typedef enum {
    FlipPassOpenTextStateNone = 0,
    FlipPassOpenTextStateGroupName,
    FlipPassOpenTextStateGroupUuid,
    FlipPassOpenTextStateEntryUuid,
    FlipPassOpenTextStateStringKey,
    FlipPassOpenTextStateStringValue,
    FlipPassOpenTextStateAutoTypeSequence,
} FlipPassOpenTextState;

typedef struct {
    uint8_t* data;
    size_t size;
    size_t capacity;
} FlipPassOpenByteBuffer;

typedef struct {
    const FlipPassOpenModelHostApiV1* host_api;
    const FlipPassOpenBuilderApiV1* builder_api;
    FuriString* error;
    XmlParser* xml_parser;
    KDBXProtectedStream protected_stream;
    FlipPassOpenByteBuffer protected_stream_key;
    FlipPassOpenByteBuffer protected_value_buffer;
    FuriString* text_value;
    FuriString* string_key;
    KDBXProtectedDiscardState protected_discard_state;
    FlipPassOpenTextState text_state;
    uint8_t progress_percent;
    uint8_t inner_header_prefix[5];
    size_t inner_header_prefix_len;
    uint8_t inner_field_id;
    uint32_t inner_field_size;
    size_t inner_field_remaining;
    uint32_t protected_stream_id;
    size_t xml_bytes;
    size_t xml_total_bytes_hint;
    size_t group_count;
    size_t entry_count;
    size_t stream_chunk_count;
    size_t history_skip_depth;
    int parsing_depth;
    bool parse_failed;
    bool inner_header_done;
    bool in_group;
    bool in_entry;
    bool in_string;
    bool in_autotype;
    bool skipping_history;
    bool history_in_string;
    bool history_collect_protected_value;
    bool history_value_protected;
    bool value_protected;
    bool deferred_stream_active;
    bool protected_discard_active;
    char parse_error[FLIPPASS_OPEN_ERROR_SIZE];
} FlipPassOpenContext;

typedef struct {
    FlipPassOpenContext ctx;
} FlipPassOpenModelState;

static bool fp_open_payload_chunk_callback(const uint8_t* data, size_t data_size, void* context);
static bool fp_open_write_streamed_value_chunk(
    FlipPassOpenContext* ctx,
    const uint8_t* data,
    size_t data_size);
static bool
    fp_open_write_streamed_protected_chunk(const uint8_t* data, size_t data_size, void* context);
static void fp_open_abort_streamed_value(FlipPassOpenContext* ctx);

static void fp_open_progress(
    FlipPassOpenContext* ctx,
    const char* stage,
    const char* detail,
    uint8_t percent) {
    if(ctx == NULL || ctx->host_api == NULL || ctx->host_api->progress == NULL) {
        return;
    }

    ctx->progress_percent = percent;
    ctx->host_api->progress(ctx->host_api->context, stage, detail, percent);
}

static void fp_open_log(FlipPassOpenContext* ctx, const char* message) {
    if(ctx != NULL && ctx->host_api != NULL && ctx->host_api->log != NULL && message != NULL) {
        ctx->host_api->log(ctx->host_api->context, message);
    }
}

static void fp_open_set_error(FlipPassOpenContext* ctx, const char* format, ...) {
    va_list args;

    if(ctx == NULL || format == NULL || ctx->parse_failed) {
        return;
    }

    ctx->parse_failed = true;
    va_start(args, format);
    vsnprintf(ctx->parse_error, sizeof(ctx->parse_error), format, args);
    va_end(args);
}

static bool fp_open_byte_buffer_reserve(FlipPassOpenByteBuffer* buffer, size_t capacity) {
    uint8_t* next = NULL;

    furi_assert(buffer);
    if(capacity <= buffer->capacity) {
        return true;
    }

    size_t next_capacity = buffer->capacity > 0U ? buffer->capacity : 512U;
    while(next_capacity < capacity) {
        if(next_capacity > (SIZE_MAX / 2U)) {
            next_capacity = capacity;
            break;
        }
        next_capacity *= 2U;
    }

    next = malloc(next_capacity);
    if(next == NULL) {
        return false;
    }

    if(buffer->data != NULL) {
        if(buffer->size > 0U) {
            memcpy(next, buffer->data, buffer->size);
        }
        memzero(buffer->data, buffer->capacity);
        free(buffer->data);
    }

    buffer->data = next;
    buffer->capacity = next_capacity;
    return true;
}

static bool fp_open_byte_buffer_append(
    FlipPassOpenByteBuffer* buffer,
    const uint8_t* data,
    size_t data_size) {
    furi_assert(buffer);
    if(data_size == 0U) {
        return true;
    }
    if(data == NULL || data_size > (SIZE_MAX - buffer->size)) {
        return false;
    }
    if(!fp_open_byte_buffer_reserve(buffer, buffer->size + data_size)) {
        return false;
    }
    memcpy(buffer->data + buffer->size, data, data_size);
    buffer->size += data_size;
    return true;
}

static void fp_open_byte_buffer_free(FlipPassOpenByteBuffer* buffer) {
    if(buffer == NULL) {
        return;
    }
    if(buffer->data != NULL) {
        memzero(buffer->data, buffer->capacity);
        free(buffer->data);
    }
    memset(buffer, 0, sizeof(*buffer));
}

static const char* fp_open_find_attribute(const char** attributes, const char* name) {
    if(attributes == NULL || name == NULL) {
        return NULL;
    }

    for(size_t index = 0U; attributes[index] != NULL && attributes[index + 1U] != NULL;
        index += 2U) {
        if(strcmp(attributes[index], name) == 0) {
            return attributes[index + 1U];
        }
    }

    return NULL;
}

static void fp_open_begin_text(FlipPassOpenContext* ctx, FlipPassOpenTextState state) {
    furi_assert(ctx);
    ctx->text_state = state;
    furi_string_reset(ctx->text_value);
    if(state != FlipPassOpenTextStateStringValue) {
        furi_string_reset(ctx->string_key);
    }
}

static bool fp_open_append_text_segment(FlipPassOpenContext* ctx, const char* data, int len) {
    furi_assert(ctx);
    furi_assert(data);

    if(!furi_string_cat_printf(ctx->text_value, "%.*s", len, data)) {
        fp_open_set_error(ctx, "Not enough RAM is available to parse the XML payload.");
        return false;
    }

    return true;
}

static bool fp_open_should_stream_string_value(FlipPassOpenContext* ctx) {
    if(ctx == NULL || ctx->builder_api == NULL ||
       ctx->builder_api->should_stream_string_value == NULL) {
        return false;
    }

    return ctx->builder_api->should_stream_string_value(
        ctx->builder_api->context, furi_string_get_cstr(ctx->string_key));
}

static bool fp_open_begin_streamed_value(FlipPassOpenContext* ctx) {
    if(ctx == NULL || ctx->builder_api == NULL || ctx->builder_api->begin_streamed_value == NULL) {
        return false;
    }

    if(!ctx->builder_api->begin_streamed_value(
           ctx->builder_api->context,
           furi_string_get_cstr(ctx->string_key),
           ctx->value_protected,
           ctx->error)) {
        if(!ctx->parse_failed && ctx->error != NULL && !furi_string_empty(ctx->error)) {
            fp_open_set_error(ctx, "%s", furi_string_get_cstr(ctx->error));
        }
        return false;
    }

    ctx->deferred_stream_active = true;
    return true;
}

static bool fp_open_drain_buffered_value_into_stream(FlipPassOpenContext* ctx) {
    const char* buffered = NULL;
    const size_t buffered_size = (ctx != NULL) ? furi_string_size(ctx->text_value) : 0U;

    if(ctx == NULL || !ctx->deferred_stream_active || buffered_size == 0U) {
        return true;
    }

    buffered = furi_string_get_cstr(ctx->text_value);
    if(ctx->value_protected) {
        kdbx_protected_discard_state_init(&ctx->protected_discard_state);
        if(!kdbx_protected_decode_state_update(
               &ctx->protected_stream,
               &ctx->protected_discard_state,
               buffered,
               buffered_size,
               fp_open_write_streamed_protected_chunk,
               ctx)) {
            fp_open_abort_streamed_value(ctx);
            fp_open_set_error(ctx, "A protected entry field could not be decoded.");
            return false;
        }
    } else if(!fp_open_write_streamed_value_chunk(ctx, (const uint8_t*)buffered, buffered_size)) {
        fp_open_abort_streamed_value(ctx);
        return false;
    }

    furi_string_reset(ctx->text_value);
    return true;
}

static bool fp_open_maybe_spill_buffered_value(FlipPassOpenContext* ctx) {
    const char* key = NULL;
    const size_t buffered_size = (ctx != NULL) ? furi_string_size(ctx->text_value) : 0U;

    if(ctx == NULL || ctx->deferred_stream_active ||
       ctx->text_state != FlipPassOpenTextStateStringValue ||
       buffered_size < KDBX_VAULT_RECORD_PLAIN_MAX || ctx->builder_api == NULL ||
       ctx->builder_api->prepare_string_value_stream == NULL) {
        return true;
    }

    key = furi_string_get_cstr(ctx->string_key);
    if(key == NULL || key[0] == '\0') {
        return true;
    }

    if(!ctx->builder_api->prepare_string_value_stream(
           ctx->builder_api->context, key, buffered_size, ctx->error)) {
        if(!ctx->parse_failed && ctx->error != NULL && !furi_string_empty(ctx->error)) {
            fp_open_set_error(ctx, "%s", furi_string_get_cstr(ctx->error));
        }
        return false;
    }

    if(!fp_open_should_stream_string_value(ctx)) {
        return true;
    }

    if(!fp_open_begin_streamed_value(ctx)) {
        return false;
    }

    return fp_open_drain_buffered_value_into_stream(ctx);
}

static bool fp_open_write_streamed_value_chunk(
    FlipPassOpenContext* ctx,
    const uint8_t* data,
    size_t data_size) {
    if(ctx == NULL || data == NULL || ctx->builder_api == NULL ||
       ctx->builder_api->write_streamed_value_chunk == NULL) {
        return false;
    }

    if(!ctx->builder_api->write_streamed_value_chunk(
           ctx->builder_api->context,
           furi_string_get_cstr(ctx->string_key),
           data,
           data_size,
           ctx->error)) {
        if(!ctx->parse_failed && ctx->error != NULL && !furi_string_empty(ctx->error)) {
            fp_open_set_error(ctx, "%s", furi_string_get_cstr(ctx->error));
        }
        return false;
    }

    return true;
}

static bool
    fp_open_write_streamed_protected_chunk(const uint8_t* data, size_t data_size, void* context) {
    if(data_size == 0U) {
        return true;
    }

    return fp_open_write_streamed_value_chunk(context, data, data_size);
}

static void fp_open_abort_streamed_value(FlipPassOpenContext* ctx) {
    if(ctx == NULL || ctx->builder_api == NULL || ctx->builder_api->abort_streamed_value == NULL) {
        return;
    }

    ctx->builder_api->abort_streamed_value(ctx->builder_api->context);
    ctx->deferred_stream_active = false;
}

static bool fp_open_commit_streamed_value(FlipPassOpenContext* ctx) {
    if(ctx == NULL || ctx->builder_api == NULL ||
       ctx->builder_api->commit_streamed_value == NULL) {
        return false;
    }

    if(!ctx->builder_api->commit_streamed_value(
           ctx->builder_api->context, furi_string_get_cstr(ctx->string_key), ctx->error)) {
        if(!ctx->parse_failed && ctx->error != NULL && !furi_string_empty(ctx->error)) {
            fp_open_set_error(ctx, "%s", furi_string_get_cstr(ctx->error));
        }
        return false;
    }

    ctx->deferred_stream_active = false;
    return true;
}

static bool fp_open_commit_entry_value(
    FlipPassOpenContext* ctx,
    const char* key,
    const char* value,
    size_t value_len) {
    if(key == NULL || key[0] == '\0') {
        return true;
    }

    if(strcmp(key, "Title") == 0) {
        return ctx->builder_api->set_entry_title(
            ctx->builder_api->context, value, value_len, ctx->error);
    }
    if(strcmp(key, "UUID") == 0) {
        return ctx->builder_api->set_entry_uuid(
            ctx->builder_api->context, value, value_len, ctx->error);
    }
    if(strcmp(key, "UserName") == 0) {
        return ctx->builder_api->set_entry_standard_field(
            ctx->builder_api->context, KDBXEntryFieldUsername, value, value_len, ctx->error);
    }
    if(strcmp(key, "Password") == 0) {
        return ctx->builder_api->set_entry_standard_field(
            ctx->builder_api->context, KDBXEntryFieldPassword, value, value_len, ctx->error);
    }
    if(strcmp(key, "URL") == 0) {
        return ctx->builder_api->set_entry_standard_field(
            ctx->builder_api->context, KDBXEntryFieldUrl, value, value_len, ctx->error);
    }
    if(strcmp(key, "Notes") == 0) {
        return ctx->builder_api->set_entry_standard_field(
            ctx->builder_api->context, KDBXEntryFieldNotes, value, value_len, ctx->error);
    }

    return ctx->builder_api->add_custom_field(
        ctx->builder_api->context, key, value, value_len, ctx->value_protected, ctx->error);
}

static bool fp_open_commit_text_value(
    FlipPassOpenContext* ctx,
    FlipPassOpenTextState state,
    const char* value,
    size_t value_len) {
    char* decoded_value = NULL;
    size_t decoded_size = 0U;
    bool ok = true;

    switch(state) {
    case FlipPassOpenTextStateGroupName:
        ok = ctx->builder_api->set_group_name(
            ctx->builder_api->context, value, value_len, ctx->error);
        break;
    case FlipPassOpenTextStateGroupUuid:
        ok = ctx->builder_api->set_group_uuid(
            ctx->builder_api->context, value, value_len, ctx->error);
        break;
    case FlipPassOpenTextStateEntryUuid:
        ok = ctx->builder_api->set_entry_uuid(
            ctx->builder_api->context, value, value_len, ctx->error);
        break;
    case FlipPassOpenTextStateAutoTypeSequence:
        ok = ctx->builder_api->set_entry_standard_field(
            ctx->builder_api->context, KDBXEntryFieldAutotype, value, value_len, ctx->error);
        break;
    case FlipPassOpenTextStateStringValue: {
        const char* key = furi_string_get_cstr(ctx->string_key);
        if(ctx->value_protected) {
            if(!ctx->protected_stream.ready || !kdbx_protected_value_decode_reuse(
                                                   &ctx->protected_stream,
                                                   value,
                                                   &decoded_value,
                                                   &decoded_size,
                                                   &ctx->protected_value_buffer.data,
                                                   &ctx->protected_value_buffer.capacity)) {
                fp_open_set_error(ctx, "A protected entry field could not be decoded.");
                return false;
            }
            ctx->protected_value_buffer.size = decoded_size + 1U;
            value = decoded_value;
            value_len = decoded_size;
        }
        ok = fp_open_commit_entry_value(ctx, key, value, value_len);
        break;
    }
    case FlipPassOpenTextStateStringKey:
    case FlipPassOpenTextStateNone:
    default:
        ok = true;
        break;
    }

    if(decoded_value != NULL) {
        memzero(decoded_value, decoded_size + 1U);
        ctx->protected_value_buffer.size = 0U;
    }

    if(!ok && !ctx->parse_failed && ctx->error != NULL && !furi_string_empty(ctx->error)) {
        fp_open_set_error(ctx, "%s", furi_string_get_cstr(ctx->error));
    }

    return ok;
}

static bool fp_open_consume_history_protected_value(FlipPassOpenContext* ctx) {
    if(!ctx->history_value_protected || furi_string_size(ctx->text_value) == 0U) {
        return true;
    }

    if(!ctx->protected_stream.ready ||
       !kdbx_protected_value_discard(
           &ctx->protected_stream, furi_string_get_cstr(ctx->text_value))) {
        fp_open_set_error(ctx, "A protected entry field could not be decoded.");
        return false;
    }

    return true;
}

static void fp_open_start_element(void* context, const char* name, const char** attributes) {
    FlipPassOpenContext* ctx = context;
    char message[32];
    furi_assert(ctx);

    if(ctx->parse_failed) {
        return;
    }

    if(ctx->skipping_history) {
        ctx->history_skip_depth++;
        if(strcmp(name, "String") == 0) {
            ctx->history_in_string = true;
            ctx->history_collect_protected_value = false;
            ctx->history_value_protected = false;
            ctx->protected_discard_active = false;
            furi_string_reset(ctx->text_value);
        } else if(strcmp(name, "Value") == 0 && ctx->history_in_string) {
            const char* protected_value = fp_open_find_attribute(attributes, "Protected");
            ctx->history_value_protected =
                protected_value != NULL &&
                (strcmp(protected_value, "True") == 0 || strcmp(protected_value, "true") == 0);
            ctx->history_collect_protected_value = ctx->history_value_protected;
            ctx->protected_discard_active = ctx->history_value_protected;
            if(ctx->protected_discard_active) {
                kdbx_protected_discard_state_init(&ctx->protected_discard_state);
            }
            furi_string_reset(ctx->text_value);
        }
        return;
    }

    if(strcmp(name, "History") == 0 && ctx->in_entry) {
        ctx->skipping_history = true;
        ctx->history_skip_depth = 1U;
        ctx->history_in_string = false;
        ctx->history_collect_protected_value = false;
        ctx->history_value_protected = false;
        ctx->protected_discard_active = false;
        furi_string_reset(ctx->text_value);
        return;
    }

    if(strcmp(name, "Group") == 0) {
        if(ctx->group_count == 0U) {
            fp_open_log(ctx, "XML_FIRST_GROUP");
        }
        if(!ctx->builder_api->begin_group(ctx->builder_api->context, ctx->error)) {
            fp_open_set_error(
                ctx,
                "%s",
                (ctx->error != NULL && !furi_string_empty(ctx->error)) ?
                    furi_string_get_cstr(ctx->error) :
                    "Unable to build the group model.");
            return;
        }
        ctx->group_count++;
        if(ctx->group_count <= 4U) {
            snprintf(
                message, sizeof(message), "XML_GROUP count=%lu", (unsigned long)ctx->group_count);
            fp_open_log(ctx, message);
        }
        ctx->in_group = true;
        ctx->parsing_depth++;
        if((size_t)ctx->parsing_depth > FLIPPASS_OPEN_MAX_XML_DEPTH) {
            fp_open_set_error(ctx, "The XML nesting depth exceeds FlipPass's safe limit.");
        }
        return;
    }

    if(strcmp(name, "Entry") == 0) {
        if(ctx->entry_count == 0U) {
            fp_open_log(ctx, "XML_FIRST_ENTRY");
        }
        if(!ctx->in_group) {
            fp_open_set_error(ctx, "The XML entry appeared outside of any group.");
            return;
        }
        if(!ctx->builder_api->begin_entry(ctx->builder_api->context, ctx->error)) {
            fp_open_set_error(
                ctx,
                "%s",
                (ctx->error != NULL && !furi_string_empty(ctx->error)) ?
                    furi_string_get_cstr(ctx->error) :
                    "Unable to build the entry model.");
            return;
        }
        ctx->entry_count++;
        if(ctx->entry_count <= 4U) {
            snprintf(
                message, sizeof(message), "XML_ENTRY count=%lu", (unsigned long)ctx->entry_count);
            fp_open_log(ctx, message);
        }
        ctx->in_entry = true;
        ctx->parsing_depth++;
        if((size_t)ctx->parsing_depth > FLIPPASS_OPEN_MAX_XML_DEPTH) {
            fp_open_set_error(ctx, "The XML nesting depth exceeds FlipPass's safe limit.");
        }
        return;
    }

    if(strcmp(name, "AutoType") == 0 && ctx->in_entry) {
        ctx->in_autotype = true;
        return;
    }

    if(strcmp(name, "Name") == 0 && ctx->in_group && !ctx->in_entry) {
        fp_open_begin_text(ctx, FlipPassOpenTextStateGroupName);
        return;
    }

    if(strcmp(name, "UUID") == 0 && ctx->in_group && !ctx->in_entry) {
        fp_open_begin_text(ctx, FlipPassOpenTextStateGroupUuid);
        return;
    }

    if(strcmp(name, "UUID") == 0 && ctx->in_entry) {
        fp_open_begin_text(ctx, FlipPassOpenTextStateEntryUuid);
        return;
    }

    if(strcmp(name, "DefaultSequence") == 0 && ctx->in_entry && ctx->in_autotype) {
        fp_open_begin_text(ctx, FlipPassOpenTextStateAutoTypeSequence);
        return;
    }

    if(strcmp(name, "String") == 0) {
        ctx->in_string = true;
        furi_string_reset(ctx->string_key);
        return;
    }

    if(strcmp(name, "Key") == 0 && ctx->in_entry && ctx->in_string) {
        fp_open_begin_text(ctx, FlipPassOpenTextStateStringKey);
        return;
    }

    if(strcmp(name, "Value") == 0 && ctx->in_entry && ctx->in_string) {
        const char* protected_value = fp_open_find_attribute(attributes, "Protected");
        ctx->value_protected = protected_value != NULL && (strcmp(protected_value, "True") == 0 ||
                                                           strcmp(protected_value, "true") == 0);
        ctx->protected_discard_active = false;
        fp_open_begin_text(ctx, FlipPassOpenTextStateStringValue);
        if(fp_open_should_stream_string_value(ctx)) {
            if(!fp_open_begin_streamed_value(ctx)) {
                return;
            }
            if(ctx->value_protected) {
                kdbx_protected_discard_state_init(&ctx->protected_discard_state);
            }
        }
    }
}

static void fp_open_end_element(void* context, const char* name) {
    FlipPassOpenContext* ctx = context;
    furi_assert(ctx);

    if(ctx->parse_failed) {
        return;
    }

    if(ctx->skipping_history) {
        if(strcmp(name, "Value") == 0 && ctx->history_in_string) {
            if(ctx->protected_discard_active) {
                if(!kdbx_protected_discard_state_finalize(
                       &ctx->protected_stream, &ctx->protected_discard_state)) {
                    fp_open_set_error(ctx, "A protected entry field could not be decoded.");
                }
                ctx->protected_discard_active = false;
            } else {
                fp_open_consume_history_protected_value(ctx);
            }
            furi_string_reset(ctx->text_value);
            ctx->history_collect_protected_value = false;
            ctx->history_value_protected = false;
        } else if(strcmp(name, "String") == 0) {
            ctx->history_in_string = false;
            ctx->history_collect_protected_value = false;
            ctx->history_value_protected = false;
            ctx->protected_discard_active = false;
            furi_string_reset(ctx->text_value);
        }

        if(ctx->history_skip_depth > 0U) {
            ctx->history_skip_depth--;
        }
        if(ctx->history_skip_depth == 0U) {
            ctx->skipping_history = false;
            ctx->history_in_string = false;
            ctx->history_collect_protected_value = false;
            ctx->history_value_protected = false;
            ctx->protected_discard_active = false;
            furi_string_reset(ctx->text_value);
        }
        return;
    }

    if(strcmp(name, "Key") == 0 && ctx->text_state == FlipPassOpenTextStateStringKey) {
        furi_string_set(ctx->string_key, ctx->text_value);
        furi_string_reset(ctx->text_value);
        ctx->text_state = FlipPassOpenTextStateNone;
        return;
    }

    if(strcmp(name, "Value") == 0 && ctx->text_state == FlipPassOpenTextStateStringValue) {
        if(ctx->deferred_stream_active) {
            if(ctx->value_protected && !kdbx_protected_decode_state_finalize(
                                           &ctx->protected_stream,
                                           &ctx->protected_discard_state,
                                           fp_open_write_streamed_protected_chunk,
                                           ctx)) {
                fp_open_abort_streamed_value(ctx);
                fp_open_set_error(ctx, "A protected entry field could not be decoded.");
            } else if(!ctx->parse_failed) {
                fp_open_commit_streamed_value(ctx);
            }
        } else if(ctx->protected_discard_active) {
            if(!kdbx_protected_discard_state_finalize(
                   &ctx->protected_stream, &ctx->protected_discard_state)) {
                fp_open_set_error(ctx, "A protected entry field could not be decoded.");
            }
        } else {
            fp_open_commit_text_value(
                ctx,
                ctx->text_state,
                furi_string_get_cstr(ctx->text_value),
                furi_string_size(ctx->text_value));
        }
        furi_string_reset(ctx->text_value);
        ctx->value_protected = false;
        ctx->protected_discard_active = false;
        ctx->text_state = FlipPassOpenTextStateNone;
        return;
    }

    if(((strcmp(name, "Name") == 0) && ctx->text_state == FlipPassOpenTextStateGroupName) ||
       ((strcmp(name, "UUID") == 0) && ctx->text_state == FlipPassOpenTextStateGroupUuid) ||
       ((strcmp(name, "UUID") == 0) && ctx->text_state == FlipPassOpenTextStateEntryUuid) ||
       ((strcmp(name, "DefaultSequence") == 0) &&
        ctx->text_state == FlipPassOpenTextStateAutoTypeSequence)) {
        fp_open_commit_text_value(
            ctx,
            ctx->text_state,
            furi_string_get_cstr(ctx->text_value),
            furi_string_size(ctx->text_value));
        furi_string_reset(ctx->text_value);
        ctx->text_state = FlipPassOpenTextStateNone;
    }

    if(strcmp(name, "String") == 0) {
        ctx->in_string = false;
        return;
    }

    if(strcmp(name, "AutoType") == 0) {
        ctx->in_autotype = false;
        return;
    }

    if(strcmp(name, "Entry") == 0) {
        if(!ctx->builder_api->end_entry(ctx->builder_api->context, ctx->error)) {
            fp_open_set_error(
                ctx,
                "%s",
                (ctx->error != NULL && !furi_string_empty(ctx->error)) ?
                    furi_string_get_cstr(ctx->error) :
                    "Unable to finalize the entry model.");
        }
        ctx->in_entry = false;
        if(ctx->parsing_depth > 0) {
            ctx->parsing_depth--;
        }
        return;
    }

    if(strcmp(name, "Group") == 0) {
        if(!ctx->builder_api->end_group(ctx->builder_api->context, ctx->error)) {
            fp_open_set_error(
                ctx,
                "%s",
                (ctx->error != NULL && !furi_string_empty(ctx->error)) ?
                    furi_string_get_cstr(ctx->error) :
                    "Unable to finalize the group model.");
        }
        if(ctx->parsing_depth > 0) {
            ctx->parsing_depth--;
        }
        ctx->in_group = ctx->parsing_depth > 0;
    }
}

static void fp_open_character_data(void* context, const char* data, int len) {
    FlipPassOpenContext* ctx = context;
    furi_assert(ctx);

    if(ctx->parse_failed || data == NULL || len <= 0) {
        return;
    }

    if(ctx->skipping_history) {
        if(!ctx->history_collect_protected_value) {
            return;
        }
        if(ctx->protected_discard_active) {
            if(!kdbx_protected_discard_state_update(
                   &ctx->protected_stream, &ctx->protected_discard_state, data, (size_t)len)) {
                fp_open_set_error(ctx, "A protected entry field could not be decoded.");
            }
            return;
        }
        fp_open_append_text_segment(ctx, data, len);
        return;
    }

    if(ctx->text_state == FlipPassOpenTextStateNone) {
        return;
    }

    if(ctx->text_state == FlipPassOpenTextStateStringValue && ctx->deferred_stream_active) {
        if(ctx->value_protected) {
            if(!kdbx_protected_decode_state_update(
                   &ctx->protected_stream,
                   &ctx->protected_discard_state,
                   data,
                   (size_t)len,
                   fp_open_write_streamed_protected_chunk,
                   ctx)) {
                fp_open_abort_streamed_value(ctx);
                fp_open_set_error(ctx, "A protected entry field could not be decoded.");
            }
        } else if(!fp_open_write_streamed_value_chunk(ctx, (const uint8_t*)data, (size_t)len)) {
            fp_open_abort_streamed_value(ctx);
        }
        return;
    }

    if(ctx->text_state == FlipPassOpenTextStateStringValue && ctx->protected_discard_active) {
        if(!kdbx_protected_discard_state_update(
               &ctx->protected_stream, &ctx->protected_discard_state, data, (size_t)len)) {
            fp_open_set_error(ctx, "A protected entry field could not be decoded.");
        }
        return;
    }

    if(!fp_open_append_text_segment(ctx, data, len)) {
        return;
    }

    if(ctx->text_state == FlipPassOpenTextStateStringValue) {
        fp_open_maybe_spill_buffered_value(ctx);
    }
}

static bool fp_open_finish_inner_header(FlipPassOpenContext* ctx) {
    if(ctx->protected_stream_id != KDBXProtectedStreamNone) {
        uint8_t material[KDBX_PROTECTED_STREAM_MATERIAL_MAX];
        size_t material_size = 0U;

        if(ctx->protected_stream_key.size == 0U) {
            fp_open_set_error(ctx, "The KDBX inner protected-value key is missing.");
            return false;
        }

        if(ctx->host_api == NULL || ctx->host_api->derive_protected_stream_material == NULL ||
           !ctx->host_api->derive_protected_stream_material(
               ctx->host_api->context,
               ctx->protected_stream_id,
               ctx->protected_stream_key.data,
               ctx->protected_stream_key.size,
               material,
               sizeof(material),
               &material_size,
               ctx->error) ||
           !kdbx_protected_stream_init_prederived(
               &ctx->protected_stream,
               (KDBXProtectedStreamAlgorithm)ctx->protected_stream_id,
               material,
               material_size)) {
            memzero(material, sizeof(material));
            fp_open_set_error(ctx, "Only Salsa20 or ChaCha20 protected values are supported.");
            return false;
        }
        memzero(material, sizeof(material));
        fp_open_byte_buffer_free(&ctx->protected_stream_key);
    }

    ctx->inner_header_done = true;
    fp_open_log(
        ctx,
        ctx->protected_stream_id != KDBXProtectedStreamNone ? "INNER_HEADER_DONE protected=1" :
                                                              "INNER_HEADER_DONE protected=0");
    return true;
}

static bool fp_open_consume_inner_header(
    FlipPassOpenContext* ctx,
    const uint8_t* data,
    size_t data_size,
    size_t* consumed) {
    furi_assert(ctx);
    furi_assert(consumed);

    *consumed = 0U;
    while(*consumed < data_size && !ctx->inner_header_done && !ctx->parse_failed) {
        if(ctx->inner_header_prefix_len == 0U && ctx->inner_field_remaining == 0U &&
           ctx->inner_field_id == 0U && (data[*consumed] == '<' || data[*consumed] == 0xEFU)) {
            return fp_open_finish_inner_header(ctx);
        }

        if(ctx->inner_field_remaining == 0U &&
           ctx->inner_header_prefix_len < sizeof(ctx->inner_header_prefix)) {
            ctx->inner_header_prefix[ctx->inner_header_prefix_len++] = data[*consumed];
            (*consumed)++;
            if(ctx->inner_header_prefix_len < sizeof(ctx->inner_header_prefix)) {
                continue;
            }

            ctx->inner_field_id = ctx->inner_header_prefix[0];
            ctx->inner_field_size = ((uint32_t)ctx->inner_header_prefix[1]) |
                                    ((uint32_t)ctx->inner_header_prefix[2] << 8) |
                                    ((uint32_t)ctx->inner_header_prefix[3] << 16) |
                                    ((uint32_t)ctx->inner_header_prefix[4] << 24);
            ctx->inner_field_remaining = ctx->inner_field_size;
            ctx->inner_header_prefix_len = 0U;
            if(ctx->inner_field_id == 1U) {
                ctx->protected_stream_id = 0U;
            } else if(ctx->inner_field_id == 2U) {
                fp_open_byte_buffer_free(&ctx->protected_stream_key);
            }
        }

        const size_t available = data_size - *consumed;
        const size_t take = (available < ctx->inner_field_remaining) ? available :
                                                                       ctx->inner_field_remaining;
        if(ctx->inner_field_id == 2U && take > 0U &&
           !fp_open_byte_buffer_append(&ctx->protected_stream_key, data + *consumed, take)) {
            fp_open_set_error(ctx, "Not enough RAM is available to parse the XML payload.");
            return false;
        }
        if(ctx->inner_field_id == 1U && ctx->inner_field_size == 4U && take > 0U) {
            const size_t field_offset = ctx->inner_field_size - ctx->inner_field_remaining;
            for(size_t index = 0U; index < take; ++index) {
                ctx->protected_stream_id |= ((uint32_t)data[*consumed + index])
                                            << ((field_offset + index) * 8U);
            }
        }

        *consumed += take;
        ctx->inner_field_remaining -= take;
        if(ctx->inner_field_remaining != 0U) {
            continue;
        }

        if(ctx->inner_field_id == 0U) {
            return fp_open_finish_inner_header(ctx);
        }

        ctx->inner_field_id = 0U;
        ctx->inner_field_size = 0U;
    }

    return !ctx->parse_failed;
}

static bool fp_open_payload_chunk_callback(const uint8_t* data, size_t data_size, void* context) {
    FlipPassOpenContext* ctx = context;
    size_t consumed = 0U;
    char message[80];

    furi_assert(ctx);

    if(data == NULL) {
        return data_size == 0U;
    }

    ctx->stream_chunk_count++;
    if(ctx->stream_chunk_count <= 8U) {
        snprintf(
            message,
            sizeof(message),
            "STAGED_XML_CHUNK idx=%lu size=%lu inner=%u xml=%lu",
            (unsigned long)ctx->stream_chunk_count,
            (unsigned long)data_size,
            ctx->inner_header_done ? 1U : 0U,
            (unsigned long)ctx->xml_bytes);
        fp_open_log(ctx, message);
    }

    while(consumed < data_size) {
        if(!ctx->inner_header_done) {
            size_t inner_consumed = 0U;
            if(!fp_open_consume_inner_header(
                   ctx, data + consumed, data_size - consumed, &inner_consumed)) {
                return false;
            }
            consumed += inner_consumed;
            continue;
        }

        const size_t xml_chunk = data_size - consumed;
        if(ctx->xml_bytes > (FLIPPASS_OPEN_MAX_XML_STREAM_BYTES - xml_chunk)) {
            fp_open_set_error(ctx, "The XML payload exceeds FlipPass's streaming limit.");
            return false;
        }

        ctx->xml_bytes += xml_chunk;
        const bool first_xml_chunk = (ctx->xml_bytes == xml_chunk);
        if(first_xml_chunk) {
            fp_open_progress(ctx, "Modeling", "", ctx->xml_total_bytes_hint > 0U ? 82U : 70U);
        }

        if(ctx->xml_total_bytes_hint > 0U) {
            char detail[32];
            uint32_t stage_percent =
                (uint32_t)((ctx->xml_bytes * 100U) / ctx->xml_total_bytes_hint);
            uint8_t percent =
                (uint8_t)(82U + ((ctx->xml_bytes * 16U) / ctx->xml_total_bytes_hint));

            if(percent > 98U) {
                percent = 98U;
            }
            if(stage_percent > 100U) {
                stage_percent = 100U;
            }

            if(percent > ctx->progress_percent || first_xml_chunk) {
                snprintf(detail, sizeof(detail), "Payload %lu%%", (unsigned long)stage_percent);
                fp_open_progress(ctx, "Modeling", detail, percent);
            }
        }

        if(!xml_parser_feed(ctx->xml_parser, (const char*)(data + consumed), xml_chunk, false)) {
            fp_open_set_error(
                ctx,
                "%s",
                xml_parser_get_last_error(ctx->xml_parser) != NULL ?
                    xml_parser_get_last_error(ctx->xml_parser) :
                    "The XML payload could not be parsed.");
            return false;
        }

        consumed += xml_chunk;
    }

    return !ctx->parse_failed;
}

static void fp_open_context_reset(FlipPassOpenContext* ctx) {
    if(ctx == NULL) {
        return;
    }

    if(ctx->deferred_stream_active) {
        fp_open_abort_streamed_value(ctx);
    }
    if(ctx->xml_parser != NULL) {
        xml_parser_free(ctx->xml_parser);
    }
    if(ctx->text_value != NULL) {
        furi_string_free(ctx->text_value);
    }
    if(ctx->string_key != NULL) {
        furi_string_free(ctx->string_key);
    }
    fp_open_byte_buffer_free(&ctx->protected_stream_key);
    fp_open_byte_buffer_free(&ctx->protected_value_buffer);
    kdbx_protected_stream_reset(&ctx->protected_stream);
    memzero(ctx, sizeof(*ctx));
}

static FlipPassOpenModelState* fp_open_model_state_alloc(void) {
    FlipPassOpenModelState* state = malloc(sizeof(*state));
    if(state == NULL) {
        return NULL;
    }

    memset(state, 0, sizeof(*state));
    return state;
}

static void fp_open_model_state_free(FlipPassOpenModelState* state) {
    if(state == NULL) {
        return;
    }

    fp_open_context_reset(&state->ctx);
    memzero(state, sizeof(*state));
    free(state);
}

static bool
    fp_open_run_finalize(FlipPassOpenContext* ctx, const FlipPassOpenBuilderApiV1* builder_api) {
    furi_assert(ctx);
    furi_assert(builder_api);

    if(!ctx->inner_header_done) {
        furi_string_set_str(ctx->error, "The KDBX inner header could not be parsed.");
        return false;
    }

    fp_open_progress(ctx, "Finalizing", "", 99U);
    if(!xml_parser_feed(ctx->xml_parser, NULL, 0U, true)) {
        furi_string_set_str(
            ctx->error,
            xml_parser_get_last_error(ctx->xml_parser) != NULL ?
                xml_parser_get_last_error(ctx->xml_parser) :
                "The XML payload could not be parsed.");
        return false;
    }

    if(ctx->parse_failed) {
        furi_string_set_str(ctx->error, ctx->parse_error);
        return false;
    }

    if(ctx->group_count == 0U) {
        furi_string_set_str(ctx->error, "The decrypted XML payload did not contain any groups.");
        return false;
    }

    if(!builder_api->finish_session(
           builder_api->context, ctx->group_count, ctx->entry_count, ctx->error)) {
        if(furi_string_empty(ctx->error)) {
            furi_string_set_str(ctx->error, "Unable to finalize the unlocked database.");
        }
        return false;
    }

    return true;
}

static bool fp_open_model_run(
    const FlipPassOpenModelRequestV1* request,
    const FlipPassOpenModelHostApiV1* host_api,
    const FlipPassOpenBuilderApiV1* builder_api,
    FuriString* error) {
    FlipPassOpenModelState* state = NULL;
    FlipPassOpenContext* ctx = NULL;
    bool ok = false;

    if(request == NULL || host_api == NULL || builder_api == NULL || error == NULL ||
       request->api_version != FLIPPASS_OPEN_MODEL_PLUGIN_API_VERSION ||
       host_api->api_version != FLIPPASS_OPEN_MODEL_HOST_API_VERSION ||
       builder_api->api_version != FLIPPASS_OPEN_MODEL_BUILDER_API_VERSION ||
       host_api->stream_staged_xml == NULL || host_api->derive_protected_stream_material == NULL ||
       builder_api->begin_session == NULL || builder_api->cancel_session == NULL ||
       builder_api->begin_group == NULL || builder_api->end_group == NULL ||
       builder_api->begin_entry == NULL || builder_api->end_entry == NULL ||
       builder_api->set_group_name == NULL || builder_api->set_group_uuid == NULL ||
       builder_api->set_entry_title == NULL || builder_api->set_entry_uuid == NULL ||
       builder_api->set_entry_standard_field == NULL || builder_api->add_custom_field == NULL ||
       builder_api->should_stream_string_value == NULL ||
       builder_api->prepare_string_value_stream == NULL ||
       builder_api->begin_streamed_value == NULL ||
       builder_api->write_streamed_value_chunk == NULL ||
       builder_api->commit_streamed_value == NULL || builder_api->abort_streamed_value == NULL ||
       builder_api->finish_session == NULL) {
        furi_string_set_str(error, "Open model ABI is unavailable or incompatible.");
        return false;
    }

    state = fp_open_model_state_alloc();
    if(state == NULL) {
        furi_string_set_str(
            error, "Not enough RAM is available to start unlocking this database.");
        return false;
    }

    ctx = &state->ctx;
    ctx->host_api = host_api;
    ctx->builder_api = builder_api;
    ctx->error = error;
    ctx->xml_total_bytes_hint = request->staged_payload_plain_size;
    ctx->xml_parser = xml_parser_alloc();
    ctx->text_value = furi_string_alloc();
    ctx->string_key = furi_string_alloc();
    if(ctx->text_value != NULL) {
        furi_string_reserve(ctx->text_value, 128U);
    }
    if(ctx->string_key != NULL) {
        furi_string_reserve(ctx->string_key, 32U);
    }

    if(ctx->xml_parser == NULL || ctx->text_value == NULL || ctx->string_key == NULL) {
        furi_string_set_str(
            error, "Not enough RAM is available to start unlocking this database.");
        goto cleanup;
    }

    if(!builder_api->begin_session(
           builder_api->context,
           request->requested_backend == KDBXVaultBackendNone ? KDBXVaultBackendRam :
                                                                request->requested_backend,
           request->allow_ext_promotion,
           error)) {
        if(furi_string_empty(error)) {
            furi_string_set_str(error, "Unable to allocate the session model.");
        }
        goto cleanup;
    }

    xml_parser_set_callback_context(ctx->xml_parser, ctx);
    xml_parser_set_element_handlers(ctx->xml_parser, fp_open_start_element, fp_open_end_element);
    xml_parser_set_character_data_handler(ctx->xml_parser, fp_open_character_data);

    fp_open_log(ctx, "OPEN_STAGE model");
    if(!host_api->stream_staged_xml(
           host_api->context, fp_open_payload_chunk_callback, ctx, error)) {
        if(ctx->parse_failed && ctx->parse_error[0] != '\0') {
            furi_string_set_str(error, ctx->parse_error);
        } else if(furi_string_empty(error)) {
            furi_string_set_str(error, "Unable to replay the staged XML payload.");
        }
        goto cleanup;
    }

    if(!fp_open_run_finalize(ctx, builder_api)) {
        goto cleanup;
    }

    ok = true;

cleanup:
    if(!ok && builder_api->cancel_session != NULL) {
        builder_api->cancel_session(builder_api->context);
    }
    fp_open_model_state_free(state);
    return ok;
}

static const FlipPassOpenModelPluginV1 flippass_open_model_plugin = {
    .api_version = FLIPPASS_OPEN_MODEL_PLUGIN_API_VERSION,
    .run = fp_open_model_run,
};

static const FlipperAppPluginDescriptor flippass_open_model_descriptor = {
    .appid = FLIPPASS_OPEN_MODEL_PLUGIN_APP_ID,
    .ep_api_version = FLIPPASS_OPEN_MODEL_PLUGIN_API_VERSION,
    .entry_point = &flippass_open_model_plugin,
};

const FlipperAppPluginDescriptor* flippass_open_model_plugin_ep(void) {
    return &flippass_open_model_descriptor;
}
