#include <core/check.h>
#include <core/log.h>
#include "infrared_transmit.h"
#include "fujitsu_general_ir.h"

static const uint32_t c_BroadcastStartOnPulse = 3270;
static const uint32_t c_BroadcastStartOffPulse = 1630;
static const uint32_t c_ShortPulse = 411;
static const uint32_t c_LongPulse = 1241;

// #define FUJI_IR_DEBUG

static const uint32_t c_Fuji56TimingsLength =
    (7 /*bytes*/ * 8 /*bits*/ * 2 /*on-off times*/) + 2 /*broadcast start*/ + 1 /*broadcast end*/;

typedef struct {
    uint8_t a0;
    uint8_t a1;
    uint8_t a2;
    uint8_t a3;
    uint8_t a4;
    uint8_t a5_cmd;
    uint8_t a6_cheksum;
} Fuji56Protocol;

static const uint32_t c_Fuji128TimingsLength =
    (16 /*bytes*/ * 8 /*bits*/ * 2 /*on-off times*/) + 2 /*broadcast start*/ + 1 /*broadcast end*/;

typedef struct {
    uint8_t a0;
    uint8_t a1;
    uint8_t a2;
    uint8_t a3;
    uint8_t a4;
    uint8_t a5;
    uint8_t a6;
    uint8_t a7;
    uint8_t a8;
    uint8_t a9;
    uint8_t a10;
    uint8_t a11;
    uint8_t a12;
    uint8_t a13;
    uint8_t a14;
    uint8_t a15;
} Fuji128Protocol;

static void encode_byte(uint32_t timings[], uint32_t* next_i, uint32_t t_size, uint8_t byte) {
    for(uint8_t i = 0; i < 8; i++) {
        furi_assert((*next_i) + 1 < t_size);

        if(byte & 0x1) {
            timings[(*next_i)++] = c_ShortPulse;
            timings[(*next_i)++] = c_LongPulse;
        } else {
            timings[(*next_i)++] = c_ShortPulse;
            timings[(*next_i)++] = c_ShortPulse;
        }
        byte = byte >> 1;
    }
}

#ifdef FUJI_IR_DEBUG
static void byte_to_str(uint8_t byte, char buff[9]) {
    for(uint8_t i = 0; i < 8; i++) {
        buff[7 - i] = '0' + (byte % 2);
        byte >>= 1;
    }
    buff[8] = '\0';
}
#endif /* FUJI_IR_DEBUG */

static void fuji56_encode_command(
    Fuji56Protocol* msg,
    Fuji56ProtocolCommand cmd,
    uint32_t timings[c_Fuji56TimingsLength]) {
    msg->a0 = 0b00010100;
    msg->a1 = 0b01100011;
    msg->a2 = 0b00000000;
    msg->a3 = 0b00010000;
    msg->a4 = 0b00010000;

    switch(cmd) {
    case Fuji56ProtocolCommand_PowerOff:
        msg->a5_cmd = 0b00000010;
        break;
    case Fuji56ProtocolCommand_Set:
        msg->a5_cmd = 0b01101100;
        break;
    case Fuji56ProtocolCommand_Economy:
        msg->a5_cmd = 0b00001001;
        break;
    case Fuji56ProtocolCommand_Powerfull:
        msg->a5_cmd = 0b00111001;
        break;
    default:
        msg->a5_cmd = 0x0; // avoid compilation warning
        furi_assert(false && "Fuji56: Unkown command");
    }
    msg->a6_cheksum = ~(msg->a5_cmd);

    uint32_t t_i = 0;
    timings[t_i++] = c_BroadcastStartOnPulse;
    timings[t_i++] = c_BroadcastStartOffPulse;

    encode_byte(timings, &t_i, c_Fuji56TimingsLength, msg->a0);
    encode_byte(timings, &t_i, c_Fuji56TimingsLength, msg->a1);
    encode_byte(timings, &t_i, c_Fuji56TimingsLength, msg->a2);
    encode_byte(timings, &t_i, c_Fuji56TimingsLength, msg->a3);
    encode_byte(timings, &t_i, c_Fuji56TimingsLength, msg->a4);
    encode_byte(timings, &t_i, c_Fuji56TimingsLength, msg->a5_cmd);
    encode_byte(timings, &t_i, c_Fuji56TimingsLength, msg->a6_cheksum);

    timings[t_i] = c_ShortPulse; // Always 1 ON at the end
}

#ifdef FUJI_IR_DEBUG
static void fuji56_ir_debug(Fuji56Protocol* msg, uint32_t timings[c_Fuji56TimingsLength]) {
    const char* TAG = "FUJI56-DEBUG";
    FURI_LOG_D(TAG, "Message Bits: ");

    char b0[9];
    char b1[9];
    char b2[9];
    char b3[9];
    char b4[9];
    char b5[9];
    char b6[9];
    byte_to_str(msg->a0, b0);
    byte_to_str(msg->a1, b1);
    byte_to_str(msg->a2, b2);
    byte_to_str(msg->a3, b3);
    byte_to_str(msg->a4, b4);
    byte_to_str(msg->a5_cmd, b5);
    byte_to_str(msg->a6_cheksum, b6);

    FURI_LOG_D(TAG, "  %s %s %s %s %s %s %s", b0, b1, b2, b3, b4, b5, b6);

    FURI_LOG_D(TAG, "Timings: ");

    for(size_t i = 0; i < c_Fuji56TimingsLength - 1; i += 2) {
        FURI_LOG_D(TAG, "%lu %lu", timings[i], timings[i + 1]);
    }

    FURI_LOG_D(TAG, "%lu", timings[c_Fuji56TimingsLength - 1]);
    FURI_LOG_D(TAG, "End Timings");
}
#endif /* FUJI_IR_DEBUG */

void send_fuji56_command(Fuji56ProtocolCommand cmd) {
    uint32_t timings[c_Fuji56TimingsLength];
    Fuji56Protocol msg;
    fuji56_encode_command(&msg, cmd, timings);

#ifndef FUJI_IR_DEBUG
    infrared_send_raw(timings, c_Fuji56TimingsLength, true);
#else
    fuji56_ir_debug(&msg, timings);
#endif /* FUJI_IR_DEBUG */
}

static uint8_t fuji128_checksum(Fuji128Protocol* msg) {
    uint8_t sum = msg->a0 + msg->a1 + msg->a2 + msg->a3 + msg->a4 + msg->a5 + msg->a6 + msg->a7 +
                  msg->a8 + msg->a9 + msg->a10 + msg->a11 + msg->a12 + msg->a13 + msg->a14;

    return 158 - sum;
}

static bool fuji128_validate_sleep_timer_value(uint32_t value) {
    switch(value) {
    case 30:
    case 60:
    case 120:
    case 180:
    case 300:
    case 420:
    case 540:
        return true;
    default:
        return false;
    }
}

static bool fuji128_validate_on_off_timer_value(uint32_t value) {
    return (value <= 600 && value % 30 == 0) || (value <= 720 && value % 60 == 0);
}

static void fuji128_validate_signal(Fuji128Signal* sig) {
    furi_assert(sig->temperature >= 20 && sig->temperature <= 44);
    furi_assert(sig->power >= 0 && sig->power <= 1);
    furi_assert(sig->timer_type >= 0 && sig->timer_type <= 4);
    furi_assert(sig->mode >= 0 && sig->mode <= 4);
    furi_assert(sig->swing >= 0 && sig->swing <= 1);
    furi_assert(sig->fan_speed >= 0 && sig->fan_speed <= 4);
    furi_assert(sig->flag_timer_off >= 0 && sig->flag_timer_off <= 1);

    furi_assert(
        (sig->timer_type == 0 && sig->timer_on == 0 && sig->timer_off == 0) ||
        (sig->timer_type == 1 && sig->flag_timer_off == 1 && sig->timer_on == 0 &&
         fuji128_validate_sleep_timer_value(sig->timer_off)) ||
        (sig->timer_type == 2 && sig->flag_timer_off == 0 && sig->timer_on == 0 &&
         fuji128_validate_on_off_timer_value(sig->timer_off)) ||
        (sig->timer_type == 3 && sig->timer_off == 0 && sig->flag_timer_off == 0 &&
         fuji128_validate_on_off_timer_value(sig->timer_on)) ||
        (sig->timer_type == 4 && sig->timer_off > 0 && sig->timer_on > 0 &&
         fuji128_validate_on_off_timer_value(sig->timer_off) &&
         fuji128_validate_on_off_timer_value(sig->timer_on)));
}

#ifdef FUJI_IR_DEBUG
static void fuji128_ir_debug(Fuji128Protocol* msg, uint32_t timings[c_Fuji128TimingsLength]) {
    FURI_LOG_D("DEBUUUUG", "DEBUUUUG 3");
    const char* TAG = "FUJI128-DEBUG";
    FURI_LOG_D(TAG, "Message Bits: ");

    char b0[9];
    char b1[9];
    char b2[9];
    char b3[9];
    char b4[9];
    char b5[9];
    char b6[9];
    char b7[9];
    byte_to_str(msg->a0, b0);
    byte_to_str(msg->a1, b1);
    byte_to_str(msg->a2, b2);
    byte_to_str(msg->a3, b3);
    byte_to_str(msg->a4, b4);
    byte_to_str(msg->a5, b5);
    byte_to_str(msg->a6, b6);
    byte_to_str(msg->a7, b7);

    FURI_LOG_D("DEBUUUUG", "DEBUUUUG 4");
    FURI_LOG_D(TAG, "  %s %s %s %s %s %s %s %s", b0, b1, b2, b3, b4, b5, b6, b7);

    byte_to_str(msg->a8, b0);
    byte_to_str(msg->a9, b1);
    byte_to_str(msg->a10, b2);
    byte_to_str(msg->a11, b3);
    byte_to_str(msg->a12, b4);
    byte_to_str(msg->a13, b5);
    byte_to_str(msg->a14, b6);
    byte_to_str(msg->a15, b7);

    FURI_LOG_D(TAG, "  %s %s %s %s %s %s %s %s", b0, b1, b2, b3, b4, b5, b6, b7);

    FURI_LOG_D(TAG, "Timings: ");

    for(size_t i = 0; i < c_Fuji128TimingsLength - 1; i += 2) {
        FURI_LOG_D(TAG, "%lu %lu", timings[i], timings[i + 1]);
    }

    FURI_LOG_D(TAG, "%lu", timings[c_Fuji128TimingsLength - 1]);
    FURI_LOG_D(TAG, "End Timings");
}
#endif /* FUJI_IR_DEBUG */

void send_fuji128_state(Fuji128Signal* sig) {
    Fuji128Protocol msg = {0};

    fuji128_validate_signal(sig);

    msg.a0 = 0b00010100;
    msg.a1 = 0b01100011;
    msg.a2 = 0b00000000;
    msg.a3 = 0b00010000;
    msg.a4 = 0b00010000;
    msg.a5 = 0b11111110;
    msg.a6 = 0b00001001;
    msg.a7 = 0b00110001;

    msg.a8 = ((sig->temperature << 2) & 0b11111100) | (sig->power & 0b00000001);

    msg.a9 = ((sig->timer_type << 4) & 0b01110000) | (sig->mode & 0b00000111);

    msg.a10 = ((sig->swing << 4) & 0b00010000) | (sig->fan_speed & 0b00000111);

    msg.a11 = sig->timer_off & 0b11111111; // low bits of timer_off

    msg.a12 = ((sig->timer_on << 4) & 0b11110000) // low bits of timer_on
              | ((sig->flag_timer_off << 3) & 0b00001000) // flag timer off
              | ((sig->timer_off >> 8) & 0b00000111); // high bits of timer_off

    msg.a13 = (sig->timer_on >> 4) & 0b11111111; // high bits of timer_on

    msg.a14 = 0b00100000;
    msg.a15 = fuji128_checksum(&msg);

    uint32_t timings[c_Fuji128TimingsLength];

    uint32_t t_i = 0;
    timings[t_i++] = c_BroadcastStartOnPulse;
    timings[t_i++] = c_BroadcastStartOffPulse;

    encode_byte(timings, &t_i, c_Fuji128TimingsLength, msg.a0);
    encode_byte(timings, &t_i, c_Fuji128TimingsLength, msg.a1);
    encode_byte(timings, &t_i, c_Fuji128TimingsLength, msg.a2);
    encode_byte(timings, &t_i, c_Fuji128TimingsLength, msg.a3);
    encode_byte(timings, &t_i, c_Fuji128TimingsLength, msg.a4);
    encode_byte(timings, &t_i, c_Fuji128TimingsLength, msg.a5);
    encode_byte(timings, &t_i, c_Fuji128TimingsLength, msg.a6);
    encode_byte(timings, &t_i, c_Fuji128TimingsLength, msg.a7);
    encode_byte(timings, &t_i, c_Fuji128TimingsLength, msg.a8);
    encode_byte(timings, &t_i, c_Fuji128TimingsLength, msg.a9);
    encode_byte(timings, &t_i, c_Fuji128TimingsLength, msg.a10);
    encode_byte(timings, &t_i, c_Fuji128TimingsLength, msg.a11);
    encode_byte(timings, &t_i, c_Fuji128TimingsLength, msg.a12);
    encode_byte(timings, &t_i, c_Fuji128TimingsLength, msg.a13);
    encode_byte(timings, &t_i, c_Fuji128TimingsLength, msg.a14);
    encode_byte(timings, &t_i, c_Fuji128TimingsLength, msg.a15);
    timings[t_i] = c_ShortPulse; // Always 1 ON at the end

#ifndef FUJI_IR_DEBUG
    infrared_send_raw(timings, c_Fuji128TimingsLength, true);
#else
    fuji128_ir_debug(&msg, timings);
#endif /* FUJI_IR_DEBUG */
}
