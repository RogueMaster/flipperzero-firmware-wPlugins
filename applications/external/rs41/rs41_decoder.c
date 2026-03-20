/*
 * rs41_decoder.c — RS41 radiosonde frame decoder
 *
 * Ported from twatch-rs41 (https://github.com/yeckel/twatch-rs41) by OK1CHP.
 *
 * On-air format:
 *   8-byte sync  +  312-byte payload
 *   Modulation  : 2-FSK 4800 bps, deviation ≈ 2400 Hz (LSB-first on air)
 *   Sync word   : 10 B6 CA 11 22 96 12 F8  (original, LSB-first)
 *                 08 6D 53 88 44 69 48 1F  (bit-reversed, as CC1101 sees it)
 *
 * The CC1101 matches the first 2 bytes (08 6D) as its sync word and places
 * the remaining 6 sync bytes + 312 payload bytes in the RX FIFO.  The caller
 * feeds us the 312 payload bytes (offset +6 in the FIFO buffer).
 *
 * Decoding pipeline:
 *   1. Bit-reverse each byte   (CC1101 RX = MSB-first; RS41 TX = LSB-first)
 *   2. XOR-descramble          (repeating 64-byte LFSR key, offset 8 for sync)
 *   3. Skip Reed-Solomon parity (bytes 0–47) and frame type byte (byte 48)
 *   4. Parse subframe blocks from byte 49 onward
 */

#include "rs41_decoder.h"
#include "rs41_rs.h"
#include <string.h>
#include <math.h>

/* ── Calibration state ───────────────────────────────────────────────────── */
/*
 * The RS41 STATUS block (0x79) embeds rotating calibration data across 51
 * consecutive frames.  Each frame's STATUS block carries 1 byte of calframe
 * index (0x00–0x32) at data[23], followed by 16 bytes of payload data[24..39].
 *
 * Temperature calibration requires calframes 3–6 (four consecutive receptions).
 * Pressure calibration additionally requires calframe 7.
 *
 * Layout (from rs1729/rs41ptu.c, calibytes[] array):
 *   Temperature (calframes 3–6):
 *     Rf1      – calibytes[61..64]  = calframe3[13..15] + calframe4[0]
 *     Rf2      – calibytes[65..68]  = calframe4[1..4]
 *     co1[0]   – calibytes[77..80]  = calframe4[13..15] + calframe5[0]
 *     co1[1]   – calibytes[81..84]  = calframe5[1..4]
 *     co1[2]   – calibytes[85..88]  = calframe5[5..8]
 *     calT1[0] – calibytes[89..92]  = calframe5[9..12]
 *     calT1[1] – calibytes[93..96]  = calframe5[13..15] + calframe6[0]
 *     calT1[2] – calibytes[97..100] = calframe6[1..4]
 *   Pressure (calframes 6–7):
 *     Fp1      – calibytes[101..104] = calframe6[5..8]
 *     Fp2      – calibytes[105..108] = calframe6[9..12]
 *     Cp[0]    – calibytes[109..112] = calframe6[13..15] + calframe7[0]
 *     Cp[1]    – calibytes[113..116] = calframe7[1..4]
 *     Cp[2]    – calibytes[117..120] = calframe7[5..8]
 *
 * Temperature formula:
 *   g  = (f2 - f1) / (Rf2 - Rf1)
 *   Rb = (f1*Rf2 - f2*Rf1) / (f2 - f1)
 *   Rc = meas_T / g - Rb
 *   R  = Rc * calT1[0]
 *   T  = (co1[0] + co1[1]*R + co1[2]*R² + calT1[1]) * (1 + calT1[2])
 *
 * Pressure formula (same resistive bridge structure):
 *   g  = (fp2 - fp1) / (Fp2 - Fp1)
 *   Rb = (fp1*Fp2 - fp2*Fp1) / (fp2 - fp1)
 *   Rc = meas_P / g - Rb
 *   P  = Cp[0] + Cp[1]*Rc + Cp[2]*Rc²           (hPa)
 */
static uint8_t s_cal_raw[5][16]; /* calframes 3–7, 16 bytes each         */
static uint8_t s_cal_ok;         /* bitmask: bit i = calframe i+3 seen   */
static bool    s_cal_valid;      /* true once calframes 3–6 collected    */
static bool    s_pres_cal_valid; /* true once calframes 6–7 collected    */
static float   s_Rf1, s_Rf2;
static float   s_co1[3], s_calT1[3];
static float   s_Fp1, s_Fp2;
static float   s_Cp[3];

static void assemble_cal(void) {
    uint8_t buf[4];
    const uint8_t* cd3 = s_cal_raw[0];
    const uint8_t* cd4 = s_cal_raw[1];
    const uint8_t* cd5 = s_cal_raw[2];
    const uint8_t* cd6 = s_cal_raw[3];

    buf[0]=cd3[13]; buf[1]=cd3[14]; buf[2]=cd3[15]; buf[3]=cd4[0];
    memcpy(&s_Rf1, buf, 4);

    memcpy(&s_Rf2, cd4 + 1, 4);

    buf[0]=cd4[13]; buf[1]=cd4[14]; buf[2]=cd4[15]; buf[3]=cd5[0];
    memcpy(&s_co1[0], buf, 4);

    memcpy(&s_co1[1],   cd5 + 1,  4);
    memcpy(&s_co1[2],   cd5 + 5,  4);
    memcpy(&s_calT1[0], cd5 + 9,  4);

    buf[0]=cd5[13]; buf[1]=cd5[14]; buf[2]=cd5[15]; buf[3]=cd6[0];
    memcpy(&s_calT1[1], buf, 4);

    memcpy(&s_calT1[2], cd6 + 1, 4);

    s_cal_valid = true;
}

static void assemble_pres_cal(void) {
    uint8_t buf[4];
    const uint8_t* cd6 = s_cal_raw[3];
    const uint8_t* cd7 = s_cal_raw[4];

    memcpy(&s_Fp1, cd6 + 5, 4);
    memcpy(&s_Fp2, cd6 + 9, 4);

    buf[0]=cd6[13]; buf[1]=cd6[14]; buf[2]=cd6[15]; buf[3]=cd7[0];
    memcpy(&s_Cp[0], buf, 4);

    memcpy(&s_Cp[1], cd7 + 1, 4);
    memcpy(&s_Cp[2], cd7 + 5, 4);

    s_pres_cal_valid = true;
}

/* ── XOR descramble key ──────────────────────────────────────────────────── */
static const uint8_t RS41_MASK[64] = {
    0x96, 0x83, 0x3E, 0x51, 0xB1, 0x49, 0x08, 0x98,
    0x32, 0x05, 0x59, 0x0E, 0xF9, 0x44, 0xC6, 0x26,
    0x21, 0x60, 0xC2, 0xEA, 0x79, 0x5D, 0x6D, 0xA1,
    0x54, 0x69, 0x47, 0x0C, 0xDC, 0xE8, 0x5C, 0xF1,
    0xF7, 0x76, 0x82, 0x7F, 0x07, 0x99, 0xA2, 0x2C,
    0x93, 0x7C, 0x30, 0x63, 0xF5, 0x10, 0x2E, 0x61,
    0xD0, 0xBC, 0xB4, 0xB6, 0x06, 0xAA, 0xF4, 0x23,
    0x78, 0x6E, 0x3B, 0xAE, 0xBF, 0x7B, 0x4C, 0xC1,
};

/* ── Bit-reversal lookup table ───────────────────────────────────────────── */
static const uint8_t BITREV[256] = {
    0x00,0x80,0x40,0xC0,0x20,0xA0,0x60,0xE0,0x10,0x90,0x50,0xD0,0x30,0xB0,0x70,0xF0,
    0x08,0x88,0x48,0xC8,0x28,0xA8,0x68,0xE8,0x18,0x98,0x58,0xD8,0x38,0xB8,0x78,0xF8,
    0x04,0x84,0x44,0xC4,0x24,0xA4,0x64,0xE4,0x14,0x94,0x54,0xD4,0x34,0xB4,0x74,0xF4,
    0x0C,0x8C,0x4C,0xCC,0x2C,0xAC,0x6C,0xEC,0x1C,0x9C,0x5C,0xDC,0x3C,0xBC,0x7C,0xFC,
    0x02,0x82,0x42,0xC2,0x22,0xA2,0x62,0xE2,0x12,0x92,0x52,0xD2,0x32,0xB2,0x72,0xF2,
    0x0A,0x8A,0x4A,0xCA,0x2A,0xAA,0x6A,0xEA,0x1A,0x9A,0x5A,0xDA,0x3A,0xBA,0x7A,0xFA,
    0x06,0x86,0x46,0xC6,0x26,0xA6,0x66,0xE6,0x16,0x96,0x56,0xD6,0x36,0xB6,0x76,0xF6,
    0x0E,0x8E,0x4E,0xCE,0x2E,0xAE,0x6E,0xEE,0x1E,0x9E,0x5E,0xDE,0x3E,0xBE,0x7E,0xFE,
    0x01,0x81,0x41,0xC1,0x21,0xA1,0x61,0xE1,0x11,0x91,0x51,0xD1,0x31,0xB1,0x71,0xF1,
    0x09,0x89,0x49,0xC9,0x29,0xA9,0x69,0xE9,0x19,0x99,0x59,0xD9,0x39,0xB9,0x79,0xF9,
    0x05,0x85,0x45,0xC5,0x25,0xA5,0x65,0xE5,0x15,0x95,0x55,0xD5,0x35,0xB5,0x75,0xF5,
    0x0D,0x8D,0x4D,0xCD,0x2D,0xAD,0x6D,0xED,0x1D,0x9D,0x5D,0xDD,0x3D,0xBD,0x7D,0xFD,
    0x03,0x83,0x43,0xC3,0x23,0xA3,0x63,0xE3,0x13,0x93,0x53,0xD3,0x33,0xB3,0x73,0xF3,
    0x0B,0x8B,0x4B,0xCB,0x2B,0xAB,0x6B,0xEB,0x1B,0x9B,0x5B,0xDB,0x3B,0xBB,0x7B,0xFB,
    0x07,0x87,0x47,0xC7,0x27,0xA7,0x67,0xE7,0x17,0x97,0x57,0xD7,0x37,0xB7,0x77,0xF7,
    0x0F,0x8F,0x4F,0xCF,0x2F,0xAF,0x6F,0xEF,0x1F,0x9F,0x5F,0xDF,0x3F,0xBF,0x7F,0xFF,
};

/* ── RS41 subframe block IDs ─────────────────────────────────────────────── */
#define RS41_BLOCK_STATUS 0x79  /* sonde serial, frame counter           */
#define RS41_BLOCK_GPS    0x7B  /* ECEF position (int32 × 0.01 m), sats */
#define RS41_BLOCK_PTU    0x7A  /* MEAS: pressure / temp / humidity ADC  */

/* ── CRC-16/CCITT-FALSE (poly 0x1021, init 0xFFFF, no reflection) ────────── */
static uint16_t crc16(const uint8_t* data, uint16_t len) {
    uint16_t crc = 0xFFFF;
    for(uint16_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for(int b = 0; b < 8; b++)
            crc = (crc & 0x8000) ? (crc << 1) ^ 0x1021 : crc << 1;
    }
    return crc;
}

/* ── ECEF (metres) → WGS-84 geodetic  (Bowring iterative, float) ────────── */
/* Float (~7 sig. digits) gives ~1 m positional accuracy — fine for sondes. */
static void ecef_to_lla(
    float x, float y, float z,
    float* lat_out, float* lon_out, float* alt_out)
{
    const float a  = 6378137.0f;
    const float e2 = 6.6943799901413e-3f;
    float p   = sqrtf(x * x + y * y);
    float lon = atan2f(y, x);
    float lat = atan2f(z, p * (1.0f - e2));
    for(int i = 0; i < 10; i++) {
        float N  = a / sqrtf(1.0f - e2 * sinf(lat) * sinf(lat));
        float lN = atan2f(z + e2 * N * sinf(lat), p);
        if(fabsf(lN - lat) < 1e-7f) { lat = lN; break; }
        lat = lN;
    }
    float N = a / sqrtf(1.0f - e2 * sinf(lat) * sinf(lat));
    *lat_out = lat * (180.0f / (float)M_PI);
    *lon_out = lon * (180.0f / (float)M_PI);
    *alt_out = p / cosf(lat) - N;
}

static int32_t read_i32_le(const uint8_t* p) {
    return (int32_t)p[0]
         | ((int32_t)p[1] << 8)
         | ((int32_t)p[2] << 16)
         | ((int32_t)p[3] << 24);
}

/*
 * Parse one subframe block at buf[offset].
 * Returns bytes consumed (≥ 1), or 0 to stop parsing.
 */
static int parse_block(const uint8_t* buf, int offset, int total, Rs41Frame* out) {
    if(offset + 4 > total) return 0;

    uint8_t block_id  = buf[offset];
    uint8_t block_len = buf[offset + 1];

    if(block_len == 0 || offset + 2 + (int)block_len + 2 > total) return 1;

    const uint8_t* data = buf + offset + 2;
    uint16_t expected = (uint16_t)buf[offset + 2 + block_len]
                      | ((uint16_t)buf[offset + 2 + block_len + 1] << 8);

    if(crc16(data, block_len) != expected)
        return 2 + (int)block_len + 2;  /* bad CRC — skip */

    switch(block_id) {
    case RS41_BLOCK_STATUS:
        /* data[0..1]: frame counter; data[2..9]: sonde ID (8 ASCII bytes)
         * data[23]: calframe index (0x00–0x32); data[24..39]: 16 cal bytes */
        if(block_len >= 10) {
            memcpy(out->id, data + 2, 8);
            out->id[8] = '\0';
        }
        if(block_len >= 40) {
            uint8_t calfr = data[23];
            if(calfr >= 3 && calfr <= 7) {
                memcpy(s_cal_raw[calfr - 3], data + 24, 16);
                s_cal_ok |= (uint8_t)(1u << (calfr - 3));
                if(!s_cal_valid && (s_cal_ok & 0x0Fu) == 0x0Fu)
                    assemble_cal();
                if(!s_pres_cal_valid && (s_cal_ok & 0x18u) == 0x18u)
                    assemble_pres_cal();
            }
        }
        break;

    case RS41_BLOCK_GPS:
        /* data[0..11]: ECEF x,y,z (int32 × 0.01 m); data[18]: numSatsFix */
        if(block_len >= 21) {
            float lat, lon, alt;
            ecef_to_lla(
                read_i32_le(data + 0) * 0.01f,
                read_i32_le(data + 4) * 0.01f,
                read_i32_le(data + 8) * 0.01f,
                &lat, &lon, &alt);
            out->sats = data[18];
            if(alt > -1000.0f && alt < 50000.0f &&
               (fabsf(lat) > 0.001f || fabsf(lon) > 0.001f)) {
                out->lat     = lat;
                out->lon     = lon;
                out->alt     = alt;
                out->has_gps = true;
            }
        }
        break;

    case RS41_BLOCK_PTU:
        /* 12 × uint24 LE ADC measurements.
         * meas[0,1,2] = T_main, T_ref1, T_ref2 — temperature.
         * meas[3,4,5] = P_main, P_ref1, P_ref2 — pressure. */
        if(block_len >= 9 && s_cal_valid) {
            uint32_t m0 = (uint32_t)data[0] | ((uint32_t)data[1]<<8) | ((uint32_t)data[2]<<16);
            uint32_t m1 = (uint32_t)data[3] | ((uint32_t)data[4]<<8) | ((uint32_t)data[5]<<16);
            uint32_t m2 = (uint32_t)data[6] | ((uint32_t)data[7]<<8) | ((uint32_t)data[8]<<16);
            float f  = (float)m0;
            float f1 = (float)m1;
            float f2 = (float)m2;
            float dRf = s_Rf2 - s_Rf1;
            float df  = f2 - f1;
            if(fabsf(dRf) > 1.0f && fabsf(df) > 1.0f) {
                float g  = df / dRf;
                float Rb = (f1 * s_Rf2 - f2 * s_Rf1) / df;
                float Rc = f / g - Rb;
                float R  = Rc * s_calT1[0];
                out->temp = (s_co1[0] + s_co1[1]*R + s_co1[2]*R*R
                             + s_calT1[1]) * (1.0f + s_calT1[2]);
            }
        }
        if(block_len >= 18 && s_pres_cal_valid) {
            uint32_t m3 = (uint32_t)data[ 9] | ((uint32_t)data[10]<<8) | ((uint32_t)data[11]<<16);
            uint32_t m4 = (uint32_t)data[12] | ((uint32_t)data[13]<<8) | ((uint32_t)data[14]<<16);
            uint32_t m5 = (uint32_t)data[15] | ((uint32_t)data[16]<<8) | ((uint32_t)data[17]<<16);
            float fp  = (float)m3;
            float fp1 = (float)m4;
            float fp2 = (float)m5;
            float dFp = s_Fp2 - s_Fp1;
            float dfp = fp2 - fp1;
            if(fabsf(dFp) > 1.0f && fabsf(dfp) > 1.0f) {
                float gp  = dfp / dFp;
                float Rbp = (fp1 * s_Fp2 - fp2 * s_Fp1) / dfp;
                float Rc  = fp / gp - Rbp;
                float P   = s_Cp[0] + s_Cp[1]*Rc + s_Cp[2]*Rc*Rc;
                if(P > 10.0f && P < 1200.0f)
                    out->pres = P;
            }
        }
        break;

    default:
        break;
    }
    return 2 + (int)block_len + 2;
}

/* ── Public API ──────────────────────────────────────────────────────────── */
bool rs41_decode(const uint8_t* fifo_bytes, int16_t rssi, Rs41Frame* out) {
    /* Step 1: bit-reverse each byte (CC1101 MSB-first → RS41 LSB-first) */
    uint8_t raw[RS41_PAYLOAD_LEN];
    for(int i = 0; i < RS41_PAYLOAD_LEN; i++)
        raw[i] = BITREV[fifo_bytes[i]];

    /* Step 2: XOR-descramble.  Mask offset 8 = sync word length. */
    uint8_t frame[RS41_PAYLOAD_LEN];
    for(int i = 0; i < RS41_PAYLOAD_LEN; i++)
        frame[i] = raw[i] ^ RS41_MASK[(i + 8) & 63];

    /* Step 3: Reed-Solomon error correction (in-place on frame[]) */
    int rs_errs = rs41_rs_correct(frame, RS41_PAYLOAD_LEN);
    (void)rs_errs; /* logged by caller if desired; negative = uncorrectable */

    /* Initialise output */
    memset(out, 0, sizeof(*out));
    out->temp    = -999.0f;
    out->pres    = -1.0f;
    out->rssi    = rssi;
    out->has_gps = false;

    /* Step 4: parse blocks from byte 49 (0-47 = RS parity, 48 = frame type) */
    bool status_found = false;
    int offset = 49, iter = 0;
    while(offset < RS41_PAYLOAD_LEN && iter < 40) {
        iter++;
        if(frame[offset] == 0x00) { offset++; continue; }
        if(frame[offset] == RS41_BLOCK_STATUS) status_found = true;
        int consumed = parse_block(frame, offset, RS41_PAYLOAD_LEN, out);
        if(consumed <= 0) break;
        offset += consumed;
    }
    return status_found && out->id[0] != '\0';
}
