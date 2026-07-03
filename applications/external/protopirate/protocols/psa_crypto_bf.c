#include "psa_crypto_bf.h"
#include "psa_crypto.h"

#define TEA_DELTA  0x9E3779B9U
#define TEA_ROUNDS 32

#define PSA_BF_PROGRESS_INTERVAL 4096U

#pragma GCC optimize("O3", "unroll-loops")

typedef struct {
    uint32_t s0[TEA_ROUNDS];
    uint32_t s1[TEA_ROUNDS];
} PsaTeaSchedule;

static void psa_bf_tea_build_schedule(const uint32_t* key, PsaTeaSchedule* out) {
    for(int i = 0; i < TEA_ROUNDS; i++) {
        uint32_t sum0 = (uint32_t)((uint64_t)i * TEA_DELTA);
        uint32_t sum1 = (uint32_t)((uint64_t)(i + 1) * TEA_DELTA);
        out->s0[i] = key[sum0 & 3] + sum0;
        out->s1[i] = key[(sum1 >> 11) & 3] + sum1;
    }
}

static inline void
    psa_bf_tea_encrypt_with_schedule(uint32_t* v0, uint32_t* v1, const PsaTeaSchedule* sched) {
    uint32_t a = *v0, b = *v1;
#pragma GCC unroll 32
    for(int i = 0; i < TEA_ROUNDS; i++) {
        a += (sched->s0[i] ^ (((b >> 5) ^ (b << 4)) + b));
        b += (sched->s1[i] ^ (((a >> 5) ^ (a << 4)) + a));
    }
    *v0 = a;
    *v1 = b;
}

static inline void psa_bf_tea_decrypt(uint32_t* v0, uint32_t* v1, const uint32_t* key) {
    uint32_t a = *v0, b = *v1;
    const uint32_t k[4] = {key[0], key[1], key[2], key[3]};
    uint32_t sum = TEA_DELTA * TEA_ROUNDS;
#pragma GCC unroll 32
    for(int i = 0; i < TEA_ROUNDS; i++) {
        uint32_t temp = k[(sum >> 11) & 3] + sum;
        sum = sum - TEA_DELTA;
        b = b - (temp ^ (((a >> 5) ^ (a << 4)) + a));
        temp = k[sum & 3] + sum;
        a = a - (temp ^ (((b >> 5) ^ (b << 4)) + b));
    }
    *v0 = a;
    *v1 = b;
}

static void psa_bf_fill_state_from_buffer(PsaBfState* state, uint8_t* buffer) {
    state->decrypted_button = (buffer[5] >> 4) & 0xF;
    state->decrypted_serial = ((uint32_t)buffer[3] << 8) | ((uint32_t)buffer[2] << 16) |
                              (uint32_t)buffer[4];
    state->decrypted_counter = ((uint32_t)buffer[7] << 8) | ((uint32_t)buffer[6] << 16) |
                               (uint32_t)buffer[8] | (((uint32_t)buffer[5] & 0xF) << 24);
    state->decrypted_crc = (uint16_t)buffer[9];
    state->decrypted_seed = state->decrypted_serial;
    state->decrypted_type = 0x36;
}

void psa_brute_force_run(PsaBfState* state) {
    uint8_t buffer[48] = {0};
    psa_crypto_setup_byte_buffer(buffer, state->key1_low, state->key1_high, state->key2_low);
    uint32_t w0, w1;
    psa_crypto_prepare_tea_data(buffer, &w0, &w1);

    state->progress_current = 0;
    state->progress_total =
        (PSA_CRYPTO_BF1_END - PSA_CRYPTO_BF1_START) + (PSA_CRYPTO_BF2_END - PSA_CRYPTO_BF2_START);
    state->status = PSA_BF_STATUS_RUNNING;

    PsaTeaSchedule bf1_sched;
    psa_bf_tea_build_schedule(psa_crypto_bf1_key_schedule, &bf1_sched);

    for(uint32_t counter = PSA_CRYPTO_BF1_START; counter < PSA_CRYPTO_BF1_END; counter++) {
        if(state->cancel) {
            state->status = PSA_BF_STATUS_CANCELLED;
            return;
        }
        if((counter & (PSA_BF_PROGRESS_INTERVAL - 1)) == 0) {
            state->progress_current = counter - PSA_CRYPTO_BF1_START;
        }

        uint32_t wk2 = PSA_CRYPTO_BF1_CONST_U4;
        uint32_t wk3 = counter;
        psa_bf_tea_encrypt_with_schedule(&wk2, &wk3, &bf1_sched);
        uint32_t wk0 = (counter << 8) | 0x0E;
        uint32_t wk1 = PSA_CRYPTO_BF1_CONST_U5;
        psa_bf_tea_encrypt_with_schedule(&wk0, &wk1, &bf1_sched);
        uint32_t working_key[4] = {wk0, wk1, wk2, wk3};

        uint32_t dec_v0 = w0;
        uint32_t dec_v1 = w1;
        psa_bf_tea_decrypt(&dec_v0, &dec_v1, working_key);

        if((counter & 0xFFFFFF) == (dec_v0 >> 8)) {
            uint8_t crc = psa_crypto_tea_crc(dec_v0, dec_v1);
            if(crc == (dec_v1 & 0xFF)) {
                psa_crypto_unpack_tea_result_to_buffer(buffer, dec_v0, dec_v1);
                psa_bf_fill_state_from_buffer(state, buffer);
                state->progress_current = counter - PSA_CRYPTO_BF1_START;
                state->status = PSA_BF_STATUS_FOUND;
                return;
            }
        }
    }

    state->progress_current = PSA_CRYPTO_BF1_END - PSA_CRYPTO_BF1_START;

    for(uint32_t counter = PSA_CRYPTO_BF2_START; counter < PSA_CRYPTO_BF2_END; counter++) {
        if(state->cancel) {
            state->status = PSA_BF_STATUS_CANCELLED;
            return;
        }
        if((counter & (PSA_BF_PROGRESS_INTERVAL - 1)) == 0) {
            state->progress_current =
                (PSA_CRYPTO_BF1_END - PSA_CRYPTO_BF1_START) + (counter - PSA_CRYPTO_BF2_START);
        }

        uint32_t working_key[4] = {
            psa_crypto_bf2_key_schedule[0] ^ counter,
            psa_crypto_bf2_key_schedule[1] ^ counter,
            psa_crypto_bf2_key_schedule[2] ^ counter,
            psa_crypto_bf2_key_schedule[3] ^ counter,
        };
        uint32_t dec_v0 = w0;
        uint32_t dec_v1 = w1;
        psa_bf_tea_decrypt(&dec_v0, &dec_v1, working_key);

        if((counter & 0xFFFFFF) == (dec_v0 >> 8)) {
            psa_crypto_unpack_tea_result_to_buffer(buffer, dec_v0, dec_v1);
            uint8_t crc_buffer[6] = {
                (uint8_t)((dec_v0 >> 24) & 0xFF),
                (uint8_t)((dec_v0 >> 8) & 0xFF),
                (uint8_t)((dec_v0 >> 16) & 0xFF),
                (uint8_t)(dec_v0 & 0xFF),
                (uint8_t)((dec_v1 >> 24) & 0xFF),
                (uint8_t)((dec_v1 >> 16) & 0xFF),
            };
            uint16_t crc16 = psa_crypto_crc16_bf2(crc_buffer, 6);
            uint16_t expected_crc = (uint16_t)(dec_v1 & 0xFFFF);
            if(crc16 == expected_crc) {
                psa_bf_fill_state_from_buffer(state, buffer);
                state->progress_current =
                    (PSA_CRYPTO_BF1_END - PSA_CRYPTO_BF1_START) + (counter - PSA_CRYPTO_BF2_START);
                state->status = PSA_BF_STATUS_FOUND;
                return;
            }
        }
    }

    state->status = PSA_BF_STATUS_NOT_FOUND;
}

int32_t psa_brute_force_thread_entry(void* arg) {
    PsaBfState* state = arg;
    psa_brute_force_run(state);

    if(state->on_done) {
        state->on_done(state->on_done_ctx);
    }
    return 0;
}
