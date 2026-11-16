#pragma once

#include <furi.h>
#include <furi_hal_usb.h>

#define USB_AUDIO_RATE_DEFAULT 48000U

typedef struct {
    bool configured; // host enumerated and set configuration
    bool suspended;
    bool streaming; // alt setting 1 selected and I2S clocks running
    bool receiving; // packets arrived within the last few ms
    uint32_t sample_rate; // rate requested by host
    uint8_t bits; // 16 or 24, selected by host via alt setting
    uint32_t actual_rate_mhz; // rate produced by the clock tree, mHz
    uint32_t measured_rate; // I2S frames per second, measured against USB SOF
    uint32_t bclk; // Hz
    uint32_t sai_clk; // Hz
    uint32_t mclk; // Hz, 0 when no MCLK
    uint32_t feedback; // 10.14 samples per frame
    uint32_t fill_pct; // ring buffer fill, %
    uint32_t packets;
    uint32_t xruns; // overflow / underflow resyncs
    int16_t volume_db256; // 1/256 dB
    bool mute;
    uint16_t peak_l; // since last read
    uint16_t peak_r;
} UsbAudioStats;

extern FuriHalUsbInterface usb_audio_interface;

/** Copies a consistent snapshot of the driver state; resets peak meters. */
void usb_audio_get_stats(UsbAudioStats* stats);

/** Rebuilds the configuration descriptor for the current I2S clock source.
 *  Call before furi_hal_usb_set_config() whenever the source changes. */
void usb_audio_build_descriptor(void);
