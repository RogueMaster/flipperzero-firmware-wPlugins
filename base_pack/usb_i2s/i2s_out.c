#include "i2s_out.h"

#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_bus.h>
#include <furi_hal_resources.h>

#include <stm32wbxx_ll_rcc.h>
#include <stm32wbxx_ll_crs.h>
#include <stm32wbxx_ll_dma.h>
#include <stm32wbxx_ll_dmamux.h>

#define TAG "I2sOut"

#define I2S_DMA       DMA1
#define I2S_DMA_CH    LL_DMA_CHANNEL_5
#define PLL_REF_HZ    16000000U // HSE 32 MHz / PLLM 2, shared with the system PLL
#define WAIT_CYCLES   200000U
#define MCLK_RATIO    256U // MCLK out: MCLK = 256 * Fs, valid for CS4398 single and double speed
#define SPEED_HIGH_HZ 50000U // CS4398 double-speed range starts here

typedef struct {
    uint32_t rate;
    uint8_t bits; // 0: any width
    uint8_t pll_n; // PLLSAI1 N (VCO = 16 MHz * N)
    uint8_t pll_p; // PLLSAI1 P divider
    uint8_t mckdiv;
} I2sPllCfg;

/* PLLSAI1 VCO must stay within 64..344 MHz. The USB feedback endpoint reports
 * the real rate to the host, so small offsets never make the stream drift. */

/* Internal, divider straight to BCK.
 * 16-bit: 32-bit frame, exact at 32/48 kHz, ~400 ppm elsewhere.
 * 24-bit: 64-bit frame, exact at 32 kHz, ~400 ppm at 44.1/48 kHz. */
static const I2sPllCfg i2s_internal_table[] = {
    {.rate = 32000, .bits = 16, .pll_n = 16, .pll_p = 5, .mckdiv = 50}, // 51.2 MHz
    {.rate = 44100, .bits = 16, .pll_n = 21, .pll_p = 7, .mckdiv = 34}, // 48.0 MHz
    {.rate = 48000, .bits = 16, .pll_n = 12, .pll_p = 5, .mckdiv = 25}, // 38.4 MHz
    {.rate = 88200, .bits = 16, .pll_n = 21, .pll_p = 7, .mckdiv = 17}, // 48.0 MHz
    {.rate = 96000, .bits = 16, .pll_n = 19, .pll_p = 9, .mckdiv = 11}, // 33.8 MHz
    {.rate = 32000, .bits = 24, .pll_n = 16, .pll_p = 5, .mckdiv = 25}, // 51.2 MHz
    {.rate = 44100, .bits = 24, .pll_n = 21, .pll_p = 7, .mckdiv = 17}, // 48.0 MHz
    {.rate = 48000, .bits = 24, .pll_n = 19, .pll_p = 9, .mckdiv = 11}, // 33.8 MHz
};

/* MCLK out, divider to MCLK = 256 Fs; BCK follows from the frame length.
 * Needing 256 Fs from 16 MHz * N / P leaves larger offsets:
 * 44.1 kHz ~400 ppm, 32/48/96 kHz ~1600 ppm, 88.2 kHz ~3900 ppm. */
static const I2sPllCfg i2s_mclk_table[] = {
    {.rate = 32000, .pll_n = 20, .pll_p = 13, .mckdiv = 3}, // MCLK 8.205 MHz
    {.rate = 44100, .pll_n = 12, .pll_p = 17, .mckdiv = 1}, // MCLK 11.294 MHz
    {.rate = 48000, .pll_n = 10, .pll_p = 13, .mckdiv = 1}, // MCLK 12.308 MHz
    {.rate = 88200, .pll_n = 17, .pll_p = 4, .mckdiv = 3}, // MCLK 22.667 MHz
    {.rate = 96000, .pll_n = 20, .pll_p = 13, .mckdiv = 1}, // MCLK 24.615 MHz
};

static const char* const i2s_source_names[I2sClockCount] = {
    "Internal",
    "MCLK out",
    "Ext 24.576M",
    "Ext 22.579M",
};

static const GpioPin* const i2s_pins[] = {&gpio_ext_pb3, &gpio_ext_pa4, &gpio_ext_pc3};
#define I2S_PIN_MCLK   (&gpio_ibutton) // PB14: header pin 17 and the iButton contact
#define I2S_PIN_EXTCLK (&gpio_ext_pb2) // header pin 6
#define I2S_PIN_SPEED  (&gpio_ext_pc1) // header pin 15
#define I2S_PIN_RESET  (&gpio_ext_pc0) // header pin 16

typedef struct {
    uint64_t sai_num; // kernel clock = sai_num / sai_den Hz
    uint32_t sai_den;
    uint32_t mckdiv;
    uint32_t frame; // bits per stereo frame
    bool mclk; // MCLK divider path (NODIV = 0)
} I2sTiming;

static struct {
    uint32_t saved_pllsai1cfgr;
    bool saved_pllsai1_on;
    uint32_t saved_clk48_src;
    uint32_t saved_sai_src;
    bool saved_hsi48_on;
    bool crs_bus_ours;
    bool sai_bus_ours;
    I2sClockSource source;
    bool running;
    uint32_t len;
    I2sTiming timing;
    bool timing_valid;
} i2s;

static bool wait_flag(volatile uint32_t* reg, uint32_t mask, bool set) {
    for(uint32_t i = 0; i < WAIT_CYCLES; i++) {
        if(((*reg & mask) != 0) == set) return true;
    }
    return false;
}

static inline bool i2s_source_external(I2sClockSource source) {
    return source == I2sClockExt24576 || source == I2sClockExt22579;
}

static inline bool i2s_source_has_dac_control(I2sClockSource source) {
    return source != I2sClockInternal;
}

static uint32_t i2s_ext_hz(I2sClockSource source) {
    return source == I2sClockExt24576 ? 24576000U : 22579200U;
}

static inline uint32_t i2s_frame_bits(uint8_t bits) {
    return bits == 16 ? 32U : 64U;
}

static bool pllsai1_stop(void) {
    LL_RCC_PLLSAI1_Disable();
    return wait_flag(&RCC->CR, RCC_CR_PLLSAI1RDY, false);
}

static void pllsai1_set(uint8_t n, uint8_t p) {
    uint32_t cfg = RCC->PLLSAI1CFGR;
    cfg &=
        ~(RCC_PLLSAI1CFGR_PLLN | RCC_PLLSAI1CFGR_PLLP | RCC_PLLSAI1CFGR_PLLPEN |
          RCC_PLLSAI1CFGR_PLLQEN | RCC_PLLSAI1CFGR_PLLREN);
    cfg |= ((uint32_t)n << RCC_PLLSAI1CFGR_PLLN_Pos) |
           ((uint32_t)(p - 1U) << RCC_PLLSAI1CFGR_PLLP_Pos) | RCC_PLLSAI1CFGR_PLLPEN;
    RCC->PLLSAI1CFGR = cfg;
}

static bool pllsai1_start(void) {
    LL_RCC_PLLSAI1_Enable();
    return wait_flag(&RCC->CR, RCC_CR_PLLSAI1RDY, true);
}

static const I2sPllCfg*
    i2s_find_pll(const I2sPllCfg* table, size_t count, uint32_t rate, uint8_t bits) {
    for(size_t i = 0; i < count; i++) {
        if(table[i].rate == rate && (table[i].bits == 0 || table[i].bits == bits)) {
            return &table[i];
        }
    }
    return NULL;
}

static bool i2s_timing_for(uint32_t rate, uint8_t bits, I2sTiming* t, const I2sPllCfg** pll) {
    const uint32_t frame = i2s_frame_bits(bits);
    const I2sPllCfg* cfg = NULL;
    *pll = NULL;

    switch(i2s.source) {
    case I2sClockInternal:
        cfg = i2s_find_pll(i2s_internal_table, COUNT_OF(i2s_internal_table), rate, bits);
        if(!cfg) return false;
        *t = (I2sTiming){PLL_REF_HZ * (uint64_t)cfg->pll_n, cfg->pll_p, cfg->mckdiv, frame, false};
        break;
    case I2sClockMclkOut:
        cfg = i2s_find_pll(i2s_mclk_table, COUNT_OF(i2s_mclk_table), rate, bits);
        if(!cfg) return false;
        *t = (I2sTiming){PLL_REF_HZ * (uint64_t)cfg->pll_n, cfg->pll_p, cfg->mckdiv, frame, true};
        break;
    default: {
        const uint32_t ext = i2s_ext_hz(i2s.source);
        const uint32_t bclk = rate * frame;
        if(ext % bclk) return false;
        const uint32_t div = ext / bclk;
        if(div < 1 || div > 63) return false;
        if(ext / rate > 512U) return false; // CS4398 stand-alone tops out at 512 x Fs
        *t = (I2sTiming){ext, 1, div, frame, false};
        break;
    }
    }
    *pll = cfg;
    return true;
}

static void i2s_source_pins(I2sClockSource source, bool enable) {
    if(source == I2sClockMclkOut) {
        if(enable) {
            furi_hal_gpio_init_ex(
                I2S_PIN_MCLK,
                GpioModeAltFunctionPushPull,
                GpioPullNo,
                GpioSpeedVeryHigh,
                GpioAltFn13SAI1);
        } else {
            furi_hal_gpio_init_simple(I2S_PIN_MCLK, GpioModeAnalog);
        }
    }
    if(i2s_source_external(source)) {
        if(enable) {
            furi_hal_gpio_init_ex(
                I2S_PIN_EXTCLK,
                GpioModeAltFunctionPushPull,
                GpioPullNo,
                GpioSpeedVeryHigh,
                GpioAltFn13SAI1);
        } else {
            furi_hal_gpio_init_simple(I2S_PIN_EXTCLK, GpioModeAnalog);
        }
    }
    if(i2s_source_has_dac_control(source)) {
        if(enable) {
            furi_hal_gpio_write(I2S_PIN_SPEED, false);
            furi_hal_gpio_write(I2S_PIN_RESET, false);
            furi_hal_gpio_init(I2S_PIN_SPEED, GpioModeOutputPushPull, GpioPullNo, GpioSpeedLow);
            furi_hal_gpio_init(I2S_PIN_RESET, GpioModeOutputPushPull, GpioPullNo, GpioSpeedLow);
        } else {
            furi_hal_gpio_init_simple(I2S_PIN_SPEED, GpioModeAnalog);
            furi_hal_gpio_init_simple(I2S_PIN_RESET, GpioModeAnalog);
        }
    }
}

void i2s_out_init(I2sClockSource source) {
    memset(&i2s, 0, sizeof(i2s));

    i2s.saved_pllsai1cfgr = RCC->PLLSAI1CFGR;
    i2s.saved_pllsai1_on = LL_RCC_PLLSAI1_IsReady();
    i2s.saved_clk48_src = LL_RCC_GetCLK48ClockSource(LL_RCC_CLK48_CLKSOURCE);
    i2s.saved_sai_src = LL_RCC_GetSAIClockSource(LL_RCC_SAI1_CLKSOURCE);
    i2s.saved_hsi48_on = LL_RCC_HSI48_IsReady();

    // USB keeps running from HSI48 trimmed by CRS against host SOF,
    // which frees PLLSAI1 to be retuned for each audio rate.
    LL_RCC_HSI48_Enable();
    if(!wait_flag(&RCC->CRRCR, RCC_CRRCR_HSI48RDY, true)) {
        FURI_LOG_E(TAG, "HSI48 failed to start");
    }
    if(!furi_hal_bus_is_enabled(FuriHalBusCRS)) {
        furi_hal_bus_enable(FuriHalBusCRS);
        i2s.crs_bus_ours = true;
    }
    LL_CRS_SetSyncSignalSource(LL_CRS_SYNC_SOURCE_USB);
    LL_CRS_SetSyncDivider(LL_CRS_SYNC_DIV_1);
    LL_CRS_SetReloadCounter(__LL_CRS_CALC_CALCULATE_RELOADVALUE(48000000U, 1000U));
    LL_CRS_SetFreqErrorLimit(LL_CRS_ERRORLIMIT_DEFAULT);
    LL_CRS_EnableAutoTrimming();
    LL_CRS_EnableFreqErrorCounter();
    LL_RCC_SetCLK48ClockSource(LL_RCC_CLK48_CLKSOURCE_HSI48);

    if(!furi_hal_bus_is_enabled(FuriHalBusSAI1)) {
        furi_hal_bus_enable(FuriHalBusSAI1);
        i2s.sai_bus_ours = true;
    }
    // Start from reset values: a block left stuck by an earlier run
    // would otherwise ignore SAIEN until the Flipper reboots
    furi_hal_bus_reset(FuriHalBusSAI1);

    for(size_t i = 0; i < COUNT_OF(i2s_pins); i++) {
        furi_hal_gpio_init_ex(
            i2s_pins[i],
            GpioModeAltFunctionPushPull,
            GpioPullNo,
            GpioSpeedVeryHigh,
            GpioAltFn13SAI1);
    }

    i2s.source = source;
    LL_RCC_SetSAIClockSource(
        i2s_source_external(source) ? LL_RCC_SAI1_CLKSOURCE_PIN : LL_RCC_SAI1_CLKSOURCE_PLLSAI1);
    i2s_source_pins(source, true);
}

void i2s_out_deinit(void) {
    i2s_out_stop();
    i2s_source_pins(i2s.source, false);

    for(size_t i = 0; i < COUNT_OF(i2s_pins); i++) {
        furi_hal_gpio_init_simple(i2s_pins[i], GpioModeAnalog);
    }

    if(i2s.sai_bus_ours) furi_hal_bus_disable(FuriHalBusSAI1);
    LL_RCC_SetSAIClockSource(i2s.saved_sai_src);

    pllsai1_stop();
    RCC->PLLSAI1CFGR = i2s.saved_pllsai1cfgr;
    if(i2s.saved_pllsai1_on && !pllsai1_start()) {
        FURI_LOG_E(TAG, "PLLSAI1 failed to relock");
    }
    LL_RCC_SetCLK48ClockSource(i2s.saved_clk48_src);

    LL_CRS_DisableAutoTrimming();
    LL_CRS_DisableFreqErrorCounter();
    if(i2s.crs_bus_ours) furi_hal_bus_disable(FuriHalBusCRS);
    if(!i2s.saved_hsi48_on) LL_RCC_HSI48_Disable();
}

void i2s_out_set_source(I2sClockSource source) {
    furi_check(source < I2sClockCount);
    i2s_out_stop();
    i2s_source_pins(i2s.source, false);
    i2s.source = source;
    i2s.timing_valid = false;
    LL_RCC_SetSAIClockSource(
        i2s_source_external(source) ? LL_RCC_SAI1_CLKSOURCE_PIN : LL_RCC_SAI1_CLKSOURCE_PLLSAI1);
    i2s_source_pins(source, true);
}

I2sClockSource i2s_out_get_source(void) {
    return i2s.source;
}

const char* i2s_out_source_name(I2sClockSource source) {
    return source < I2sClockCount ? i2s_source_names[source] : "?";
}

bool i2s_out_rate_supported(uint32_t rate, uint8_t bits) {
    I2sTiming t;
    const I2sPllCfg* pll;
    return i2s_timing_for(rate, bits, &t, &pll);
}

static void i2s_configure_blocks(uint8_t bits, const I2sTiming* t) {
    SAI_Block_TypeDef* clk = SAI1_Block_B; // drives BCK/LRCK pins
    SAI_Block_TypeDef* data = SAI1_Block_A; // drives DIN (and MCLK)

    const bool wide = bits == 24;
    const uint32_t ds = wide ? (SAI_xCR1_DS_2 | SAI_xCR1_DS_1) : SAI_xCR1_DS_2;
    // Signals change on falling SCK edge (I2S)
    const uint32_t cr1 = ds | SAI_xCR1_CKSTR | ((uint32_t)t->mckdiv << SAI_xCR1_MCKDIV_Pos);

    if(t->mclk) {
        // B: master receiver, MCLK divider path. A: synchronous slave sending
        // data, its own divider set to the same MCKDIV to drive MCLK_A
        clk->CR1 = cr1 | SAI_xCR1_MODE_0 | SAI_xCR1_MCKEN;
        data->CR1 = cr1 | SAI_xCR1_MODE_1 | SAI_xCR1_SYNCEN_0 | SAI_xCR1_MCKEN;
    } else {
        clk->CR1 = cr1 | SAI_xCR1_NODIV | SAI_xCR1_MODE_0; // master receiver
        data->CR1 = ds | SAI_xCR1_CKSTR | SAI_xCR1_MODE_1 | SAI_xCR1_SYNCEN_0; // sync slave tx
    }

    // FS active low for left slot, asserted one bit early
    const uint32_t frcr = ((t->frame - 1U) << SAI_xFRCR_FRL_Pos) |
                          ((t->frame / 2U - 1U) << SAI_xFRCR_FSALL_Pos) | SAI_xFRCR_FSDEF |
                          SAI_xFRCR_FSOFF;
    clk->FRCR = frcr;
    data->FRCR = frcr;

    // 16-bit: slot = data size. 24-bit: MSB-first data in a 32-bit slot
    const uint32_t slotr = (1U << SAI_xSLOTR_NBSLOT_Pos) | (0x3U << SAI_xSLOTR_SLOTEN_Pos) |
                           (wide ? SAI_xSLOTR_SLOTSZ_1 : 0);
    clk->SLOTR = slotr;
    data->SLOTR = slotr;

    clk->CR2 = SAI_xCR2_FFLUSH;
    data->CR2 = SAI_xCR2_FFLUSH | SAI_xCR2_FTH_0; // DMA request below 1/4 FIFO
}

bool i2s_out_start(uint32_t rate, uint8_t bits, void* buf, uint32_t len_samples) {
    I2sTiming t;
    const I2sPllCfg* pll;
    if(!i2s_timing_for(rate, bits, &t, &pll)) return false;

    i2s_out_stop();
    if(pll) {
        if(!pllsai1_stop()) return false;
        pllsai1_set(pll->pll_n, pll->pll_p);
        // SAIEN is ignored while the kernel clock is stopped, so lock first
        if(!pllsai1_start()) {
            FURI_LOG_E(TAG, "PLLSAI1 lock failed");
            return false;
        }
    }

    i2s_configure_blocks(bits, &t);

    const bool wide = bits == 24;
    LL_DMA_InitTypeDef dma = {
        .PeriphOrM2MSrcAddress = (uint32_t)&SAI1_Block_A->DR,
        .MemoryOrM2MDstAddress = (uint32_t)buf,
        .Direction = LL_DMA_DIRECTION_MEMORY_TO_PERIPH,
        .Mode = LL_DMA_MODE_CIRCULAR,
        .PeriphOrM2MSrcIncMode = LL_DMA_PERIPH_NOINCREMENT,
        .MemoryOrM2MDstIncMode = LL_DMA_MEMORY_INCREMENT,
        .PeriphOrM2MSrcDataSize = wide ? LL_DMA_PDATAALIGN_WORD : LL_DMA_PDATAALIGN_HALFWORD,
        .MemoryOrM2MDstDataSize = wide ? LL_DMA_MDATAALIGN_WORD : LL_DMA_MDATAALIGN_HALFWORD,
        .NbData = len_samples,
        .PeriphRequest = LL_DMAMUX_REQ_SAI1_A,
        .Priority = LL_DMA_PRIORITY_VERYHIGH,
    };
    LL_DMA_Init(I2S_DMA, I2S_DMA_CH, &dma);
    LL_DMA_EnableChannel(I2S_DMA, I2S_DMA_CH);

    // Data block first so it is ready on the first FS edge, then the clock block
    SAI1_Block_A->CR1 |= SAI_xCR1_DMAEN;
    SAI1_Block_A->CR1 |= SAI_xCR1_SAIEN;
    SAI1_Block_B->CR1 |= SAI_xCR1_SAIEN;

    if(i2s_source_has_dac_control(i2s.source)) {
        furi_hal_gpio_write(I2S_PIN_SPEED, rate > SPEED_HIGH_HZ);
        furi_hal_gpio_write(I2S_PIN_RESET, true); // clocks are stable from here
    }

    i2s.len = len_samples;
    i2s.timing = t;
    i2s.timing_valid = true;
    i2s.running = true;
    return true;
}

void i2s_out_stop(void) {
    if(i2s_source_has_dac_control(i2s.source)) {
        furi_hal_gpio_write(I2S_PIN_RESET, false);
    }
    SAI1_Block_B->CR1 &= ~SAI_xCR1_SAIEN;
    SAI1_Block_A->CR1 &= ~(SAI_xCR1_SAIEN | SAI_xCR1_DMAEN);
    wait_flag(&SAI1_Block_A->CR1, SAI_xCR1_SAIEN, false);
    wait_flag(&SAI1_Block_B->CR1, SAI_xCR1_SAIEN, false);
    SAI1_Block_A->CR1 &= ~SAI_xCR1_MCKEN;
    SAI1_Block_B->CR1 &= ~SAI_xCR1_MCKEN;
    LL_DMA_DisableChannel(I2S_DMA, I2S_DMA_CH);
    i2s.running = false;
}

bool i2s_out_is_running(void) {
    return i2s.running;
}

uint32_t i2s_out_get_read_pos(void) {
    if(!i2s.running) return 0;
    uint32_t left = LL_DMA_GetDataLength(I2S_DMA, I2S_DMA_CH);
    return (i2s.len - left) % i2s.len;
}

uint32_t i2s_out_get_sai_clk(void) {
    if(!i2s.timing_valid) return 0;
    return (uint32_t)(i2s.timing.sai_num / i2s.timing.sai_den);
}

uint32_t i2s_out_get_mclk(void) {
    if(!i2s.timing_valid) return 0;
    const I2sTiming* t = &i2s.timing;
    if(t->mclk) return (uint32_t)(t->sai_num / ((uint64_t)t->sai_den * t->mckdiv));
    if(i2s_source_external(i2s.source)) return (uint32_t)(t->sai_num / t->sai_den);
    return 0;
}

uint32_t i2s_out_get_bclk(void) {
    if(!i2s.timing_valid) return 0;
    const I2sTiming* t = &i2s.timing;
    uint64_t den = (uint64_t)t->sai_den * t->mckdiv;
    // MCLK path: BCK = MCLK * frame / 256
    if(t->mclk) return (uint32_t)(t->sai_num * t->frame / (den * MCLK_RATIO));
    return (uint32_t)(t->sai_num / den);
}

uint32_t i2s_out_get_actual_rate_mhz(void) {
    if(!i2s.timing_valid) return 0;
    const I2sTiming* t = &i2s.timing;
    uint64_t den = (uint64_t)t->sai_den * t->mckdiv * (t->mclk ? MCLK_RATIO : t->frame);
    return (uint32_t)(t->sai_num * 1000U / den);
}
