#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// Minimal QR code generator – byte mode, ECC-L, versions 1-10.
// Buffer size for version n: ((n*4+17)^2 + 7) / 8 + 1 bytes.
// Use QRCODEGEN_BUF_LEN(10) = 196 bytes (version 10 worst case).

#define QRCODEGEN_BUF_LEN(ver) (((((ver)*4+17)*((ver)*4+17))+7)/8+1)
#define QRCODEGEN_BUF_MAX      QRCODEGEN_BUF_LEN(10)  // 196 bytes

// Encode text as a QR code using byte mode and ECC level L.
// tempBuffer and qrcode must each be at least QRCODEGEN_BUF_MAX bytes.
// Returns true on success, false if text is too long for version 10-L.
bool nfc_tools_qr_encode(
    const char* text,
    uint8_t     tempBuffer[QRCODEGEN_BUF_MAX],
    uint8_t     qrcode[QRCODEGEN_BUF_MAX]);

// Return the side length (in modules) of an encoded QR code.
int  nfc_tools_qr_size(const uint8_t qrcode[QRCODEGEN_BUF_MAX]);

// Return whether module (x,y) is dark (true = black pixel).
bool nfc_tools_qr_module(const uint8_t qrcode[QRCODEGEN_BUF_MAX], int x, int y);
