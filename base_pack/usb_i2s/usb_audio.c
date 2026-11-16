#include "usb_audio.h"
#include "i2s_out.h"

#include <furi.h>
#include <furi_hal.h>
#include <math.h>

#include "usb.h"

#define TAG "UsbAudio"

#define AUDIO_VID 0x0483
#define AUDIO_PID 0x5730

#define AUDIO_EP0_SIZE       8 // must match furi_hal_usb core
#define AUDIO_EP_OUT         0x01
#define AUDIO_EP_FB          0x82
/* 16-bit: 96 kHz * 4 bytes = 384 bytes/ms, +2 frames headroom for feedback.
 * 24-bit: 48 kHz * 6 bytes = 288 bytes/ms, +2 frames.
 * The endpoint buffer is sized for the larger one; double-buffered it takes
 * 832 of the 1024 bytes of USB packet memory, which is why 24-bit stops at
 * 48 kHz (96 kHz would need 1216 bytes). */
#define AUDIO_EP_OUT_SIZE    392
#define AUDIO_EP_OUT_SIZE_24 300
#define AUDIO_EP_FB_SIZE     3

#define AUDIO_CHANNELS 2

enum {
    StrIdxLang = 0,
    StrIdxManuf = 1,
    StrIdxProduct = 2,
};

enum {
    AudioIdInputTerminal = 1,
    AudioIdFeatureUnit = 2,
    AudioIdOutputTerminal = 3,
};

/* UAC1 constants */
#define UAC_SUBCLASS_AUDIOCONTROL   0x01
#define UAC_SUBCLASS_AUDIOSTREAMING 0x02
#define UAC_AC_HEADER               0x01
#define UAC_AC_INPUT_TERMINAL       0x02
#define UAC_AC_OUTPUT_TERMINAL      0x03
#define UAC_AC_FEATURE_UNIT         0x06
#define UAC_AS_GENERAL              0x01
#define UAC_AS_FORMAT_TYPE          0x02
#define UAC_EP_GENERAL              0x01
#define UAC_TERMINAL_USB_STREAMING  0x0101
#define UAC_TERMINAL_SPEAKER        0x0301
#define UAC_FORMAT_PCM              0x0001

#define UAC_SET_CUR 0x01
#define UAC_GET_CUR 0x81
#define UAC_GET_MIN 0x82
#define UAC_GET_MAX 0x83
#define UAC_GET_RES 0x84

#define UAC_FU_MUTE         0x01
#define UAC_FU_VOLUME       0x02
#define UAC_EP_SAMPLING_FRQ 0x01

#define VOLUME_MIN (-64 * 256)
#define VOLUME_MAX 0
#define VOLUME_RES 256

#define RATE3(r) {(r) & 0xFF, ((r) >> 8) & 0xFF, ((r) >> 16) & 0xFF}

#define MAX_RATE_24 48000U // 24-bit packets above this do not fit packet memory

enum {
    AltIdle = 0,
    Alt16Bit = 1,
    Alt24Bit = 2,
    AltCount,
};

/* Ring buffer feeding SAI DMA: 2048 stereo frames (21 ms at 96 kHz).
 * Holds int16_t samples in 16-bit mode, int32_t in 24-bit mode. */
#define RING_SAMPLES   4096U
#define RING_TARGET    (RING_SAMPLES / 2U)
#define RING_MARGIN    64U
#define RX_TIMEOUT_MS  8U
#define FB_GAIN        4 // feedback units per frame of fill error
#define FB_LIMIT_SHIFT 7 // clamp correction to nominal / 128 (~0.8%)

static struct usb_device_descriptor audio_device_desc = {
    .bLength = sizeof(struct usb_device_descriptor),
    .bDescriptorType = USB_DTYPE_DEVICE,
    .bcdUSB = VERSION_BCD(2, 0, 0),
    .bDeviceClass = USB_CLASS_PER_INTERFACE,
    .bDeviceSubClass = USB_SUBCLASS_NONE,
    .bDeviceProtocol = USB_PROTO_NONE,
    .bMaxPacketSize0 = AUDIO_EP0_SIZE,
    .idVendor = AUDIO_VID,
    .idProduct = AUDIO_PID,
    .bcdDevice = VERSION_BCD(1, 0, 0),
    .iManufacturer = StrIdxManuf,
    .iProduct = StrIdxProduct,
    .iSerialNumber = NO_DESCRIPTOR,
    .bNumConfigurations = 1,
};

static const struct usb_string_descriptor audio_manuf_desc = USB_STRING_DESC("Flipper Zero");
static const struct usb_string_descriptor audio_prod_desc = USB_STRING_DESC("Flipper USB-I2S");

/* The config descriptor is built at runtime: its rate lists depend on the
 * clock source. Largest case (5 + 3 rates) is 189 bytes. */
#define CFG_DESC_MAX    256
#define AC_BLOCK_LEN    (9 + 12 + 10 + 9) // header, input terminal, feature unit, output terminal
#define CANDIDATE_RATES 5
static const uint32_t candidate_rates[CANDIDATE_RATES] = {32000, 44100, 48000, 88200, 96000};

static uint8_t audio_cfg_desc[CFG_DESC_MAX] __attribute__((aligned(2)));

typedef struct {
    uint8_t* p;
} DescWriter;

static void put8(DescWriter* w, uint8_t v) {
    *w->p++ = v;
}

static void put16(DescWriter* w, uint16_t v) {
    put8(w, v & 0xFF);
    put8(w, v >> 8);
}

static void put_rate(DescWriter* w, uint32_t rate) {
    put16(w, rate & 0xFFFF);
    put8(w, (rate >> 16) & 0xFF);
}

static void
    desc_interface(DescWriter* w, uint8_t alt, uint8_t num, uint8_t eps, uint8_t subclass) {
    put8(w, sizeof(struct usb_interface_descriptor));
    put8(w, USB_DTYPE_INTERFACE);
    put8(w, num);
    put8(w, alt);
    put8(w, eps);
    put8(w, USB_CLASS_AUDIO);
    put8(w, subclass);
    put8(w, USB_PROTO_NONE);
    put8(w, NO_DESCRIPTOR);
}

/* Audio class endpoints carry two extra bytes (bRefresh, bSynchAddress) */
static void desc_endpoint(
    DescWriter* w,
    uint8_t addr,
    uint8_t attr,
    uint16_t size,
    uint8_t refresh,
    uint8_t synch) {
    put8(w, 9);
    put8(w, USB_DTYPE_ENDPOINT);
    put8(w, addr);
    put8(w, attr);
    put16(w, size);
    put8(w, 1); // bInterval: every frame
    put8(w, refresh);
    put8(w, synch);
}

static void
    desc_stream_alt(DescWriter* w, uint8_t alt, uint8_t bits, const uint32_t* rates, size_t count) {
    const uint32_t bytes = bits / 8U;
    const uint16_t max_packet = (rates[count - 1] / 1000U + 2U) * AUDIO_CHANNELS * bytes;

    desc_interface(w, alt, 1, 2, UAC_SUBCLASS_AUDIOSTREAMING);

    // AS general
    put8(w, 7);
    put8(w, USB_DTYPE_CS_INTERFACE);
    put8(w, UAC_AS_GENERAL);
    put8(w, AudioIdInputTerminal);
    put8(w, 1); // bDelay
    put16(w, UAC_FORMAT_PCM);

    // Format type I with discrete rates
    put8(w, 8 + 3 * count);
    put8(w, USB_DTYPE_CS_INTERFACE);
    put8(w, UAC_AS_FORMAT_TYPE);
    put8(w, 1);
    put8(w, AUDIO_CHANNELS);
    put8(w, bytes);
    put8(w, bits);
    put8(w, count);
    for(size_t i = 0; i < count; i++)
        put_rate(w, rates[i]);

    desc_endpoint(
        w,
        AUDIO_EP_OUT,
        USB_EPTYPE_ISOCHRONUS | USB_EPATTR_ASYNC,
        MIN(max_packet, AUDIO_EP_OUT_SIZE),
        0,
        AUDIO_EP_FB);

    // CS endpoint: sampling frequency control
    put8(w, 7);
    put8(w, USB_DTYPE_CS_ENDPOINT);
    put8(w, UAC_EP_GENERAL);
    put8(w, 0x01);
    put8(w, 0);
    put16(w, 0);

    // Feedback, host reads it every 2^3 = 8 ms
    desc_endpoint(
        w, AUDIO_EP_FB, USB_EPTYPE_ISOCHRONUS | USB_EPUSAGE_FEEDBACK, AUDIO_EP_FB_SIZE, 3, 0);
}

static size_t audio_supported_rates(uint8_t bits, uint32_t* out) {
    size_t count = 0;
    for(size_t i = 0; i < CANDIDATE_RATES; i++) {
        uint32_t rate = candidate_rates[i];
        if(bits == 24 && rate > MAX_RATE_24) continue;
        if(i2s_out_rate_supported(rate, bits)) out[count++] = rate;
    }
    return count;
}

void usb_audio_build_descriptor(void) {
    uint32_t rates16[CANDIDATE_RATES], rates24[CANDIDATE_RATES];
    const size_t count16 = audio_supported_rates(16, rates16);
    const size_t count24 = audio_supported_rates(24, rates24);
    furi_check(count16 > 0);

    DescWriter w = {audio_cfg_desc};

    put8(&w, sizeof(struct usb_config_descriptor));
    put8(&w, USB_DTYPE_CONFIGURATION);
    put16(&w, 0); // wTotalLength, patched below
    put8(&w, 2); // AudioControl + AudioStreaming
    put8(&w, 1);
    put8(&w, NO_DESCRIPTOR);
    put8(&w, USB_CFG_ATTR_RESERVED);
    put8(&w, USB_CFG_POWER_MA(100));

    desc_interface(&w, 0, 0, 0, UAC_SUBCLASS_AUDIOCONTROL);

    // AC header
    put8(&w, 9);
    put8(&w, USB_DTYPE_CS_INTERFACE);
    put8(&w, UAC_AC_HEADER);
    put16(&w, 0x0100);
    put16(&w, AC_BLOCK_LEN);
    put8(&w, 1);
    put8(&w, 1); // streaming interface number

    // Input terminal: USB stream, stereo
    put8(&w, 12);
    put8(&w, USB_DTYPE_CS_INTERFACE);
    put8(&w, UAC_AC_INPUT_TERMINAL);
    put8(&w, AudioIdInputTerminal);
    put16(&w, UAC_TERMINAL_USB_STREAMING);
    put8(&w, 0);
    put8(&w, AUDIO_CHANNELS);
    put16(&w, 0x0003); // left front, right front
    put8(&w, 0);
    put8(&w, 0);

    // Feature unit: master mute + volume
    put8(&w, 10);
    put8(&w, USB_DTYPE_CS_INTERFACE);
    put8(&w, UAC_AC_FEATURE_UNIT);
    put8(&w, AudioIdFeatureUnit);
    put8(&w, AudioIdInputTerminal);
    put8(&w, 1);
    put8(&w, 0x03);
    put8(&w, 0x00);
    put8(&w, 0x00);
    put8(&w, 0);

    // Output terminal: speaker
    put8(&w, 9);
    put8(&w, USB_DTYPE_CS_INTERFACE);
    put8(&w, UAC_AC_OUTPUT_TERMINAL);
    put8(&w, AudioIdOutputTerminal);
    put16(&w, UAC_TERMINAL_SPEAKER);
    put8(&w, 0);
    put8(&w, AudioIdFeatureUnit);
    put8(&w, 0);

    desc_interface(&w, AltIdle, 1, 0, UAC_SUBCLASS_AUDIOSTREAMING);
    desc_stream_alt(&w, Alt16Bit, 16, rates16, count16);
    if(count24) desc_stream_alt(&w, Alt24Bit, 24, rates24, count24);

    const size_t total = w.p - audio_cfg_desc;
    furi_check(total <= CFG_DESC_MAX);
    audio_cfg_desc[2] = total & 0xFF;
    audio_cfg_desc[3] = total >> 8;
}

static void audio_init(usbd_device* dev, FuriHalUsbInterface* intf, void* ctx);
static void audio_deinit(usbd_device* dev);
static void audio_on_wakeup(usbd_device* dev);
static void audio_on_suspend(usbd_device* dev);

FuriHalUsbInterface usb_audio_interface = {
    .init = audio_init,
    .deinit = audio_deinit,
    .wakeup = audio_on_wakeup,
    .suspend = audio_on_suspend,
    .dev_descr = &audio_device_desc,
    .str_manuf_descr = (void*)&audio_manuf_desc,
    .str_prod_descr = (void*)&audio_prod_desc,
    .str_serial_descr = NULL,
    .cfg_descr = (void*)audio_cfg_desc,
};

/* Packet memory buffer descriptor, BTABLE at offset 0 */
typedef struct {
    uint16_t addr;
    uint16_t cnt;
} PmaRec;

static int32_t ring32[RING_SAMPLES];
static int16_t* const ring16 = (int16_t*)ring32;

/* All fields below are touched only from the USB interrupt, except where
 * usb_audio_get_stats copies them inside a critical section. */
static struct {
    usbd_device* dev;
    uint8_t alt;
    uint32_t rate;
    uint8_t bits; // sample width of the selected alt setting
    bool streaming;
    bool primed;
    uint32_t wr; // ring write position, samples
    uint32_t fb_nominal;
    int32_t fill_avg; // fill in frames, Q8
    uint32_t idle_ms;
    uint32_t last_rd;
    uint32_t consumed; // samples consumed in current 1 s window
    uint16_t sof_ms;
    int32_t gain_q15;
    uint8_t ctl_buf[4];
    UsbAudioStats st;
} audio;

static inline PmaRec* pma_table(uint8_t ep) {
    return (PmaRec*)(USB1_PMAADDR + (ep & 0x07) * 8);
}

static inline uint16_t* pma_ptr(uint16_t addr) {
    return (uint16_t*)(USB1_PMAADDR + addr);
}

static void audio_write_feedback(uint32_t fb) {
    PmaRec* tbl = pma_table(AUDIO_EP_FB);
    // Fill both halves of the double buffer so whichever one is sent is current
    for(size_t i = 0; i < 2; i++) {
        if(!tbl[i].addr) continue;
        uint16_t* p = pma_ptr(tbl[i].addr);
        p[0] = fb & 0xFFFF;
        p[1] = (fb >> 16) & 0xFF;
        tbl[i].cnt = AUDIO_EP_FB_SIZE;
    }
}

static void audio_update_gain(void) {
    if(audio.st.mute) {
        audio.gain_q15 = 0;
    } else {
        float db = (float)audio.st.volume_db256 / 256.0f;
        audio.gain_q15 = (int32_t)(powf(10.0f, db / 20.0f) * 32768.0f);
    }
}

static inline uint32_t ring_fill(void) {
    return (audio.wr + RING_SAMPLES - i2s_out_get_read_pos()) % RING_SAMPLES;
}

static void ring_clear(void) {
    memset(ring32, 0, RING_SAMPLES * (audio.bits == 24 ? sizeof(int32_t) : sizeof(int16_t)));
}

static void ring_resync(void) {
    uint32_t rd = i2s_out_get_read_pos();
    ring_clear();
    audio.wr = (rd + RING_TARGET) % RING_SAMPLES;
    audio.fill_avg = (RING_TARGET / AUDIO_CHANNELS) << 8;
}

static void audio_stream_stop(void) {
    if(!audio.streaming) return;
    i2s_out_stop();
    ring_clear();
    audio.streaming = false;
    audio.primed = false;
    audio.st.streaming = false;
    audio.st.receiving = false;
    audio.st.fill_pct = 0;
    audio.st.measured_rate = 0;
}

static void audio_stream_start(void) {
    audio_stream_stop();
    ring_clear();
    if(!i2s_out_rate_supported(audio.rate, audio.bits)) {
        // Host may switch to 24-bit before lowering the rate; it sends SET_CUR next
        audio.rate = USB_AUDIO_RATE_DEFAULT;
        audio.st.sample_rate = audio.rate;
    }
    if(!i2s_out_start(audio.rate, audio.bits, ring32, RING_SAMPLES)) {
        FURI_LOG_E(TAG, "I2S start failed at %lu Hz %u bit", audio.rate, audio.bits);
        return;
    }
    audio.streaming = true;
    audio.primed = false;
    audio.idle_ms = RX_TIMEOUT_MS;
    audio.last_rd = i2s_out_get_read_pos();
    audio.consumed = 0;
    audio.sof_ms = 0;

    audio.st.actual_rate_mhz = i2s_out_get_actual_rate_mhz();
    audio.st.bclk = i2s_out_get_bclk();
    audio.st.sai_clk = i2s_out_get_sai_clk();
    audio.st.mclk = i2s_out_get_mclk();
    audio.fb_nominal = (uint32_t)((uint64_t)audio.st.actual_rate_mhz * 16384U / 1000000U);
    audio.st.feedback = audio.fb_nominal;
    audio_write_feedback(audio.fb_nominal);
    audio.st.bits = audio.bits;
    audio.st.streaming = true;
}

static void audio_set_rate(uint32_t rate) {
    audio.rate = rate;
    audio.st.sample_rate = rate;
    if(audio.streaming) audio_stream_start();
}

static void audio_rx_callback(usbd_device* dev, uint8_t event, uint8_t ep) {
    UNUSED(dev);
    UNUSED(event);

    // The driver's double-buffer index for ISO OUT is unreliable, so read
    // whichever half the hardware just filled (its byte count is non-zero).
    PmaRec* tbl = pma_table(ep);
    PmaRec* rec = NULL;
    for(size_t i = 0; i < 2; i++) {
        if(tbl[i].cnt & 0x3FF) {
            rec = &tbl[i];
            break;
        }
    }
    if(!rec) return;
    uint32_t len = rec->cnt & 0x3FF;
    const uint16_t* src = pma_ptr(rec->addr);
    rec->cnt &= ~0x3FF;

    if(!audio.streaming) return;
    audio.st.packets++;
    audio.idle_ms = 0;
    audio.st.receiving = true;

    const bool wide = audio.bits == 24;
    const uint32_t frame_bytes = AUDIO_CHANNELS * (wide ? 3U : 2U);
    const uint32_t samples = (len / frame_bytes) * AUDIO_CHANNELS; // whole frames only
    if(!samples) return;

    uint32_t fill = ring_fill();
    if(!audio.primed || fill + samples > RING_SAMPLES - RING_MARGIN || fill < RING_MARGIN) {
        if(audio.primed) audio.st.xruns++;
        ring_resync();
        audio.primed = true;
    }

    uint16_t peak[AUDIO_CHANNELS] = {audio.st.peak_l, audio.st.peak_r};
    const int32_t gain = audio.gain_q15;
    uint32_t wr = audio.wr;
    if(wide) {
        // Packet memory is halfword-addressed; 3-byte samples straddle halfwords
        const uint8_t* bytes = NULL;
        static uint16_t tmp[AUDIO_EP_OUT_SIZE_24 / 2 + 1]; // keep off the ISR stack
        const uint32_t words = (len + 1) / 2;
        for(uint32_t i = 0; i < words && i < COUNT_OF(tmp); i++)
            tmp[i] = src[i];
        bytes = (const uint8_t*)tmp;
        for(uint32_t i = 0; i < samples; i++, bytes += 3) {
            int32_t v = (int32_t)((uint32_t)bytes[0] << 8 | (uint32_t)bytes[1] << 16 |
                                  (uint32_t)bytes[2] << 24) >>
                        8;
            v = (int32_t)(((int64_t)v * gain) >> 15);
            ring32[wr] = v;
            wr = (wr + 1) % RING_SAMPLES;
            uint16_t a = (uint16_t)((v < 0 ? -v : v) >> 8);
            if(a > peak[i & 1]) peak[i & 1] = a;
        }
    } else {
        for(uint32_t i = 0; i < samples; i++) {
            int32_t v = ((int32_t)(int16_t)src[i] * gain) >> 15;
            ring16[wr] = (int16_t)v;
            wr = (wr + 1) % RING_SAMPLES;
            uint16_t a = (uint16_t)(v < 0 ? -v : v);
            if(a > peak[i & 1]) peak[i & 1] = a;
        }
    }
    audio.wr = wr;
    audio.st.peak_l = peak[0];
    audio.st.peak_r = peak[1];
}

static void audio_fb_callback(usbd_device* dev, uint8_t event, uint8_t ep) {
    UNUSED(dev);
    UNUSED(event);
    UNUSED(ep);
    audio_write_feedback(audio.st.feedback);
}

static void audio_sof_callback(usbd_device* dev, uint8_t event, uint8_t ep) {
    UNUSED(dev);
    UNUSED(event);
    UNUSED(ep);
    if(!audio.streaming) return;

    uint32_t rd = i2s_out_get_read_pos();
    // Divide once per window: DMA may stop mid-frame, per-ms rounding would drop samples
    audio.consumed += (rd + RING_SAMPLES - audio.last_rd) % RING_SAMPLES;
    audio.last_rd = rd;
    if(++audio.sof_ms >= 1000) {
        audio.st.measured_rate = audio.consumed / AUDIO_CHANNELS;
        audio.consumed = 0;
        audio.sof_ms = 0;
    }

    if(audio.idle_ms < RX_TIMEOUT_MS) {
        audio.idle_ms++;
    } else if(audio.primed) {
        // Host stopped sending: play silence instead of looping the buffer
        ring_clear();
        audio.primed = false;
        audio.st.receiving = false;
    }

    uint32_t fb = audio.fb_nominal;
    if(audio.primed) {
        int32_t fill = (int32_t)(ring_fill() / AUDIO_CHANNELS);
        audio.fill_avg += ((fill << 8) - audio.fill_avg) / 32;
        audio.st.fill_pct = (uint32_t)(audio.fill_avg >> 8) * 100U * AUDIO_CHANNELS / RING_SAMPLES;

        int32_t err = (audio.fill_avg >> 8) - (int32_t)(RING_TARGET / AUDIO_CHANNELS);
        int32_t corr = -err * FB_GAIN;
        int32_t limit = (int32_t)(audio.fb_nominal >> FB_LIMIT_SHIFT);
        if(corr > limit) corr = limit;
        if(corr < -limit) corr = -limit;
        fb = (uint32_t)((int32_t)audio.fb_nominal + corr);
    }
    if(fb != audio.st.feedback) {
        audio.st.feedback = fb;
        audio_write_feedback(fb);
    }
}

static usbd_respond audio_ep_config(usbd_device* dev, uint8_t cfg) {
    switch(cfg) {
    case 0:
        audio_stream_stop();
        usbd_ep_deconfig(dev, AUDIO_EP_OUT);
        usbd_ep_deconfig(dev, AUDIO_EP_FB);
        usbd_reg_endpoint(dev, AUDIO_EP_OUT, NULL);
        usbd_reg_endpoint(dev, AUDIO_EP_FB, NULL);
        audio.alt = 0;
        audio.st.configured = false;
        return usbd_ack;
    case 1:
        // Release first: a repeated SET_CONFIGURATION would leak packet memory
        usbd_ep_deconfig(dev, AUDIO_EP_OUT);
        usbd_ep_deconfig(dev, AUDIO_EP_FB);
        if(!usbd_ep_config(dev, AUDIO_EP_OUT, USB_EPTYPE_ISOCHRONUS, AUDIO_EP_OUT_SIZE) ||
           !usbd_ep_config(dev, AUDIO_EP_FB, USB_EPTYPE_ISOCHRONUS, AUDIO_EP_FB_SIZE)) {
            FURI_LOG_E(TAG, "Out of packet memory");
            return usbd_fail;
        }
        usbd_reg_endpoint(dev, AUDIO_EP_OUT, audio_rx_callback);
        usbd_reg_endpoint(dev, AUDIO_EP_FB, audio_fb_callback);
        audio_write_feedback(audio.st.feedback);
        audio.alt = 0;
        audio.st.configured = true;
        return usbd_ack;
    default:
        return usbd_fail;
    }
}

static usbd_respond
    audio_reply(usbd_device* dev, usbd_ctlreq* req, const void* data, uint16_t len) {
    if(len > sizeof(audio.ctl_buf)) return usbd_fail;
    memcpy(audio.ctl_buf, data, len);
    dev->status.data_ptr = audio.ctl_buf;
    dev->status.data_count = MIN(len, req->wLength);
    return usbd_ack;
}

static usbd_respond audio_feature_unit_request(usbd_device* dev, usbd_ctlreq* req) {
    uint8_t cs = req->wValue >> 8;
    uint8_t cn = req->wValue & 0xFF;
    if(cn != 0) return usbd_fail; // master channel only

    if(cs == UAC_FU_MUTE) {
        if(req->bRequest == UAC_SET_CUR && req->wLength >= 1) {
            audio.st.mute = req->data[0] != 0;
            audio_update_gain();
            return usbd_ack;
        } else if(req->bRequest == UAC_GET_CUR) {
            uint8_t mute = audio.st.mute;
            return audio_reply(dev, req, &mute, 1);
        }
    } else if(cs == UAC_FU_VOLUME) {
        int16_t value;
        switch(req->bRequest) {
        case UAC_SET_CUR:
            if(req->wLength < 2) return usbd_fail;
            value = (int16_t)(req->data[0] | (req->data[1] << 8));
            if(value < VOLUME_MIN) value = VOLUME_MIN;
            if(value > VOLUME_MAX) value = VOLUME_MAX;
            audio.st.volume_db256 = value;
            audio_update_gain();
            return usbd_ack;
        case UAC_GET_CUR:
            value = audio.st.volume_db256;
            return audio_reply(dev, req, &value, 2);
        case UAC_GET_MIN:
            value = VOLUME_MIN;
            return audio_reply(dev, req, &value, 2);
        case UAC_GET_MAX:
            value = VOLUME_MAX;
            return audio_reply(dev, req, &value, 2);
        case UAC_GET_RES:
            value = VOLUME_RES;
            return audio_reply(dev, req, &value, 2);
        default:
            break;
        }
    }
    return usbd_fail;
}

static usbd_respond audio_endpoint_request(usbd_device* dev, usbd_ctlreq* req) {
    if((req->wValue >> 8) != UAC_EP_SAMPLING_FRQ) return usbd_fail;
    if(req->bRequest == UAC_SET_CUR && req->wLength >= 3) {
        uint32_t rate = req->data[0] | (req->data[1] << 8) | (req->data[2] << 16);
        // Validate against the active width, or either width before streaming
        if(!i2s_out_rate_supported(rate, audio.bits) && !i2s_out_rate_supported(rate, 16))
            return usbd_fail;
        audio_set_rate(rate);
        return usbd_ack;
    } else if(req->bRequest == UAC_GET_CUR) {
        uint8_t rate[3] = RATE3(audio.rate);
        return audio_reply(dev, req, rate, 3);
    }
    return usbd_fail;
}

static usbd_respond
    audio_control(usbd_device* dev, usbd_ctlreq* req, usbd_rqc_callback* callback) {
    UNUSED(callback);
    const uint8_t type = req->bmRequestType & (USB_REQ_TYPE | USB_REQ_RECIPIENT);
    const uint8_t index = req->wIndex & 0xFF;

    if(type == (USB_REQ_STANDARD | USB_REQ_INTERFACE)) {
        if(req->bRequest == USB_STD_SET_INTERFACE) {
            if(index == 0 && req->wValue == 0) return usbd_ack;
            if(index != 1 || req->wValue >= AltCount) return usbd_fail;
            audio.alt = req->wValue;
            if(audio.alt != AltIdle) {
                audio.bits = audio.alt == Alt24Bit ? 24 : 16;
                audio.st.bits = audio.bits;
                audio_stream_start();
            } else {
                audio_stream_stop();
            }
            return usbd_ack;
        } else if(req->bRequest == USB_STD_GET_INTERFACE) {
            uint8_t alt = (index == 1) ? audio.alt : 0;
            return audio_reply(dev, req, &alt, 1);
        }
        return usbd_fail;
    }

    if(type == (USB_REQ_CLASS | USB_REQ_INTERFACE) && index == 0 &&
       (req->wIndex >> 8) == AudioIdFeatureUnit) {
        return audio_feature_unit_request(dev, req);
    }

    if(type == (USB_REQ_CLASS | USB_REQ_ENDPOINT) && index == AUDIO_EP_OUT) {
        return audio_endpoint_request(dev, req);
    }

    return usbd_fail;
}

static void audio_init(usbd_device* dev, FuriHalUsbInterface* intf, void* ctx) {
    UNUSED(intf);
    UNUSED(ctx);
    memset(&audio, 0, sizeof(audio));
    audio.dev = dev;
    audio.rate = USB_AUDIO_RATE_DEFAULT;
    audio.bits = 16;
    audio.st.sample_rate = audio.rate;
    audio.st.bits = audio.bits;
    audio.st.volume_db256 = VOLUME_MAX;
    audio_update_gain();
    audio.fb_nominal = audio.rate * 16384U / 1000U;
    audio.st.feedback = audio.fb_nominal;

    usbd_reg_config(dev, audio_ep_config);
    usbd_reg_control(dev, audio_control);
    usbd_reg_event(dev, usbd_evt_sof, audio_sof_callback);

    usbd_connect(dev, true);
}

static void audio_deinit(usbd_device* dev) {
    FURI_CRITICAL_ENTER();
    audio_stream_stop();
    FURI_CRITICAL_EXIT();
    usbd_reg_event(dev, usbd_evt_sof, NULL);
    usbd_reg_config(dev, NULL);
    usbd_reg_control(dev, NULL);
    audio.st.configured = false;
}

static void audio_on_wakeup(usbd_device* dev) {
    UNUSED(dev);
    audio.st.suspended = false;
}

static void audio_on_suspend(usbd_device* dev) {
    UNUSED(dev);
    FURI_CRITICAL_ENTER();
    audio_stream_stop();
    FURI_CRITICAL_EXIT();
    audio.st.suspended = true;
}

void usb_audio_get_stats(UsbAudioStats* stats) {
    FURI_CRITICAL_ENTER();
    *stats = audio.st;
    if(audio.dev) {
        stats->configured = audio.dev->status.device_state == usbd_state_configured;
    }
    audio.st.peak_l = 0;
    audio.st.peak_r = 0;
    FURI_CRITICAL_EXIT();
}
