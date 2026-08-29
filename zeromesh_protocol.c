#include "zeromesh_protocol.h"
#include "zeromesh_history.h"
#include "zeromesh_notify.h"
#include "zeromesh_roster.h"
#include "zeromesh_transport.h"
#include "lib/meshtastic_api/meshtastic/telemetry.pb.h"
#include "lib/meshtastic_api/meshtastic/admin.pb.h"
#include "lib/meshtastic_api/meshtastic/config.pb.h"

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

typedef struct {
    char* buf;
    size_t size;
} StrSink;

static bool str_sink_cb(pb_istream_t* stream, const pb_field_t* field, void** arg) {
    UNUSED(field);
    StrSink* sink = (StrSink*)(*arg);
    size_t avail = stream->bytes_left;
    size_t take = avail;
    if(sink && sink->size) {
        if(take >= sink->size) take = sink->size - 1;
        if(!pb_read(stream, (pb_byte_t*)sink->buf, take)) return false;
        sink->buf[take] = 0;
    } else {
        take = 0;
    }
    size_t rest = avail - take;
    while(rest > 0) {
        pb_byte_t skip[32];
        size_t chunk = rest > sizeof(skip) ? sizeof(skip) : rest;
        if(!pb_read(stream, skip, chunk)) return false;
        rest -= chunk;
    }
    return true;
}

static void bind_user_names(meshtastic_User* u, StrSink* shortsink, StrSink* longsink) {
    u->short_name.funcs.decode = str_sink_cb;
    u->short_name.arg = shortsink;
    u->long_name.funcs.decode = str_sink_cb;
    u->long_name.arg = longsink;
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
            if(d->portnum == meshtastic_PortNum_TEXT_MESSAGE_APP ||
               d->portnum == meshtastic_PortNum_TELEMETRY_APP ||
               d->portnum == meshtastic_PortNum_POSITION_APP ||
               d->portnum == meshtastic_PortNum_NODEINFO_APP ||
               d->portnum == meshtastic_PortNum_ROUTING_APP) {
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
                    } else if(d->portnum == meshtastic_PortNum_ROUTING_APP) {
                        meshtastic_Routing r = meshtastic_Routing_init_default;
                        pb_istream_t is_r = pb_istream_from_buffer(cap.buf, cap.len);
                        if(pb_decode(&is_r, meshtastic_Routing_fields, &r) &&
                           r.which_variant == meshtastic_Routing_error_reason_tag) {
                            bool ours = false;
                            for(uint8_t k = 0; k < 8; k++) {
                                if(d->request_id && app->sent_msg_ids[k] == d->request_id) {
                                    ours = true;
                                    break;
                                }
                            }
                            if(ours) {
                                if(r.variant.error_reason == meshtastic_Routing_Error_NONE) {
                                    set_status(app, "Delivered");
                                    log_line(app, "ACK from %08lX", (unsigned long)sender_id);
                                } else {
                                    set_status(app, "Delivery failed");
                                    log_line(
                                        app,
                                        "NAK from %08lX (%d)",
                                        (unsigned long)sender_id,
                                        (int)r.variant.error_reason);
                                }
                            }
                        }
                    } else if(d->portnum == meshtastic_PortNum_NODEINFO_APP) {
                        meshtastic_User u = meshtastic_User_init_default;
                        char sn[8] = {0};
                        char ln[24] = {0};
                        StrSink ssink = {sn, sizeof(sn)};
                        StrSink lsink = {ln, sizeof(ln)};
                        bind_user_names(&u, &ssink, &lsink);
                        pb_istream_t is_u = pb_istream_from_buffer(cap.buf, cap.len);
                        if(pb_decode(&is_u, meshtastic_User_fields, &u)) {
                            roster_update_name(app, sender_id, sn, ln);
                            log_line(app, "Node %s (%08lX)", ln[0] ? ln : sn, (unsigned long)sender_id);
                        }
                    } else if(d->portnum == meshtastic_PortNum_POSITION_APP) {
                        meshtastic_Position pos = meshtastic_Position_init_default;
                        pb_istream_t is_pos = pb_istream_from_buffer(cap.buf, cap.len);
                        if(pb_decode(&is_pos, meshtastic_Position_fields, &pos)) {
                            if(pos.has_latitude_i && pos.has_longitude_i) {
                                roster_update_position(
                                    app,
                                    sender_id,
                                    pos.latitude_i,
                                    pos.longitude_i,
                                    pos.has_altitude ? pos.altitude : 0,
                                    pos.time);
                                log_line(app, "RX: Position from %08lX", (unsigned long)sender_id);
                            }
                        }
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
        } else if(p->which_payload_variant == meshtastic_MeshPacket_encrypted_tag) {
            log_line(app, "RX encrypted from %08lX", (unsigned long)sender_id);
        }
    } else if(from.which_payload_variant == meshtastic_FromRadio_node_info_tag) {
        meshtastic_NodeInfo ni = meshtastic_NodeInfo_init_default;
        char sn[8] = {0};
        char ln[24] = {0};
        StrSink ssink = {sn, sizeof(sn)};
        StrSink lsink = {ln, sizeof(ln)};
        bind_user_names(&ni.user, &ssink, &lsink);

        pb_istream_t is_ni = pb_istream_from_buffer(frame, len);
        meshtastic_FromRadio fr2 = meshtastic_FromRadio_init_default;
        fr2.which_payload_variant = meshtastic_FromRadio_node_info_tag;
        bind_user_names(&fr2.payload_variant.node_info.user, &ssink, &lsink);
        if(pb_decode(&is_ni, meshtastic_FromRadio_fields, &fr2)) {
            const meshtastic_NodeInfo* n = &fr2.payload_variant.node_info;
            if(n->num) {
                roster_add_node(app, n->num, (int8_t)n->snr, 0);
                roster_update_name(app, n->num, sn, ln);
                if(app->my_node_num && n->num == app->my_node_num) {
                    if(ln[0]) strncpy(app->my_long_name, ln, sizeof(app->my_long_name) - 1);
                    if(sn[0]) strncpy(app->my_short_name, sn, sizeof(app->my_short_name) - 1);
                }
                if(n->has_position && n->position.has_latitude_i && n->position.has_longitude_i) {
                    roster_update_position(
                        app,
                        n->num,
                        n->position.latitude_i,
                        n->position.longitude_i,
                        n->position.has_altitude ? n->position.altitude : 0,
                        n->position.time);
                }
                log_line(app, "Node %s (%08lX)", ln[0] ? ln : sn, (unsigned long)n->num);
            }
        }
    } else if(from.which_payload_variant == meshtastic_FromRadio_config_tag) {
        const meshtastic_Config* c = &from.payload_variant.config;
        if(c->which_payload_variant == meshtastic_Config_lora_tag) {
            app->cfg_region = (uint8_t)c->payload_variant.lora.region;
            app->cfg_preset = (uint8_t)c->payload_variant.lora.modem_preset;
            app->has_node_config = true;
        } else if(c->which_payload_variant == meshtastic_Config_device_tag) {
            app->cfg_role = (uint8_t)c->payload_variant.device.role;
            app->has_node_config = true;
        }
    } else if(from.which_payload_variant == meshtastic_FromRadio_my_info_tag) {
        const meshtastic_MyNodeInfo* info = &from.payload_variant.my_info;
        app->my_node_num = info->my_node_num;
        log_line(app, "My ID: %08lX", (unsigned long)app->my_node_num);
        set_status(app, "Ready");
    }
}

static void send_frame(ZeroMeshApp* app, const uint8_t* payload, size_t len) {
    if(!app || !transport_is_up(app)) return;
    if(len > MAX_FRAME_SIZE) return;

    /* Header and payload go out as one buffer so a packet transport sends one
       notification per frame rather than a 4-byte runt followed by the body. */
    uint8_t frame[4 + MAX_FRAME_SIZE];
    frame[0] = ZEROMESH_MAGIC0;
    frame[1] = ZEROMESH_MAGIC1;
    frame[2] = (uint8_t)((len >> 8) & 0xFF);
    frame[3] = (uint8_t)(len & 0xFF);
    memcpy(frame + 4, payload, len);

    transport_tx(app, frame, len + 4);
    app->tx_frames++;
}

void send_text_message(ZeroMeshApp* app, const char* text, uint32_t to_node) {
    if(!app || !transport_is_up(app) || !text) return;
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

static void send_admin(ZeroMeshApp* app, meshtastic_AdminMessage* am) {
    if(!app || !transport_is_up(app)) return;

    uint8_t abuf[256];
    pb_ostream_t aos = pb_ostream_from_buffer(abuf, sizeof(abuf));
    if(!pb_encode(&aos, meshtastic_AdminMessage_fields, am)) {
        app->tx_encode_fail++;
        return;
    }

    meshtastic_ToRadio to = meshtastic_ToRadio_init_default;
    to.which_payload_variant = meshtastic_ToRadio_packet_tag;
    meshtastic_MeshPacket* p = &to.payload_variant.packet;
    p->to = app->my_node_num;
    p->id = (uint32_t)furi_hal_random_get();
    p->want_ack = true;
    p->which_payload_variant = meshtastic_MeshPacket_decoded_tag;

    meshtastic_Data* d = &p->payload_variant.decoded;
    d->portnum = meshtastic_PortNum_ADMIN_APP;
    d->want_response = false;
    PayloadSend ps = {.buf = abuf, .len = aos.bytes_written};
    d->payload.funcs.encode = payload_encode_cb;
    d->payload.arg = &ps;

    uint8_t buf[MAX_FRAME_SIZE];
    pb_ostream_t os = pb_ostream_from_buffer(buf, sizeof(buf));
    if(!pb_encode(&os, meshtastic_ToRadio_fields, &to)) {
        app->tx_encode_fail++;
        return;
    }
    send_frame(app, buf, os.bytes_written);
}

void set_node_lora(ZeroMeshApp* app, uint8_t region, uint8_t preset) {
    if(!app) return;
    meshtastic_AdminMessage am = meshtastic_AdminMessage_init_default;
    am.which_payload_variant = meshtastic_AdminMessage_set_config_tag;
    am.payload_variant.set_config.which_payload_variant = meshtastic_Config_lora_tag;
    meshtastic_Config_LoRaConfig* l = &am.payload_variant.set_config.payload_variant.lora;
    l->region = (meshtastic_Config_LoRaConfig_RegionCode)region;
    l->modem_preset = (meshtastic_Config_LoRaConfig_ModemPreset)preset;
    l->use_preset = true;
    l->hop_limit = 3;
    l->tx_enabled = true;
    send_admin(app, &am);
    log_line(app, "LoRa config sent");
}

void set_node_role(ZeroMeshApp* app, uint8_t role) {
    if(!app) return;
    meshtastic_AdminMessage am = meshtastic_AdminMessage_init_default;
    am.which_payload_variant = meshtastic_AdminMessage_set_config_tag;
    am.payload_variant.set_config.which_payload_variant = meshtastic_Config_device_tag;
    am.payload_variant.set_config.payload_variant.device.role =
        (meshtastic_Config_DeviceConfig_Role)role;
    send_admin(app, &am);
    log_line(app, "Role sent");
}

void set_node_owner(ZeroMeshApp* app, const char* long_name, const char* short_name) {
    if(!app) return;
    meshtastic_AdminMessage am = meshtastic_AdminMessage_init_default;
    am.which_payload_variant = meshtastic_AdminMessage_set_owner_tag;
    PayloadSend lps = {.buf = (const uint8_t*)long_name, .len = long_name ? strlen(long_name) : 0};
    PayloadSend sps = {.buf = (const uint8_t*)short_name, .len = short_name ? strlen(short_name) : 0};
    am.payload_variant.set_owner.long_name.funcs.encode = payload_encode_cb;
    am.payload_variant.set_owner.long_name.arg = &lps;
    am.payload_variant.set_owner.short_name.funcs.encode = payload_encode_cb;
    am.payload_variant.set_owner.short_name.arg = &sps;
    send_admin(app, &am);
    log_line(app, "Owner sent");
}

void reboot_node(ZeroMeshApp* app, int32_t seconds) {
    if(!app) return;
    meshtastic_AdminMessage am = meshtastic_AdminMessage_init_default;
    am.which_payload_variant = meshtastic_AdminMessage_reboot_seconds_tag;
    am.payload_variant.reboot_seconds = seconds;
    send_admin(app, &am);
    log_line(app, "Reboot in %ds", (int)seconds);
}

void request_position(ZeroMeshApp* app, uint32_t to_node) {
    if(!app || !transport_is_up(app)) return;

    meshtastic_ToRadio to = meshtastic_ToRadio_init_default;
    to.which_payload_variant = meshtastic_ToRadio_packet_tag;
    meshtastic_MeshPacket* p = &to.payload_variant.packet;
    p->to = to_node;
    p->id = (uint32_t)furi_hal_random_get();
    p->hop_limit = 3;
    p->want_ack = false;
    p->which_payload_variant = meshtastic_MeshPacket_decoded_tag;

    meshtastic_Data* d = &p->payload_variant.decoded;
    d->portnum = meshtastic_PortNum_POSITION_APP;
    d->want_response = true;

    uint8_t buf[MAX_FRAME_SIZE];
    pb_ostream_t os = pb_ostream_from_buffer(buf, sizeof(buf));
    if(!pb_encode(&os, meshtastic_ToRadio_fields, &to)) {
        app->tx_encode_fail++;
        return;
    }
    send_frame(app, buf, os.bytes_written);
    log_line(app, "Position requested from %08lX", (unsigned long)to_node);
}

void request_node_info(ZeroMeshApp* app, uint32_t to_node) {
    if(!app || !transport_is_up(app)) return;

    meshtastic_ToRadio to = meshtastic_ToRadio_init_default;
    to.which_payload_variant = meshtastic_ToRadio_packet_tag;
    meshtastic_MeshPacket* p = &to.payload_variant.packet;
    p->to = to_node;
    p->id = (uint32_t)furi_hal_random_get();
    p->hop_limit = 3;
    p->want_ack = false;
    p->which_payload_variant = meshtastic_MeshPacket_decoded_tag;

    meshtastic_Data* d = &p->payload_variant.decoded;
    d->portnum = meshtastic_PortNum_NODEINFO_APP;
    d->want_response = true;

    uint8_t buf[MAX_FRAME_SIZE];
    pb_ostream_t os = pb_ostream_from_buffer(buf, sizeof(buf));
    if(!pb_encode(&os, meshtastic_ToRadio_fields, &to)) {
        app->tx_encode_fail++;
        return;
    }
    send_frame(app, buf, os.bytes_written);
    log_line(app, "Info requested from %08lX", (unsigned long)to_node);
}

void request_info(ZeroMeshApp* app) {
    if(!app || !transport_is_up(app)) return;
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

void send_heartbeat(ZeroMeshApp* app) {
    if(!app || !transport_is_up(app)) return;
    meshtastic_ToRadio to = meshtastic_ToRadio_init_default;
    to.which_payload_variant = meshtastic_ToRadio_heartbeat_tag;
    uint8_t buf[32];
    pb_ostream_t os = pb_ostream_from_buffer(buf, sizeof(buf));
    if(pb_encode(&os, meshtastic_ToRadio_fields, &to)) {
        send_frame(app, buf, os.bytes_written);
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
