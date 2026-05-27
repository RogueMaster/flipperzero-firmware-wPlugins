#include "zeromesh_protocol.h"
#include "zeromesh_history.h"
#include "zeromesh_notify.h"
#include "zeromesh_roster.h"
#include "lib/meshtastic_api/meshtastic/telemetry.pb.h"

#define TAG "zeromesh_serial"

static void framing_reset(ZeroMeshApp* app) {
    app->hdr_pos = 0;
    app->frame_len = 0;
    app->frame_pos = 0;
}

static bool framing_feed(ZeroMeshApp* app, uint8_t b) {
    if(app->hdr_pos < 4) {
        app->hdr[app->hdr_pos++] = b;
        if(app->hdr_pos == 1 && app->hdr[0] != ZEROMESH_MAGIC0) {
            app->rx_bad_magic++;
            app->hdr_pos = 0;
        } else if(app->hdr_pos == 2 && app->hdr[1] != ZEROMESH_MAGIC1) {
            app->rx_bad_magic++;
            app->hdr_pos = 0;
        } else if(app->hdr_pos == 4) {
            app->frame_len = ((uint16_t)app->hdr[2] << 8) | (uint16_t)app->hdr[3];
            app->frame_pos = 0;
            if(app->frame_len == 0 || app->frame_len > MAX_FRAME_SIZE) {
                app->rx_bad_len++;
                log_line(app, "Bad Len: %u", app->frame_len);
                framing_reset(app);
            }
        }
        return false;
    }
    if(app->frame_pos < app->frame_len) {
        app->frame_buf[app->frame_pos++] = b;
        if(app->frame_pos == app->frame_len) return true;
    } else {
        app->rx_bad_len++;
        framing_reset(app);
    }
    return false;
}

typedef struct {
    uint8_t buf[PAYLOAD_CAPTURE_MAX];
    size_t len;
    bool truncated;
} PayloadCapture;

static bool payload_decode_cb(pb_istream_t* stream, const pb_field_t* field, void** arg) {
    (void)field;
    PayloadCapture* cap = (PayloadCapture*)(*arg);
    if(!cap) return false;
    size_t n = stream->bytes_left;
    if(n > PAYLOAD_CAPTURE_MAX) {
        n = PAYLOAD_CAPTURE_MAX;
        cap->truncated = true;
    }
    cap->len = n;
    if(n > 0) {
        if(!pb_read(stream, cap->buf, n)) return false;
    }
    while(stream->bytes_left > 0) {
        uint8_t tmp[16];
        size_t chunk = (stream->bytes_left > sizeof(tmp)) ? sizeof(tmp) : stream->bytes_left;
        if(!pb_read(stream, tmp, chunk)) return false;
    }
    return true;
}

typedef struct {
    const uint8_t* buf;
    size_t len;
} PayloadSend;

static bool payload_encode_cb(pb_ostream_t* stream, const pb_field_t* field, void* const* arg) {
    const PayloadSend* ps = (const PayloadSend*)(*arg);
    if(!ps || !ps->buf) return false;
    if(!pb_encode_tag_for_field(stream, field)) return false;
    return pb_encode_string(stream, ps->buf, ps->len);
}

static void decode_fromradio(ZeroMeshApp* app, const uint8_t* frame, size_t len) {
    PayloadCapture cap = {0};
    meshtastic_FromRadio from = meshtastic_FromRadio_init_default;
    pb_istream_t is1 = pb_istream_from_buffer(frame, len);
    bool ok1 = pb_decode(&is1, meshtastic_FromRadio_fields, &from);
    if(!ok1) {
        app->rx_decode_fail++;
        log_line(app, "Decode Fail!");
        return;
    }
    app->rx_frames_ok++;
    if(from.which_payload_variant == meshtastic_FromRadio_packet_tag) {
        const meshtastic_MeshPacket* p = &from.payload_variant.packet;
        bool is_echo = false;
        for(uint8_t i = 0; i < 8; i++) {
            if(app->sent_msg_ids[i] == p->id && p->id != 0) {
                is_echo = true;
                break;
            }
        }
        if(is_echo) return;
        uint32_t sender_id = p->from;
        app->last_rx_from = p->from;
        app->last_rx_to = p->to;
        app->last_rx_id = p->id;
        if(p->rx_rssi != 0) {
            app->last_rx_rssi = p->rx_rssi;
            app->has_rx_signal_data = true;
        }
        if(p->rx_snr != 0) {
            app->last_rx_snr = p->rx_snr;
            app->has_rx_signal_data = true;
        }
        roster_add_node(app, sender_id, p->rx_snr, p->rx_rssi);
        if(p->which_payload_variant == meshtastic_MeshPacket_decoded_tag) {
            const meshtastic_Data* d = &p->payload_variant.decoded;
            if(d->portnum == meshtastic_PortNum_TEXT_MESSAGE_APP || d->portnum == meshtastic_PortNum_TELEMETRY_APP) {
                pb_istream_t walk = pb_istream_from_buffer(frame, len);
                bool found_payload = false;
                while(walk.bytes_left > 0) {
                    pb_wire_type_t wire_type;
                    uint32_t tag;
                    bool eof;
                    if(!pb_decode_tag(&walk, &wire_type, &tag, &eof)) break;
                    if(eof) break;
                    if(tag == meshtastic_FromRadio_packet_tag && wire_type == PB_WT_STRING) {
                        uint32_t pkt_len;
                        if(!pb_decode_varint32(&walk, &pkt_len)) break;
                        if(pkt_len > walk.bytes_left) break;
                        pb_istream_t pkt_stream = pb_istream_from_buffer(walk.state, pkt_len);
                        while(pkt_stream.bytes_left > 0) {
                            pb_wire_type_t pkt_wt;
                            uint32_t pkt_tag;
                            bool pkt_eof;
                            if(!pb_decode_tag(&pkt_stream, &pkt_wt, &pkt_tag, &pkt_eof)) break;
                            if(pkt_eof) break;
                            if(pkt_tag == meshtastic_MeshPacket_decoded_tag && pkt_wt == PB_WT_STRING) {
                                uint32_t data_len;
                                if(!pb_decode_varint32(&pkt_stream, &data_len)) break;
                                if(data_len > pkt_stream.bytes_left) break;
                                meshtastic_Data data_msg = meshtastic_Data_init_default;
                                data_msg.payload.funcs.decode = payload_decode_cb;
                                data_msg.payload.arg = &cap;
                                pb_istream_t data_stream = pb_istream_from_buffer(pkt_stream.state, data_len);
                                if(pb_decode(&data_stream, meshtastic_Data_fields, &data_msg)) found_payload = true;
                                break;
                            } else {
                                if(!pb_skip_field(&pkt_stream, pkt_wt)) break;
                            }
                        }
                        break;
                    } else {
                        if(!pb_skip_field(&walk, wire_type)) break;
                    }
                }
                if(found_payload && cap.len > 0) {
                    if(d->portnum == meshtastic_PortNum_TEXT_MESSAGE_APP) {
                        size_t copy_len = cap.len;
                        if(copy_len >= sizeof(app->last_rx_text)) copy_len = sizeof(app->last_rx_text) - 1;
                        memcpy(app->last_rx_text, cap.buf, copy_len);
                        app->last_rx_text[copy_len] = '\0';
                        history_add(app, app->last_rx_text, sender_id, p->to, false);
                        if(p->to == app->my_node_num) {
                            for(uint8_t i = 0; i < app->roster.count; i++) {
                                if(app->roster.nodes[i].node_id == sender_id) {
                                    app->roster.nodes[i].has_new_dm = true;
                                    break;
                                }
                            }
                        }
                        log_line(app, "Msg: %s", app->last_rx_text);
                        set_status(app, "New message");
                        notify_rx_message(app);
                        view_port_update(app->vp);
                    } else if(d->portnum == meshtastic_PortNum_TELEMETRY_APP) {
                        meshtastic_Telemetry tel = meshtastic_Telemetry_init_default;
                        pb_istream_t is_tel = pb_istream_from_buffer(cap.buf, cap.len);
                        if(pb_decode(&is_tel, meshtastic_Telemetry_fields, &tel)) {
                            if(tel.which_variant == meshtastic_Telemetry_device_metrics_tag) {
                                roster_update_telemetry(app, sender_id, tel.variant.device_metrics.battery_level, tel.variant.device_metrics.voltage);
                                log_line(app, "RX: Telemetry from %08lX", (unsigned long)sender_id);
                            }
                        }
                    }
                }
            } else {
                log_line(app, "RX Port: %d", (int)d->portnum);
            }
        }
    } else if(from.which_payload_variant == meshtastic_FromRadio_my_info_tag) {
        const meshtastic_MyNodeInfo* info = &from.payload_variant.my_info;
        app->my_node_num = info->my_node_num;
        log_line(app, "My ID: %08lX", (unsigned long)app->my_node_num);
        set_status(app, "Ready");
    }
}

static void send_frame(ZeroMeshApp* app, const uint8_t* payload, size_t len) {
    if(!app || !app->serial) return;
    uint8_t hdr[4] = {ZEROMESH_MAGIC0, ZEROMESH_MAGIC1, (uint8_t)((len >> 8) & 0xFF), (uint8_t)(len & 0xFF)};
    furi_hal_serial_tx(app->serial, hdr, sizeof(hdr));
    furi_hal_serial_tx(app->serial, payload, len);
    app->tx_frames++;
}

void send_text_message(ZeroMeshApp* app, const char* text, uint32_t to_node) {
    if(!app || !app->serial || !text) return;
    size_t text_len = strlen(text);
    if(text_len == 0) return;
    meshtastic_ToRadio to = meshtastic_ToRadio_init_default;
    to.which_payload_variant = meshtastic_ToRadio_packet_tag;
    meshtastic_MeshPacket* p = &to.payload_variant.packet;
    p->to = to_node;
    p->id = (uint32_t)furi_hal_random_get();
    p->hop_limit = 3;
    p->want_ack = true;
    app->sent_msg_ids[app->sent_msg_head] = p->id;
    app->sent_msg_head = (app->sent_msg_head + 1) % 8;
    p->which_payload_variant = meshtastic_MeshPacket_decoded_tag;
    meshtastic_Data* d = &p->payload_variant.decoded;
    d->portnum = meshtastic_PortNum_TEXT_MESSAGE_APP;
    d->want_response = false;
    PayloadSend ps = {.buf = (const uint8_t*)text, .len = text_len};
    d->payload.funcs.encode = payload_encode_cb;
    d->payload.arg = &ps;
    uint8_t buf[MAX_FRAME_SIZE];
    pb_ostream_t os = pb_ostream_from_buffer(buf, sizeof(buf));
    if(!pb_encode(&os, meshtastic_ToRadio_fields, &to)) {
        app->tx_encode_fail++;
        log_line(app, "TX Encode Fail");
        set_status(app, "Send failed");
        return;
    }
    send_frame(app, buf, os.bytes_written);
    history_add(app, text, app->my_node_num, to_node, true);
    log_line(app, "TX: %s", text);
    set_status(app, "Sent!");
}

void request_info(ZeroMeshApp* app) {
    if(!app || !app->serial) return;
    meshtastic_ToRadio to = meshtastic_ToRadio_init_default;
    to.which_payload_variant = meshtastic_ToRadio_want_config_id_tag;
    to.payload_variant.want_config_id = 12345;
    uint8_t buf[MAX_FRAME_SIZE];
    pb_ostream_t os = pb_ostream_from_buffer(buf, sizeof(buf));
    if(pb_encode(&os, meshtastic_ToRadio_fields, &to)) {
        send_frame(app, buf, os.bytes_written);
        log_line(app, "Info Request Sent");
    }
}

int32_t rx_thread_fn(void* ctx) {
    ZeroMeshApp* app = (ZeroMeshApp*)ctx;
    framing_reset(app);
    uint8_t b;
    while(!app->stop_thread) {
        if(furi_stream_buffer_receive(app->rx_stream, &b, 1, 100) > 0) {
            if(framing_feed(app, b)) {
                decode_fromradio(app, app->frame_buf, app->frame_len);
                framing_reset(app);
            }
        }
    }
    return 0;
}
