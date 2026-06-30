#include "../ami_tool_i.h"

#include <nfc/nfc_poller.h>
#include <nfc/protocols/iso14443_3a/iso14443_3a_poller.h>
#include <string.h>

#define AMI_TOOL_WRITE_COMPLETE_EVENT (1UL << 0)
#define AMI_TOOL_UID_COMPARE_LEN (7U)
#define AMI_TOOL_REQUIRED_MAX_PAGE (134U)

typedef struct {
    AmiToolApp* app;
    FuriThreadId thread_id;
    AmiToolWriteStatus status;
    MfUltralightError io_error;
    uint16_t failed_page;
} AmiToolCustomWriteContext;

static bool ami_tool_write_uid_matches(AmiToolCustomWriteContext* ctx, MfUltralightPoller* poller) {
    furi_check(ctx);
    furi_check(poller);

    MfUltralightPageReadCommandData page_data = {};
    MfUltralightError error = mf_ultralight_poller_read_page(poller, 0, &page_data);
    if(error != MfUltralightErrorNone) {
        ctx->status = AmiToolWriteStatusIoError;
        ctx->io_error = error;
        return false;
    }

    uint8_t uid[AMI_TOOL_UID_COMPARE_LEN];
    uid[0] = page_data.page[0].data[0];
    uid[1] = page_data.page[0].data[1];
    uid[2] = page_data.page[0].data[2];
    uid[3] = page_data.page[1].data[0];
    uid[4] = page_data.page[1].data[1];
    uid[5] = page_data.page[1].data[2];
    uid[6] = page_data.page[1].data[3];

    return memcmp(uid, ctx->app->last_uid, AMI_TOOL_UID_COMPARE_LEN) == 0;
}

static bool ami_tool_write_page_or_fail(
    AmiToolCustomWriteContext* ctx,
    MfUltralightPoller* poller,
    uint16_t page) {
    furi_check(ctx);
    furi_check(poller);

    MfUltralightError error =
        mf_ultralight_poller_write_page(poller, (uint8_t)page, &ctx->app->tag_data->page[page]);
    if(error != MfUltralightErrorNone) {
        ctx->status = AmiToolWriteStatusIoError;
        ctx->io_error = error;
        ctx->failed_page = page;
        return false;
    }

    return true;
}

static NfcCommand ami_tool_write_custom_callback(NfcGenericEventEx event, void* context) {
    furi_check(context);
    furi_check(event.poller);
    furi_check(event.parent_event_data);

    AmiToolCustomWriteContext* ctx = context;
    MfUltralightPoller* poller = (MfUltralightPoller*)event.poller;
    Iso14443_3aPollerEvent* iso_event = (Iso14443_3aPollerEvent*)event.parent_event_data;

    if(iso_event->type == Iso14443_3aPollerEventTypeError) {
        ctx->status = AmiToolWriteStatusIoError;
        ctx->io_error = MfUltralightErrorProtocol;
        furi_thread_flags_set(ctx->thread_id, AMI_TOOL_WRITE_COMPLETE_EVENT);
        return NfcCommandStop;
    }

    if(iso_event->type != Iso14443_3aPollerEventTypeReady) {
        return NfcCommandContinue;
    }

    MfUltralightVersion version = {};
    MfUltralightError error = mf_ultralight_poller_read_version(poller, &version);
    if(error != MfUltralightErrorNone) {
        ctx->status = AmiToolWriteStatusIoError;
        ctx->io_error = error;
        furi_thread_flags_set(ctx->thread_id, AMI_TOOL_WRITE_COMPLETE_EVENT);
        return NfcCommandStop;
    }

    MfUltralightType type = mf_ultralight_get_type_by_version(&version);
    if(type != MfUltralightTypeNTAG215) {
        ctx->status = AmiToolWriteStatusNotNtag215;
        furi_thread_flags_set(ctx->thread_id, AMI_TOOL_WRITE_COMPLETE_EVENT);
        return NfcCommandStop;
    }

    if(!ami_tool_write_uid_matches(ctx, poller)) {
        if(ctx->status != AmiToolWriteStatusIoError) {
            ctx->status = AmiToolWriteStatusUidMismatch;
        }
        furi_thread_flags_set(ctx->thread_id, AMI_TOOL_WRITE_COMPLETE_EVENT);
        return NfcCommandStop;
    }

    for(uint16_t page = 4; page <= 129; page++) {
        if(!ami_tool_write_page_or_fail(ctx, poller, page)) {
            furi_thread_flags_set(ctx->thread_id, AMI_TOOL_WRITE_COMPLETE_EVENT);
            return NfcCommandStop;
        }
    }

    const uint16_t trailing_order[] = {134, 133, 3, 131, 132, 130, 2};
    for(size_t i = 0; i < COUNT_OF(trailing_order); i++) {
        if(!ami_tool_write_page_or_fail(ctx, poller, trailing_order[i])) {
            furi_thread_flags_set(ctx->thread_id, AMI_TOOL_WRITE_COMPLETE_EVENT);
            return NfcCommandStop;
        }
    }

    ctx->status = AmiToolWriteStatusOk;
    furi_thread_flags_set(ctx->thread_id, AMI_TOOL_WRITE_COMPLETE_EVENT);
    return NfcCommandStop;
}

const char* ami_tool_write_status_to_string(AmiToolWriteStatus status) {
    switch(status) {
    case AmiToolWriteStatusOk:
        return "Write completed.";
    case AmiToolWriteStatusInvalidArgs:
        return "Invalid write arguments.";
    case AmiToolWriteStatusNoTagData:
        return "No source tag data available.";
    case AmiToolWriteStatusNoUid:
        return "No cached UID available for matching.";
    case AmiToolWriteStatusOutOfRange:
        return "Source tag data does not include all required pages.";
    case AmiToolWriteStatusNotNtag215:
        return "Detected tag is not an NTAG215.";
    case AmiToolWriteStatusUidMismatch:
        return "Detected tag UID does not match cached UID.";
    case AmiToolWriteStatusIoError:
        return "NFC I/O error during write operation.";
    default:
        return "Unknown write status.";
    }
}

AmiToolWriteStatus ami_tool_write_custom_sequence(
    AmiToolApp* app,
    MfUltralightError* io_error,
    uint16_t* failed_page) {
    if(io_error) {
        *io_error = MfUltralightErrorNone;
    }
    if(failed_page) {
        *failed_page = UINT16_MAX;
    }

    if(!app || !app->nfc) {
        return AmiToolWriteStatusInvalidArgs;
    }
    if(!app->tag_data || !app->tag_data_valid) {
        return AmiToolWriteStatusNoTagData;
    }
    if(!app->last_uid_valid || app->last_uid_len < AMI_TOOL_UID_COMPARE_LEN) {
        return AmiToolWriteStatusNoUid;
    }
    if(app->tag_data->pages_total <= AMI_TOOL_REQUIRED_MAX_PAGE) {
        return AmiToolWriteStatusOutOfRange;
    }

    AmiToolCustomWriteContext context = {
        .app = app,
        .thread_id = furi_thread_get_current_id(),
        .status = AmiToolWriteStatusIoError,
        .io_error = MfUltralightErrorNone,
        .failed_page = UINT16_MAX,
    };

    NfcPoller* poller = nfc_poller_alloc(app->nfc, NfcProtocolMfUltralight);
    if(!poller) {
        return AmiToolWriteStatusIoError;
    }

    nfc_poller_start_ex(poller, ami_tool_write_custom_callback, &context);
    furi_thread_flags_wait(AMI_TOOL_WRITE_COMPLETE_EVENT, FuriFlagWaitAny, FuriWaitForever);
    furi_thread_flags_clear(AMI_TOOL_WRITE_COMPLETE_EVENT);

    nfc_poller_stop(poller);
    nfc_poller_free(poller);

    if(io_error) {
        *io_error = context.io_error;
    }
    if(failed_page) {
        *failed_page = context.failed_page;
    }

    return context.status;
}
