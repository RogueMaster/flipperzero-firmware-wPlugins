#include "rfid_modem.h"
#include <string.h>

// ===== Shared constants ======================================================

#define HB_CYCLES RFID_MODEM_HALFBIT_CYCLES // 16 carrier cycles = one half-bit
#define SIG_LEN   ((int)(2u * RFID_MODEM_PREAMBLE_MATCH_BITS + 16u)) // signature half-bits

// Half-bit level windows: [level of half 0][level of half 1] for a bit value b are
// {b, !b} (Manchester). The decoder reads the value from the first half of each
// bit only, so the exact convention just needs to match the encoder.

// ===== Encoder ===============================================================

// Append one half-bit at `level`, merging into the current run (runs alternate
// level by construction, so a merge only happens across a bit boundary).
static void enc_push_half(RfidModemEnc* enc, int* cur_level, uint32_t* cur_len, int level) {
    if(*cur_level == level) {
        *cur_len += HB_CYCLES;
    } else {
        if(*cur_level >= 0 && enc->nruns < RFID_MODEM_MAX_RUNS) {
            enc->run_cyc[enc->nruns++] = (uint8_t)*cur_len;
        }
        *cur_level = level;
        *cur_len = HB_CYCLES;
    }
}

static void enc_emit_bit(RfidModemEnc* enc, int* cur_level, uint32_t* cur_len, int b) {
    enc_push_half(enc, cur_level, cur_len, b ? 1 : 0);
    enc_push_half(enc, cur_level, cur_len, b ? 0 : 1);
}

static void enc_emit_byte(RfidModemEnc* enc, int* cur_level, uint32_t* cur_len, uint8_t v) {
    for(int i = 0; i < 8; i++)
        enc_emit_bit(enc, cur_level, cur_len, (v >> i) & 1); // LSB-first
}

void rfid_modem_enc_set_frame(RfidModemEnc* enc, const uint8_t* packet, size_t len) {
    enc->nruns = 0;
    enc->idx = 0;
    if(len < 1 || len > RFID_MODEM_MAX_PACKET) return; // empty -> idle encoder

    int cur_level = -1;
    uint32_t cur_len = 0;

    for(uint32_t i = 0; i < RFID_MODEM_PREAMBLE_BITS; i++)
        enc_emit_bit(enc, &cur_level, &cur_len, 1); // preamble: all ones -> starts High
    enc_emit_byte(enc, &cur_level, &cur_len, RFID_MODEM_SYNC_BYTE);
    enc_emit_byte(enc, &cur_level, &cur_len, (uint8_t)len);
    for(size_t i = 0; i < len; i++)
        enc_emit_byte(enc, &cur_level, &cur_len, packet[i]);

    // Flush the final run.
    if(cur_level >= 0 && enc->nruns < RFID_MODEM_MAX_RUNS) {
        enc->run_cyc[enc->nruns++] = (uint8_t)cur_len;
    }

    // Runs alternate starting High (preamble bit0 = 1). Make the sequence end on a
    // Low run with an even count so it pairs cleanly into (High, Low) periods, and
    // fold in the inter-frame gap (load off).
    uint32_t gap = RFID_MODEM_IFG_BITS * RFID_MODEM_RF_K; // carrier cycles of silence
    if(enc->nruns & 1u) {
        // Last run is High -> append a Low gap run.
        if(enc->nruns < RFID_MODEM_MAX_RUNS) enc->run_cyc[enc->nruns++] = (uint8_t)gap;
    } else if(enc->nruns > 0) {
        // Last run is Low -> extend it by the gap (cap at a byte).
        uint32_t v = enc->run_cyc[enc->nruns - 1] + gap;
        enc->run_cyc[enc->nruns - 1] = (uint8_t)(v > 255u ? 255u : v);
    }
}

bool rfid_modem_enc_next(RfidModemEnc* enc, uint32_t* duration, uint32_t* pulse) {
    if(enc->idx >= enc->nruns) return false; // frame + gap exhausted -> idle
    uint32_t high = enc->run_cyc[enc->idx]; // even index -> High run
    uint32_t low = (enc->idx + 1 < enc->nruns) ? enc->run_cyc[enc->idx + 1] : 0;
    *pulse = high;
    *duration = high + low;
    enc->idx += 2;
    return true;
}

// ===== Decoder ===============================================================

void rfid_modem_dec_reset(RfidModemDec* dec) {
    dec->state = RfidModemHunt;
    dec->out_level = -1;
    dec->out_dur = 0;
    dec->cand_level = -1;
    dec->cand_dur = 0;
    dec->win_len = 0;
    dec->pol = 0;
    dec->hb_count = 0;
    dec->cur_byte = 0;
    dec->bit_in_byte = 0;
    dec->byte_idx = 0;
    dec->need_bytes = 0;
}

// Build the 32-entry signature (last PREAMBLE_MATCH_BITS preamble ones + sync) as
// half-bit levels, direct phase. Deterministic, so compute once into a static.
static const uint8_t* signature(void) {
    static uint8_t sig[SIG_LEN];
    static int built = 0;
    if(!built) {
        int k = 0;
        for(uint32_t i = 0; i < RFID_MODEM_PREAMBLE_MATCH_BITS; i++) {
            sig[k++] = 1; // preamble bit '1' -> half-bits {1,0}
            sig[k++] = 0;
        }
        for(int i = 0; i < 8; i++) {
            int b = (RFID_MODEM_SYNC_BYTE >> i) & 1; // LSB-first
            sig[k++] = (uint8_t)(b ? 1 : 0);
            sig[k++] = (uint8_t)(b ? 0 : 1);
        }
        built = 1;
    }
    return sig;
}

// Classify a level run duration into half-bit (1), full-bit (2), or invalid (0).
static int classify_run(uint32_t dur_us) {
    // Threshold classification with NO dead zone between the half-bit and
    // full-bit windows: a run in a valid Manchester stream is only ever ~128 us
    // or ~256 us, so split at the midpoint (192 us) and assign the nearer class.
    // A dead-zone (windowed) classifier rejected borderline runs as invalid and
    // reset the decoder — fatal here because reconstructing the low run as
    // (period - high) doubles per-edge jitter, so real captures routinely land
    // between the two windows. Runs longer than the inter-frame gap are already
    // filtered by the caller before this point.
    if(dur_us < RFID_MODEM_HALFBIT_US / 2u) return 0; // < 64 us: glitch -> desync
    if(dur_us < RFID_MODEM_CLASS_SPLIT_US) return 1; // half-bit
    return 2; // full-bit
}

// Process one reconstructed half-bit at raw `level`. Returns the packet length
// (>=1) when a frame closes, else 0.
static size_t dec_push_half(RfidModemDec* dec, int level, uint8_t* out, size_t out_cap) {
    if(dec->state == RfidModemHunt) {
        // Slide the window and append this half-bit.
        if(dec->win_len < SIG_LEN) {
            dec->win[dec->win_len++] = (uint8_t)level;
        } else {
            memmove(dec->win, dec->win + 1, SIG_LEN - 1);
            dec->win[SIG_LEN - 1] = (uint8_t)level;
        }
        if(dec->win_len < SIG_LEN) return 0;

        // Compare the window against the signature in both polarities.
        const uint8_t* sig = signature();
        int match_direct = 1, match_inv = 1;
        for(int i = 0; i < SIG_LEN; i++) {
            if(dec->win[i] != sig[i]) match_direct = 0;
            if(dec->win[i] == sig[i]) match_inv = 0;
        }
        if(match_direct || match_inv) {
            dec->state = RfidModemRead;
            dec->pol = match_direct ? 0 : 1;
            dec->hb_count = 0;
            dec->cur_byte = 0;
            dec->bit_in_byte = 0;
            dec->byte_idx = 0;
            dec->need_bytes = 0;
        }
        return 0;
    }

    // RfidModemRead: even half-bit index carries the bit value; odd is skipped.
    if((dec->hb_count & 1u) == 0) {
        int bit = level ^ dec->pol; // undo polarity
        dec->cur_byte |= (uint8_t)((bit & 1) << dec->bit_in_byte); // LSB-first
        dec->bit_in_byte++;
        if(dec->bit_in_byte == 8) {
            uint8_t done = dec->cur_byte;
            dec->cur_byte = 0;
            dec->bit_in_byte = 0;

            if(dec->byte_idx == 0) {
                // len byte. `done` is a uint8_t so it can never exceed 255; only
                // compare against the cap when the cap is actually below 255
                // (avoids an always-false comparison warning under -Werror).
#if RFID_MODEM_MAX_PACKET < 255
                if(done < 1 || done > RFID_MODEM_MAX_PACKET) {
#else
                if(done < 1) {
#endif
                    rfid_modem_dec_reset(dec);
                    return 0;
                }
                dec->frame[0] = done;
                dec->need_bytes = 1u + done;
                dec->byte_idx = 1;
            } else {
                dec->frame[dec->byte_idx] = done;
                dec->byte_idx++;
                if(dec->byte_idx >= dec->need_bytes) {
                    // Frame complete: emit the packet (bytes after the len prefix).
                    size_t plen = dec->need_bytes - 1u;
                    if(plen > out_cap) plen = out_cap;
                    memcpy(out, dec->frame + 1, plen);
                    rfid_modem_dec_reset(dec);
                    return plen;
                }
            }
        }
    }
    dec->hb_count++;
    return 0;
}

// Classify and push one clean (glitch-filtered) constant-level run into the
// half-bit decoder. Returns a packet length when a frame closes, else 0.
static size_t
    dec_emit_run(RfidModemDec* dec, int level, uint32_t dur_us, uint8_t* out, size_t cap) {
    if(dur_us > RFID_MODEM_GAP_US) {
        // Inter-frame gap / dropout: back to sync-hunt.
        rfid_modem_dec_reset(dec);
        return 0;
    }
    int count = classify_run(dur_us);
    if(count == 0) {
        // Neither half-bit nor full-bit: desync.
        rfid_modem_dec_reset(dec);
        return 0;
    }
    size_t result = 0;
    for(int i = 0; i < count; i++) {
        size_t r = dec_push_half(dec, level, out, cap);
        if(r) result = r;
    }
    return result;
}

size_t rfid_modem_dec_feed(
    RfidModemDec* dec,
    bool level,
    uint32_t duration_us,
    uint8_t* out,
    size_t out_cap) {
    int lvl = level ? 1 : 0;

    // Glitch-filter / debounce. The shortest real run is a half-bit (128 us). A
    // comparator-noise edge splits a real run into short pieces; treating any short
    // run as a glitch would also swallow the two halves of a mid-run split. Instead
    // a level change is only committed once the new level has persisted for
    // RFID_MODEM_GLITCH_US, and brief returns to the confirmed level are absorbed.
    if(dec->out_level < 0) {
        dec->out_level = lvl; // first run: open the confirmed run
        dec->out_dur = duration_us;
        return 0;
    }

    if(dec->cand_level < 0) {
        if(lvl == dec->out_level) {
            dec->out_dur += duration_us; // continuation of the confirmed run
        } else {
            dec->cand_level = lvl; // a possible level change begins
            dec->cand_dur = duration_us;
        }
    } else {
        if(lvl == dec->cand_level) {
            dec->cand_dur += duration_us; // candidate persists
        } else if(duration_us < RFID_MODEM_GLITCH_US) {
            dec->cand_dur += duration_us; // brief blip back to out level: absorb it
        } else {
            // A real run back at the confirmed level: the candidate was just noise.
            dec->out_dur += dec->cand_dur + duration_us;
            dec->cand_level = -1;
            dec->cand_dur = 0;
            return 0;
        }
    }

    // Candidate has lasted long enough to be a real level change: emit the now-
    // finished confirmed run and promote the candidate to be the confirmed run.
    if(dec->cand_level >= 0 && dec->cand_dur >= RFID_MODEM_GLITCH_US) {
        int new_level = dec->cand_level;
        uint32_t new_dur = dec->cand_dur;
        size_t r = dec_emit_run(dec, dec->out_level, dec->out_dur, out, out_cap);
        // dec_emit_run may have reset filter+decoder state (gap / frame / desync);
        // re-open the confirmed run from the saved candidate regardless.
        dec->out_level = new_level;
        dec->out_dur = new_dur;
        dec->cand_level = -1;
        dec->cand_dur = 0;
        return r;
    }
    return 0;
}
