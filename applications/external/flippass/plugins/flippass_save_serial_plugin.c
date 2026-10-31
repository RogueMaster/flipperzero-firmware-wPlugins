#include "flippass_save_serial_plugin.h"

#include "../kdbx/memzero.h"

#include <datetime.h>
#include <furi_hal_random.h>

#include <stdlib.h>
#include <string.h>

#define FLIPPASS_KDBX_EPOCH_DELTA_SECONDS             62135596800ULL
#define FPSER_PROTECTED_STREAM_CHACHA20               3U
#define FPSER_PROTECTED_STREAM_CHACHA20_MATERIAL_SIZE (32U + 12U)
#define FPSER_PROTECTED_STREAM_MATERIAL_MAX           64U

typedef struct {
    const FlipPassSaveSerialRequestV1* request;
    const FlipPassSaveSerialHostApiV1* host_api;
    FuriString* uuid;
    char now_base64[16];
    FlipPassCryptoProtectedState protected_stream;
} FlipPassSaveSerialCtx;

typedef bool (*FpSerialChunkCb)(const uint8_t* data, size_t data_size, void* context);

typedef struct {
    FlipPassSaveSerialCtx* ctx;
    FpSerialChunkCb callback;
    void* callback_context;
    FuriString* error;
    bool protected_value;
    uint8_t triplet[3];
    size_t triplet_len;
} FpSerialValueStream;

/* ---------- write helper ---------- */

static bool fpser_emit(FpSerialChunkCb cb, void* cbctx, const void* data, size_t size) {
    if(size == 0U) return true;
    return cb(data, size, cbctx);
}

static bool fpser_emit_cstr(FpSerialChunkCb cb, void* cbctx, const char* text) {
    furi_assert(text);
    return fpser_emit(cb, cbctx, text, strlen(text));
}

static bool fpser_emit_xml_escaped_bytes(
    FpSerialChunkCb cb,
    void* cbctx,
    const uint8_t* data,
    size_t size) {
    size_t start = 0U;
    for(size_t i = 0U; i < size; i++) {
        const char* esc = NULL;
        switch((char)data[i]) {
        case '&':
            esc = "&amp;";
            break;
        case '<':
            esc = "&lt;";
            break;
        case '>':
            esc = "&gt;";
            break;
        case '"':
            esc = "&quot;";
            break;
        case '\'':
            esc = "&apos;";
            break;
        default:
            break;
        }
        if(esc) {
            if(i > start && !fpser_emit(cb, cbctx, data + start, i - start)) return false;
            if(!fpser_emit_cstr(cb, cbctx, esc)) return false;
            start = i + 1U;
        }
    }
    return start == size || fpser_emit(cb, cbctx, data + start, size - start);
}

static bool fpser_emit_xml_escaped(FpSerialChunkCb cb, void* cbctx, const char* text) {
    const char* value = text != NULL ? text : "";
    return fpser_emit_xml_escaped_bytes(cb, cbctx, (const uint8_t*)value, strlen(value));
}

/* ---------- base64 ---------- */

static bool fpser_base64(const uint8_t* src, size_t src_size, char* out, size_t out_size) {
    static const char alpha[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    size_t oi = 0U;
    for(size_t i = 0U; i < src_size; i += 3U) {
        const size_t rem = src_size - i;
        const uint32_t tri = ((uint32_t)src[i] << 16) | ((rem > 1U ? src[i + 1U] : 0U) << 8) |
                             (rem > 2U ? src[i + 2U] : 0U);
        if((oi + 4U) >= out_size) return false;
        out[oi++] = alpha[(tri >> 18) & 0x3FU];
        out[oi++] = alpha[(tri >> 12) & 0x3FU];
        out[oi++] = (rem > 1U) ? alpha[(tri >> 6) & 0x3FU] : '=';
        out[oi++] = (rem > 2U) ? alpha[tri & 0x3FU] : '=';
    }
    if(oi >= out_size) return false;
    out[oi] = '\0';
    return true;
}

static bool fpser_build_time_base64(char out[16]) {
    DateTime now;
    uint8_t raw[8];
    uint64_t seconds;
    furi_hal_rtc_get_datetime(&now);
    seconds = (uint64_t)datetime_datetime_to_timestamp(&now) + FLIPPASS_KDBX_EPOCH_DELTA_SECONDS;
    for(size_t i = 0U; i < sizeof(raw); i++) {
        raw[i] = (uint8_t)((seconds >> (i * 8U)) & 0xFFU);
    }
    return fpser_base64(raw, sizeof(raw), out, 16U);
}

/* ---------- protected value stream ---------- */

static void fpser_encode_protected_triplet(FpSerialValueStream* s, char q[4]) {
    static const char alpha[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    furi_assert(s && s->triplet_len == 3U);
    const uint32_t tri = ((uint32_t)s->triplet[0] << 16) | ((uint32_t)s->triplet[1] << 8) |
                         (uint32_t)s->triplet[2];
    q[0] = alpha[(tri >> 18) & 0x3FU];
    q[1] = alpha[(tri >> 12) & 0x3FU];
    q[2] = alpha[(tri >> 6) & 0x3FU];
    q[3] = alpha[tri & 0x3FU];
    s->triplet_len = 0U;
    memzero(s->triplet, sizeof(s->triplet));
}

static bool fpser_value_chunk(const uint8_t* data, size_t size, void* context) {
    FpSerialValueStream* s = context;
    if(s == NULL || s->ctx == NULL || s->callback == NULL) return false;
    if(size == 0U) return true;

    if(!s->protected_value) {
        if(!fpser_emit_xml_escaped_bytes(s->callback, s->callback_context, data, size)) {
            furi_string_set_str(s->error, "FlipPass could not serialize a KDBX field.");
            return false;
        }
        return true;
    }

    /* 48 input bytes plus at most two carried bytes produce at most 64
     * complete Base64 bytes. Scratch remains fixed-size for any field length. */
    for(size_t pos = 0U; pos < size;) {
        uint8_t block[48];
        char encoded[64];
        size_t count = size - pos;
        if(count > sizeof(block)) count = sizeof(block);
        memcpy(block, data + pos, count);
        if(s->ctx->host_api->crypto == NULL || !s->ctx->host_api->crypto->protected_stream_apply(
                                                   &s->ctx->protected_stream, block, count)) {
            memzero(block, sizeof(block));
            furi_string_set_str(s->error, "FlipPass could not protect a KDBX field.");
            return false;
        }
        size_t encoded_len = 0U;
        for(size_t i = 0U; i < count; ++i) {
            s->triplet[s->triplet_len++] = block[i];
            if(s->triplet_len == 3U) {
                fpser_encode_protected_triplet(s, encoded + encoded_len);
                encoded_len += 4U;
            }
        }
        memzero(block, sizeof(block));
        const bool ok = fpser_emit(s->callback, s->callback_context, encoded, encoded_len);
        memzero(encoded, sizeof(encoded));
        if(!ok) {
            furi_string_set_str(s->error, "FlipPass could not serialize a protected field.");
            return false;
        }
        pos += count;
    }
    return true;
}

static bool fpser_value_stream_finish(FpSerialValueStream* s) {
    static const char alpha[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    char q[4];
    if(s == NULL || !s->protected_value || s->triplet_len == 0U) return true;
    const uint32_t tri = ((uint32_t)s->triplet[0] << 16) |
                         ((s->triplet_len > 1U ? s->triplet[1] : 0U) << 8) |
                         (s->triplet_len > 2U ? s->triplet[2] : 0U);
    q[0] = alpha[(tri >> 18) & 0x3FU];
    q[1] = alpha[(tri >> 12) & 0x3FU];
    q[2] = (s->triplet_len > 1U) ? alpha[(tri >> 6) & 0x3FU] : '=';
    q[3] = (s->triplet_len > 2U) ? alpha[tri & 0x3FU] : '=';
    s->triplet_len = 0U;
    memzero(s->triplet, sizeof(s->triplet));
    return fpser_emit(s->callback, s->callback_context, q, sizeof(q));
}

static bool fpser_emit_ref_value(
    FlipPassSaveSerialCtx* ctx,
    FpSerialChunkCb cb,
    void* cbctx,
    const KDBXFieldRef* ref,
    bool protected_value,
    FuriString* error) {
    FpSerialValueStream s = {
        .ctx = ctx,
        .callback = cb,
        .callback_context = cbctx,
        .error = error,
        .protected_value = protected_value,
    };

    if(ref == NULL) {
        furi_string_set_str(error, "FlipPass could not locate a deferred field.");
        return false;
    }

    if(!ctx->host_api->stream_ref(ctx->host_api->context, ref, fpser_value_chunk, &s, error)) {
        if(furi_string_empty(error)) {
            furi_string_set_str(error, "FlipPass could not read a deferred field.");
        }
        return false;
    }

    return fpser_value_stream_finish(&s);
}

/* ---------- field ref helper — inlined to avoid linking kdbx_data.c ---------- */

static const KDBXFieldRef* fpser_entry_field_ref(const KDBXEntry* entry, uint32_t field_mask) {
    if(entry == NULL) return NULL;
    switch(field_mask) {
    case KDBXEntryFieldUuid:
        return &entry->uuid_ref;
    case KDBXEntryFieldTitle:
        return &entry->title_ref;
    case KDBXEntryFieldUsername:
        return &entry->username_ref;
    case KDBXEntryFieldPassword:
        return &entry->password_ref;
    case KDBXEntryFieldUrl:
        return &entry->url_ref;
    case KDBXEntryFieldNotes:
        return &entry->notes_ref;
    case KDBXEntryFieldAutotype:
        return &entry->autotype_ref;
    default:
        return NULL;
    }
}

/* ---------- XML structural emitters ---------- */

static bool fpser_emit_times(FlipPassSaveSerialCtx* ctx, FpSerialChunkCb cb, void* cbctx) {
    return fpser_emit_cstr(cb, cbctx, "<Times><LastModificationTime>") &&
           fpser_emit_cstr(cb, cbctx, ctx->now_base64) &&
           fpser_emit_cstr(cb, cbctx, "</LastModificationTime><CreationTime>") &&
           fpser_emit_cstr(cb, cbctx, ctx->now_base64) &&
           fpser_emit_cstr(cb, cbctx, "</CreationTime><LastAccessTime>") &&
           fpser_emit_cstr(cb, cbctx, ctx->now_base64) &&
           fpser_emit_cstr(cb, cbctx, "</LastAccessTime><ExpiryTime>") &&
           fpser_emit_cstr(cb, cbctx, ctx->now_base64) &&
           fpser_emit_cstr(
               cb,
               cbctx,
               "</ExpiryTime><Expires>False</Expires><UsageCount>0</UsageCount><LocationChanged>") &&
           fpser_emit_cstr(cb, cbctx, ctx->now_base64) &&
           fpser_emit_cstr(cb, cbctx, "</LocationChanged></Times>");
}

static bool fpser_emit_meta(
    FlipPassSaveSerialCtx* ctx,
    FpSerialChunkCb cb,
    void* cbctx,
    const char* database_name) {
    return fpser_emit_cstr(
               cb, cbctx, "<KeePassFile><Meta><Generator>FlipPass</Generator><DatabaseName>") &&
           fpser_emit_xml_escaped(cb, cbctx, database_name) &&
           fpser_emit_cstr(cb, cbctx, "</DatabaseName><DatabaseNameChanged>") &&
           fpser_emit_cstr(cb, cbctx, ctx->now_base64) &&
           fpser_emit_cstr(
               cb,
               cbctx,
               "</DatabaseNameChanged><DatabaseDescription/><DatabaseDescriptionChanged>") &&
           fpser_emit_cstr(cb, cbctx, ctx->now_base64) &&
           fpser_emit_cstr(
               cb,
               cbctx,
               "</DatabaseDescriptionChanged><DefaultUserName/><DefaultUserNameChanged>") &&
           fpser_emit_cstr(cb, cbctx, ctx->now_base64) &&
           fpser_emit_cstr(
               cb,
               cbctx,
               "</DefaultUserNameChanged><MaintenanceHistoryDays>365</MaintenanceHistoryDays>"
               "<Color/><MasterKeyChanged>") &&
           fpser_emit_cstr(cb, cbctx, ctx->now_base64) &&
           fpser_emit_cstr(
               cb,
               cbctx,
               "</MasterKeyChanged><MasterKeyChangeRec>-1</MasterKeyChangeRec>"
               "<MasterKeyChangeForce>-1</MasterKeyChangeForce><MemoryProtection>"
               "<ProtectTitle>False</ProtectTitle><ProtectUserName>False</ProtectUserName>"
               "<ProtectPassword>True</ProtectPassword><ProtectURL>False</ProtectURL>"
               "<ProtectNotes>False</ProtectNotes></MemoryProtection><CustomIcons/>"
               "<RecycleBinEnabled>False</RecycleBinEnabled>"
               "<RecycleBinUUID>AAAAAAAAAAAAAAAAAAAAAA==</RecycleBinUUID>"
               "<RecycleBinChanged>") &&
           fpser_emit_cstr(cb, cbctx, ctx->now_base64) &&
           fpser_emit_cstr(
               cb,
               cbctx,
               "</RecycleBinChanged><EntryTemplatesGroup>AAAAAAAAAAAAAAAAAAAAAA==</EntryTemplatesGroup>"
               "<EntryTemplatesGroupChanged>") &&
           fpser_emit_cstr(cb, cbctx, ctx->now_base64) &&
           fpser_emit_cstr(
               cb,
               cbctx,
               "</EntryTemplatesGroupChanged><LastSelectedGroup>AAAAAAAAAAAAAAAAAAAAAA==</LastSelectedGroup>"
               "<LastTopVisibleGroup>AAAAAAAAAAAAAAAAAAAAAA==</LastTopVisibleGroup>"
               "<HistoryMaxItems>10</HistoryMaxItems><HistoryMaxSize>6291456</HistoryMaxSize>"
               "<SettingsChanged>") &&
           fpser_emit_cstr(cb, cbctx, ctx->now_base64) &&
           fpser_emit_cstr(cb, cbctx, "</SettingsChanged></Meta><Root>");
}

static bool fpser_emit_icon_id(FpSerialChunkCb cb, void* cbctx, uint8_t icon_id) {
    char text[sizeof("<IconID>255</IconID>")] = "<IconID>";
    size_t length = sizeof("<IconID>") - 1U;
    if(icon_id >= 100U) text[length++] = (char)('0' + icon_id / 100U);
    if(icon_id >= 10U) text[length++] = (char)('0' + (icon_id / 10U) % 10U);
    text[length++] = (char)('0' + icon_id % 10U);
    memcpy(&text[length], "</IconID>", sizeof("</IconID>"));
    return fpser_emit_cstr(cb, cbctx, text);
}

static bool fpser_emit_entry_ref_string(
    FlipPassSaveSerialCtx* ctx,
    FpSerialChunkCb cb,
    void* cbctx,
    const char* key,
    const KDBXFieldRef* ref,
    bool protected_value,
    FuriString* error) {
    if(!fpser_emit_cstr(cb, cbctx, "<String><Key>") || !fpser_emit_xml_escaped(cb, cbctx, key) ||
       !fpser_emit_cstr(
           cb, cbctx, protected_value ? "</Key><Value Protected=\"True\">" : "</Key><Value>")) {
        return false;
    }
    if(!fpser_emit_ref_value(ctx, cb, cbctx, ref, protected_value, error)) {
        return false;
    }
    return fpser_emit_cstr(cb, cbctx, "</Value></String>");
}

static bool fpser_emit_entry(
    FlipPassSaveSerialCtx* ctx,
    FpSerialChunkCb cb,
    void* cbctx,
    KDBXEntry* entry,
    FuriString* error) {
    if(!ctx->host_api->copy_entry_uuid(ctx->host_api->context, entry, ctx->uuid, error)) {
        return false;
    }

    if(!fpser_emit_cstr(cb, cbctx, "<Entry><UUID>") ||
       !fpser_emit_xml_escaped(cb, cbctx, furi_string_get_cstr(ctx->uuid)) ||
       !fpser_emit_cstr(cb, cbctx, "</UUID>") || !fpser_emit_icon_id(cb, cbctx, entry->icon_id) ||
       !fpser_emit_times(ctx, cb, cbctx) ||
       !fpser_emit_entry_ref_string(
           ctx,
           cb,
           cbctx,
           "Title",
           fpser_entry_field_ref(entry, KDBXEntryFieldTitle),
           false,
           error)) {
        return false;
    }

    if(ctx->host_api->entry_has_field(ctx->host_api->context, entry, KDBXEntryFieldUsername) &&
       !fpser_emit_entry_ref_string(
           ctx,
           cb,
           cbctx,
           "UserName",
           fpser_entry_field_ref(entry, KDBXEntryFieldUsername),
           false,
           error)) {
        return false;
    }

    if(ctx->host_api->entry_has_field(ctx->host_api->context, entry, KDBXEntryFieldPassword) &&
       !fpser_emit_entry_ref_string(
           ctx,
           cb,
           cbctx,
           "Password",
           fpser_entry_field_ref(entry, KDBXEntryFieldPassword),
           true,
           error)) {
        return false;
    }

    if(ctx->host_api->entry_has_field(ctx->host_api->context, entry, KDBXEntryFieldUrl) &&
       !fpser_emit_entry_ref_string(
           ctx, cb, cbctx, "URL", fpser_entry_field_ref(entry, KDBXEntryFieldUrl), false, error)) {
        return false;
    }

    if(ctx->host_api->entry_has_field(ctx->host_api->context, entry, KDBXEntryFieldNotes) &&
       !fpser_emit_entry_ref_string(
           ctx,
           cb,
           cbctx,
           "Notes",
           fpser_entry_field_ref(entry, KDBXEntryFieldNotes),
           false,
           error)) {
        return false;
    }

    for(KDBXCustomField* f = entry->custom_fields; f != NULL; f = f->next) {
        if(!fpser_emit_entry_ref_string(
               ctx, cb, cbctx, f->key, &f->value_ref, f->protected_value, error)) {
            return false;
        }
    }

    if(ctx->host_api->entry_has_field(ctx->host_api->context, entry, KDBXEntryFieldAutotype)) {
        if(!fpser_emit_cstr(
               cb,
               cbctx,
               "<AutoType><Enabled>True</Enabled><DataTransferObfuscation>0</DataTransferObfuscation>"
               "<DefaultSequence>") ||
           !fpser_emit_ref_value(
               ctx, cb, cbctx, fpser_entry_field_ref(entry, KDBXEntryFieldAutotype), false, error) ||
           !fpser_emit_cstr(
               cb,
               cbctx,
               "</DefaultSequence><Association><Window/><KeystrokeSequence/></Association></AutoType>")) {
            return false;
        }
    }

    return fpser_emit_cstr(cb, cbctx, "</Entry>");
}

static bool fpser_emit_group_start(
    FlipPassSaveSerialCtx* ctx,
    FpSerialChunkCb cb,
    void* cbctx,
    KDBXGroup* group,
    bool root_group,
    FuriString* error) {
    if(!ctx->host_api->copy_group_uuid(ctx->host_api->context, group, ctx->uuid, error) ||
       !fpser_emit_cstr(cb, cbctx, "<Group><UUID>") ||
       !fpser_emit_xml_escaped(cb, cbctx, furi_string_get_cstr(ctx->uuid)) ||
       !fpser_emit_cstr(cb, cbctx, "</UUID>") || !fpser_emit_icon_id(cb, cbctx, group->icon_id)) {
        return false;
    }

    if(root_group) {
        if(!fpser_emit_cstr(cb, cbctx, "<Notes/>") || !fpser_emit_times(ctx, cb, cbctx) ||
           !fpser_emit_cstr(
               cb,
               cbctx,
               "<IsExpanded>True</IsExpanded><DefaultAutoTypeSequence/>"
               "<EnableAutoType>null</EnableAutoType><EnableSearching>null</EnableSearching>"
               "<LastTopVisibleEntry>AAAAAAAAAAAAAAAAAAAAAA==</LastTopVisibleEntry><Name>") ||
           !fpser_emit_xml_escaped(cb, cbctx, group->name != NULL ? group->name : "Root") ||
           !fpser_emit_cstr(cb, cbctx, "</Name>")) {
            return false;
        }
    } else if(
        !fpser_emit_times(ctx, cb, cbctx) || !fpser_emit_cstr(cb, cbctx, "<Name>") ||
        !fpser_emit_xml_escaped(cb, cbctx, group->name != NULL ? group->name : "") ||
        !fpser_emit_cstr(cb, cbctx, "</Name>")) {
        return false;
    }

    return true;
}

static bool fpser_emit_group(
    FlipPassSaveSerialCtx* ctx,
    FpSerialChunkCb cb,
    void* cbctx,
    KDBXGroup* root,
    bool root_group,
    FuriString* error) {
    KDBXGroup* group = root;
    while(group != NULL) {
        if(!fpser_emit_group_start(ctx, cb, cbctx, group, group == root && root_group, error)) {
            return false;
        }
        if(group->children != NULL) {
            group = group->children;
            continue;
        }
        /* Children precede entries in the existing wire format. Ascend only
         * after emitting the entries and closing tag for the current group. */
        while(true) {
            for(KDBXEntry* entry = group->entries; entry != NULL; entry = entry->next) {
                if(!fpser_emit_entry(ctx, cb, cbctx, entry, error)) return false;
            }
            if(!fpser_emit_cstr(cb, cbctx, "</Group>")) return false;
            if(group == root) return true;
            if(group->next != NULL) {
                group = group->next;
                break;
            }
            group = group->parent;
            if(group == NULL) {
                furi_string_set_str(error, "FlipPass group parent link is missing.");
                return false;
            }
        }
    }
    return false;
}

/* ---------- main XML stream ---------- */

typedef struct {
    const FlipPassSaveSerialHostApiV1* host_api;
    FuriString* error;
} FpSerialWriteCtx;

static bool fpser_write_xml_to_host(const uint8_t* data, size_t size, void* context) {
    FpSerialWriteCtx* c = context;
    return c->host_api->write_xml_chunk(c->host_api->context, data, size, c->error);
}

static bool fpser_stream_xml(FlipPassSaveSerialCtx* ctx, FuriString* error) {
    uint8_t inner_key[64];
    uint8_t inner_material[FPSER_PROTECTED_STREAM_MATERIAL_MAX];
    /* KDBX inner random stream field header: type=ChaCha20(3), length=4+1 binary header bytes */
    const uint8_t inner_header_alg[4] = {3U, 0U, 0U, 0U};
    uint8_t inner_field_header[5] = {1U, 4U, 0U, 0U, 0U};
    const char* database_name =
        (ctx->request->database_name != NULL && ctx->request->database_name[0] != '\0') ?
            ctx->request->database_name :
            "Database";

    FpSerialWriteCtx write_ctx = {
        .host_api = ctx->host_api,
        .error = error,
    };
    FpSerialChunkCb cb = fpser_write_xml_to_host;
    void* cbctx = &write_ctx;

    ctx->host_api->crypto->protected_stream_clear(&ctx->protected_stream);

    if(!fpser_build_time_base64(ctx->now_base64)) {
        furi_string_set_str(error, "FlipPass could not encode KDBX timestamp fields.");
        return false;
    }

    furi_hal_random_fill_buf(inner_key, sizeof(inner_key));
    {
        const FlipPassCryptoChunk inner_key_chunk = {
            .data = inner_key,
            .size = sizeof(inner_key),
        };
        if(!ctx->host_api->crypto->sha512_chunks(&inner_key_chunk, 1U, inner_material)) {
            memzero(inner_key, sizeof(inner_key));
            furi_string_set_str(error, "FlipPass could not derive the inner protected stream.");
            return false;
        }
    }

    if(!ctx->host_api->crypto->protected_stream_init_prederived(
           &ctx->protected_stream,
           FPSER_PROTECTED_STREAM_CHACHA20,
           inner_material,
           FPSER_PROTECTED_STREAM_CHACHA20_MATERIAL_SIZE)) {
        memzero(inner_key, sizeof(inner_key));
        memzero(inner_material, sizeof(inner_material));
        furi_string_set_str(error, "FlipPass could not initialize the inner protected stream.");
        return false;
    }
    memzero(inner_material, sizeof(inner_material));

    /* Field 1: InnerRandomStreamID (type=0x01, length=4) */
    if(!fpser_emit(cb, cbctx, inner_field_header, sizeof(inner_field_header)) ||
       !fpser_emit(cb, cbctx, inner_header_alg, sizeof(inner_header_alg))) {
        memzero(inner_key, sizeof(inner_key));
        return false;
    }

    /* Field 2: InnerRandomStreamKey (type=0x02, length=64) */
    inner_field_header[0] = 2U;
    inner_field_header[1] = (uint8_t)sizeof(inner_key);
    inner_field_header[2] = 0U;
    inner_field_header[3] = 0U;
    inner_field_header[4] = 0U;
    if(!fpser_emit(cb, cbctx, inner_field_header, sizeof(inner_field_header)) ||
       !fpser_emit(cb, cbctx, inner_key, sizeof(inner_key))) {
        memzero(inner_key, sizeof(inner_key));
        return false;
    }
    memzero(inner_key, sizeof(inner_key));

    /* Field 3: EndOfHeader (type=0x00, length=0) */
    inner_field_header[0] = 0U;
    inner_field_header[1] = 0U;
    inner_field_header[2] = 0U;
    inner_field_header[3] = 0U;
    inner_field_header[4] = 0U;
    if(!fpser_emit(cb, cbctx, inner_field_header, sizeof(inner_field_header))) {
        return false;
    }

    /* XML body */
    if(!fpser_emit_meta(ctx, cb, cbctx, database_name) ||
       !fpser_emit_group(ctx, cb, cbctx, ctx->request->root_group, true, error) ||
       !fpser_emit_cstr(cb, cbctx, "<DeletedObjects/></Root></KeePassFile>")) {
        if(furi_string_empty(error)) {
            furi_string_set_str(error, "FlipPass could not serialize the editable database.");
        }
        return false;
    }

    return true;
}

/* ---------- plugin entry point ---------- */

static bool flippass_save_serial_run(
    const FlipPassSaveSerialRequestV1* request,
    const FlipPassSaveSerialHostApiV1* host_api,
    FuriString* error) {
    FlipPassSaveSerialCtx ctx;
    bool ok = false;

    if(request == NULL || host_api == NULL || error == NULL) {
        if(error != NULL) {
            furi_string_set_str(error, "FlipPass save serial received an invalid request.");
        }
        return false;
    }

    if(request->api_version != FLIPPASS_SAVE_SERIAL_PLUGIN_API_VERSION ||
       request->root_group == NULL) {
        furi_string_set_str(error, "FlipPass save serial has no database model to serialize.");
        return false;
    }
    if(host_api->api_version != FLIPPASS_SAVE_SERIAL_HOST_API_VERSION ||
       host_api->crypto == NULL || host_api->crypto->api_version != FLIPPASS_CRYPTO_API_VERSION ||
       host_api->crypto->sha512_chunks == NULL ||
       host_api->crypto->protected_stream_init_prederived == NULL ||
       host_api->crypto->protected_stream_apply == NULL ||
       host_api->crypto->protected_stream_clear == NULL || host_api->write_xml_chunk == NULL ||
       host_api->stream_ref == NULL || host_api->copy_group_uuid == NULL ||
       host_api->copy_entry_uuid == NULL || host_api->entry_has_field == NULL) {
        furi_string_set_str(error, "FlipPass save serial ABI is unavailable or incompatible.");
        return false;
    }

    memset(&ctx, 0, sizeof(ctx));
    ctx.request = request;
    ctx.host_api = host_api;

    ctx.uuid = furi_string_alloc();
    if(ctx.uuid == NULL) {
        furi_string_set_str(error, "Not enough RAM for FlipPass save serial.");
        return false;
    }

    ok = fpser_stream_xml(&ctx, error);

    host_api->crypto->protected_stream_clear(&ctx.protected_stream);
    furi_string_free(ctx.uuid);
    memzero(ctx.now_base64, sizeof(ctx.now_base64));
    return ok;
}

static const FlipPassSaveSerialPluginV1 flippass_save_serial_plugin = {
    .api_version = FLIPPASS_SAVE_SERIAL_PLUGIN_API_VERSION,
    .run = flippass_save_serial_run,
};

static const FlipperAppPluginDescriptor flippass_save_serial_descriptor = {
    .appid = FLIPPASS_SAVE_SERIAL_PLUGIN_APP_ID,
    .ep_api_version = FLIPPASS_SAVE_SERIAL_PLUGIN_API_VERSION,
    .entry_point = &flippass_save_serial_plugin,
};

const FlipperAppPluginDescriptor* flippass_save_serial_plugin_ep(void) {
    return &flippass_save_serial_descriptor;
}
