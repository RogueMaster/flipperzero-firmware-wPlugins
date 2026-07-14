/**
 * @file TEA5767.h
 * @brief TEA5767 FM tuner over I2C (register map + high-level API).
 *
 * Datasheet: https://www.sparkfun.com/datasheets/Wireless/General/TEA5767.pdf
 */
#ifndef TEA5767_H
#define TEA5767_H

#include <stdbool.h>
#include <stdint.h>

#define TEA5767_ADR 0xC0

#define QUARTZ 32768
#define FILTER 225000

#define REG_1      0x00
#define REG_1_MUTE 0x80
#define REG_1_SM   0x40
#define REG_1_PLL  0x3F

#define REG_2     0x01
#define REG_2_PLL 0xFF

#define REG_3            0x02
#define REG_3_SUD        0x80
#define REG_3_SSL        0x60
#define REG_3_SSL_LOW    0x20
#define REG_3_SSL_MEDIUM 0x40
#define REG_3_SSL_HIGH   0x60
#define REG_3_HLSI       0x10
#define REG_3_MS         0x08
#define REG_3_MR         0x04
#define REG_3_ML         0x02
#define REG_3_SWP1       0x01

#define REG_4      0x03
#define REG_4_SWP2 0x80
#define REG_4_STBY 0x40
#define REG_4_BL   0x20
#define REG_4_XTAL 0x10
#define REG_4_HCC  0x04
#define REG_4_SNC  0x02
#define REG_4_SI   0x01

#define REG_5        0x04
#define REG_5_PLLREF 0x80
#define REG_5_DTC    0x40

struct RADIO_INFO {
    float frequency;
    int signalLevel;
    bool stereo;
    bool muted;
    bool ready;
    bool bandLimit;
    uint8_t ifCounter;
    char signalQuality[10];
};

/** Read five status/write registers into @p buffer. */
bool tea5767_read_registers(uint8_t* buffer);

/** Write five registers from @p buffer. */
bool tea5767_write_registers(uint8_t* buffer);

/** Load default register image into @p buffer. */
bool tea5767_init(uint8_t* buffer);

/** Start hardware seek up or down. */
bool tea5767_seek(uint8_t* buffer, bool seek_up);

/** Read tuned frequency in 10 kHz units into @p value. */
bool tea5767_get_frequency(uint8_t* buffer, int* value);

/** Set tuned frequency from 10 kHz units in @p value. */
bool tea5767_set_frequency(uint8_t* buffer, int value);

/** Parse status registers into @p info. */
bool tea5767_get_radio_info(uint8_t* buffer, struct RADIO_INFO* info);

/** Remember SNC preference for subsequent register writes. */
void tea5767_set_snc_enabled(bool enabled);

/** Apply stereo noise cancelling bit in register image. */
bool tea5767_set_snc(bool enabled);

/** Remember de-emphasis preference (50 µs EU vs 75 µs US). */
void tea5767_set_deemphasis_75us_enabled(bool enabled);

/** Apply de-emphasis time constant bit. */
bool tea5767_set_deemphasis_75us(bool enabled);

/** Remember high-cut preference. */
void tea5767_set_high_cut_enabled(bool enabled);

/** Apply high cut control bit. */
bool tea5767_set_high_cut(bool enabled);

/** Remember force-mono preference. */
void tea5767_set_force_mono_enabled(bool enabled);

/** Apply force-mono bit. */
bool tea5767_set_force_mono(bool enabled);

/** Seek from frequency in 10 kHz units. */
void tea5767_seekFrom10kHz(uint32_t freq_10khz, bool seek_up);

/** Tune to @p freq_mhz and update cached MHz value. */
void tea5767_SetFreqMHz(float freq_mhz);

/** Return last tuned frequency in MHz. */
float tea5767_GetFreq(void);

#endif
