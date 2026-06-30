#include "seos_att_i.h"

#define TAG "SeosAtt"

struct att_read_by_type_req {
    uint8_t opcode;
    uint16_t start_handle;
    uint16_t end_handle;
    union {
        uint16_t short_uuid;
        uint8_t long_uuid[16];
    } attribute_type;
} __packed;

struct att_find_info_req {
    uint8_t opcode;
    uint16_t start_handle;
    uint16_t end_handle;
} __packed;

struct att_find_type_value_req {
    uint8_t opcode;
    uint16_t start_handle;
    uint16_t end_handle;
    uint16_t att_type;
    uint8_t att_value[0];
} __packed;

struct att_write_req {
    uint8_t opcode;
    uint16_t handle;
} __packed;

static uint8_t seos_reader_service_backwards[] =
    {0x02, 0x00, 0x00, 0x7a, 0x17, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0x00, 0x98, 0x00, 0x00};
static uint8_t seos_cred_service_backwards[] =
    {0x02, 0x00, 0x00, 0x7a, 0x17, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0x01, 0x98, 0x00, 0x00};

SeosAtt* seos_att_alloc(Seos* seos) {
    SeosAtt* seos_att = malloc(sizeof(SeosAtt));
    memset(seos_att, 0, sizeof(SeosAtt));

    seos_att->seos = seos;
    seos_att->seos_l2cap = seos_l2cap_alloc(seos);
    seos_l2cap_set_receive_callback(seos_att->seos_l2cap, seos_att_process_payload, seos_att);
    seos_l2cap_set_central_connection_callback(
        seos_att->seos_l2cap, seos_att_central_connection_start, seos_att);

    seos_att->tx_buf = bit_buffer_alloc(128);

    seos_att->tx_mtu = 0;
    seos_att->rx_mtu = 0x0200;

    return seos_att;
}

void seos_att_free(SeosAtt* seos_att) {
    furi_assert(seos_att);

    seos_l2cap_free(seos_att->seos_l2cap);

    bit_buffer_free(seos_att->tx_buf);
    free(seos_att);
}

void seos_att_start(SeosAtt* seos_att, BleMode mode, FlowMode flow_mode) {
    seos_att->ble_mode = mode;
    seos_att->flow_mode = flow_mode;
    seos_l2cap_start(seos_att->seos_l2cap, mode, flow_mode);
}

void seos_att_stop(SeosAtt* seos_att) {
    seos_l2cap_stop(seos_att->seos_l2cap);
}

void seos_att_central_connection_start(void* context) {
    SeosAtt* seos_att = (SeosAtt*)context;
    FURI_LOG_D(TAG, "central connnection start");
    bit_buffer_reset(seos_att->tx_buf);

    if(seos_att->flow_mode == FLOW_READER) {
        uint16_t start = 0x0001;
        uint16_t end = 0xffff;
        uint16_t attribute_type = PRIMARY_SERVICE;

        bit_buffer_append_byte(seos_att->tx_buf, ATT_FIND_BY_TYPE_VALUE_REQ);
        bit_buffer_append_bytes(seos_att->tx_buf, (uint8_t*)&start, sizeof(start));
        bit_buffer_append_bytes(seos_att->tx_buf, (uint8_t*)&end, sizeof(end));
        bit_buffer_append_bytes(
            seos_att->tx_buf, (uint8_t*)&attribute_type, sizeof(attribute_type));
        bit_buffer_append_bytes(
            seos_att->tx_buf, seos_cred_service_backwards, sizeof(seos_cred_service_backwards));
    } else if(seos_att->flow_mode == FLOW_CRED) {
        // First thing to do with any new connection is the MTU
        bit_buffer_append_byte(seos_att->tx_buf, ATT_EXCHANGE_MTU_REQ);
        bit_buffer_append_bytes(
            seos_att->tx_buf, (uint8_t*)&seos_att->rx_mtu, sizeof(seos_att->rx_mtu));
    }

    seos_l2cap_send(seos_att->seos_l2cap, seos_att->tx_buf);
}

void seos_att_notify(SeosAtt* seos_att, uint16_t handle, BitBuffer* content) {
    seos_log_bitbuffer(TAG, "notify", content);
    size_t content_len = bit_buffer_get_size_bytes(content);

    BitBuffer* tx = bit_buffer_alloc(1 + sizeof(handle) + content_len);

    bit_buffer_append_byte(tx, ATT_HANDLE_VALUE_NTF);
    bit_buffer_append_bytes(tx, (uint8_t*)&handle, sizeof(handle));
    bit_buffer_append_bytes(tx, bit_buffer_get_data(content), content_len);

    seos_l2cap_send(seos_att->seos_l2cap, tx);
    bit_buffer_free(tx);
}

void seos_att_write_request(SeosAtt* seos_att, BitBuffer* content) {
    seos_log_bitbuffer(TAG, "write_request", content);
    size_t content_len = bit_buffer_get_size_bytes(content);

    BitBuffer* tx = bit_buffer_alloc(1 + sizeof(seos_att->value_handle) + content_len);

    bit_buffer_append_byte(tx, ATT_WRITE_CMD);
    bit_buffer_append_bytes(
        tx, (uint8_t*)&(seos_att->value_handle), sizeof(seos_att->value_handle));
    bit_buffer_append_bytes(tx, bit_buffer_get_data(content), content_len);

    seos_l2cap_send(seos_att->seos_l2cap, tx);
    bit_buffer_free(tx);
}

// TODO: figure out the proper name for data that comes in
void seos_att_process_payload(void* context, BitBuffer* message) {
    SeosAtt* seos_att = (SeosAtt*)context;
    // seos_log_bitbuffer(TAG, "process payload", message);

    bit_buffer_reset(seos_att->tx_buf);
    const uint8_t* data = bit_buffer_get_data(message);
    uint8_t opcode = data[0];
    struct att_read_by_type_req* s;
    uint16_t* start_handle;
    uint16_t* end_handle;
    uint16_t attribute_type;
    size_t length = 0;

    if(seos_att->ble_mode == BLE_CENTRAL && ((opcode & 0x01) == 0x00)) {
        bool reject = true;
        FURI_LOG_I(
            TAG,
            "%s request(0x%02x) when operating as Central",
            reject ? "Rejecting" : "Ignoring",
            opcode);
        if(reject) {
            bit_buffer_append_byte(seos_att->tx_buf, ATT_ERROR_RSP);
            bit_buffer_append_byte(seos_att->tx_buf, opcode);
            bit_buffer_append_bytes(seos_att->tx_buf, data + 1, sizeof(uint16_t));
            bit_buffer_append_byte(seos_att->tx_buf, 0x0a);
            seos_l2cap_send(seos_att->seos_l2cap, seos_att->tx_buf);
        }
        return;
    }

    switch(opcode) {
    case ATT_ERROR_RSP:
        uint8_t error_with_opcode = data[1];
        FURI_LOG_W(TAG, "Error with command %02x", error_with_opcode);
        break;
    case ATT_EXCHANGE_MTU_REQ: // MTU request

        // Trying a new way to copy the uint16_t
        seos_att->tx_mtu = *(uint16_t*)(data + 1);
        FURI_LOG_D(TAG, "MTU REQ = %04x", seos_att->tx_mtu);

        bit_buffer_append_byte(seos_att->tx_buf, ATT_EXCHANGE_MTU_RSP);
        bit_buffer_append_bytes(
            seos_att->tx_buf, (uint8_t*)&seos_att->rx_mtu, sizeof(seos_att->rx_mtu));
        break;
    case ATT_EXCHANGE_MTU_RSP:
        seos_att->tx_mtu = *(uint16_t*)(data + 1);
        FURI_LOG_D(TAG, "MTU RSP = %04x", seos_att->tx_mtu);

        uint16_t start = 0x0001;
        uint16_t end = 0xffff;
        attribute_type = PRIMARY_SERVICE;

        bit_buffer_append_byte(seos_att->tx_buf, ATT_FIND_BY_TYPE_VALUE_REQ);
        bit_buffer_append_bytes(seos_att->tx_buf, (uint8_t*)&start, sizeof(start));
        bit_buffer_append_bytes(seos_att->tx_buf, (uint8_t*)&end, sizeof(end));
        bit_buffer_append_bytes(
            seos_att->tx_buf, (uint8_t*)&attribute_type, sizeof(attribute_type));
        bit_buffer_append_bytes(
            seos_att->tx_buf,
            seos_reader_service_backwards,
            sizeof(seos_reader_service_backwards));
        break;
    case ATT_READ_BY_TYPE_REQ:
        s = (struct att_read_by_type_req*)(data);
        FURI_LOG_D(
            TAG,
            "ATT read by type %04x - %04x, type %04x",
            s->start_handle,
            s->end_handle,
            s->attribute_type.short_uuid);

        if(s->attribute_type.short_uuid == CHARACTERISTIC) {
            if(s->start_handle == 0x0006) {
                bit_buffer_append_byte(seos_att->tx_buf, ATT_READ_BY_TYPE_RSP);
                uint8_t response[] = {0x07, 0x07, 0x00, 0x20, 0x08, 0x00, 0x05, 0x2a};
                bit_buffer_append_bytes(seos_att->tx_buf, response, sizeof(response));

            } else if(s->start_handle == 0x000a) {
                bit_buffer_append_byte(seos_att->tx_buf, ATT_READ_BY_TYPE_RSP);
                uint8_t flow_mode_byte = seos_att->flow_mode == FLOW_READER ? 0x00 : 0x01;
                uint8_t response[] = {0x15, 0x0b, 0x00,           0x14, 0x0c, 0x00, 0x02, 0x00,
                                      0x00, 0x7a, 0x17,           0x00, 0x00, 0x80, 0x00, 0x10,
                                      0x00, 0x00, flow_mode_byte, 0xaa, 0x00, 0x00};
                bit_buffer_append_bytes(seos_att->tx_buf, response, sizeof(response));
            }
        } else if(s->attribute_type.short_uuid == DEVICE_NAME) {
            if(s->start_handle == 0x0001) {
                uint8_t response[] = {0x09, 0x03, 0x00, 0x46, 0x6c, 0x69, 0x70, 0x70, 0x65, 0x72};
                bit_buffer_append_bytes(seos_att->tx_buf, response, sizeof(response));
            }
        }

        if(bit_buffer_get_size_bytes(seos_att->tx_buf) == 0) {
            FURI_LOG_W(TAG, "Return error");
            bit_buffer_reset(seos_att->tx_buf);
            bit_buffer_append_byte(seos_att->tx_buf, ATT_ERROR_RSP);
            bit_buffer_append_byte(seos_att->tx_buf, opcode);
            bit_buffer_append_bytes(
                seos_att->tx_buf, (uint8_t*)&s->start_handle, sizeof(s->start_handle));
            bit_buffer_append_byte(seos_att->tx_buf, ATT_ERROR_CODE_ATTRIBUTE_NOT_FOUND);
        }
        break;
    case ATT_READ_BY_TYPE_RSP:
        seos_log_buffer(
            TAG, "ATT_READ_BY_TYPE_RSP", (uint8_t*)data, bit_buffer_get_size_bytes(message));

        uint16_t* handle;
        handle = (uint16_t*)(data + 2);
        seos_att->characteristic_handle = *handle;
        // skip properties byte
        handle = (uint16_t*)(data + 5);
        seos_att->value_handle = *handle;

        *handle = *handle + 1;
        bit_buffer_append_byte(seos_att->tx_buf, ATT_FIND_INFORMATION_REQ);
        bit_buffer_append_bytes(seos_att->tx_buf, (uint8_t*)handle, sizeof(uint16_t));
        bit_buffer_append_bytes(
            seos_att->tx_buf, (uint8_t*)&seos_att->service_end_handle, sizeof(uint16_t));
        break;
    case ATT_READ_BY_GROUP_TYPE_REQ:
        s = (struct att_read_by_type_req*)(data);
        FURI_LOG_D(
            TAG,
            "ATT read by group type %04x - %04x, type %04x",
            s->start_handle,
            s->end_handle,
            s->attribute_type.short_uuid);
        if(s->attribute_type.short_uuid == PRIMARY_SERVICE) {
            if(s->start_handle == 0x0001) {
                bit_buffer_append_byte(seos_att->tx_buf, ATT_READ_BY_GROUP_TYPE_RSP);
                uint8_t response[] = {
                    0x06, 0x01, 0x00, 0x05, 0x00, 0x00, 0x18, 0x06, 0x00, 0x09, 0x00, 0x01, 0x18};
                bit_buffer_append_bytes(seos_att->tx_buf, response, sizeof(response));
            } else if(s->start_handle == 0x000a) {
                bit_buffer_append_byte(seos_att->tx_buf, ATT_READ_BY_GROUP_TYPE_RSP);
                uint8_t flow_mode_byte = seos_att->flow_mode == FLOW_READER ? 0x00 : 0x01;
                uint8_t response[] = {0x14, 0x0a, 0x00, 0x0d,           0x00, 0x02, 0x00,
                                      0x00, 0x7a, 0x17, 0x00,           0x00, 0x80, 0x00,
                                      0x10, 0x00, 0x00, flow_mode_byte, 0x98, 0x00, 0x00};
                bit_buffer_append_bytes(seos_att->tx_buf, response, sizeof(response));
            }
        }

        // Didn't match either attribute_type or start_handle
        if(bit_buffer_get_size_bytes(seos_att->tx_buf) == 0) {
            FURI_LOG_W(TAG, "Return error");
            bit_buffer_reset(seos_att->tx_buf);
            bit_buffer_append_byte(seos_att->tx_buf, ATT_ERROR_RSP);
            bit_buffer_append_byte(seos_att->tx_buf, opcode);
            bit_buffer_append_bytes(
                seos_att->tx_buf, (uint8_t*)&s->start_handle, sizeof(s->start_handle));
            bit_buffer_append_byte(seos_att->tx_buf, ATT_ERROR_CODE_ATTRIBUTE_NOT_FOUND);
        }
        break;
    case ATT_READ_BY_GROUP_TYPE_RSP:
        seos_log_buffer(
            TAG, "ATT_READ_BY_GROUP_TYPE_RSP", (uint8_t*)data, bit_buffer_get_size_bytes(message));
        // NOTE: this might not be actively used

        uint8_t size = data[1];
        size_t i = 2;
        do {
            start_handle = (uint16_t*)(data + i);
            end_handle = (uint16_t*)(data + sizeof(uint16_t) + i);

            // +4 for 2 uint16_t
            if(size == (sizeof(seos_cred_service_backwards) + 4)) {
                if(memcmp(
                       seos_cred_service_backwards,
                       data + i + 4,
                       sizeof(seos_cred_service_backwards)) == 0) {
                    seos_att->service_start_handle = *start_handle;
                    seos_att->service_end_handle = *end_handle;
                }
            }
            i += size;
        } while(i < bit_buffer_get_size_bytes(message));
        *end_handle = *end_handle + 1;

        bit_buffer_append_byte(seos_att->tx_buf, ATT_READ_BY_GROUP_TYPE_REQ);
        bit_buffer_append_bytes(seos_att->tx_buf, (uint8_t*)end_handle, sizeof(uint16_t));
        uint8_t suffix[] = {0xff, 0xff, 0x00, 0x28};
        bit_buffer_append_bytes(seos_att->tx_buf, suffix, sizeof(suffix));

        break;
    case ATT_FIND_INFORMATION_REQ:
        struct att_find_info_req* e = (struct att_find_info_req*)(data);
        FURI_LOG_D(TAG, "ATT find information %04x - %04x", e->start_handle, e->end_handle);
        bit_buffer_append_byte(seos_att->tx_buf, ATT_FIND_INFORMATION_RSP);

        if(e->start_handle == 0x0009) {
            uint8_t response[] = {0x01, 0x09, 0x00, 0x02, 0x29};
            bit_buffer_append_bytes(seos_att->tx_buf, response, sizeof(response));
        } else if(e->start_handle == 0x000d) {
            uint8_t response[] = {0x01, 0x0d, 0x00, 0x02, 0x29};
            bit_buffer_append_bytes(seos_att->tx_buf, response, sizeof(response));
        } else {
            FURI_LOG_W(TAG, "unhandled handle in ATT_FIND_INFORMATION_REQ");
        }
        break;
    case ATT_FIND_INFORMATION_RSP:
        seos_log_buffer(
            TAG, "ATT_FIND_INFORMATION_RSP", (uint8_t*)data, bit_buffer_get_size_bytes(message));

        // 05 01 3c00 0029 3d00 0229
        uint8_t format = data[1];
        if(format == 1) { // short UUID
            uint16_t* handle;
            uint16_t* uuid;
            size_t i = 2;
            do {
                handle = (uint16_t*)(data + i);
                uuid = (uint16_t*)(data + i + 2);
                i += 4;
                if(*uuid == CCCD) {
                    seos_att->cccd_handle = *handle;
                }
            } while(i < bit_buffer_get_size_bytes(message));

            if(seos_att->cccd_handle > 0) {
                FURI_LOG_I(TAG, "Subscribe to phone/device");
                uint16_t value = ENABLE_NOTIFICATION_VALUE;
                bit_buffer_append_byte(seos_att->tx_buf, ATT_WRITE_CMD);
                bit_buffer_append_bytes(
                    seos_att->tx_buf, (uint8_t*)&seos_att->cccd_handle, sizeof(uint16_t));
                bit_buffer_append_bytes(seos_att->tx_buf, (uint8_t*)&value, sizeof(value));
            }
        }

        break;
    case ATT_FIND_BY_TYPE_VALUE_REQ:
        struct att_find_type_value_req* t = (struct att_find_type_value_req*)(data);
        FURI_LOG_D(
            TAG,
            "ATT_FIND_BY_TYPE_VALUE_REQ %04x - %04x for %04x",
            t->start_handle,
            t->end_handle,
            t->att_type);
        if(t->att_type == PRIMARY_SERVICE) {
            bit_buffer_append_byte(seos_att->tx_buf, ATT_FIND_BY_TYPE_VALUE_RSP);
            if(seos_att->flow_mode == FLOW_CRED) {
                uint8_t response[] = {0x0a, 0x00, 0x0e, 0x00};
                bit_buffer_append_bytes(seos_att->tx_buf, response, sizeof(response));
            } else if(seos_att->flow_mode == FLOW_READER) {
                uint8_t response[] = {0x0c, 0x00, 0x0e, 0x00};
                bit_buffer_append_bytes(seos_att->tx_buf, response, sizeof(response));
            }
        }
        break;
    case ATT_FIND_BY_TYPE_VALUE_RSP:
        seos_log_buffer(
            TAG, "ATT_FIND_BY_TYPE_VALUE_RSP", (uint8_t*)data, bit_buffer_get_size_bytes(message));
        start_handle = (uint16_t*)(data + 1);
        end_handle = (uint16_t*)(data + 3);

        seos_att->service_start_handle = *start_handle;
        seos_att->service_end_handle = *end_handle;

        attribute_type = CHARACTERISTIC;
        bit_buffer_append_byte(seos_att->tx_buf, ATT_READ_BY_TYPE_REQ);
        bit_buffer_append_bytes(seos_att->tx_buf, (uint8_t*)start_handle, sizeof(uint16_t));
        bit_buffer_append_bytes(seos_att->tx_buf, (uint8_t*)end_handle, sizeof(uint16_t));
        bit_buffer_append_bytes(
            seos_att->tx_buf, (uint8_t*)&attribute_type, sizeof(attribute_type));
        break;
    case ATT_WRITE_REQ:
        struct att_write_req* w = (struct att_write_req*)(data);
        length = bit_buffer_get_size_bytes(message) - sizeof(struct att_write_req);
        FURI_LOG_D(TAG, "ATT Write Req %d bytes to %04x", length, w->handle);
        if(w->handle == 0x0009) {
            bit_buffer_append_byte(seos_att->tx_buf, ATT_WRITE_RSP);
        } else if(w->handle == 0x000d) {
            uint16_t* value = (uint16_t*)(data + sizeof(struct att_write_req));
            if(*value == DISABLE_NOTIFICATION_VALUE) {
                FURI_LOG_I(TAG, "Unsubscribe");
            } else if(*value == ENABLE_NOTIFICATION_VALUE) {
                FURI_LOG_I(TAG, "Subscribe");
                if(seos_att->on_subscribe) {
                    // comes in as 0x000d, but we need to use 0x000c: I'm sure there is a reason for this that I'm just not aware of
                    seos_att->on_subscribe(seos_att->on_subscribe_context, w->handle - 1);
                    bit_buffer_append_byte(seos_att->tx_buf, ATT_WRITE_RSP);
                } else {
                    FURI_LOG_W(TAG, "No on_subscribe callback defined");
                }
            }
        }
        break;
    case ATT_WRITE_RSP:
        FURI_LOG_D(TAG, "ATT_WRITE_RSP");
        break;
    case ATT_WRITE_CMD:
        struct att_write_req* c = (struct att_write_req*)(data);
        length = bit_buffer_get_size_bytes(message) - sizeof(struct att_write_req);
        FURI_LOG_D(TAG, "ATT Write CMD %d bytes to %04x", length, c->handle);

        if(c->handle == 0x000c) {
            BitBuffer* attribute_value = bit_buffer_alloc(length);
            bit_buffer_append_bytes(
                attribute_value,
                bit_buffer_get_data(message) + sizeof(struct att_write_req),
                length);
            if(seos_att->write_request) {
                seos_att->write_request(seos_att->write_request_context, attribute_value);
            }
            bit_buffer_free(attribute_value);
        } else {
            seos_log_bitbuffer(TAG, "write to unsupported handle", message);
        }
        // No response to CMD expected
        break;
    case ATT_HANDLE_VALUE_NTF:
        struct att_write_req* n = (struct att_write_req*)(data);
        length = bit_buffer_get_size_bytes(message) - sizeof(struct att_write_req);
        FURI_LOG_D(TAG, "ATT handle value notify %d bytes to %04x", length, n->handle);
        if(n->handle == 0x000d) {
            if(seos_att->notify) {
                seos_att->notify(
                    seos_att->notify_context,
                    bit_buffer_get_data(message) + sizeof(struct att_write_req),
                    length);
            }
        } else {
            FURI_LOG_W(TAG, "Notify with unhandled handle");
        }
        break;
    case ATT_HANDLE_VALUE_CFM:
        FURI_LOG_I(TAG, "Indication confirmation");
        break;
    default:
        FURI_LOG_W(TAG, "seos_att_process_message no handler for 0x%02x", opcode);
        break;
    }

    if(bit_buffer_get_size_bytes(seos_att->tx_buf) > 0) {
        seos_log_bitbuffer(TAG, "sending", seos_att->tx_buf);
        seos_l2cap_send(seos_att->seos_l2cap, seos_att->tx_buf);
    }
}

void seos_att_set_on_subscribe_callback(
    SeosAtt* seos_att,
    SeosAttOnSubscribeCallback callback,
    void* context) {
    seos_att->on_subscribe = callback;
    seos_att->on_subscribe_context = context;
}

void seos_att_set_write_request_callback(
    SeosAtt* seos_att,
    SeosAttWriteRequestCallback callback,
    void* context) {
    seos_att->write_request = callback;
    seos_att->write_request_context = context;
}

void seos_att_set_notify_callback(SeosAtt* seos_att, SeosAttNotifyCallback callback, void* context) {
    seos_att->notify = callback;
    seos_att->notify_context = context;
}
