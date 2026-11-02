#include "fn_status.h"

#include "fn_wire.h"

#include <string.h>

#define FN_STATUS_30H_LEN 30u
#define FN_STATUS_30H_SERIAL_OFFSET 10u
#define FN_STATUS_30H_SERIAL_LEN 16u
#define FN_STATUS_30H_LAST_DOC_NUMBER_OFFSET 26u
#define FN_SESSION_10H_LEN 5u

static FnStage fn_stage_from_raw(uint8_t raw) {
    switch(raw) {
    case FnStageReadyRegistration:
    case FnStageRegistered:
    case FnStageArchiveClosed:
    case FnStageArchiveClosedFinal:
        return (FnStage)raw;
    default:
        return FnStageUnknown;
    }
}

static FnDocCode fn_doc_from_raw(uint8_t raw) {
    switch(raw) {
    case FnDocNone:
    case FnDocRegistration:
    case FnDocSessionOpen:
    case FnDocReceipt:
    case FnDocSessionClose:
    case FnDocFnClose:
    case FnDocStrictReport:
    case FnDocRegChangeFnReplace:
    case FnDocRegChange:
    case FnDocReceiptCorrection:
    case FnDocStrictReportCorrection:
    case FnDocCurrentSettlements:
        return (FnDocCode)raw;
    default:
        return FnDocUnknown;
    }
}

bool fn_parse_status_30h(const uint8_t* data, size_t len, FnStatus* out) {
    if(!data || !out || len < FN_STATUS_30H_LEN) return false;

    FnStatus status;
    memset(&status, 0, sizeof(status));

    status.stage_raw = data[0];
    status.stage = fn_stage_from_raw(data[0]);
    status.current_doc_raw = data[1];
    status.current_doc = fn_doc_from_raw(data[1]);
    status.doc_data_received = data[2] != 0;
    status.session_open = data[3] != 0;
    status.warn_flags = data[4];

    status.last_doc_dt.date.year = data[5];
    status.last_doc_dt.date.month = data[6];
    status.last_doc_dt.date.day = data[7];
    status.last_doc_dt.hour = data[8];
    status.last_doc_dt.minute = data[9];

    for(size_t i = 0; i < FN_STATUS_30H_SERIAL_LEN; ++i) {
        status.serial[i] = (char)data[FN_STATUS_30H_SERIAL_OFFSET + i];
    }
    status.serial[FN_STATUS_30H_SERIAL_LEN] = '\0';
    status.has_serial = true;

    status.last_doc_number = fn_wire_u32le_get(&data[FN_STATUS_30H_LAST_DOC_NUMBER_OFFSET]);

    const bool date_valid = fn_datetime_is_valid(&status.last_doc_dt);
    status.has_last_doc = date_valid;
    status.has_last_doc_number = date_valid;

    *out = status;
    return true;
}

bool fn_parse_session_10h(const uint8_t* data, size_t len, FnSession* out) {
    if(!data || !out || len != FN_SESSION_10H_LEN) return false;

    FnSession session;
    session.session_open = data[0] != 0;
    session.shift_number = fn_wire_u16le_get(&data[1]);
    session.receipt_number = fn_wire_u16le_get(&data[3]);

    *out = session;
    return true;
}
