#include "marauder_gui_pcap_index.h"
#include <string.h>

#define PCAP_GLOBAL_HEADER_SIZE 24
#define PCAP_RECORD_HEADER_SIZE 16
#define PCAP_MAGIC_LE           0xa1b2c3d4u
#define PCAP_MAX_SANE_INCL_LEN  4096u

const char* marauder_pcap_frame_type_label(MarauderPcapFrameType type, bool english) {
    switch(type) {
    case MarauderPcapFrameBeacon:
        return "Beacon";
    case MarauderPcapFrameProbe:
        return english ? "Probe" : "Probe";
    case MarauderPcapFrameDeauth:
        return "Deauth";
    case MarauderPcapFrameAuth:
        return english ? "Auth" : "Auth";
    case MarauderPcapFrameAssoc:
        return english ? "Assoc" : "Assoc";
    case MarauderPcapFrameData:
        return "Data";
    case MarauderPcapFrameEapol:
        return "EAPOL";
    case MarauderPcapFrameOther:
    default:
        return english ? "Other" : "Diger";
    }
}

/* 802.11 Frame Control field: byte0 bits[3:2] = Type, bits[7:4] = Subtype; byte1 bit0 = ToDS,
   bit1 = FromDS. Management subtypes: 0x8 Beacon, 0x4 Probe Req, 0x5 Probe Resp, 0xB Auth,
   0xC Deauth, 0x0-0x3 (Re)Assoc Req/Resp. Data subtypes 0x8-0xF are QoS Data (add a 2-byte QoS
   Control field after the MAC header). SSID is the first tagged parameter (tag 0) in Beacon/
   Probe Resp, right after their 12-byte fixed fields (timestamp+interval+capabilities); Probe
   Req has no fixed fields, so its tagged parameters start immediately after the MAC header. */
MarauderPcapFrameType marauder_pcap_classify_frame(
    const uint8_t* data,
    size_t len,
    char* ssid_out,
    size_t ssid_out_size) {
    if(ssid_out && ssid_out_size) ssid_out[0] = '\0';
    if(!data || len < 2) return MarauderPcapFrameOther;

    uint8_t type = (data[0] >> 2) & 0x3;
    uint8_t subtype = (data[0] >> 4) & 0xF;

    if(type == 0) {
        /* Management */
        if(subtype == 0x8 || subtype == 0x5 || subtype == 0x4) {
            size_t fixed = (subtype == 0x4) ? 0 : 12;
            size_t pos = 24 + fixed; /* 24-byte MAC header + fixed mgmt fields (if any) */
            if(pos + 2 <= len && data[pos] == 0 /* tag 0 = SSID */) {
                uint8_t tag_len = data[pos + 1];
                if(ssid_out && pos + 2 + tag_len <= len) {
                    size_t copy = (tag_len < ssid_out_size - 1) ? tag_len : ssid_out_size - 1;
                    memcpy(ssid_out, data + pos + 2, copy);
                    ssid_out[copy] = '\0';
                }
            }
            return (subtype == 0x8) ? MarauderPcapFrameBeacon : MarauderPcapFrameProbe;
        }
        if(subtype == 0xC) return MarauderPcapFrameDeauth;
        if(subtype == 0xB) return MarauderPcapFrameAuth;
        if(subtype <= 0x3) return MarauderPcapFrameAssoc;
        return MarauderPcapFrameOther;
    }

    if(type == 2) {
        /* Data - only handle the common 3-address (non-WDS) case; 4-address (ToDS+FromDS both
           set) frames are rare in a client capture and just fall back to plain "Data". */
        bool to_ds = data[1] & 0x1;
        bool from_ds = (data[1] >> 1) & 0x1;
        if(to_ds && from_ds) return MarauderPcapFrameData;

        size_t hdr_len = 24 + ((subtype >= 0x8) ? 2 : 0); /* +2 for QoS Control */
        size_t llc = hdr_len;
        /* LLC/SNAP: AA AA 03 00 00 00 <2-byte EtherType>. EAPOL's EtherType is 0x888E. */
        if(llc + 8 <= len && data[llc] == 0xAA && data[llc + 1] == 0xAA && data[llc + 2] == 0x03 &&
           data[llc + 3] == 0x00 && data[llc + 4] == 0x00 && data[llc + 5] == 0x00 &&
           data[llc + 6] == 0x88 && data[llc + 7] == 0x8E) {
            return MarauderPcapFrameEapol;
        }
        return MarauderPcapFrameData;
    }

    return MarauderPcapFrameOther;
}

size_t marauder_pcap_index_build(
    Storage* storage,
    const char* path,
    MarauderPcapIndexEntry* out,
    size_t out_capacity,
    bool* out_truncated) {
    if(out_truncated) *out_truncated = false;
    if(!storage || !path || !out || out_capacity == 0) return 0;

    File* file = storage_file_alloc(storage);
    if(!storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_free(file);
        return 0;
    }

    uint8_t global_header[PCAP_GLOBAL_HEADER_SIZE];
    if(storage_file_read(file, global_header, sizeof(global_header)) != sizeof(global_header)) {
        storage_file_close(file);
        storage_file_free(file);
        return 0;
    }
    uint32_t magic = (uint32_t)global_header[0] | ((uint32_t)global_header[1] << 8) |
                     ((uint32_t)global_header[2] << 16) | ((uint32_t)global_header[3] << 24);
    if(magic != PCAP_MAGIC_LE) {
        storage_file_close(file);
        storage_file_free(file);
        return 0;
    }

    /* A file that was left open when Marauder lost power (unplugged mid-capture instead of a
       clean stop - the common case for an older file that isn't the one just recorded) can end
       with a partially-written trailing record whose incl_len is whatever garbage happened to be
       on the SD card. Trusting that value verbatim turns the "skip what we didn't peek" seek
       below into a multi-gigabyte forward seek, which hangs long enough to trip the watchdog and
       reboot the device. Bound incl_len against both a sane 802.11-frame ceiling and the file's
       actual remaining size, and stop indexing (rather than seek) the moment a record fails
       either check - that's always the truncated tail of the file, never a real packet. */
    uint64_t file_size = storage_file_size(file);

    size_t count = 0;
    uint8_t record_header[PCAP_RECORD_HEADER_SIZE];
    uint8_t packet_buf[512];
    uint32_t offset = PCAP_GLOBAL_HEADER_SIZE;

    while(count < out_capacity) {
        if(storage_file_read(file, record_header, sizeof(record_header)) !=
           sizeof(record_header)) {
            break; /* EOF or short read - done */
        }
        uint32_t incl_len = (uint32_t)record_header[8] | ((uint32_t)record_header[9] << 8) |
                            ((uint32_t)record_header[10] << 16) |
                            ((uint32_t)record_header[11] << 24);

        if(incl_len > PCAP_MAX_SANE_INCL_LEN ||
           (uint64_t)offset + PCAP_RECORD_HEADER_SIZE + incl_len > file_size) {
            break; /* corrupt/truncated trailing record - nothing valid left to index */
        }

        MarauderPcapIndexEntry* entry = &out[count];
        entry->record_offset = offset;
        entry->incl_len = (incl_len > 0xFFFF) ? 0xFFFF : (uint16_t)incl_len;
        entry->ssid[0] = '\0';
        entry->frame_type = MarauderPcapFrameOther;

        size_t to_read = (incl_len < sizeof(packet_buf)) ? incl_len : sizeof(packet_buf);
        size_t got = storage_file_read(file, packet_buf, to_read);
        if(got > 0) {
            entry->frame_type = (uint8_t)marauder_pcap_classify_frame(
                packet_buf, got, entry->ssid, sizeof(entry->ssid));
        }
        /* Skip whatever we didn't read into packet_buf (packet longer than our peek window). */
        if(incl_len > to_read) {
            storage_file_seek(file, incl_len - to_read, false);
        }

        offset += PCAP_RECORD_HEADER_SIZE + incl_len;
        count++;
    }

    if(count == out_capacity) {
        /* Peek one more record header to tell "exactly full" from "there was more". */
        if(storage_file_read(file, record_header, sizeof(record_header)) ==
           sizeof(record_header)) {
            if(out_truncated) *out_truncated = true;
        }
    }

    storage_file_close(file);
    storage_file_free(file);
    return count;
}

bool marauder_pcap_entry_matches_filter(
    const MarauderPcapIndexEntry* entry,
    uint16_t type_mask,
    const char (*ssid_filters)[16],
    size_t ssid_filter_count) {
    if(!entry) return false;
    if(!((1u << entry->frame_type) & type_mask)) return false;
    if(ssid_filter_count > 0 && ssid_filters) {
        bool matched = false;
        for(size_t i = 0; i < ssid_filter_count; i++) {
            if(strcmp(entry->ssid, ssid_filters[i]) == 0) {
                matched = true;
                break;
            }
        }
        if(!matched) return false;
    }
    return true;
}

void marauder_pcap_live_parser_reset(MarauderPcapLiveParser* parser) {
    memset(parser, 0, sizeof(*parser));
    parser->state = MarauderPcapLiveWaitGlobalHeader;
}

void marauder_pcap_live_parser_feed(
    MarauderPcapLiveParser* p,
    const uint8_t* data,
    size_t len,
    MarauderPcapLiveFrameCallback on_frame,
    void* ctx) {
    size_t i = 0;
    while(i < len) {
        switch(p->state) {
        case MarauderPcapLiveWaitGlobalHeader: {
            /* Just skip these 24 bytes - the same bytes are already being written verbatim to
               the .pcap file, so there is nothing useful to validate here that the file itself
               doesn't already carry. */
            size_t need = PCAP_GLOBAL_HEADER_SIZE - p->hdr_len;
            size_t take = (len - i < need) ? (len - i) : need;
            p->hdr_len += take;
            i += take;
            if(p->hdr_len >= PCAP_GLOBAL_HEADER_SIZE) {
                p->hdr_len = 0;
                p->state = MarauderPcapLiveWaitRecordHeader;
            }
            break;
        }
        case MarauderPcapLiveWaitRecordHeader: {
            size_t need = PCAP_RECORD_HEADER_SIZE - p->hdr_len;
            size_t take = (len - i < need) ? (len - i) : need;
            memcpy(p->hdr + p->hdr_len, data + i, take);
            p->hdr_len += take;
            i += take;
            if(p->hdr_len >= PCAP_RECORD_HEADER_SIZE) {
                uint32_t incl_len = (uint32_t)p->hdr[8] | ((uint32_t)p->hdr[9] << 8) |
                                    ((uint32_t)p->hdr[10] << 16) | ((uint32_t)p->hdr[11] << 24);
                p->hdr_len = 0;
                p->peek_len = 0;
                p->body_consumed = 0;
                /* An insane incl_len (a dropped/garbled byte on the wire) would otherwise make
                   WaitBody swallow every future byte as "the rest of this one record" forever,
                   silently freezing the dashboard. Treat it as 0 instead - the state machine
                   completes the "record" immediately and starts trying to read a header again
                   from the very next byte, which is the best this parser can do without a real
                   resync marker to hunt for. */
                p->body_incl_len = (incl_len > PCAP_MAX_SANE_INCL_LEN) ? 0 : incl_len;
                p->state = MarauderPcapLiveWaitBody;
            }
            break;
        }
        case MarauderPcapLiveWaitBody: {
            size_t remaining_in_record = p->body_incl_len - p->body_consumed;
            size_t take = (len - i < remaining_in_record) ? (len - i) : remaining_in_record;
            size_t peek_room = MARAUDER_PCAP_LIVE_PEEK_MAX - p->peek_len;
            size_t peek_take = (take < peek_room) ? take : peek_room;
            if(peek_take > 0) {
                memcpy(p->peek + p->peek_len, data + i, peek_take);
                p->peek_len += peek_take;
            }
            p->body_consumed += take;
            i += take;
            if(p->body_consumed >= p->body_incl_len) {
                if(on_frame) on_frame(ctx, p->peek, p->peek_len);
                p->state = MarauderPcapLiveWaitRecordHeader;
            }
            break;
        }
        }
    }
}
