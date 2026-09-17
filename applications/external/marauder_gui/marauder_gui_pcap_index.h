#pragma once

#include <furi.h>
#include <storage/storage.h>

/* Shared 802.11-frame classification + .pcap indexing, used by the saved-file viewer
   (marauder_gui_scene_pcap_view.c) and meant to be reused later for live-capture filtering
   without duplicating the parsing logic. Marauder's own .pcap files use LINKTYPE 105
   (DLT_IEEE802_11 - a raw 802.11 MAC frame, no radiotap wrapper), confirmed against
   PcapHeader.cpp's makePcapGlobalHeader(), so no radiotap offset handling is needed here. */

typedef enum {
    MarauderPcapFrameOther,
    MarauderPcapFrameBeacon,
    MarauderPcapFrameProbe,
    MarauderPcapFrameDeauth,
    MarauderPcapFrameAuth,
    MarauderPcapFrameAssoc,
    MarauderPcapFrameData,
    MarauderPcapFrameEapol,
} MarauderPcapFrameType;

#define MARAUDER_PCAP_FRAME_TYPE_COUNT (MarauderPcapFrameEapol + 1)

/* One entry per packet in an indexed .pcap file - deliberately tiny (not the packet bytes
   themselves) so a multi-thousand-packet capture still fits comfortably in RAM. record_offset
   points at that packet's 16-byte pcap record header, so the full frame can be re-read from SD
   on demand (e.g. for a future packet-detail screen) without keeping it all resident. */
typedef struct {
    uint32_t record_offset;
    uint16_t incl_len;
    uint8_t frame_type; /* MarauderPcapFrameType */
    char ssid[16]; /* truncated SSID snippet for Beacon/Probe frames, else empty */
} MarauderPcapIndexEntry;

/* Human-readable label for a frame type, TR/EN pair (caller picks based on language). */
const char* marauder_pcap_frame_type_label(MarauderPcapFrameType type, bool english);

/* Classifies one raw 802.11 frame (as stored in a .pcap record, LINKTYPE 105) and, for
   Beacon/Probe frames, extracts a truncated SSID into ssid_out (ssid_out[0] = '\0' if not
   applicable or the frame is too short to contain one). Safe to call with any length >= 0;
   returns MarauderPcapFrameOther for anything malformed/too short to classify. */
MarauderPcapFrameType marauder_pcap_classify_frame(
    const uint8_t* data,
    size_t len,
    char* ssid_out,
    size_t ssid_out_size);

/* Reads a .pcap file's global header (24 bytes) and every following [16-byte record
   header][incl_len bytes] pair, classifying each with marauder_pcap_classify_frame() into out[]
   (caller-allocated, out_capacity entries). Stops early (leaving *out_truncated = true) once
   out_capacity is reached rather than growing unbounded. Returns the number of entries written,
   or 0 on a file/format error (bad magic, truncated global header, etc.). */
size_t marauder_pcap_index_build(
    Storage* storage,
    const char* path,
    MarauderPcapIndexEntry* out,
    size_t out_capacity,
    bool* out_truncated);

/* True if entry passes the current type-mask + SSID-set filter. The type mask is tested as
   (1 << entry->frame_type) & type_mask. ssid_filters is a picked-from-the-file allow-list (see
   pcap_filter_ssid.c) - an empty list (ssid_filter_count == 0) matches every SSID (including
   frames with no SSID at all); a non-empty list only matches entries whose ssid exactly equals
   one of the listed strings, so frames with no SSID never match once any SSID is selected. */
bool marauder_pcap_entry_matches_filter(
    const MarauderPcapIndexEntry* entry,
    uint16_t type_mask,
    const char (*ssid_filters)[16],
    size_t ssid_filter_count);

/* ---- Live-capture streaming reassembly (marauder_gui.c's shared tick + pcap_sniff.c) ----
   marauder_pcap_index_build() above works on a seekable file; a live capture only has an
   in-order byte stream arriving in whatever chunk sizes the UART drain happens to produce, which
   almost never line up with pcap record boundaries. MarauderPcapLiveParser reassembles that
   stream into records without ever buffering a whole (possibly large) frame body - only the
   first MARAUDER_PCAP_LIVE_PEEK_MAX bytes of each record are kept (plenty for
   marauder_pcap_classify_frame() and for reading the 802.11 address fields out of the MAC
   header); the rest of the body is counted off and discarded as it streams past. */

#define MARAUDER_PCAP_LIVE_PEEK_MAX 128

typedef enum {
    MarauderPcapLiveWaitGlobalHeader,
    MarauderPcapLiveWaitRecordHeader,
    MarauderPcapLiveWaitBody,
} MarauderPcapLiveState;

typedef struct {
    MarauderPcapLiveState state;
    uint8_t hdr[16]; /* accumulates the 16-byte record header across chunk boundaries */
    size_t hdr_len;
    uint8_t peek[MARAUDER_PCAP_LIVE_PEEK_MAX];
    size_t peek_len;
    uint32_t body_incl_len;
    uint32_t body_consumed;
} MarauderPcapLiveParser;

typedef void (*MarauderPcapLiveFrameCallback)(void* ctx, const uint8_t* data, size_t len);

/* Resets the parser to "expecting a fresh global header next" - call once when a new live
   capture starts (the same moment marauder_uart_reset_capture() is called), since the very first
   bytes of a session are always the 24-byte pcap global header, not a record. */
void marauder_pcap_live_parser_reset(MarauderPcapLiveParser* parser);

/* Feeds newly-arrived raw pcap-stream bytes (the same bytes being written verbatim to the .pcap
   file, in order, with no gaps) through the parser. Calls on_frame(ctx, peeked_bytes, peeked_len)
   exactly once per complete record found - possibly more than once per call, possibly zero times,
   and possibly split across several calls before a record actually completes. */
void marauder_pcap_live_parser_feed(
    MarauderPcapLiveParser* parser,
    const uint8_t* data,
    size_t len,
    MarauderPcapLiveFrameCallback on_frame,
    void* ctx);

/* One tracked AP's live packet-type breakdown (see marauder_gui_pcap_live_frame_callback in
   marauder_gui.c) - bssid is the grouping key (works for every frame type, not just the ones that
   carry an SSID); ssid is filled in once a Beacon/ProbeResp for that bssid is seen, empty until
   then (the dashboard falls back to showing the bssid as text meanwhile). */
typedef struct {
    uint8_t bssid[6];
    char ssid[16];
    uint32_t counts[MARAUDER_PCAP_FRAME_TYPE_COUNT];
} MarauderPcapLiveApStat;
