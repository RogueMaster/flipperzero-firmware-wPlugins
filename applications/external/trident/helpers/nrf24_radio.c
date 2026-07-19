#include "nrf24_radio.h"

#include <furi_hal_spi.h>
#include <furi_hal_gpio.h>
#include <furi_hal_resources.h>
#include <string.h>
#include <stdio.h>

/* ---- nRF24L01+ SPI command / register map ---- */
#define NRF_CMD_R_REGISTER  0x00
#define NRF_CMD_W_REGISTER  0x20
#define NRF_CMD_R_RX_PAYLOAD 0x61
#define NRF_CMD_FLUSH_RX    0xE2
#define NRF_CMD_FLUSH_TX    0xE1
#define NRF_CMD_NOP         0xFF

#define NRF_REG_CONFIG    0x00
#define NRF_REG_EN_AA     0x01
#define NRF_REG_EN_RXADDR 0x02
#define NRF_REG_SETUP_AW  0x03
#define NRF_REG_RF_CH     0x05
#define NRF_REG_RF_SETUP  0x06
#define NRF_REG_STATUS    0x07
#define NRF_REG_RPD       0x09 // bit0 = carrier detected
#define NRF_REG_RX_ADDR_P0 0x0A
#define NRF_REG_RX_PW_P0  0x11

#define NRF_CONFIG_RX     0x03 // PWR_UP | PRIM_RX (CRC on by default bit3=0 here)
#define NRF_CONFIG_SNIFF  0x03 // CRC disabled for promiscuous capture
#define NRF_RF_SETUP_2M   0x0E // 2 Mbps, 0 dBm

#define NRF_CHANNELS      126 // 0..125 -> 2400..2525 MHz
#define NRF_DWELL_US      240 // sweep dwell per channel
#define NRF_HIT_GAIN      26
#define NRF_CAMP_SAMPLES  40 // RPD samples per camp publish
#define NRF_SNIFF_PW      32 // promiscuous payload width
#define NRF_WORKER_STACK  2048

#define NRF_CE_PIN (&gpio_ext_pb2) // pin 6
#define NRF_SPI    (&furi_hal_spi_bus_handle_external)

struct Nrf24Radio {
    FuriThread* thread;
    FuriMutex* mutex; // guards snap + meter
    volatile bool running;
    volatile bool reset_req;
    volatile uint8_t camp_ch;
    uint8_t mode; // Nrf24Mode

    Nrf24LineCallback line_cb;
    void* line_ctx;

    uint16_t hits[NRF_CHANNELS]; // sweep accumulator
    uint8_t camp_peak; // camp peak-hold
    uint32_t sniff_count; // sniffed frames
    SpectrumSnapshot snap;
    MeterSnapshot meter;
};

/* ---------------- low-level SPI ---------------- */

static void nrf_ce(bool level) {
    furi_hal_gpio_write(NRF_CE_PIN, level);
}

static void nrf_write_reg(uint8_t reg, uint8_t val) {
    uint8_t tx[2] = {(uint8_t)(NRF_CMD_W_REGISTER | (reg & 0x1F)), val};
    furi_hal_spi_acquire(NRF_SPI);
    furi_hal_spi_bus_trx(NRF_SPI, tx, NULL, sizeof(tx), furi_ms_to_ticks(10));
    furi_hal_spi_release(NRF_SPI);
}

static uint8_t nrf_read_reg(uint8_t reg) {
    uint8_t tx[2] = {(uint8_t)(NRF_CMD_R_REGISTER | (reg & 0x1F)), NRF_CMD_NOP};
    uint8_t rx[2] = {0};
    furi_hal_spi_acquire(NRF_SPI);
    furi_hal_spi_bus_trx(NRF_SPI, tx, rx, sizeof(tx), furi_ms_to_ticks(10));
    furi_hal_spi_release(NRF_SPI);
    return rx[1];
}

static void nrf_write_buf(uint8_t reg, const uint8_t* data, size_t len) {
    uint8_t tx[6];
    if(len > sizeof(tx) - 1) len = sizeof(tx) - 1;
    tx[0] = (uint8_t)(NRF_CMD_W_REGISTER | (reg & 0x1F));
    memcpy(&tx[1], data, len);
    furi_hal_spi_acquire(NRF_SPI);
    furi_hal_spi_bus_trx(NRF_SPI, tx, NULL, len + 1, furi_ms_to_ticks(10));
    furi_hal_spi_release(NRF_SPI);
}

static void nrf_read_payload(uint8_t* out, size_t len) {
    uint8_t tx[1 + NRF_SNIFF_PW];
    uint8_t rx[1 + NRF_SNIFF_PW];
    memset(tx, NRF_CMD_NOP, sizeof(tx));
    tx[0] = NRF_CMD_R_RX_PAYLOAD;
    if(len > NRF_SNIFF_PW) len = NRF_SNIFF_PW;
    furi_hal_spi_acquire(NRF_SPI);
    furi_hal_spi_bus_trx(NRF_SPI, tx, rx, len + 1, furi_ms_to_ticks(10));
    furi_hal_spi_release(NRF_SPI);
    memcpy(out, &rx[1], len);
}

static void nrf_cmd(uint8_t cmd) {
    furi_hal_spi_acquire(NRF_SPI);
    furi_hal_spi_bus_trx(NRF_SPI, &cmd, NULL, 1, furi_ms_to_ticks(10));
    furi_hal_spi_release(NRF_SPI);
}

static bool nrf_present(void) {
    nrf_write_reg(NRF_REG_RF_CH, 0x4C);
    return nrf_read_reg(NRF_REG_RF_CH) == 0x4C;
}

/* ---------------- publishers ---------------- */

static void nrf_publish_sweep(Nrf24Radio* radio) {
    SpectrumSnapshot s;
    memset(&s, 0, sizeof(s));
    s.running = true;
    s.present = true;
    s.count = NRF_CHANNELS;
    strncpy(s.title, "NRF24 2.4GHz", sizeof(s.title) - 1);
    strncpy(s.unit, "%", sizeof(s.unit) - 1);
    strncpy(s.lo_label, "2400", sizeof(s.lo_label) - 1);
    strncpy(s.hi_label, "2525", sizeof(s.hi_label) - 1);

    int peak_bin = -1;
    uint16_t peak_val = 0;
    for(int ch = 0; ch < NRF_CHANNELS; ch++) {
        uint16_t v = radio->hits[ch];
        if(v > 100) v = 100;
        s.level[ch] = (uint8_t)v;
        if(v > peak_val) {
            peak_val = v;
            peak_bin = ch;
        }
    }
    s.peak_bin = peak_bin;
    s.peak_value = peak_val;
    if(peak_bin >= 0 && peak_val > 0) {
        int ch = peak_bin;
        if(ch > NRF_CHANNELS - 1) ch = NRF_CHANNELS - 1;
        snprintf(s.peak_label, sizeof(s.peak_label), "Ch%d %dMHz", ch, 2400 + ch);
    } else {
        strncpy(s.peak_label, "listening...", sizeof(s.peak_label) - 1);
    }

    furi_mutex_acquire(radio->mutex, FuriWaitForever);
    uint32_t sweeps = radio->snap.sweeps;
    radio->snap = s;
    radio->snap.sweeps = sweeps;
    furi_mutex_release(radio->mutex);
}

static void nrf_publish_meter(Nrf24Radio* radio, const char* title, uint8_t ch, uint8_t level) {
    uint8_t peak = radio->camp_peak;
    peak = (uint8_t)(peak - (peak >> 4));
    if(level > peak) peak = level;
    radio->camp_peak = peak;

    MeterSnapshot m;
    memset(&m, 0, sizeof(m));
    m.running = true;
    m.present = true;
    strncpy(m.title, title, sizeof(m.title) - 1);
    m.level = level;
    m.peak = peak;
    snprintf(m.value, sizeof(m.value), "%u", level);
    strncpy(m.unit, "%", sizeof(m.unit) - 1);
    snprintf(m.sub, sizeof(m.sub), "Ch %u  %u MHz", ch, 2400 + ch);
    m.count = radio->sniff_count;

    furi_mutex_acquire(radio->mutex, FuriWaitForever);
    radio->meter = m;
    furi_mutex_release(radio->mutex);
}

/* ---------------- worker ---------------- */

static void nrf_mark_absent(Nrf24Radio* radio) {
    furi_mutex_acquire(radio->mutex, FuriWaitForever);
    memset(&radio->snap, 0, sizeof(radio->snap));
    memset(&radio->meter, 0, sizeof(radio->meter));
    radio->snap.present = false;
    radio->meter.present = false;
    strncpy(radio->snap.title, "NRF24 2.4GHz", sizeof(radio->snap.title) - 1);
    strncpy(radio->meter.title, "NRF24", sizeof(radio->meter.title) - 1);
    furi_mutex_release(radio->mutex);
}

static void nrf_configure_common(void) {
    nrf_ce(false);
    nrf_write_reg(NRF_REG_EN_AA, 0x00);
    nrf_write_reg(NRF_REG_EN_RXADDR, 0x01);
    nrf_write_reg(NRF_REG_SETUP_AW, 0x03);
    nrf_write_reg(NRF_REG_RF_SETUP, NRF_RF_SETUP_2M);
    nrf_cmd(NRF_CMD_FLUSH_RX);
    nrf_cmd(NRF_CMD_FLUSH_TX);
    nrf_write_reg(NRF_REG_STATUS, 0x70);
}

static int32_t nrf24_worker(void* context) {
    Nrf24Radio* radio = context;

    furi_hal_gpio_init(NRF_CE_PIN, GpioModeOutputPushPull, GpioPullNo, GpioSpeedVeryHigh);
    nrf_ce(false);

    if(!nrf_present()) {
        nrf_mark_absent(radio);
        while(radio->running) furi_delay_ms(100);
        furi_hal_gpio_init_simple(NRF_CE_PIN, GpioModeAnalog);
        return 0;
    }

    if(radio->mode == Nrf24ModeSweep) {
        nrf_configure_common();
        nrf_write_reg(NRF_REG_CONFIG, NRF_CONFIG_RX);
        furi_delay_ms(2);
        nrf_ce(true);
        while(radio->running) {
            if(radio->reset_req) {
                radio->reset_req = false;
                memset(radio->hits, 0, sizeof(radio->hits));
            }
            for(int ch = 0; ch < NRF_CHANNELS && radio->running; ch++) {
                nrf_write_reg(NRF_REG_RF_CH, (uint8_t)ch);
                furi_delay_us(NRF_DWELL_US);
                bool hit = (nrf_read_reg(NRF_REG_RPD) & 0x01) != 0;
                uint16_t v = radio->hits[ch];
                v = (uint16_t)(v - (v >> 3));
                if(hit) {
                    v += NRF_HIT_GAIN;
                    if(v > 100) v = 100;
                }
                radio->hits[ch] = v;
            }
            furi_mutex_acquire(radio->mutex, FuriWaitForever);
            radio->snap.sweeps++;
            furi_mutex_release(radio->mutex);
            nrf_publish_sweep(radio);
        }
    } else if(radio->mode == Nrf24ModeCamp) {
        nrf_configure_common();
        nrf_write_reg(NRF_REG_CONFIG, NRF_CONFIG_RX);
        furi_delay_ms(2);
        nrf_ce(true);
        uint8_t current = 0xFF;
        while(radio->running) {
            if(radio->reset_req) {
                radio->reset_req = false;
                radio->camp_peak = 0;
            }
            uint8_t ch = radio->camp_ch % NRF_CHANNELS;
            if(ch != current) {
                nrf_write_reg(NRF_REG_RF_CH, ch);
                current = ch;
                furi_delay_us(300);
            }
            uint16_t hits = 0;
            for(int i = 0; i < NRF_CAMP_SAMPLES && radio->running; i++) {
                furi_delay_us(NRF_DWELL_US);
                if(nrf_read_reg(NRF_REG_RPD) & 0x01) hits++;
            }
            uint8_t level = (uint8_t)(hits * 100 / NRF_CAMP_SAMPLES);
            nrf_publish_meter(radio, "NRF24 Finder", ch, level);
        }
    } else { // Nrf24ModeSniff
        nrf_configure_common();
        uint8_t addr[2] = {0x00, 0xAA}; // classic promiscuous preamble-as-address
        nrf_write_reg(NRF_REG_SETUP_AW, 0x01); // 2-byte address
        nrf_write_buf(NRF_REG_RX_ADDR_P0, addr, sizeof(addr));
        nrf_write_reg(NRF_REG_RX_PW_P0, NRF_SNIFF_PW);
        nrf_write_reg(NRF_REG_CONFIG, NRF_CONFIG_SNIFF); // CRC off
        furi_delay_ms(2);
        uint8_t current = 0xFF;
        while(radio->running) {
            uint8_t ch = radio->camp_ch % NRF_CHANNELS;
            if(ch != current) {
                nrf_ce(false);
                nrf_write_reg(NRF_REG_RF_CH, ch);
                nrf_cmd(NRF_CMD_FLUSH_RX);
                nrf_ce(true);
                current = ch;
                furi_delay_us(300);
            }
            uint8_t fifo = nrf_read_reg(0x17); // FIFO_STATUS
            if((fifo & 0x01) == 0) { // RX not empty
                uint8_t pl[NRF_SNIFF_PW] = {0};
                nrf_read_payload(pl, NRF_SNIFF_PW);
                nrf_write_reg(NRF_REG_STATUS, 0x40); // clear RX_DR
                radio->sniff_count++;
                if(radio->line_cb) {
                    char line[52];
                    int n = snprintf(line, sizeof(line), "c%02u ", ch);
                    for(int b = 0; b < 8 && n < (int)sizeof(line) - 3; b++) {
                        n += snprintf(line + n, sizeof(line) - n, "%02X ", pl[b]);
                    }
                    radio->line_cb(radio->line_ctx, line);
                }
                furi_mutex_acquire(radio->mutex, FuriWaitForever);
                radio->meter.count = radio->sniff_count;
                furi_mutex_release(radio->mutex);
            } else {
                furi_delay_us(500);
            }
        }
    }

    nrf_ce(false);
    nrf_write_reg(NRF_REG_CONFIG, 0x00); // power down
    furi_hal_gpio_init_simple(NRF_CE_PIN, GpioModeAnalog);
    return 0;
}

/* ---------------- public API ---------------- */

Nrf24Radio* nrf24_radio_alloc(void) {
    Nrf24Radio* radio = malloc(sizeof(Nrf24Radio));
    memset(radio, 0, sizeof(Nrf24Radio));
    radio->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    radio->camp_ch = 42;
    return radio;
}

void nrf24_radio_free(Nrf24Radio* radio) {
    furi_assert(radio);
    nrf24_radio_stop(radio);
    furi_mutex_free(radio->mutex);
    free(radio);
}

void nrf24_radio_set_mode(Nrf24Radio* radio, Nrf24Mode mode) {
    furi_assert(radio);
    radio->mode = (uint8_t)mode;
}

void nrf24_radio_set_channel(Nrf24Radio* radio, uint8_t ch) {
    furi_assert(radio);
    radio->camp_ch = ch % NRF_CHANNELS;
}

uint8_t nrf24_radio_get_channel(Nrf24Radio* radio) {
    furi_assert(radio);
    return radio->camp_ch;
}

void nrf24_radio_set_line_callback(Nrf24Radio* radio, Nrf24LineCallback cb, void* context) {
    furi_assert(radio);
    radio->line_cb = cb;
    radio->line_ctx = context;
}

void nrf24_radio_start(Nrf24Radio* radio) {
    furi_assert(radio);
    if(radio->running) return;
    memset(radio->hits, 0, sizeof(radio->hits));
    memset(&radio->snap, 0, sizeof(radio->snap));
    memset(&radio->meter, 0, sizeof(radio->meter));
    radio->camp_peak = 0;
    radio->sniff_count = 0;
    radio->reset_req = false;
    radio->running = true;
    radio->thread = furi_thread_alloc_ex("TridentNrf24", NRF_WORKER_STACK, nrf24_worker, radio);
    furi_thread_start(radio->thread);
}

void nrf24_radio_stop(Nrf24Radio* radio) {
    furi_assert(radio);
    if(!radio->running) return;
    radio->running = false;
    if(radio->thread) {
        furi_thread_join(radio->thread);
        furi_thread_free(radio->thread);
        radio->thread = NULL;
    }
}

bool nrf24_radio_is_running(Nrf24Radio* radio) {
    furi_assert(radio);
    return radio->running;
}

void nrf24_radio_reset(Nrf24Radio* radio) {
    furi_assert(radio);
    radio->reset_req = true;
}

void nrf24_radio_get_snapshot(Nrf24Radio* radio, SpectrumSnapshot* out) {
    furi_assert(radio);
    furi_assert(out);
    furi_mutex_acquire(radio->mutex, FuriWaitForever);
    *out = radio->snap;
    furi_mutex_release(radio->mutex);
}

void nrf24_radio_get_meter(Nrf24Radio* radio, MeterSnapshot* out) {
    furi_assert(radio);
    furi_assert(out);
    furi_mutex_acquire(radio->mutex, FuriWaitForever);
    *out = radio->meter;
    furi_mutex_release(radio->mutex);
}
