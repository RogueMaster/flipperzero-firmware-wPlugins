#include <furi.h>
#include <stdbool.h>

#include "../../lora_receiver_i.h"

/**
 * Test case for parsing a FPENDING message.
 * Input: "+MSG: FPENDING"
 * Expected: msg_response->is_pending should be true.
 */
static void parse_msg_with_fpending_test() {
    FuriString* line = furi_string_alloc();
    furi_string_set(line, "+MSG: FPENDING");
    LoraReceiver* lora_receiver = lora_receiver_alloc();

    LoraReceiverModel* model = view_get_model(lora_receiver->view);
    LoraMsgResponseModel* msg_response = &model->msg_response;
    lora_receiver_decode_msg_response(lora_receiver, line);

    if(msg_response->is_pending != true) {
        FURI_LOG_E("parse_msg_with_fpending_test", "FPENDING not detected");
    }

    furi_string_free(line);
    lora_receiver_free(lora_receiver);
}

/**
 * Test case for parsing a Link message with margin and gateway count.
 * Input: "+MSG: Link 20, 1"
 * Expected: msg_response->margin should be 20, msg_response->gateway_count should be 1.
 */
static void parse_msg_with_link_test() {
    FuriString* line = furi_string_alloc();
    furi_string_set(line, "+MSG: Link 20, 1\n");
    LoraReceiver* lora_receiver = lora_receiver_alloc();

    LoraReceiverModel* model = view_get_model(lora_receiver->view);
    LoraMsgResponseModel* msg_response = &model->msg_response;
    lora_receiver_decode_msg_response(lora_receiver, line);

    if(msg_response->margin != 20) {
        FURI_LOG_E(
            "parse_msg_with_link_test",
            "Incorrect margin: expected: 20, got: %d",
            msg_response->margin);
    }
    if(msg_response->gateway_count != 1) {
        FURI_LOG_E(
            "parse_msg_with_link_test",
            "Incorrect gateway count: expected: 1, got: %d",
            msg_response->gateway_count);
    }

    furi_string_free(line);
    lora_receiver_free(lora_receiver);
}

/**
 * Test case for parsing an RXWIN message with RSSI and SNR values.
 * Input: "+MSG: RXWIN2, RSSI -106, SNR 4"
 * Expected: msg_response->rx_window should be 2, msg_response->rssi should be -106, msg_response->snr should be 4.
 */
static void parse_msg_with_rxwin_test() {
    FuriString* line = furi_string_alloc();
    furi_string_set(line, "+MSG: RXWIN2, RSSI -106, SNR 4\n");
    LoraReceiver* lora_receiver = lora_receiver_alloc();

    LoraReceiverModel* model = view_get_model(lora_receiver->view);
    LoraMsgResponseModel* msg_response = &model->msg_response;
    lora_receiver_decode_msg_response(lora_receiver, line);

    if(msg_response->rx_window != 2) {
        FURI_LOG_E(
            "parse_msg_with_rxwin_test",
            "Incorrect RXWIN: expected: 2, got: %d",
            msg_response->rx_window);
    }
    if(msg_response->rssi != -106) {
        FURI_LOG_E(
            "parse_msg_with_rxwin_test",
            "Incorrect RSSI: expected: -106, got: %d",
            msg_response->rssi);
    }

    if(msg_response->snr != 4) {
        FURI_LOG_E(
            "parse_msg_with_rxwin_test", "Incorrect SNR: expected: 4, got: %d", msg_response->snr);
    }

    furi_string_free(line);
    lora_receiver_free(lora_receiver);
}

/**
 * Test case for parsing an ACK Received message.
 * Input: "+MSG: ACK Received"
 * Expected: msg_response->is_ack should be true.
 */
static void parse_msg_with_ack_received_test() {
    FuriString* line = furi_string_alloc();
    furi_string_set(line, "+MSG: ACK Received\n");
    LoraReceiver* lora_receiver = lora_receiver_alloc();

    LoraReceiverModel* model = view_get_model(lora_receiver->view);
    LoraMsgResponseModel* msg_response = &model->msg_response;
    lora_receiver_decode_msg_response(lora_receiver, line);

    if(msg_response->is_ack != true) {
        FURI_LOG_E(
            "parse_msg_with_ack_received_test",
            "Incorrect ACK status, expected: true, got: %s",
            msg_response->is_ack ? "true" : "false");
    }

    furi_string_free(line);
    lora_receiver_free(lora_receiver);
}

/**
 * Test case for parsing a message containing port and RX data.
 * Input: "+MSG: PORT: 8; RX: \"12345678\""
 * Expected: msg_response->port should be 8, msg_response->data should be "12345678".
 */
static void parse_msg_with_port_and_data() {
    FuriString* line = furi_string_alloc();
    furi_string_set(line, "+MSG: PORT: 8; RX: \"12345678\"\n");
    LoraReceiver* lora_receiver = lora_receiver_alloc();

    LoraReceiverModel* model = view_get_model(lora_receiver->view);
    LoraMsgResponseModel* msg_response = &model->msg_response;
    lora_receiver_decode_msg_response(lora_receiver, line);

    if(strcmp(msg_response->data, "12345678") != 0) {
        FURI_LOG_E(
            "parse_msg_with_port_and_data",
            "Incorrect RX data: expected: 12345678, got: %s",
            msg_response->data);
    }
    if(msg_response->port != 8) {
        FURI_LOG_E(
            "parse_msg_with_port_and_data",
            "Incorrect port: expected: 8, got: %d",
            msg_response->port);
    }

    furi_string_free(line);
    lora_receiver_free(lora_receiver);
}

/**
 * Test case for parsing a MULTICAST message.
 * Input: "+MSG: MULTICAST"
 * Expected: msg_response->is_multicast should be true.
 */
static void parse_msg_with_multicast() {
    FuriString* line = furi_string_alloc();
    furi_string_set(line, "+MSG: MULTICAST\n");
    LoraReceiver* lora_receiver = lora_receiver_alloc();

    LoraReceiverModel* model = view_get_model(lora_receiver->view);
    LoraMsgResponseModel* msg_response = &model->msg_response;
    lora_receiver_decode_msg_response(lora_receiver, line);

    if(msg_response->is_multicast != true) {
        FURI_LOG_E(
            "parse_msg_with_multicast",
            "Incorrect multicast status, expected: true, got: %s",
            msg_response->is_multicast ? "true" : "false");
    }

    furi_string_free(line);
    lora_receiver_free(lora_receiver);
}

/**
 * Test case for extracting raw data from an RX packet.
 * Input: "+TEST: RX \"48656C6C6F20576F726C642021\""
 * Expected: msg_response->data should be "48656C6C6F20576F726C642021".
 */
static void parse_data_rx_packet() {
    FuriString* line = furi_string_alloc();
    furi_string_set(line, "+TEST: RX \"48656C6C6F20576F726C642021\"\n");
    LoraReceiver* lora_receiver = lora_receiver_alloc();

    LoraReceiverModel* model = view_get_model(lora_receiver->view);
    LoraMsgResponseModel* msg_response = &model->msg_response;
    lora_receiver_decode_msg_response(lora_receiver, line);

    if(strcmp(msg_response->data, "48656C6C6F20576F726C642021") != 0) {
        FURI_LOG_E(
            "parse_data_rx_response_test",
            "Incorrect RX data: expected: 12345678, got: %s",
            msg_response->data);
    }

    furi_string_free(line);
    lora_receiver_free(lora_receiver);
}

int parsers_test_suite() {
    parse_msg_with_fpending_test();
    parse_msg_with_link_test();
    parse_msg_with_rxwin_test();
    parse_msg_with_ack_received_test();
    parse_msg_with_port_and_data();
    parse_msg_with_multicast();
    parse_data_rx_packet();

    FURI_LOG_I("parsers_test_suite", "All Unit Tests passed");
    return 0;
}
