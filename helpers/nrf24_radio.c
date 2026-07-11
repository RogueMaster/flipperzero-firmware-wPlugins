#include "nrf24_radio.h"

#include <furi_hal_spi.h>
#include <furi_hal_gpio.h>
#include <furi_hal_resources.h>
#include <string.h>
#include <stdio.h>

/* ---- nRF24L01+ SPI command / register map ---- */
#define NRF_CMD_R_REGISTER 0x00
#define NRF_CMD_W_REGISTER 0x20
#define NRF_CMD_FLUSH_RX   0xE2
#define NRF_CMD_FLUSH_TX   0xE1
#define NRF_CMD_NOP        0xFF

#define NRF_REG_CONFIG   0x00
#define NRF_REG_EN_AA    0x01
#define NRF_REG_EN_RXADDR 0x02
#define NRF_REG_SETUP_AW 0x03
#define NRF_REG_RF_CH    0x05
#define NRF_REG_RF_SETUP 0x06
#define NRF_REG_STATUS   0x07
#define NRF_REG_RPD      0x09 // CD on the non-plus part; bit0 = carrier detected

#define NRF_CONFIG_RX  0x03 // PWR_UP | PRIM_RX
#define NRF_RF_SETUP_2M 0x0E // 2 Mbps, 0 dBm (widest channel bandwidth for scanning)

#define NRF_CHANNELS   126 // 0..125 -> 2400..2525 MHz
#define NRF_DWELL_US   240 // PLL settle + RPD sample window per channel
#define NRF_HIT_GAIN   26 // activity added on a positive RPD sample
#define NRF_WORKER_STACK 2048

// CE line: pin 6 / PB2. CSN is handled by the external SPI bus (pin 4 / PA4).
#define NRF_CE_PIN (&gpio_ext_pb2)
#define NRF_SPI    (&furi_hal_spi_bus_handle_external)

struct Nrf24Radio {
    FuriThread* thread;
    FuriMutex* mutex; // guards `snap`
    volatile bool running;
    volatile bool reset_req;

    SpectrumSnapshot snap;
    uint16_t hits[NRF_CHANNELS]; // decayed activity accumulator, worker-owned
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

static void nrf_cmd(uint8_t cmd) {
    furi_hal_spi_acquire(NRF_SPI);
    furi_hal_spi_bus_trx(NRF_SPI, &cmd, NULL, 1, furi_ms_to_ticks(10));
    furi_hal_spi_release(NRF_SPI);
}

/* ---------------- radio bring-up ---------------- */

static bool nrf_present(void) {
    // Write a mid-value channel and read it back; a floating/absent bus returns
    // 0x00 or 0xFF, so a clean round-trip means the NRF24 answered.
    nrf_write_reg(NRF_REG_RF_CH, 0x4C);
    return nrf_read_reg(NRF_REG_RF_CH) == 0x4C;
}

static void nrf_configure(void) {
    nrf_ce(false);
    nrf_write_reg(NRF_REG_CONFIG, NRF_CONFIG_RX);
    nrf_write_reg(NRF_REG_EN_AA, 0x00); // no auto-ack: we sample raw energy
    nrf_write_reg(NRF_REG_EN_RXADDR, 0x01); // pipe 0
    nrf_write_reg(NRF_REG_SETUP_AW, 0x03); // 5-byte address (default)
    nrf_write_reg(NRF_REG_RF_SETUP, NRF_RF_SETUP_2M);
    nrf_cmd(NRF_CMD_FLUSH_RX);
    nrf_cmd(NRF_CMD_FLUSH_TX);
    nrf_write_reg(NRF_REG_STATUS, 0x70); // clear latched IRQ flags
    furi_delay_ms(2); // power-up settle
}

/* ---------------- worker ---------------- */

static void nrf_publish(Nrf24Radio* radio) {
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
        if(ch > NRF_CHANNELS - 1) ch = NRF_CHANNELS - 1; // bound for the formatter
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

static int32_t nrf24_worker(void* context) {
    Nrf24Radio* radio = context;

    furi_hal_gpio_init(NRF_CE_PIN, GpioModeOutputPushPull, GpioPullNo, GpioSpeedVeryHigh);
    nrf_ce(false);

    bool present = nrf_present();
    if(!present) {
        furi_mutex_acquire(radio->mutex, FuriWaitForever);
        memset(&radio->snap, 0, sizeof(radio->snap));
        radio->snap.running = radio->running;
        radio->snap.present = false;
        strncpy(radio->snap.title, "NRF24 2.4GHz", sizeof(radio->snap.title) - 1);
        furi_mutex_release(radio->mutex);
        // Idle until stopped so the "not detected" screen stays put.
        while(radio->running) furi_delay_ms(100);
        furi_hal_gpio_init_simple(NRF_CE_PIN, GpioModeAnalog);
        return 0;
    }

    nrf_configure();
    nrf_ce(true); // stay in RX; we retune RF_CH and sample RPD per channel

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
            v = (uint16_t)(v - (v >> 3)); // ~12% decay
            if(hit) {
                v += NRF_HIT_GAIN;
                if(v > 100) v = 100;
            }
            radio->hits[ch] = v;
        }

        furi_mutex_acquire(radio->mutex, FuriWaitForever);
        radio->snap.sweeps++;
        furi_mutex_release(radio->mutex);
        nrf_publish(radio);
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
    return radio;
}

void nrf24_radio_free(Nrf24Radio* radio) {
    furi_assert(radio);
    nrf24_radio_stop(radio);
    furi_mutex_free(radio->mutex);
    free(radio);
}

void nrf24_radio_start(Nrf24Radio* radio) {
    furi_assert(radio);
    if(radio->running) return;
    memset(radio->hits, 0, sizeof(radio->hits));
    memset(&radio->snap, 0, sizeof(radio->snap));
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
