#include "hvac_hitachi.h"

#include <core/check.h>

#define TAG "hvac_hitachi"

#define HVAC_HITACHI_HDR_MARK   3350
#define HVAC_HITACHI_HDR_SPACE  1650
#define HVAC_HITACHI_BIT_MARK   450
#define HVAC_HITACHI_ONE_SPACE  1250
#define HVAC_HITACHI_ZERO_SPACE 400
#define HVAC_HITACHI_RPT_MARK   430

#define HVAC_HITACHI_FC   (38000u)
#define HVAC_HITACHI_DUTY (0.33f)

static const uint8_t SYNC_BYTES[3] = {0x01, 0x10, 0x00};

static uint8_t convert_temp(uint_fast8_t temperature) {
    if(temperature < HVAC_HITACHI_TEMPERATURE_MIN) {
        temperature = HVAC_HITACHI_TEMPERATURE_MIN;
    } else if(temperature > HVAC_HITACHI_TEMPERATURE_MAX) {
        temperature = HVAC_HITACHI_TEMPERATURE_MAX;
    }

    return (temperature - 7) << 1;
}

static inline void
    set_timer(HvacHitachiContext* const ctx, uint_fast16_t off_timer, uint_fast16_t on_timer) {
    ctx->msg.nm.set_timer.timer_lo = (off_timer & 0xfff) | ((on_timer & 0xf) << 12);
    ctx->msg.nm.set_timer.timer_hi = (on_timer & 0xfff) >> 4;
}

static void
    convert_to_nm(HvacHitachiContext* const ctx, uint8_t size, HvacHitachiKeycode keycode) {
    ctx->msg.header.size = size;
    ctx->msg.common.keycode_side &= ~HVAC_HITACHI_KEYCODE_MASK;
    ctx->msg.common.keycode_side |= keycode;
    ctx->msg.nm.sb3f = 0x3f;
}

static void convert_to_tm(HvacHitachiContext* const ctx) {
    ctx->msg.header.size = HVAC_HITACHI_SIZE_TM;
    ctx->msg.common.keycode_side &= ~HVAC_HITACHI_KEYCODE_MASK;
    ctx->msg.common.keycode_side |= HvacHitachiKeycodeTest;
}

static inline void convert_to_set1(HvacHitachiContext* const ctx, HvacHitachiKeycode keycode) {
    convert_to_nm(ctx, HVAC_HITACHI_SIZE_SET1, keycode);
}

static size_t calc_message_size(HvacHitachiContext* const ctx) {
    size_t payload_size = ctx->msg.header.size - 0xdf;
    return sizeof(HvacHitachiMessageHeader) + payload_size;
}

static void write_preamble(HvacHitachiContext* const ctx) {
    size_t next_sample = ctx->num_samples;
    furi_check(next_sample + 2 <= sizeof(ctx->samples) / 4);
    ctx->samples[next_sample++] = HVAC_HITACHI_HDR_MARK;
    ctx->samples[next_sample++] = HVAC_HITACHI_HDR_SPACE;
    ctx->num_samples = next_sample;
}

static void write_epilogue(HvacHitachiContext* const ctx) {
    size_t next_sample = ctx->num_samples;
    furi_check(next_sample + 1 <= sizeof(ctx->samples) / 4);
    ctx->samples[next_sample++] = HVAC_HITACHI_RPT_MARK;
    ctx->num_samples = next_sample;
}

static void write_byte(HvacHitachiContext* const ctx, uint8_t byte) {
    size_t next_sample = ctx->num_samples;
    furi_check(next_sample + 16 <= sizeof(ctx->samples) / 4);
    for(size_t i = 0; i < 8; i++) {
        ctx->samples[next_sample++] = HVAC_HITACHI_BIT_MARK;
        ctx->samples[next_sample++] = (byte & 1) ? HVAC_HITACHI_ONE_SPACE :
                                                   HVAC_HITACHI_ZERO_SPACE;
        byte >>= 1;
    }
    ctx->num_samples = next_sample;
}

HvacHitachiContext* hvac_hitachi_init() {
    HvacHitachiContext* ctx = malloc(sizeof(HvacHitachiContext));
    furi_check(ctx != NULL);
    hvac_hitachi_reset(ctx);
    return ctx;
}

void hvac_hitachi_deinit(HvacHitachiContext* ctx) {
    if(ctx != NULL) {
        free(ctx);
    }
}

void hvac_hitachi_reset(HvacHitachiContext* const ctx) {
    furi_check(ctx);
    memset(&ctx->msg, 0, sizeof(ctx->msg));
    ctx->num_samples = 0;
    ctx->msg.header.type = 0x40;
    ctx->msg.header.sbff = 0xff;
    ctx->msg.header.size = 0;
    ctx->msg.common.keycode_side = 0;
    ctx->msg.common.sb89 = 0x89;
}

void hvac_hitachi_switch_side(HvacHitachiContext* const ctx, HvacHitachiSide side) {
    furi_check(ctx);
    ctx->msg.common.keycode_side &= ~HVAC_HITACHI_ADDRESS_MASK;
    ctx->msg.common.keycode_side |= side;
}

void hvac_hitachi_set_temperature(
    HvacHitachiContext* const ctx,
    uint_fast8_t temperature,
    bool is_up) {
    furi_check(ctx);
    convert_to_set1(ctx, is_up ? HvacHitachiKeycodeTempUp : HvacHitachiKeycodeTempDown);
    ctx->msg.nm.set1.temperature = convert_temp(temperature);
}

void hvac_hitachi_set_fan_speed_mode(
    HvacHitachiContext* const ctx,
    HvacHitachiFanSpeed fan_speed,
    HvacHitachiMode mode) {
    furi_check(ctx);
    convert_to_set1(ctx, HvacHitachiKeycodeFanSpeed);
    ctx->msg.nm.set1.fan_speed_mode = fan_speed | mode;
}

void hvac_hitachi_set_vane(HvacHitachiContext* const ctx, HvacHitachiVane vane) {
    furi_check(ctx);
    convert_to_set1(ctx, HvacHitachiKeycodeVane);
    ctx->msg.nm.set1.vane = vane;
}

void hvac_hitachi_set_mode(
    HvacHitachiContext* const ctx,
    uint_fast8_t temperature,
    HvacHitachiFanSpeed fan_speed,
    HvacHitachiMode mode,
    HvacHitachiVane vane) {
    furi_check(ctx);
    convert_to_nm(ctx, HVAC_HITACHI_SIZE_MODE, HvacHitachiKeycodeMode);
    ctx->msg.nm.set_mode_power.temperature = convert_temp(temperature);
    ctx->msg.nm.set_mode_power.fan_speed_mode = fan_speed | mode;
    ctx->msg.nm.set_mode_power.vane = vane;
}

void hvac_hitachi_set_power(
    HvacHitachiContext* const ctx,
    uint_fast8_t temperature,
    HvacHitachiFanSpeed fan_speed,
    HvacHitachiMode mode,
    HvacHitachiVane vane,
    HvacHitachiControl control) {
    furi_check(ctx);
    convert_to_nm(ctx, HVAC_HITACHI_SIZE_POWER, HvacHitachiKeycodePower);
    ctx->msg.nm.set_mode_power.temperature = convert_temp(temperature);
    ctx->msg.nm.set_mode_power.fan_speed_mode = fan_speed | mode;
    ctx->msg.nm.set_mode_power.vane = vane;
    ctx->msg.nm.set_mode_power.control = control;
}

void hvac_hitachi_set_timer(
    HvacHitachiContext* const ctx,
    uint_fast8_t temperature,
    uint_fast16_t off_timer,
    uint_fast16_t on_timer,
    HvacHitachiFanSpeed fan_speed,
    HvacHitachiMode mode,
    HvacHitachiVane vane) {
    furi_check(ctx);
    convert_to_nm(ctx, HVAC_HITACHI_SIZE_TIMER, HvacHitachiKeycodeTimerSet);
    ctx->msg.nm.set_timer.temperature = convert_temp(temperature);
    set_timer(ctx, off_timer, on_timer);
    ctx->msg.nm.set_timer.fan_speed_mode = fan_speed | mode;
    ctx->msg.nm.set_timer.vane = vane;
}

void hvac_hitachi_reset_timer(HvacHitachiContext* const ctx) {
    furi_check(ctx);
    convert_to_nm(ctx, HVAC_HITACHI_SIZE_SET0, HvacHitachiKeycodeTimerReset);
}

void hvac_hitachi_reset_filter(HvacHitachiContext* const ctx) {
    furi_check(ctx);
    convert_to_nm(ctx, HVAC_HITACHI_SIZE_KEYCODE_ONLY, HvacHitachiKeycodeResetFilter);
}

void hvac_hitachi_test_mode(
    HvacHitachiContext* const ctx,
    uint_fast8_t temperature,
    HvacHitachiMode mode,
    uint_fast16_t off_timer) {
    furi_check(ctx);
    convert_to_tm(ctx);
    ctx->msg.tm.temperature = convert_temp(temperature);
    ctx->msg.tm.timer_mode = (mode << 12) | (off_timer & 0xfff);
}

void hvac_hitachi_build_samples(HvacHitachiContext* const ctx) {
    furi_check(ctx);
    ctx->num_samples = 0;
    // Simple sanity check.
    if(ctx->msg.header.size == 0 || ctx->msg.common.keycode_side == 0) {
        return;
    }
    write_preamble(ctx);
    for(size_t i = 0; i < sizeof(SYNC_BYTES); i++) {
        write_byte(ctx, SYNC_BYTES[i]);
    }
    uint8_t* message_buffer = (uint8_t*)&ctx->msg;
    for(size_t i = 0; i < calc_message_size(ctx); i++) {
        write_byte(ctx, message_buffer[i]);
        write_byte(ctx, 0xffu ^ message_buffer[i]);
    }
    write_epilogue(ctx);
}

void hvac_hitachi_send(HvacHitachiContext* const ctx) {
    furi_check(ctx);
    if(ctx->num_samples == 0) {
        hvac_hitachi_build_samples(ctx);
    }
    infrared_send_raw_ext(
        ctx->samples, ctx->num_samples, true, HVAC_HITACHI_FC, HVAC_HITACHI_DUTY);
}
