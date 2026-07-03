#include "seos_hci_i.h"

#define TAG "SeosHci"

#define OGF_LINK_CTL   0x01
#define OCF_DISCONNECT 0x0006

#define OGF_HOST_CTL                0x03
#define OCF_SET_EVENT_MASK          0x0001
#define OCF_RESET                   0x0003
#define OCF_READ_LE_HOST_SUPPORTED  0x006c
#define OCF_WRITE_LE_HOST_SUPPORTED 0x006d

#define OGF_INFO_PARAM         0x04
#define OCF_READ_LOCAL_VERSION 0x0001
#define OCF_READ_BUFFER_SIZE   0x0005
#define OCF_READ_BD_ADDR       0x0009

#define OGF_STATUS_PARAM 0x05
#define OCF_READ_RSSI    0x0005

#define OGF_LE_CTL                           0x08
#define OCF_LE_SET_EVENT_MASK                0x0001
#define OCF_LE_READ_BUFFER_SIZE              0x0002
#define OCF_LE_READ_LOCAL_SUPPORTED_FEATURES 0x0003
#define OCF_LE_SET_RANDOM_ADDRESS            0x0005
#define OCF_LE_SET_ADVERTISING_PARAMETERS    0x0006
#define OCF_LE_SET_ADVERTISING_DATA          0x0008
#define OCF_LE_SET_SCAN_RESPONSE_DATA        0x0009
#define OCF_LE_SET_ADVERTISE_ENABLE          0x000a
#define OCF_LE_SET_SCAN_PARAMETERS           0x000b
#define OCF_LE_SET_SCAN_ENABLE               0x000c
#define OCF_LE_CREATE_CONNECTION             0x000d

#define OGF_VENDOR_CTL       0x3F
#define OCF_LE_LTK_NEG_REPLY 0x001B

/* Obtain OGF from OpCode */
#define BT_OGF(opcode) (((opcode) >> 10) & 0x3f)
/* Obtain OCF from OpCode */
#define BT_OCF(opcode) ((opcode) & 0x3FF)

#define BT_OP(ogf, ocf) ((ocf) | ((ogf) << 10))

#define BT_HCI_EVT_DISCONN_COMPLETE      0x05 // HCI_Disconnection_Complete
#define BT_HCI_EVT_QOS_SETUP_COMPLETE    0x0d
#define BT_HCI_EVT_CMD_COMPLETE          0x0e
#define BT_HCI_EVT_CMD_STATUS            0x0f
#define BT_HCI_EVT_HARDWARE_ERROR        0x10
#define BT_HCI_EVT_NUM_COMPLETED_PACKETS 0x13
#define BT_HCI_EVT_LE_META               0x3e // HCI_LE_Connection_Complete

#define HCI_LE_CONNECTION_COMPLETE 0x01
#define HCI_LE_ADVERTISING_REPORT  0x02

// Consider making this an enum that shifts a bit in the apropriate amount
#define CAP_TWIST_AND_GO 0x02
#define CAP_ALLOW_TAP    0x04
#define CAP_APP_SPECIFIC 0x08
#define CAP_ENHANCED_TAP 0x40

static uint8_t seos_reader_service_backwards[] =
    {0x02, 0x00, 0x00, 0x7a, 0x17, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0x00, 0x98, 0x00, 0x00};
static uint8_t seos_cred_service_backwards[] =
    {0x02, 0x00, 0x00, 0x7a, 0x17, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0x01, 0x98, 0x00, 0x00};

static uint8_t empty_mac[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

// Occationally scan stop's completion doesn't get caught.
// Use the timer callback to call it again
void seos_hci_timer(void* context) {
    FURI_LOG_I(TAG, "RUN TIMER");
    SeosHci* seos_hci = (SeosHci*)context;
    if(seos_hci->mode == BLE_PERIPHERAL) {
        seos_hci_enable_advertising(seos_hci, seos_hci->adv_status);
    } else if(seos_hci->mode == BLE_CENTRAL) {
        seos_hci_set_scan(seos_hci, seos_hci->scan_status);
    }
}

void seos_hci_clear_known_addresses(SeosHci* seos_hci) {
    for(size_t i = 0; i < MAX_SCANNED_ADDRESS; i++) {
        ScanAddress* scan_address = &seos_hci->scanned_addresses[i];
        scan_address->used = false;
        memset(scan_address->address, 0, MAC_ADDRESS_LEN);
    }
}

SeosHci* seos_hci_alloc(Seos* seos) {
    SeosHci* seos_hci = malloc(sizeof(SeosHci));
    memset(seos_hci, 0, sizeof(SeosHci));

    seos_hci->device_found = false;
    seos_hci->connection_handle = 0;

    seos_hci->seos = seos;
    seos_hci->seos_hci_h5 = seos_hci_h5_alloc();
    seos_hci->timer = furi_timer_alloc(seos_hci_timer, FuriTimerTypeOnce, seos_hci);
    seos_hci_h5_set_init_callback(seos_hci->seos_hci_h5, seos_hci_init, seos_hci);
    seos_hci_h5_set_receive_callback(seos_hci->seos_hci_h5, seos_hci_recv, seos_hci);

    seos_hci_clear_known_addresses(seos_hci);

    return seos_hci;
}

void seos_hci_free(SeosHci* seos_hci) {
    furi_assert(seos_hci);
    if(furi_timer_is_running(seos_hci->timer)) {
        FURI_LOG_D(TAG, "clear timer");
        furi_timer_stop(seos_hci->timer);
    }
    furi_timer_free(seos_hci->timer);

    seos_hci_h5_set_init_callback(seos_hci->seos_hci_h5, NULL, NULL);
    seos_hci_h5_set_receive_callback(seos_hci->seos_hci_h5, NULL, NULL);

    seos_hci_h5_free(seos_hci->seos_hci_h5);
    free(seos_hci);
}

void seos_hci_start(SeosHci* seos_hci, BleMode mode, FlowMode flow_mode) {
    seos_hci->device_found = false;
    seos_hci->connection_handle = 0;
    seos_hci->flow_mode = flow_mode;
    seos_hci->mode = mode;
    seos_hci_h5_start(seos_hci->seos_hci_h5);
}

void seos_hci_stop(SeosHci* seos_hci) {
    if(seos_hci->connection_handle > 0) {
        uint16_t opcode = BT_OP(OGF_LINK_CTL, OCF_DISCONNECT);
        BitBuffer* disconnect = bit_buffer_alloc(5);
        bit_buffer_append_bytes(disconnect, (uint8_t*)&opcode, sizeof(opcode));
        bit_buffer_append_bytes(
            disconnect,
            (uint8_t*)&seos_hci->connection_handle,
            sizeof(seos_hci->connection_handle));
        bit_buffer_append_byte(disconnect, 0x00);
        seos_hci_h5_send(seos_hci->seos_hci_h5, HCI_COMMAND_PKT, disconnect);
    }

    seos_hci->device_found = false;
    seos_hci->connection_handle = 0;
    seos_hci_h5_stop(seos_hci->seos_hci_h5);
    if(seos_hci->mode == BLE_PERIPHERAL) {
        if(seos_hci->adv_status) {
            seos_hci_enable_advertising(seos_hci, false);
        }
    } else if(seos_hci->mode == BLE_CENTRAL) {
        if(seos_hci->scan_status) {
            seos_hci_set_scan(seos_hci, false);
        }
    }
    if(furi_timer_is_running(seos_hci->timer)) {
        FURI_LOG_D(TAG, "clear timer");
        furi_timer_stop(seos_hci->timer);
    }
    seos_hci_clear_known_addresses(seos_hci);
}

bool seos_hci_known_address(SeosHci* seos_hci, const uint8_t Address[MAC_ADDRESS_LEN]) {
    // Does it exist in the list?
    for(size_t i = 0; i < MAX_SCANNED_ADDRESS; i++) {
        ScanAddress* scan_address = &seos_hci->scanned_addresses[i];
        if(scan_address->used) {
            if(memcmp(Address, scan_address->address, MAC_ADDRESS_LEN) == 0) {
                return true;
            }
        }
    }

    // Not in list, add
    for(size_t i = 0; i < MAX_SCANNED_ADDRESS; i++) {
        ScanAddress* scan_address = &seos_hci->scanned_addresses[i];
        if(!scan_address->used) {
            memcpy(scan_address->address, Address, MAC_ADDRESS_LEN);
            scan_address->used = true;
            // It wasn't previously known
            return false;
        }
    }

    return false;
}

void seos_hci_handle_event_cmd_complete_ogf_host(SeosHci* seos_hci, uint16_t OCF, BitBuffer* frame) {
    UNUSED(frame);

    BitBuffer* message = bit_buffer_alloc(128);
    switch(OCF) {
    case OCF_RESET:
        uint8_t le_read_local_supported_features[] = {0x03, 0x20, 0x00};
        bit_buffer_append_bytes(
            message, le_read_local_supported_features, sizeof(le_read_local_supported_features));
        break;
    case OCF_SET_EVENT_MASK:
        uint8_t set_le_event_mask[] = {
            0x01, 0x20, 0x08, 0x1f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
        bit_buffer_append_bytes(message, set_le_event_mask, sizeof(set_le_event_mask));
        break;
    case OCF_READ_LE_HOST_SUPPORTED:
        FURI_LOG_D(TAG, "OCF_READ_LE_HOST_SUPPORTED");
        break;
    case OCF_WRITE_LE_HOST_SUPPORTED:
        FURI_LOG_D(TAG, "OCF_WRITE_LE_HOST_SUPPORTED");
        uint8_t read_le_host_supported[] = {0x6c, 0x0c, 0x00};
        bit_buffer_append_bytes(message, read_le_host_supported, sizeof(read_le_host_supported));
        break;
    default:
        FURI_LOG_W(TAG, "Unhandled OCF %04x", OCF);
        break;
    }
    if(bit_buffer_get_size_bytes(message) > 0) {
        seos_hci_h5_send(seos_hci->seos_hci_h5, HCI_COMMAND_PKT, message);
    }
    bit_buffer_free(message);
}

void seos_hci_handle_event_cmd_complete_ogf_info(SeosHci* seos_hci, uint16_t OCF, BitBuffer* frame) {
    UNUSED(frame);

    BitBuffer* message = bit_buffer_alloc(128);
    switch(OCF) {
    case OCF_READ_LOCAL_VERSION:
        FURI_LOG_D(TAG, "OCF_READ_LOCAL_VERSION");
        uint8_t read_bd_addr[] = {0x09, 0x10, 0x00};
        bit_buffer_append_bytes(message, read_bd_addr, sizeof(read_bd_addr));
        break;
    case OCF_READ_BD_ADDR:
        // 040e0a05091000 e2f284 dad4d4
        FURI_LOG_D(TAG, "OCF_READ_BD_ADDR");
        if(memcmp(bit_buffer_get_data(frame) + 7, empty_mac, sizeof(empty_mac)) == 0) {
            uint8_t vendor_set_addr[] = {0x06, 0xfc, 0x06, 0x0, 0x0, 0x1, 0x2, 0x21, 0xAD};
            bit_buffer_append_bytes(message, vendor_set_addr, sizeof(vendor_set_addr));
        } else {
            uint16_t opcode = BT_OP(OGF_LE_CTL, OCF_LE_READ_BUFFER_SIZE);
            uint8_t length = 0;
            bit_buffer_append_bytes(message, (uint8_t*)&opcode, sizeof(opcode));
            bit_buffer_append_byte(message, length);
        }
        break;
    default:
        FURI_LOG_W(TAG, "Unhandled OCF %04x", OCF);
        break;
    }

    if(bit_buffer_get_size_bytes(message) > 0) {
        seos_hci_h5_send(seos_hci->seos_hci_h5, HCI_COMMAND_PKT, message);
    }
    bit_buffer_free(message);
}

void seos_hci_handle_event_cmd_complete_ogf_le(SeosHci* seos_hci, uint16_t OCF, BitBuffer* frame) {
    UNUSED(frame);

    BitBuffer* message = bit_buffer_alloc(128);
    switch(OCF) {
    case OCF_LE_SET_EVENT_MASK:
        uint8_t read_local_version[] = {0x01, 0x10, 0x00};
        bit_buffer_append_bytes(message, read_local_version, sizeof(read_local_version));
        break;
    case OCF_LE_READ_BUFFER_SIZE:
        FURI_LOG_D(TAG, "OCF_LE_READ_BUFFER_SIZE");
        uint8_t le_set_random_address[] = {0x05, 0x20, 0x06, 0xCA, 0xFE, 0x00, 0x00, 0x00, 0x03};
        bit_buffer_append_bytes(message, le_set_random_address, sizeof(le_set_random_address));
        break;
    case OCF_LE_SET_ADVERTISING_DATA:
        seos_hci_enable_advertising(seos_hci, true);
        break;
    case OCF_LE_SET_SCAN_RESPONSE_DATA:
        uint8_t flow_mode_byte = seos_hci->flow_mode == FLOW_READER ? 0x00 : 0x01;
        // TODO: Use seos_reader_service_backwards
        uint8_t adv_data[] = {0x08, 0x20, 0x20, 0x15,           0x02, 0x01, 0x06, 0x11, 0x07,
                              0x02, 0x00, 0x00, 0x7a,           0x17, 0x00, 0x00, 0x80, 0x00,
                              0x10, 0x00, 0x00, flow_mode_byte, 0x98, 0x00, 0x00, 0x00, 0x00,
                              0x00, 0x00, 0x00, 0x00,           0x00, 0x00, 0x00, 0x00};
        bit_buffer_append_bytes(message, adv_data, sizeof(adv_data));
        break;
    case OCF_LE_SET_ADVERTISE_ENABLE:
        if(furi_timer_is_running(seos_hci->timer)) {
            FURI_LOG_D(TAG, "clear timer");
            furi_timer_stop(seos_hci->timer);
        }
        uint8_t status = bit_buffer_get_byte(frame, 6);
        if(status == 0) {
            if(seos_hci->adv_status) {
                FURI_LOG_I(TAG, "*** Advertising enabled ***");
                view_dispatcher_send_custom_event(
                    seos_hci->seos->view_dispatcher, SeosCustomEventAdvertising);

            } else {
                FURI_LOG_I(TAG, "*** Advertising disabled ***");
            }
        } else {
            FURI_LOG_W(TAG, "Advertising enabled FAILED");
        }
        break;
    case OCF_LE_SET_SCAN_PARAMETERS:
        seos_hci_set_scan(seos_hci, true);
        break;
    case OCF_LE_SET_SCAN_ENABLE:
        if(furi_timer_is_running(seos_hci->timer)) {
            FURI_LOG_D(TAG, "clear timer");
            furi_timer_stop(seos_hci->timer);
        }
        if(seos_hci->scan_status) { // enabled
            FURI_LOG_I(TAG, "Scan enable complete. new state: %d", seos_hci->scan_status);
            view_dispatcher_send_custom_event(
                seos_hci->seos->view_dispatcher, SeosCustomEventScan);
        } else if(seos_hci->device_found) {
            // Scanning stopped, try to connect
            seos_hci_connect(seos_hci);
        }

        break;
    case OCF_LE_READ_LOCAL_SUPPORTED_FEATURES:
        // FURI_LOG_D(TAG, "Local Supported Features");
        uint8_t set_event_mask[] = {
            0x01, 0x0c, 0x08, 0xff, 0xff, 0xfb, 0xff, 0x07, 0xf8, 0xbf, 0x3d};
        bit_buffer_append_bytes(message, set_event_mask, sizeof(set_event_mask));
        break;
    case OCF_LE_SET_RANDOM_ADDRESS:
        // FURI_LOG_D(TAG, "opcode = %04x", BT_OP(0x3f, 0x0006)); <--- reverse this in byte array
        uint8_t vendor_set_addr[] = {0x06, 0xfc, 0x06, 0x0, 0x0, 0x1, 0x2, 0x21, 0xAD};
        bit_buffer_append_bytes(message, vendor_set_addr, sizeof(vendor_set_addr));
        break;
    case OCF_LE_SET_ADVERTISING_PARAMETERS:
        // TODO: make this more dynamic
        uint8_t capabilities = CAP_TWIST_AND_GO | CAP_ALLOW_TAP | CAP_APP_SPECIFIC |
                               CAP_ENHANCED_TAP;
        int8_t tap_rssi = -75;
        int8_t twist_rssi = -75;
        int8_t seamless_rssi = -75;
        int8_t app_rssi = -75;
        uint8_t mfg_data[] = {0x14,     0xff,       0x2e,          0x01,     0x15, capabilities,
                              tap_rssi, twist_rssi, seamless_rssi, app_rssi, 0x2a, 0x46,
                              0x4c,     0x30,       0x4b,          0x37,     0x5a, 0x30,
                              0x31,     0x55,       0x31};
        uint8_t device_name[] = {0x08, 0x09, 0x46, 0x6c, 0x69, 0x70, 0x70, 0x65, 0x72};
        uint8_t ad_len = 0;
        ad_len += sizeof(device_name);
        ad_len += sizeof(mfg_data);

        uint8_t header[] = {0x09, 0x20, 0x20, ad_len};
        bit_buffer_append_bytes(message, header, sizeof(header));
        bit_buffer_append_bytes(message, device_name, sizeof(device_name));
        bit_buffer_append_bytes(message, mfg_data, sizeof(mfg_data));
        for(int i = 0; i < (31 - ad_len); i++) {
            bit_buffer_append_byte(message, 0);
        }
        break;
    default:
        FURI_LOG_W(TAG, "Unhandled OCF %04x", OCF);
        break;
    }

    if(bit_buffer_get_size_bytes(message) > 0) {
        seos_hci_h5_send(seos_hci->seos_hci_h5, HCI_COMMAND_PKT, message);
    }
    bit_buffer_free(message);
}

void seos_hci_handle_event_cmd_complete_ogf_vendor(
    SeosHci* seos_hci,
    uint16_t OCF,
    BitBuffer* frame) {
    UNUSED(frame);

    BitBuffer* message = bit_buffer_alloc(128);
    switch(OCF) {
    case 0x0006:
        if(seos_hci->mode == BLE_PERIPHERAL) {
            // Flipper as Reader
            uint8_t adv_param[] = {
                0x06,
                0x20,
                0x0f,
                0xa0,
                0x00,
                0xa0,
                0x00,
                0x00,
                0x00,
                0x01,
                0xDE,
                0xAF,
                0xBE,
                0xEF,
                0xCA,
                0xFE,
                0x07,
                0x00};
            bit_buffer_append_bytes(message, adv_param, sizeof(adv_param));
        } else if(seos_hci->mode == BLE_CENTRAL) {
            // Flipper as device/credential
            seos_hci_send_scan_params(seos_hci);
        }
        break;
    default:
        FURI_LOG_W(TAG, "Unhandled OCF %04x", OCF);
        break;
    }

    if(bit_buffer_get_size_bytes(message) > 0) {
        seos_hci_h5_send(seos_hci->seos_hci_h5, HCI_COMMAND_PKT, message);
    }
    bit_buffer_free(message);
}

void seos_hci_handle_event_cmd_complete(SeosHci* seos_hci, BitBuffer* frame) {
    BitBuffer* message = bit_buffer_alloc(128);
    const uint8_t* data = bit_buffer_get_data(frame);

    uint8_t event_type = data[0];
    uint8_t sub_event_type = data[1];
    uint8_t ncmd = data[3];
    uint16_t cmd = data[5] << 8 | data[4];
    uint8_t status = data[6];
    if(status == 0) {
        /*
        FURI_LOG_D(
            TAG,
            "event %d sub event %d ncmd %d cmd %04x status %d",
            event_type,
            sub_event_type,
            ncmd,
            cmd,
            status);
            */
    } else {
        FURI_LOG_W(
            TAG,
            "event %d sub event %d ncmd %d cmd %d status %d",
            event_type,
            sub_event_type,
            ncmd,
            cmd,
            status);
        bit_buffer_free(message);
        return;
    }

    uint16_t OGF = BT_OGF(cmd);
    uint16_t OCF = BT_OCF(cmd);
    // FURI_LOG_D(TAG, "OGF = %04x OCF = %04x", OGF, OCF);

    switch(OGF) {
    case OGF_HOST_CTL:
        seos_hci_handle_event_cmd_complete_ogf_host(seos_hci, OCF, frame);
        break;
    case OGF_INFO_PARAM:
        seos_hci_handle_event_cmd_complete_ogf_info(seos_hci, OCF, frame);
        break;
    case OGF_LE_CTL:
        seos_hci_handle_event_cmd_complete_ogf_le(seos_hci, OCF, frame);
        break;
    case OGF_VENDOR_CTL:
        seos_hci_handle_event_cmd_complete_ogf_vendor(seos_hci, OCF, frame);
        break;
    default:
        FURI_LOG_W(TAG, "Unhandled OGF %04x", OGF);
        break;
    }

    bit_buffer_free(message);
}

void seos_hci_enable_advertising(SeosHci* seos_hci, bool enable) {
    seos_hci->adv_status = enable;
    FURI_LOG_I(TAG, "Enable Advertising: %s", enable ? "true" : "false");
    uint8_t adv_enable[] = {0x0a, 0x20, 0x01, enable ? 0x01 : 0x00};
    BitBuffer* message = bit_buffer_alloc(sizeof(adv_enable));
    bit_buffer_append_bytes(message, adv_enable, sizeof(adv_enable));
    seos_hci_h5_send(seos_hci->seos_hci_h5, HCI_COMMAND_PKT, message);
    bit_buffer_free(message);

    FURI_LOG_I(TAG, "Start timer to make sure adv change ran");
    size_t delay = 1000 /*ms*/ / (1000.0f / furi_kernel_get_tick_frequency());
    furi_check(furi_timer_start(seos_hci->timer, delay) == FuriStatusOk);
}

void seos_hci_send_scan_params(SeosHci* seos_hci) {
    uint8_t LE_Scan_Type = 0x00;
    uint8_t Scanning_Filter_Policy = 0x00;
    uint16_t opcode = BT_OP(OGF_LE_CTL, OCF_LE_SET_SCAN_PARAMETERS);
    uint8_t scan_param[] = {
        0xff, 0xff, 0x07, LE_Scan_Type, 0x10, 0x00, 0x10, 0x00, 0x00, Scanning_Filter_Policy};
    BitBuffer* message = bit_buffer_alloc(sizeof(scan_param));
    memcpy(scan_param, (uint8_t*)&opcode, sizeof(opcode));
    bit_buffer_append_bytes(message, scan_param, sizeof(scan_param));
    seos_hci_h5_send(seos_hci->seos_hci_h5, HCI_COMMAND_PKT, message);
    bit_buffer_free(message);
}

void seos_hci_set_scan(SeosHci* seos_hci, bool enable) {
    FURI_LOG_I(TAG, "Start Scan: %s", enable ? "true" : "false");
    seos_hci->scan_status = enable;
    uint16_t opcode = BT_OP(OGF_LE_CTL, OCF_LE_SET_SCAN_ENABLE);
    uint8_t set_scan[] = {0xff, 0xff, 0x02, enable ? 0x01 : 0x00, 0x00};
    memcpy(set_scan, (uint8_t*)&opcode, sizeof(opcode));
    BitBuffer* message = bit_buffer_alloc(sizeof(set_scan));
    bit_buffer_append_bytes(message, set_scan, sizeof(set_scan));
    seos_hci_h5_send(seos_hci->seos_hci_h5, HCI_COMMAND_PKT, message);
    bit_buffer_free(message);

    FURI_LOG_I(TAG, "Start timer to make sure set scan ran");
    size_t delay = 100 /*ms*/ / (1000.0f / furi_kernel_get_tick_frequency());
    furi_check(furi_timer_start(seos_hci->timer, delay) == FuriStatusOk);
}

// TODO: test this: hci create le conn - writing: 010d 2019 6000 3000 00 01 2db88ee137c3 000600120000002a0004000600
void seos_hci_connect(SeosHci* seos_hci) {
    FURI_LOG_I(TAG, "seos_hci_connect");
    uint16_t opcode = BT_OP(OGF_LE_CTL, OCF_LE_CREATE_CONNECTION);
    // Values arbitrarily copied from https://stackoverflow.com/questions/71250571/how-to-send-le-extended-create-connection-in-ble-with-raspberry-pi
    uint8_t connect[] = {
        0xff,
        0xff, //opcode
        0x19, // length
        0x60,
        0x00, // LE_Scan_Interval
        0x60,
        0x00, // LE_Scan_Window
        0x00, // Initiator_Filter_Policy
        seos_hci->address_type, // Peer_Address_Type
        0xFF,
        0xFF,
        0xFF,
        0xFF,
        0xFF,
        0xFF, // Peer_Address
        0x01, // Own_Address_Type
        0x18,
        0x00, // Connection_Interval_Min
        0x28,
        0x00, // Connection_Interval_Max
        0x00,
        0x00, // Max_Latency
        0x90,
        0x00, // Supervision_Timeout
        0x00,
        0x00, // Min_CE_Length
        0x00,
        0x00, // Max_CE_Length
    };
    memcpy(connect, (uint8_t*)&opcode, sizeof(opcode));
    memcpy(connect + 9, seos_hci->address, 6);
    BitBuffer* message = bit_buffer_alloc(sizeof(connect));
    bit_buffer_append_bytes(message, connect, sizeof(connect));
    seos_hci_h5_send(seos_hci->seos_hci_h5, HCI_COMMAND_PKT, message);
    bit_buffer_free(message);
}

void seos_hci_handle_event_le_meta(SeosHci* seos_hci, BitBuffer* frame) {
    const uint8_t* data = bit_buffer_get_data(frame);
    // uint8_t length = data[2];
    uint8_t subevent_code = data[3];

    switch(subevent_code) {
    case HCI_LE_CONNECTION_COMPLETE:
        uint8_t status = data[4];
        if(status != 0x00) {
            FURI_LOG_W(TAG, "Connection complete with non-zero status");
            return;
        }

        seos_hci->connection_handle = data[6] << 8 | data[5];
        uint8_t role = data[7];
        uint8_t peer_address_type = data[8];
        // and more...

        FURI_LOG_D(
            TAG,
            "connection complete: handle %04x role %d peer_address_type %d",
            seos_hci->connection_handle,
            role,
            peer_address_type);
        view_dispatcher_send_custom_event(
            seos_hci->seos->view_dispatcher, SeosCustomEventConnected);

        if(role == 0x00) { // I'm a central!
            if(seos_hci->central_connection_callback) {
                seos_hci->central_connection_callback(seos_hci->central_connection_context);
            } else {
                FURI_LOG_W(TAG, "No central_connection_callback defined");
            }
        } else if(role == 0x01) { // I'm a peripheral!
        }

        break;
    case HCI_LE_ADVERTISING_REPORT:
        // Prevent interruptions to handling a device by a second advertisement
        if(seos_hci->device_found) {
            break;
        }
        // TODO: Support single packet with multiple reports
        uint8_t num_reports = data[4];
        uint8_t Event_Type = data[5];
        uint8_t Address_Type = data[6];
        const uint8_t* Address = data + 7;
        uint8_t Data_Length = data[13];
        const uint8_t* adv_data = data + 14;
        char name[20];
        memset(name, 0, sizeof(name));
        if(Event_Type != 0 || Data_Length < sizeof(seos_reader_service_backwards)) {
            break;
        }

        /*
        FURI_LOG_D(
            TAG,
            "Adv %d reports: event type %d address type %d data len %d",
            num_reports,
            Event_Type,
            Address_Type,
            Data_Length);
            */
        // seos_log_buffer(TAG, "ADV_IND", (uint8_t*)adv_data, Data_Length);

        uint8_t i = 0;
        do {
            uint8_t l = adv_data[i++];
            uint8_t t = adv_data[i++];
            const uint8_t* val = adv_data + i;
            i += l - 1; // subtract one so we don't overcount the type byte
            switch(t) {
            case 0x07:
                if(seos_hci->flow_mode == FLOW_CRED) {
                    // You're acting like a credential, looking for readers to connect and send to
                    if(memcmp(
                           val,
                           seos_reader_service_backwards,
                           sizeof(seos_reader_service_backwards)) == 0) {
                        seos_hci->device_found = true;
                    }
                } else if(seos_hci->flow_mode == FLOW_READER) {
                    if(memcmp(
                           val,
                           seos_cred_service_backwards,
                           sizeof(seos_cred_service_backwards)) == 0) {
                        seos_hci->device_found = true;
                    }
                } else if(seos_hci->flow_mode == FLOW_READER_SCANNER) {
                    // Reader scanner looks for readers, it doesn't act like a reader (as in FLOW_READER)
                    if(memcmp(
                           val,
                           seos_reader_service_backwards,
                           sizeof(seos_reader_service_backwards)) == 0) {
                        if(!seos_hci_known_address(seos_hci, Address)) {
                            notification_message(
                                seos_hci->seos->notifications, &sequence_single_vibro);
                        }
                    }
                } else if(seos_hci->flow_mode == FLOW_CRED_SCANNER) {
                    // Cred scanner looks for devices advertising credential service, it doesn't act like a credential (as in FLOW_CRED)
                    if(memcmp(
                           val,
                           seos_cred_service_backwards,
                           sizeof(seos_cred_service_backwards)) == 0) {
                        if(!seos_hci_known_address(seos_hci, Address)) {
                            notification_message(
                                seos_hci->seos->notifications, &sequence_single_vibro);
                        }
                    }
                }
                break;
            case 0x08: // Short device name
            case 0x09: // full device name
                memcpy(name, val, l - 1);
                break;
            }
        } while(i < Data_Length - 1);

        seos_hci->adv_report_count += num_reports;

        if(seos_hci->device_found) {
            FURI_LOG_I(TAG, "Matched Seos Reader Service: %s", name);
            seos_hci->address_type = Address_Type;
            memcpy(seos_hci->address, Address, sizeof(seos_hci->address));
            seos_hci_set_scan(seos_hci, false);
            view_dispatcher_send_custom_event(
                seos_hci->seos->view_dispatcher, SeosCustomEventFound);
        }
        break;
    default:
        FURI_LOG_W(TAG, "LE Meta event with unknown subevent code");
        break;
    }
}

void seos_hci_event_handler(SeosHci* seos_hci, BitBuffer* frame) {
    const uint8_t* data = bit_buffer_get_data(frame);
    uint8_t sub_event_type = data[1];
    // uint8_t length = data[2];

    if(sub_event_type == BT_HCI_EVT_CMD_STATUS) {
        struct bt_hci_evt_cmd_status {
            uint8_t status;
            uint8_t ncmd;
            uint16_t opcode;
        } __packed;
        struct bt_hci_evt_cmd_status* status = (struct bt_hci_evt_cmd_status*)(data + 3);
        if(status->status == 0) {
            /*
            FURI_LOG_D(
                TAG,
                "Status: status %d ncmd 0x%02x opcode %04x",
                status->status,
                status->ncmd,
                status->opcode);
                */
        } else {
            // Unknown HCI command (0x01)
            FURI_LOG_W(
                TAG,
                "Status: status %d ncmd 0x%02x opcode %04x",
                status->status,
                status->ncmd,
                status->opcode);
        }
    } else if(sub_event_type == BT_HCI_EVT_CMD_COMPLETE) {
        seos_hci_handle_event_cmd_complete(seos_hci, frame);
    } else if(sub_event_type == BT_HCI_EVT_LE_META) {
        seos_hci_handle_event_le_meta(seos_hci, frame);
    } else if(sub_event_type == BT_HCI_EVT_DISCONN_COMPLETE) {
        // FURI_LOG_D(TAG, "BT_HCI_EVT_CMD_COMPLETE");
        seos_hci_handle_event_cmd_complete(seos_hci, frame);
    } else if(sub_event_type == BT_HCI_EVT_LE_META) {
        // FURI_LOG_D(TAG, "BT_HCI_EVT_LE_META");
        seos_hci_handle_event_le_meta(seos_hci, frame);
    } else if(sub_event_type == BT_HCI_EVT_DISCONN_COMPLETE) {
        // FURI_LOG_D(TAG, "BT_HCI_EVT_DISCONN_COMPLETE");
        seos_hci->connection_handle = 0;

        if(seos_hci->mode == BLE_PERIPHERAL) {
            if(seos_hci->adv_status) {
                FURI_LOG_W(TAG, "Disconnect. Restart Advertising");
                seos_hci_enable_advertising(seos_hci, true);
            }
        } else if(seos_hci->mode == BLE_CENTRAL) {
            FURI_LOG_W(TAG, "Disconnect. Scan again");
            seos_hci->device_found = false;
            seos_hci_set_scan(seos_hci, true);
        }
    } else if(sub_event_type == BT_HCI_EVT_HARDWARE_ERROR) {
        FURI_LOG_W(TAG, "BT_HCI_EVT_HARDWARE_ERROR");
    } else if(sub_event_type == BT_HCI_EVT_NUM_COMPLETED_PACKETS) {
        // FURI_LOG_D(TAG, "BT_HCI_EVT_NUM_COMPLETED_PACKETS");
        struct bt_hci_evt_num_completed_packets {
            uint8_t num_handles;
            uint16_t handle;
            uint16_t count;
        } __attribute__((packed));
        struct bt_hci_evt_num_completed_packets* evt =
            (struct bt_hci_evt_num_completed_packets*)(data + 3);
        if(evt->num_handles == 1) {
            // FURI_LOG_D(TAG, "Number of completed packets for %04x: %d", evt->handle, evt->count);
        } else {
            FURI_LOG_D(TAG, "Number of completed packets for multiple handles");
        }
        if(seos_hci->completed_packets_callback) {
            seos_hci->completed_packets_callback(seos_hci->completed_packets_context);
        }
    } else {
        FURI_LOG_W(TAG, "Unhandled event subtype %02x", sub_event_type);
    }
}

void seos_hci_acldata_send(SeosHci* seos_hci, uint8_t flags, BitBuffer* tx) {
    // seos_log_buffer("seos_hci_acldata_send", tx);
    uint16_t tx_len = bit_buffer_get_size_bytes(tx);

    uint16_t handle = seos_hci->connection_handle | (flags << 12);

    BitBuffer* response = bit_buffer_alloc(tx_len + sizeof(handle) + sizeof(tx_len));
    bit_buffer_append_bytes(response, (uint8_t*)&handle, sizeof(handle));
    bit_buffer_append_bytes(response, (uint8_t*)&tx_len, sizeof(tx_len));
    // tx
    bit_buffer_append_bytes(response, bit_buffer_get_data(tx), tx_len);

    seos_hci_h5_send(seos_hci->seos_hci_h5, HCI_ACLDATA_PKT, response);

    bit_buffer_free(response);
}

void seos_hci_acldata_handler(SeosHci* seos_hci, BitBuffer* frame) {
    const uint8_t* data = bit_buffer_get_data(frame);
    // 0 is 0x02 for ACL DATA

    uint16_t handle = (data[2] << 8 | data[1]) & 0x0FFF;
    uint8_t flags = data[2] >> 4;
    uint16_t length = data[4] << 8 | data[3];

    /*
    uint8_t Broadcast_Flag = flags >> 2;
    uint8_t Packet_Boundary_Flag = flags & 0x03;

    FURI_LOG_D(
        TAG,
        "ACLDATA handle %04x Broadcast_Flag %02x Packet_Boundary_Flag %02x length %d",
        handle,
        Broadcast_Flag,
        Packet_Boundary_Flag,
        length);
        */
    if(handle != seos_hci->connection_handle) {
        FURI_LOG_W(TAG, "Mismatched handle values");
    }

    BitBuffer* pdu = bit_buffer_alloc(length);
    bit_buffer_append_bytes(pdu, data + 1 /*ACL DATA */ + sizeof(handle) + sizeof(length), length);
    if(seos_hci->receive_callback) {
        seos_hci->receive_callback(seos_hci->receive_callback_context, handle, flags, pdu);
    }
    bit_buffer_free(pdu);
}

size_t seos_hci_recv(void* context, BitBuffer* frame) {
    SeosHci* seos_hci = (SeosHci*)context;
    // seos_log_buffer("HCI Frame", frame);

    const uint8_t* data = bit_buffer_get_data(frame);
    uint8_t event_type = data[0];
    // TODO: consider `bit_buffer_starts_with_byte`
    switch(event_type) {
    case HCI_EVENT_PKT:
        seos_hci_event_handler(seos_hci, frame);
        break;
    case HCI_ACLDATA_PKT:
        seos_hci_acldata_handler(seos_hci, frame);
        break;
    default:
        FURI_LOG_W(TAG, "Haven't added support for other HCI commands yet: %02x", event_type);
        seos_log_bitbuffer(TAG, "unhandled", frame);
        break;
    }

    return 0;
}

// TODO: Consider making this a general "when the state changes" callback which would check if H5 is active (or needs to be reset)
void seos_hci_init(void* context) {
    SeosHci* seos_hci = (SeosHci*)context;
    BitBuffer* message = bit_buffer_alloc(128);

    uint8_t reset[] = {0x03, 0x0c, 0x00};
    bit_buffer_append_bytes(message, reset, sizeof(reset));
    seos_hci_h5_send(seos_hci->seos_hci_h5, HCI_COMMAND_PKT, message);
    view_dispatcher_send_custom_event(seos_hci->seos->view_dispatcher, SeosCustomEventHCIInit);

    bit_buffer_free(message);
}

void seos_hci_set_receive_callback(
    SeosHci* seos_hci,
    SeosHciReceiveCallback callback,
    void* context) {
    seos_hci->receive_callback = callback;
    seos_hci->receive_callback_context = context;
}

void seos_hci_set_completed_packets_callback(
    SeosHci* seos_hci,
    SeosHciCompletedPacketsCallback callback,
    void* context) {
    seos_hci->completed_packets_callback = callback;
    seos_hci->completed_packets_context = context;
}

void seos_hci_set_central_connection_callback(
    SeosHci* seos_hci,
    SeosHciCentralConnectionCallback callback,
    void* context) {
    seos_hci->central_connection_callback = callback;
    seos_hci->central_connection_context = context;
}
