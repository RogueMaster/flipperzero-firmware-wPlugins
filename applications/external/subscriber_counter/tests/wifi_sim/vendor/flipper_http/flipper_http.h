/* Host stand-in for FlipperHTTP: same names, a scripted board behind it. */
#pragma once
#include <stdbool.h>
typedef void (*FlipperHTTP_Callback)(const char* line, void* context);
typedef enum {
    GET,
    POST
} HTTPMethod;
typedef enum {
    HTTP_CMD_WIFI_CONNECT,
    HTTP_CMD_PING
} HTTPCommand;
typedef struct {
    FlipperHTTP_Callback user_rx_line_cb;
    void* user_callback_context;
} FlipperHTTP;
FlipperHTTP* flipper_http_alloc(void);
void flipper_http_free(FlipperHTTP* fhttp);
bool flipper_http_send_command(FlipperHTTP* fhttp, HTTPCommand command);
bool flipper_http_save_wifi(FlipperHTTP* fhttp, const char* ssid, const char* password);
bool flipper_http_request(
    FlipperHTTP* fhttp,
    HTTPMethod method,
    const char* url,
    const char* headers,
    const char* payload);
