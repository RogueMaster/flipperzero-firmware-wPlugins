#include "sr_io.h"

#include "sr_rx_policy.h"

#include <furi_hal.h>
#include <expansion/expansion.h>
#include <power/power_service/power.h>
#include <string.h>

#define SR_IO_TAG "SrIo"
#define SR_IO_RX_CAP 2048u
#define SR_IO_RX_TRIGGER 1u

/* These three numbers come from the neighboring project's on-device values (otg_gate.h:6-7 /
 * scout_drive.c:33); they are not measured optima for this project. B1 must record which sample
 * index actually turned physical=true. */
#define SR_IO_OTG_SAMPLES 6u
#define SR_IO_OTG_SAMPLE_MS 20u
#define SR_IO_OTG_SETTLE_MS 200u
/* Must stay equal to the upstream threshold in
 * applications/services/power/power_service/power.c ("voltage_vbus < 4.5f", read 2026-09-02).
 * Above it the charger refuses to boost and pin 1 is fed from USB VBUS instead (V-072). */
#define SR_IO_VBUS_PRESENT_V 4.5f

_Static_assert(
    (uint32_t)FuriHalSerialRxEventData == SR_RX_BIT_DATA,
    "FuriHalSerialRxEventData must match SR_RX_BIT_DATA");
_Static_assert(
    (uint32_t)FuriHalSerialRxEventIdle == SR_RX_BIT_IDLE,
    "FuriHalSerialRxEventIdle must match SR_RX_BIT_IDLE");
_Static_assert(
    (uint32_t)FuriHalSerialRxEventFrameError == SR_RX_BIT_FRAME,
    "FuriHalSerialRxEventFrameError must match SR_RX_BIT_FRAME");
_Static_assert(
    (uint32_t)FuriHalSerialRxEventNoiseError == SR_RX_BIT_NOISE,
    "FuriHalSerialRxEventNoiseError must match SR_RX_BIT_NOISE");
_Static_assert(
    (uint32_t)FuriHalSerialRxEventOverrunError == SR_RX_BIT_OVERRUN,
    "FuriHalSerialRxEventOverrunError must match SR_RX_BIT_OVERRUN");
_Static_assert(
    (uint32_t)FuriHalSerialRxEventParityError == SR_RX_BIT_PARITY,
    "FuriHalSerialRxEventParityError must match SR_RX_BIT_PARITY");

struct SrIo {
    FuriStreamBuffer* rx;
    FuriHalSerialHandle* handle;
    Expansion* expansion;
    Power* power;
    bool open;
    bool closing;
    bool rx_running;
    bool otg_by_us;
    SrIoStats stats;
};

const char* sr_io_status_str(SrIoStatus st) {
    switch(st) {
    case SrIoOk:
        return "ok";
    case SrIoErrState:
        return "bad state";
    case SrIoErrLogDevice:
        return "log device";
    case SrIoErrNo5v:
        return "no 5v";
    case SrIoErrPortBusy:
        return "port busy";
    case SrIoErrBadBaud:
        return "bad baud";
    case SrIoErrAlloc:
        return "alloc";
    default:
        return "unknown";
    }
}

static void sr_io_rx_cb(FuriHalSerialHandle* handle, FuriHalSerialRxEvent event, void* context) {
    SrIo* io = context;
    uint32_t bits;
    bool closing;
    SrRxDecision d;

    if(io == NULL) {
        return;
    }

    bits = (uint32_t)event;
    __atomic_fetch_or(&io->stats.rx_event_mask, bits, __ATOMIC_RELAXED);
    closing = __atomic_load_n(&io->closing, __ATOMIC_ACQUIRE);
    d = sr_rx_decide(bits, closing);

    if(d.error) {
        __atomic_add_fetch(&io->stats.rx_errors, 1u, __ATOMIC_RELAXED);
    }

    if(d.drain) {
        uint8_t byte = furi_hal_serial_async_rx(handle);
        if(d.enqueue) {
            if(furi_stream_buffer_send(io->rx, &byte, 1, 0) != 1) {
                __atomic_add_fetch(&io->stats.rx_dropped, 1u, __ATOMIC_RELAXED);
            } else {
                __atomic_add_fetch(&io->stats.rx_bytes, 1u, __ATOMIC_RELAXED);
            }
        }
    }
}

SrIo* sr_io_alloc(void) {
    SrIo* io = malloc(sizeof(SrIo));
    if(io == NULL) {
        return NULL;
    }
    memset(io, 0, sizeof(*io));
    io->rx = furi_stream_buffer_alloc(SR_IO_RX_CAP, SR_IO_RX_TRIGGER);
    if(io->rx == NULL) {
        free(io);
        return NULL;
    }
    return io;
}

void sr_io_rx_stop(SrIo* io) {
    if(io == NULL || !io->open || !io->rx_running) {
        return;
    }

    __atomic_store_n(&io->closing, true, __ATOMIC_RELEASE);
    furi_hal_serial_async_rx_stop(io->handle);
    io->rx_running = false;
}

void sr_io_close(SrIo* io) {
    if(io == NULL || !io->open) {
        return;
    }

    sr_io_rx_stop(io);
    furi_hal_serial_deinit(io->handle);
    furi_hal_serial_control_release(io->handle);
    io->handle = NULL;

    expansion_enable(io->expansion);
    furi_record_close(RECORD_EXPANSION);
    io->expansion = NULL;

    if(io->otg_by_us) {
        power_enable_otg(io->power, false);
    }
    furi_record_close(RECORD_POWER);
    io->otg_by_us = false;
    io->power = NULL;

    io->open = false;
    io->stats.closes++;
}

void sr_io_free(SrIo* io) {
    if(io == NULL) {
        return;
    }
    if(io->open) {
        sr_io_close(io);
    }
    if(io->rx != NULL) {
        furi_stream_buffer_free(io->rx);
        io->rx = NULL;
    }
    free(io);
}

SrIoStatus sr_io_open(SrIo* io, uint32_t baud) {
    bool requested;
    bool physical;
    bool usb_5v;
    float vbus;
    uint32_t vbus_cv; /* VBUS in centivolts, rounded -- see the note at the log line */
    uint32_t sample;

    /* 1 */ if(io == NULL) {
        return SrIoErrState;
    }
    /* 2 */ if(io->open) {
        return SrIoErrState;
    }
    /* 3 */ if(furi_hal_rtc_get_log_device() == FuriHalRtcLogDeviceUsart) {
        return SrIoErrLogDevice;
    }

    /*
     * 4 Power gate. Pin 1's 5V has **two independent sources** and they must not be conflated
     * with each other, nor with the OTG *request* flag (V-072):
     *
     *   a) USB VBUS pass-through -- live whenever VBUS >= 4.5 V. In this case the power service
     *      deliberately refuses to start the boost, so furi_hal_power_is_otg_enabled() is false
     *      **by design** and says nothing about whether pin 1 is powered. Upstream
     *      applications/services/power/power_service/power.c verbatim:
     *          // Only try to enable if VBUS voltage is low, otherwise charger will refuse
     *          if(power->info.voltage_vbus < 4.5f) { ...enable... }
     *          else { FURI_LOG_W(TAG, "Postponing OTG enable: VBUS(%0.1f) >= 4.5v", ...); }
     *   b) the battery boost -- only reachable while VBUS < 4.5 V.
     *
     * Gating solely on (b), as this function used to, refused to open the port whenever the
     * Flipper was on USB even though the board was powered the whole time. That is what made
     * "CLI and data stream are mutually exclusive" (V-053) look like a hardware law; it was ours.
     *
     * ** The OTG request is deliberately left standing in the USB case.** It is postponed rather
     * than rejected, and the power service's own tick enables the boost as soon as VBUS drops
     * (power.c, comment verbatim: "Change OTG state if needed (i.e. after disconnecting USB
     * power)"). Withdrawing it here -- which the old rollback path did -- is exactly why
     * unplugging USB used to require leaving and re-entering the app (V-053 note 8). Keeping it
     * standing gives a seamless car-charger -> battery handover.
     */
    io->power = furi_record_open(RECORD_POWER);
    requested = power_is_otg_enabled(io->power);
    io->otg_by_us = false;
    if(!requested) {
        power_enable_otg(io->power, true);
        io->otg_by_us = true;
    }

    vbus = furi_hal_power_get_usb_voltage();
    usb_5v = (vbus >= SR_IO_VBUS_PRESENT_V);

    /* One immediate read for the log in either case; only wait for the boost to rise when it is
     * the source we actually depend on. Polling under USB would burn SAMPLES*SAMPLE_MS waiting
     * for a state the charger has already refused to enter. */
    physical = furi_hal_power_is_otg_enabled();
    sample = 0;
    if(!usb_5v) {
        for(; !physical && sample < SR_IO_OTG_SAMPLES - 1u; sample++) {
            furi_delay_ms(SR_IO_OTG_SAMPLE_MS);
            physical = furi_hal_power_is_otg_enabled();
        }
    }

    /* Round into centivolts before splitting. Truncating the fraction separately gets it wrong
     * whenever the float sits just below the decimal value -- measured: 5.04 printed as "5.03",
     * 5.2 as "5.19", and a negative reading as "0.-50". This line's whole purpose is to record
     * VBUS as evidence, so an off-by-0.01 defeats it. The ADC cannot report negative; the clamp
     * exists so a bad reading degrades to 0.00 rather than to garbage. */
    vbus_cv = (vbus >= 0.0f) ? (uint32_t)(vbus * 100.0f + 0.5f) : 0u;

    FURI_LOG_I(
        SR_IO_TAG,
        "pwr: vbus=%lu.%02lu usb_5v=%d requested=%d physical=%d otg_by_us=%d sample=%lu",
        (unsigned long)(vbus_cv / 100u),
        (unsigned long)(vbus_cv % 100u),
        usb_5v ? 1 : 0,
        requested ? 1 : 0,
        physical ? 1 : 0,
        io->otg_by_us ? 1 : 0,
        (unsigned long)(physical ? (sample + 1u) : 0u));

    if(!usb_5v && !physical) {
        if(io->otg_by_us) {
            power_enable_otg(io->power, false);
        }
        io->otg_by_us = false;
        furi_record_close(RECORD_POWER);
        io->power = NULL;
        return SrIoErrNo5v;
    }
    furi_delay_ms(SR_IO_OTG_SETTLE_MS);

    /* 5 expansion MUST be disabled before acquire. */
    io->expansion = furi_record_open(RECORD_EXPANSION);
    expansion_disable(io->expansion);
    io->handle = furi_hal_serial_control_acquire(FuriHalSerialIdUsart);
    if(io->handle == NULL) {
        expansion_enable(io->expansion);
        furi_record_close(RECORD_EXPANSION);
        io->expansion = NULL;
        if(io->otg_by_us) {
            power_enable_otg(io->power, false);
        }
        io->otg_by_us = false;
        furi_record_close(RECORD_POWER);
        io->power = NULL;
        return SrIoErrPortBusy;
    }

    /* 6 The baud rate check must come after acquire (it needs the handle). */
    if(!furi_hal_serial_is_baud_rate_supported(io->handle, baud)) {
        furi_hal_serial_control_release(io->handle);
        io->handle = NULL;
        expansion_enable(io->expansion);
        furi_record_close(RECORD_EXPANSION);
        io->expansion = NULL;
        if(io->otg_by_us) {
            power_enable_otg(io->power, false);
        }
        io->otg_by_us = false;
        furi_record_close(RECORD_POWER);
        io->power = NULL;
        return SrIoErrBadBaud;
    }

    if(furi_stream_buffer_reset(io->rx) != FuriStatusOk) {
        furi_hal_serial_control_release(io->handle);
        io->handle = NULL;
        expansion_enable(io->expansion);
        furi_record_close(RECORD_EXPANSION);
        io->expansion = NULL;
        if(io->otg_by_us) {
            power_enable_otg(io->power, false);
        }
        io->otg_by_us = false;
        furi_record_close(RECORD_POWER);
        io->power = NULL;
        return SrIoErrState;
    }

    furi_hal_serial_init(io->handle, baud);
    /* closing must be cleared before async_rx_start, or the first interrupt may read the true
     * left over from the previous close. */
    __atomic_store_n(&io->closing, false, __ATOMIC_RELEASE);
    furi_hal_serial_async_rx_start(io->handle, sr_io_rx_cb, io, /*report_errors=*/true);
    io->rx_running = true;
    io->open = true;
    io->stats.opens++;
    return SrIoOk;
}

bool sr_io_is_open(const SrIo* io) {
    return io != NULL && io->open;
}

size_t sr_io_read(SrIo* io, uint8_t* buf, size_t cap, uint32_t timeout_ms) {
    size_t n;
    size_t fill;
    uint32_t prev;

    if(io == NULL || buf == NULL || cap == 0u || !io->open) {
        return 0;
    }
    n = furi_stream_buffer_receive(io->rx, buf, cap, timeout_ms);
    /* receive atomically takes min(cap, available), so "what was taken + what remains after" is
     * approximately the available at the moment of taking.
     * Sampling happens on the worker thread, not in the ISR (per the known hazard: the RX callback
     * does nothing but furi_stream_buffer_send).
     * This is a lower bound: a peak that appears and is drained between two reads is invisible,
     * while concurrent ISR writes bias the estimate slightly high.
     * Later readers must not treat it as an exact peak. */
    fill = n + furi_stream_buffer_bytes_available(io->rx);
    prev = __atomic_load_n(&io->stats.rx_max_fill, __ATOMIC_RELAXED);
    if((uint32_t)fill > prev) {
        __atomic_store_n(&io->stats.rx_max_fill, (uint32_t)fill, __ATOMIC_RELAXED);
    }
    return n;
}

bool sr_io_write(SrIo* io, const void* data, size_t len) {
    if(io == NULL || !io->open || io->handle == NULL) {
        return false;
    }
    if(len == 0u) {
        return true;
    }
    if(data == NULL) {
        return false;
    }
    furi_hal_serial_tx(io->handle, (const uint8_t*)data, len);
    furi_hal_serial_tx_wait_complete(io->handle);
    __atomic_add_fetch(&io->stats.tx_bytes, (uint32_t)len, __ATOMIC_RELAXED);
    return true;
}

void sr_io_get_stats(const SrIo* io, SrIoStats* out) {
    if(out == NULL) {
        return;
    }
    if(io == NULL) {
        memset(out, 0, sizeof(*out));
        return;
    }
    out->rx_bytes = __atomic_load_n(&io->stats.rx_bytes, __ATOMIC_RELAXED);
    out->rx_dropped = __atomic_load_n(&io->stats.rx_dropped, __ATOMIC_RELAXED);
    out->rx_event_mask = __atomic_load_n(&io->stats.rx_event_mask, __ATOMIC_RELAXED);
    out->rx_errors = __atomic_load_n(&io->stats.rx_errors, __ATOMIC_RELAXED);
    out->tx_bytes = __atomic_load_n(&io->stats.tx_bytes, __ATOMIC_RELAXED);
    out->rx_max_fill = __atomic_load_n(&io->stats.rx_max_fill, __ATOMIC_RELAXED);
    out->opens = io->stats.opens;
    out->closes = io->stats.closes;
}
