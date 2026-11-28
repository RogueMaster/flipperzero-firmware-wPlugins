#include "seos_inspect.h"

#include "seos_tlv.h"
#include "seos_protocol.h"

#include <string.h>

#define TAG "SeosInspect"

#define SEOS_INSPECT_POLL_TIMEOUT 50

// ATS we answer RATS with: TL, T0 (TA1/TB1/TC1 present, FSCI=8), TA1, TB1,
// TC1 (CID supported). FSCI=8 matches the reader and keeps its response
// fragmentation the same as the standalone tool we ported this from.
static const uint8_t HID_ATS[] = {0x05, 0x78, 0x80, 0x70, 0x02};

// The reader offers the two operation selectors, then passes our commands
// through the application it selects as MOBILE_SEOS_ADMIN_CARD.
static const uint8_t RESP_ENABLE_INSPECTION[] =
    {0x6f, 0x08, 0x85, 0x06, 0x02, 0x01, 0x40, 0x02, 0x01, 0x00};
static const uint8_t SW_INS_NOT_SUPPORTED[] = {0x6d, 0x00};

// IPC commands; a 9000 status word is appended when each is sent.
static const uint8_t CMD_SAM_VER[] = {0x44, 0x0a, 0x44, 0x00, 0x00, 0x00, 0xa0, 0x02, 0x82, 0x00};
static const uint8_t CMD_CORE_VER[] = {0x44, 0x01, 0x44, 0x00, 0x00, 0x00, 0xa5, 0x02, 0x86, 0x00};
static const uint8_t CMD_PERSIST_MEM[] =
    {0x44, 0x0a, 0x44, 0x00, 0x00, 0x00, 0xa0, 0x04, 0xb9, 0x02, 0x8a, 0x00};
static const uint8_t CMD_SUSPEND[] = {0x44, 0x0a, 0x44, 0x00, 0x00, 0x00, 0xa0, 0x02, 0x83, 0x00};

// IPC-framed SNMPv3 GET for the credential table (OID 03 01 07 01 5F). The
// flags and context here are what the reader's SAM accepts over passthrough;
// the textbook RFC form is rejected with authorizationError.
static const uint8_t CMD_SNMP_CRED[] = {
    0x44, 0x0a, 0x44, 0x00, 0x00, 0x00, 0xa0, 0x54, 0x94, 0x52, 0x30, 0x50, 0x02, 0x01, 0x03, 0x30,
    0x0e, 0x02, 0x02, 0x03, 0xe9, 0x02, 0x02, 0x02, 0xf4, 0x04, 0x01, 0x04, 0x02, 0x01, 0x03, 0x04,
    0x18, 0x30, 0x16, 0x04, 0x04, 0x03, 0x01, 0x07, 0x05, 0x02, 0x01, 0x00, 0x02, 0x01, 0x00, 0x04,
    0x04, 0x03, 0x01, 0x07, 0x04, 0x04, 0x00, 0x04, 0x00, 0x30, 0x21, 0x04, 0x04, 0x03, 0x01, 0x07,
    0x05, 0x04, 0x00, 0xa0, 0x17, 0x02, 0x02, 0x03, 0xe9, 0x02, 0x01, 0x00, 0x02, 0x01, 0x00, 0x30,
    0x0b, 0x30, 0x09, 0x06, 0x05, 0x03, 0x01, 0x07, 0x01, 0x5f, 0x05, 0x00};

#define CT_ANY   (-1)
#define PROX_IDX 7

typedef struct {
    const char* name;
    uint32_t hivecode;
    int16_t card_type;
} CredItem;

static const CredItem CRED_ITEMS[SEOS_INSPECT_CRED_COUNT] = {
    {"iCLASS", 0x63BC8AFF, CT_ANY},
    {"iCLASS SE/SR", 0x4C87F761, 3},
    {"DESFire SIO", 0x4C87F761, 1},
    {"MIFARE SIO", 0x4C87F761, 2},
    {"DESFire CSN", 0x87554C40, 1},
    {"MIFARE CSN", 0x87554C40, 2},
    {"CSN Only", 0x87554C40, CT_ANY},
    {"125kHz Prox", 0xB1CD2E0F, CT_ANY},
};

const char* seos_inspect_cred_name(size_t i) {
    return (i < SEOS_INSPECT_CRED_COUNT) ? CRED_ITEMS[i].name : "";
}

static bool is_get_data(const uint8_t* d, size_t n) {
    return n >= 4 && d[0] == 0x00 && d[1] == 0xca;
}

static bool is_put_data(const uint8_t* d, size_t n) {
    return n >= 4 && d[0] == 0x00 && d[1] == 0xda;
}

static bool aid_is(const uint8_t* aid, size_t aid_len, const uint8_t* want, size_t want_len) {
    return aid_len == want_len && memcmp(aid, want, want_len) == 0;
}

static uint32_t be_uint(const uint8_t* d, size_t len) {
    uint32_t v = 0;
    for(size_t i = 0; i < len && i < 4; i++)
        v = (v << 8) | d[i];
    return v;
}

// How many objects deep find_tag looks. The reader decides how its answer
// nests and each layer is a call on the NFC worker's stack.
#define FIND_TAG_DEPTH 8

// Depth-first search for the first object with the given tag, `depth` objects
// down at most.
static bool find_tag(
    const uint8_t* data,
    size_t len,
    uint16_t tag,
    uint8_t depth,
    const uint8_t** value,
    size_t* value_len) {
    SeosTlvCursor cursor;
    seos_tlv_cursor_init(&cursor, data, len);
    SeosTlvObject obj;
    while(!seos_tlv_cursor_done(&cursor)) {
        if(!seos_tlv_read(&cursor, &obj)) break;
        if(obj.tag == tag) {
            *value = obj.value;
            *value_len = obj.value_len;
            return true;
        }
        uint8_t first = (obj.tag > 0xff) ? (uint8_t)(obj.tag >> 8) : (uint8_t)obj.tag;
        if((first & 0x20) && obj.value_len > 0 && depth > 0) {
            if(find_tag(obj.value, obj.value_len, tag, depth - 1, value, value_len)) return true;
        }
    }
    return false;
}

// Length of a BER tag+length header, or 0 if malformed.
static size_t ber_header_len(const uint8_t* d, size_t len) {
    if(len < 2) return 0;
    size_t pos = 1;
    if((d[0] & 0x1f) == 0x1f) {
        if(pos >= len) return 0;
        pos++;
    }
    uint8_t l0 = d[pos++];
    if(l0 < 0x80) return pos;
    if(l0 == 0x81) return (pos < len) ? pos + 1 : 0;
    if(l0 == 0x82) return (pos + 1 < len) ? pos + 2 : 0;
    return 0;
}

// Raw data field of a PUT DATA APDU, after CLA INS P1 P2 Lc.
static const uint8_t* put_data_field(const uint8_t* apdu, size_t apdu_len, size_t* out_len) {
    *out_len = 0;
    if(apdu_len < 5 || apdu[1] != 0xda) return NULL;
    size_t hdr, lc;
    if(apdu[4] == 0x00 && apdu_len >= 7) {
        hdr = 7;
        lc = ((size_t)apdu[5] << 8) | apdu[6];
    } else {
        hdr = 5;
        lc = apdu[4];
    }
    if(hdr > apdu_len) return NULL;
    if(hdr + lc > apdu_len) lc = apdu_len - hdr;
    *out_len = lc;
    return &apdu[hdr];
}

// Unwrap the two-level IPC BER around a PUT DATA response payload. Falls back
// to skipping the headers when the declared length runs past the frame, which
// happens on the first (truncated) fragment of a large response.
static const uint8_t* ipc_payload(const uint8_t* apdu, size_t apdu_len, size_t* out_len) {
    *out_len = 0;
    size_t lc;
    const uint8_t* ipc = put_data_field(apdu, apdu_len, &lc);
    if(!ipc || lc < 4) return NULL;

    SeosTlvObject outer, inner;
    if(seos_tlv_read_at(ipc, lc, 0, &outer) && outer.value_len > 0 &&
       seos_tlv_read_at(outer.value, outer.value_len, 0, &inner) && inner.value_len > 0) {
        *out_len = inner.value_len;
        return inner.value;
    }

    size_t h1 = ber_header_len(ipc, lc);
    if(h1 > 0 && h1 < lc) {
        size_t h2 = ber_header_len(ipc + h1, lc - h1);
        if(h2 > 0 && h1 + h2 < lc) {
            *out_len = lc - h1 - h2;
            return ipc + h1 + h2;
        }
    }
    return NULL;
}

// Find the credential-table octet string that follows the OID in an SNMP
// response. Reports the length the value declares as well as what is present.
static bool snmp_cred_value(
    const uint8_t* data,
    size_t len,
    const uint8_t** out,
    size_t* out_len,
    size_t* declared_len) {
    // OBJECT IDENTIFIER (tag 06, len 05) for OID 03 01 07 01 5F.
    static const uint8_t OID[] = {0x06, 0x05, 0x03, 0x01, 0x07, 0x01, 0x5f};
    for(size_t i = 0; i + 8 < len; i++) {
        if(memcmp(&data[i], OID, sizeof(OID)) == 0) {
            size_t vp = i + sizeof(OID);
            if(vp >= len) return false;
            uint8_t vtag = data[vp++];
            if(vtag == 0x05) return false;
            if(vp >= len) return false;
            uint8_t vl = data[vp];
            size_t vlen;
            if(vl < 0x80) {
                vlen = vl;
                vp += 1;
            } else if(vl == 0x81 && vp + 1 < len) {
                vlen = data[vp + 1];
                vp += 2;
            } else if(vl == 0x82 && vp + 2 < len) {
                vlen = ((size_t)data[vp + 1] << 8) | data[vp + 2];
                vp += 3;
            } else {
                return false;
            }
            *declared_len = vlen;
            if(vp + vlen > len) vlen = len - vp;
            *out = &data[vp];
            *out_len = vlen;
            return true;
        }
    }
    return false;
}

static bool proto_is_prox(uint8_t proto) {
    return proto == 0x09 || proto == 0x0a || proto == 0x0b;
}

static void cred_table_entry(SeosInspectData* ins, const uint8_t* entry) {
    uint32_t hv = be_uint(entry, 4);
    uint8_t ct = entry[7];
    FURI_LOG_D(TAG, "cred proto=%02X hc=%08lX ct=%u", ins->cred_proto, (unsigned long)hv, ct);

    for(size_t ci = 0; ci < SEOS_INSPECT_CRED_COUNT; ci++) {
        if(hv == CRED_ITEMS[ci].hivecode && (CRED_ITEMS[ci].card_type == CT_ANY ||
                                             (int16_t)ct == CRED_ITEMS[ci].card_type || ct == 0)) {
            ins->cred_enabled[ci] = true;
        }
    }
    if(hv == 0x4C87F761 && ct == 7) ins->seos_enabled = true;
}

static void cred_table_begin(SeosInspectData* ins, size_t declared) {
    memset(ins->cred_enabled, 0, sizeof(ins->cred_enabled));
    ins->seos_enabled = false;
    ins->cred_started = true;
    ins->cred_declared = declared;
    ins->cred_received = 0;
    ins->cred_remaining = 0;
    ins->cred_part_len = 0;
}

// The table is a run of (protocol, count, count * 9-byte entry) groups. It is
// taken a byte at a time so a group or an entry can straddle two fragments,
// and nothing more than the piece in progress has to be kept. Bytes past the
// length the table declared are not part of it and are left alone.
static void cred_table_feed(SeosInspectData* ins, const uint8_t* data, size_t len) {
    size_t wanted = ins->cred_declared - ins->cred_received;
    if(len > wanted) len = wanted;
    ins->cred_received += len;

    for(size_t i = 0; i < len; i++) {
        ins->cred_part[ins->cred_part_len++] = data[i];

        if(ins->cred_remaining == 0) {
            // Between groups: the next two bytes are a header.
            if(ins->cred_part_len < 2) continue;
            ins->cred_proto = ins->cred_part[0];
            ins->cred_remaining = ins->cred_part[1];
            ins->cred_part_len = 0;
            if(ins->cred_remaining > 0) {
                if(ins->cred_proto == 0x15) ins->seos_enabled = true;
                if(proto_is_prox(ins->cred_proto)) ins->cred_enabled[PROX_IDX] = true;
            }
        } else {
            if(ins->cred_part_len < SEOS_INSPECT_CRED_ENTRY_LEN) continue;
            cred_table_entry(ins, ins->cred_part);
            ins->cred_part_len = 0;
            ins->cred_remaining--;
        }
    }
}

static bool cred_table_complete(const SeosInspectData* ins) {
    return ins->cred_started && ins->cred_received >= ins->cred_declared;
}

static void cred_table_finish(SeosInspectData* ins) {
    // A 125kHz group cut off after its protocol byte still implies Prox.
    if(ins->cred_remaining == 0 && ins->cred_part_len == 1 && proto_is_prox(ins->cred_part[0])) {
        ins->cred_enabled[PROX_IDX] = true;
    }
    ins->cred_parsed = true;
}

static void append_sw(BitBuffer* tx, const uint8_t sw[2]) {
    bit_buffer_append_bytes(tx, sw, 2);
}

static void send_cmd(SeosInspectData* ins, const uint8_t* cmd, size_t len, BitBuffer* tx) {
    bit_buffer_append_bytes(tx, cmd, len);
    append_sw(tx, SEOS_SW_SUCCESS);
    ins->cmd_sent = true;
}

static void accumulate_cred(
    SeosInspectData* ins,
    const uint8_t* apdu,
    size_t apdu_len,
    const uint8_t* payload,
    size_t payload_len) {
    if(!ins->cred_started) {
        // The first fragment says how long the table is and starts it.
        const uint8_t* tbl;
        size_t tbl_len, declared;
        if(payload && snmp_cred_value(payload, payload_len, &tbl, &tbl_len, &declared)) {
            cred_table_begin(ins, declared);
            cred_table_feed(ins, tbl, tbl_len);
        }
        return;
    }
    // Every fragment after it is bare table bytes.
    size_t raw_len;
    const uint8_t* raw = put_data_field(apdu, apdu_len, &raw_len);
    if(raw && raw_len) cred_table_feed(ins, raw, raw_len);
}

void seos_inspect_reset(SeosInspectData* inspect) {
    memset(inspect, 0, sizeof(SeosInspectData));
    seos_iso14443_4_card_init(&inspect->card, HID_ATS, sizeof(HID_ATS));
}

// Answer a SELECT in any pre-inspection state, or begin the IPC poll loop.
static SeosInspectEvent
    handshake(SeosInspectData* ins, const uint8_t* apdu, size_t apdu_len, BitBuffer* tx) {
    const uint8_t* aid = NULL;
    size_t aid_len = 0;

    if(seos_parse_select_aid(apdu, apdu_len, &aid, &aid_len)) {
        if(aid_is(aid, aid_len, MOBILE_SEOS_ADMIN_CARD, sizeof(MOBILE_SEOS_ADMIN_CARD))) {
            ins->state = SeosInspectSelected;
            seos_emulator_select_aid(tx, aid, aid_len);
            append_sw(tx, SEOS_SW_SUCCESS);
            return SeosInspectEventNone;
        }
        if(aid_is(aid, aid_len, OPERATION_SELECTOR, sizeof(OPERATION_SELECTOR)) ||
           aid_is(
               aid,
               aid_len,
               OPERATION_SELECTOR_POST_RESET,
               sizeof(OPERATION_SELECTOR_POST_RESET))) {
            SeosInspectEvent ev = SeosInspectEventNone;
            if(ins->state == SeosInspectIdle) {
                ins->state = SeosInspectOpSelected;
                ev = SeosInspectEventAidSelected;
            }
            bit_buffer_append_bytes(tx, RESP_ENABLE_INSPECTION, sizeof(RESP_ENABLE_INSPECTION));
            append_sw(tx, SEOS_SW_SUCCESS);
            return ev;
        }
        append_sw(tx, SEOS_SW_FILE_NOT_FOUND);
        return SeosInspectEventNone;
    }

    if(ins->state != SeosInspectIdle) {
        if(is_get_data(apdu, apdu_len)) {
            ins->state = SeosInspectSamVer;
            send_cmd(ins, CMD_SAM_VER, sizeof(CMD_SAM_VER), tx);
            return SeosInspectEventNone;
        }
        if(is_put_data(apdu, apdu_len)) {
            append_sw(tx, SEOS_SW_SUCCESS);
            return SeosInspectEventNone;
        }
    }
    append_sw(tx, SW_INS_NOT_SUPPORTED);
    return SeosInspectEventNone;
}

static SeosInspectEvent
    collection(SeosInspectData* ins, const uint8_t* apdu, size_t apdu_len, BitBuffer* tx) {
    bool is_ca = is_get_data(apdu, apdu_len);
    bool is_da = is_put_data(apdu, apdu_len);
    size_t payload_len = 0;
    const uint8_t* payload = is_da ? ipc_payload(apdu, apdu_len, &payload_len) : NULL;

    // The credential table spans several PUT DATA fragments; it is read as
    // they arrive and is done once the length it declared has come in.
    if(is_da && ins->state == SeosInspectCredConfig) {
        if(!ins->cred_started && !payload) {
            if(++ins->poll_count >= SEOS_INSPECT_POLL_TIMEOUT) ins->state = SeosInspectSuspendEnd;
        } else {
            accumulate_cred(ins, apdu, apdu_len, payload, payload_len);
            if(cred_table_complete(ins)) {
                FURI_LOG_I(TAG, "Cred table complete: %u bytes", (unsigned)ins->cred_received);
                cred_table_finish(ins);
                ins->state = SeosInspectSuspendEnd;
            }
        }
        ins->cmd_sent = false;
        append_sw(tx, SEOS_SW_SUCCESS);
        return SeosInspectEventNone;
    }

    // Version / memory responses.
    if(payload) {
        const uint8_t* v = NULL;
        size_t vl = 0;
        switch(ins->state) {
        case SeosInspectSamVer:
            if(find_tag(payload, payload_len, 0x80, FIND_TAG_DEPTH, &v, &vl))
                ins->sam_ver = (uint16_t)be_uint(v, vl);
            FURI_LOG_I(TAG, "SAM=0x%04X", ins->sam_ver);
            ins->state = SeosInspectCoreVer;
            break;
        case SeosInspectCoreVer:
            if(find_tag(payload, payload_len, 0x80, FIND_TAG_DEPTH, &v, &vl))
                ins->core_ver = (uint16_t)be_uint(v, vl);
            FURI_LOG_I(TAG, "Core=0x%04X", ins->core_ver);
            ins->state = SeosInspectPersistMem;
            break;
        case SeosInspectPersistMem:
            if(find_tag(payload, payload_len, 0x80, FIND_TAG_DEPTH, &v, &vl))
                ins->segment_count = (uint8_t)be_uint(v, vl);
            if(find_tag(payload, payload_len, 0x81, FIND_TAG_DEPTH, &v, &vl))
                ins->assembly_count = (uint8_t)be_uint(v, vl);
            if(find_tag(payload, payload_len, 0x82, FIND_TAG_DEPTH, &v, &vl))
                ins->persist_mem = be_uint(v, vl);
            FURI_LOG_I(
                TAG,
                "Apps=%u Segs=%u Mem=%lu",
                ins->assembly_count,
                ins->segment_count,
                (unsigned long)ins->persist_mem);
            ins->state = SeosInspectCredConfig;
            break;
        default:
            break;
        }
        ins->cmd_sent = false;
        append_sw(tx, SEOS_SW_SUCCESS);
        return SeosInspectEventNone;
    }

    // Empty PUT DATA ack, including the one after the final suspend.
    if(is_da) {
        SeosInspectEvent ev = SeosInspectEventNone;
        if(ins->state == SeosInspectSuspendEnd && ins->cmd_sent) {
            ins->state = SeosInspectDone;
            ev = SeosInspectEventDone;
        }
        ins->cmd_sent = false;
        append_sw(tx, SEOS_SW_SUCCESS);
        return ev;
    }

    // Reader poll: issue the command for the current step.
    if(is_ca && !ins->cmd_sent) {
        switch(ins->state) {
        case SeosInspectCoreVer:
            send_cmd(ins, CMD_CORE_VER, sizeof(CMD_CORE_VER), tx);
            return SeosInspectEventNone;
        case SeosInspectPersistMem:
            send_cmd(ins, CMD_PERSIST_MEM, sizeof(CMD_PERSIST_MEM), tx);
            return SeosInspectEventNone;
        case SeosInspectCredConfig:
            ins->poll_count = 0;
            send_cmd(ins, CMD_SNMP_CRED, sizeof(CMD_SNMP_CRED), tx);
            return SeosInspectEventNone;
        case SeosInspectSuspendEnd:
            send_cmd(ins, CMD_SUSPEND, sizeof(CMD_SUSPEND), tx);
            return SeosInspectEventNone;
        default:
            break;
        }
    }

    append_sw(tx, SEOS_SW_SUCCESS);
    return SeosInspectEventNone;
}

// Run one complete APDU through the inspection step, appending the reply (data
// plus status word) to tx after the I-block header already written there.
static SeosInspectEvent
    handle_apdu(SeosInspectData* ins, const uint8_t* apdu, size_t apdu_len, BitBuffer* tx) {
    if(apdu_len == 0) {
        append_sw(tx, SEOS_SW_SUCCESS);
        return SeosInspectEventNone;
    }
    if(ins->state == SeosInspectDone) {
        append_sw(tx, SEOS_SW_FILE_NOT_FOUND);
        return SeosInspectEventNone;
    }
    if(ins->state <= SeosInspectSelected) {
        return handshake(ins, apdu, apdu_len, tx);
    }
    return collection(ins, apdu, apdu_len, tx);
}

void seos_inspect_field_off(SeosInspectData* ins) {
    seos_iso14443_4_card_drop_command(&ins->card);
}

void seos_inspect_handle_frame(
    SeosInspectData* ins,
    const uint8_t* frame,
    size_t frame_len,
    BitBuffer* tx,
    SeosInspectEvent* event) {
    *event = SeosInspectEventNone;
    bit_buffer_reset(tx);

    const uint8_t* reply = NULL;
    size_t reply_len = 0;
    SeosIso14443_4CardResult result =
        seos_iso14443_4_card_receive(&ins->card, frame, frame_len, &reply, &reply_len);
    if(result == SeosIso14443_4CardSilent) return;

    bit_buffer_copy_bytes(tx, reply, reply_len);
    if(result != SeosIso14443_4CardCommand) return;

    // A whole command: the reply so far is the answering block's header, and
    // the inspection step appends its answer after it.
    *event = handle_apdu(ins, ins->card.command, ins->card.command_len, tx);
    seos_iso14443_4_card_answered(
        &ins->card, bit_buffer_get_data(tx), bit_buffer_get_size_bytes(tx));
}
