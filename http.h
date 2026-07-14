// Minimal FlipperHTTP client over the GPIO UART (USART, 115200). Speaks just the
// commands Pack Track needs: PING, WiFi connect, and GET-with-headers.
#pragma once

#include <stdbool.h>
#include <stddef.h>

typedef struct FhttpClient FhttpClient;

FhttpClient* fhttp_alloc(void);
void fhttp_free(FhttpClient* c);

// Acquire the UART (disables the expansion listener). Returns false if the port
// is unavailable. Pair with fhttp_close().
bool fhttp_open(FhttpClient* c);
void fhttp_close(FhttpClient* c);

// [PING] -> [PONG]. Confirms a FlipperHTTP board is attached and responsive.
bool fhttp_ping(FhttpClient* c);

// Save credentials and connect. Returns true once connected.
bool fhttp_wifi(FhttpClient* c, const char* ssid, const char* pass);

// GET `url` with optional "Name: Value" headers. The response body (between
// [GET/SUCCESS] and [GET/END]) is copied into out (bounded). Returns false on
// timeout or [ERROR].
bool fhttp_get(
    FhttpClient* c,
    const char* url,
    const char* const* headers,
    int header_count,
    char* out,
    size_t out_cap);
