#pragma once
#include "furi.h"

void http_deinit();
bool http_init(void *httpInstance);
bool http_get_http_response(char *buffer, size_t buffer_size);
bool http_get_websocket_response(char *buffer, size_t buffer_size);
bool http_websocket_is_connected();
bool http_is_finished();
bool http_send_request(
    const char *url,
    const char *method,
    const char *headers = "{\"Content-Type\": \"application/json\"}",
    const char *payload = nullptr);
bool http_websocket_send(const char *message);
bool http_websocket_start(const char *url, uint16_t port);
bool http_websocket_stop();