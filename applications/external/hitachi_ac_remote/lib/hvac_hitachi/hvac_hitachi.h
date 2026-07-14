/**
 * @file hvac_hitachi.h
 * @brief Hitachi remote code generator.
 * @version 0.1
 * @date 2025-05-23
 */

#pragma once

#include <stdint.h>

#include <infrared_transmit.h>
#include <infrared_worker.h>

#define HVAC_HITACHI_TEMPERATURE_MIN 17
#define HVAC_HITACHI_TEMPERATURE_MAX 30

#define HVAC_HITACHI_ADDRESS_MASK (0xf0u)
#define HVAC_HITACHI_KEYCODE_MASK (0x0fu)

typedef enum {
    HvacHitachiKeycodePower = 0x01,
    HvacHitachiKeycodeMode = 0x04,
    HvacHitachiKeycodeFanSpeed,
    HvacHitachiKeycodeTempDown = 0x08,
    HvacHitachiKeycodeTempUp,
    HvacHitachiKeycodeTest,
    HvacHitachiKeycodeTimerSet = 0x0b,
    HvacHitachiKeycodeTimerReset = 0x0d,
    HvacHitachiKeycodeVane,
    HvacHitachiKeycodeResetFilter,
} HvacHitachiKeycode;

typedef enum {
    HvacHitachiSideA,
    HvacHitachiSideB = 0x80,
} HvacHitachiSide;

typedef enum {
    HvacHitachiFanSpeedLow = 0x10,
    HvacHitachiFanSpeedMedium = 0x20,
    HvacHitachiFanSpeedHigh = 0x40,
} HvacHitachiFanSpeed;

typedef enum {
    HvacHitachiModeFan = 0x2,
    HvacHitachiModeDehumidifying = 0x4,
    HvacHitachiModeCooling = 0x6,
    HvacHitachiModeHeating = 0x8,
    HvacHitachiModeAuto = 0xb,
} HvacHitachiMode;

typedef enum {
    HvacHitachiVanePos0,
    HvacHitachiVaneAuto,
    HvacHitachiVanePos1,
    HvacHitachiVanePos2 = 0x4,
    HvacHitachiVanePos3 = 0x6,
    HvacHitachiVanePos4 = 0x8,
    HvacHitachiVanePos5 = 0xa,
    HvacHitachiVanePos6 = 0xc,
} HvacHitachiVane;

typedef enum {
    HvacHitachiControlPowerOn = 0x90,
    HvacHitachiControlPowerOff = 0xa0,
} HvacHitachiControl;

typedef struct FURI_PACKED HvacHitachiPayloadSet1_s {
    union {
        uint8_t temperature;
        uint8_t fan_speed_mode;
        uint8_t vane;
    };
} HvacHitachiPayloadSet1;

typedef struct FURI_PACKED HvacHitachiPayloadModePower_s {
    uint8_t temperature;
    uint8_t fan_speed_mode;
    uint8_t vane;
    uint8_t control;
} HvacHitachiPayloadModePower;

typedef struct FURI_PACKED HvacHitachiPayloadTimer_s {
    uint8_t temperature;
    uint16_t timer_lo;
    uint8_t timer_hi;
    uint8_t fan_speed_mode;
    uint8_t vane;
} HvacHitachiPayloadModeTimer;

typedef struct FURI_PACKED HvacHitachiMessageHeader_s {
    uint8_t type;
    uint8_t sbff;
    uint8_t size;
} HvacHitachiMessageHeader;

typedef struct FURI_PACKED HvacHitachiMessageCommon_s {
    uint8_t sb89;
    uint8_t keycode_side;
} HvacHitachiMessageCommon;

typedef struct FURI_PACKED HvacHitachiMessageNormal_s {
    uint8_t sb3f;
    union {
        HvacHitachiPayloadSet1 set1;
        HvacHitachiPayloadModePower set_mode_power;
        HvacHitachiPayloadModeTimer set_timer;
    };
} HvacHitachiMessageNormal;

typedef struct FURI_PACKED HvacHitachiMessageTest_s {
    uint8_t temperature;
    uint16_t timer_mode;
} HvacHitachiMessageTest;

typedef struct FURI_PACKED HvacHitachiMessage_s {
    HvacHitachiMessageHeader header;
    HvacHitachiMessageCommon common;
    union {
        HvacHitachiMessageNormal nm;
        HvacHitachiMessageTest tm;
    };
} HvacHitachiMessage;

#define HVAC_HITACHI_SIZE_NM_COMMON (sizeof(HvacHitachiMessageCommon) + sizeof(uint8_t))
#define HVAC_HITACHI_SIZE_SET0      (0xe0 - 1 + HVAC_HITACHI_SIZE_NM_COMMON)
#define HVAC_HITACHI_SIZE_SET1 \
    (0xe0 - 1 + HVAC_HITACHI_SIZE_NM_COMMON + sizeof(HvacHitachiPayloadSet1))
#define HVAC_HITACHI_SIZE_POWER \
    (0xe0 - 1 + HVAC_HITACHI_SIZE_NM_COMMON + sizeof(HvacHitachiPayloadModePower))
#define HVAC_HITACHI_SIZE_MODE (HVAC_HITACHI_SIZE_POWER - 1)
#define HVAC_HITACHI_SIZE_TIMER \
    (0xe0 - 1 + HVAC_HITACHI_SIZE_NM_COMMON + sizeof(HvacHitachiPayloadModeTimer))
#define HVAC_HITACHI_SIZE_KEYCODE_ONLY (0xe0 - 1 + sizeof(HvacHitachiMessageCommon))
#define HVAC_HITACHI_SIZE_TM \
    (0xe0 - 1 + sizeof(HvacHitachiMessageCommon) + sizeof(HvacHitachiMessageTest))

typedef struct HvacHitachiContext_s {
    /**
   * @brief Raw message buffer.
   */
    HvacHitachiMessage msg;
    /**
   * @brief Number of samples actually in the sample buffer.
   */
    size_t num_samples;
    /**
   * @brief The sample buffer.
   * @details Preamble (2 samples) + sync code (3 bytes) + max message size (w/
   * parity) + epilogue (1-2 samples) = 436 samples (1744 bytes)
   */
    uint32_t samples[2 + 8 * (3 + sizeof(HvacHitachiMessage) * 2) * 2 + 2];
} HvacHitachiContext;

/**
 * @brief Allocate the context buffer.
 *
 * @return Allocated context buffer.
 */
extern HvacHitachiContext* hvac_hitachi_init();

/**
 * @brief Deallocate the context buffer.
 *
 * @param ctx Allocated context buffer.
 */
extern void hvac_hitachi_deinit(HvacHitachiContext* const ctx);

/**
 * @brief Reset the context buffer.
 *
 * @param ctx Allocated context buffer.
 */
extern void hvac_hitachi_reset(HvacHitachiContext* const ctx);

/**
 * @brief Mark the message as a message for AC units operating on the specified
 * side.
 *
 * @param ctx Context buffer.
 * @param side Side the targeted AC unit is operating on.
 */
extern void hvac_hitachi_switch_side(HvacHitachiContext* const ctx, HvacHitachiSide side);

/**
 * @brief Build set temperature message.
 *
 * @param ctx Context buffer.
 * @param temperature Temperature in degree centigrade.
 */
extern void hvac_hitachi_set_temperature(
    HvacHitachiContext* const ctx,
    uint_fast8_t temperature,
    bool is_up);

/**
 * @brief Build set fan speed/mode message.
 *
 * @param ctx Context buffer.
 * @param fan_speed Fan speed.
 * @param mode Mode.
 */
extern void hvac_hitachi_set_fan_speed_mode(
    HvacHitachiContext* const ctx,
    HvacHitachiFanSpeed fan_speed,
    HvacHitachiMode mode);

/**
 * @brief Build set angle of vane message.
 *
 * @param ctx Context buffer.
 * @param vane Vane Angle of vane.
 */
extern void hvac_hitachi_set_vane(HvacHitachiContext* const ctx, HvacHitachiVane vane);

/**
 * @brief Build set mode message.
 *
 * @param ctx Context buffer.
 * @param temperature Temperature in degree centigrade.
 * @param fan_speed Fan speed.
 * @param mode Mode.
 * @param vane Angle of vane.
 */
extern void hvac_hitachi_set_mode(
    HvacHitachiContext* const ctx,
    uint_fast8_t temperature,
    HvacHitachiFanSpeed fan_speed,
    HvacHitachiMode mode,
    HvacHitachiVane vane);

/**
 * @brief Build set power message.
 *
 * @param ctx Context buffer.
 * @param temperature Temperature in degree centigrade.
 * @param fan_speed Fan speed.
 * @param mode Mode.
 * @param vane Angle of vane.
 * @param control Control flag.
 */
extern void hvac_hitachi_set_power(
    HvacHitachiContext* const ctx,
    uint_fast8_t temperature,
    HvacHitachiFanSpeed fan_speed,
    HvacHitachiMode mode,
    HvacHitachiVane vane,
    HvacHitachiControl control);

/**
 * @brief Build set timer message.
 *
 * @param ctx Context buffer.
 * @param temperature Temperature in degree centigrade.
 * @param off_timer Off timer in minutes.
 * @param on_timer On timer in minutes.
 * @param fan_speed Fan speed.
 * @param mode Mode.
 * @param vane Angle of vane.
 */
extern void hvac_hitachi_set_timer(
    HvacHitachiContext* const ctx,
    uint_fast8_t temperature,
    uint_fast16_t off_timer,
    uint_fast16_t on_timer,
    HvacHitachiFanSpeed fan_speed,
    HvacHitachiMode mode,
    HvacHitachiVane vane);

/**
 * @brief Build reset timer message.
 *
 * @param ctx Context buffer.
 */
extern void hvac_hitachi_reset_timer(HvacHitachiContext* const ctx);

/**
 * @brief Build reset filter message.
 *
 * @param ctx Context buffer.
 */
extern void hvac_hitachi_reset_filter(HvacHitachiContext* const ctx);

/**
 * @brief Build enter test mode message.
 * 
 * @param ctx Context buffer.
 * @param temperature Temperature in degree centigrade.
 * @param mode Mode.
 * @param off_timer Off timer in minutes. Set this to 120 to mimick the behavior of the original remote.
 */
extern void hvac_hitachi_test_mode(
    HvacHitachiContext* const ctx,
    uint_fast8_t temperature,
    HvacHitachiMode mode,
    uint_fast16_t off_timer);

/**
 * @brief Build message before sending.
 * 
 * @param ctx Context buffer.
 */
extern void hvac_hitachi_build_samples(HvacHitachiContext* const ctx);

/**
 * @brief Send a built message.
 *
 * @param ctx Context buffer.
 */
extern void hvac_hitachi_send(HvacHitachiContext* const ctx);
