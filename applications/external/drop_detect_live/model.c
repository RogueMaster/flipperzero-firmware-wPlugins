/*
 * model.c — forward pass + model.json loader for the FliPort drop detector.
 *
 * The math mirrors the BeanTensor forward passes in model/src/main.cpp
 * (Conv1D::forward, GlobalMaxPool1D::forward, Dense::forward, Softmax::forward)
 * and the feature/normalization step in make_window_tensor() — kept in lockstep
 * so on-device predictions match the PC model.
 */
#include "model.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h> /* strtof, malloc, free */
#include <string.h>

/*
 * Per-feature normalization (a_mag_sq, g_mag_sq, a_mag_sq_delta, g_mag_sq_delta).
 * Baked from model/data/TRAIN.bin's header (printed by data.py as FEATURE_MEAN /
 * FEATURE_INV_STD). Inference applies: x_norm = (x - mean) * inv_std.
 */
static const float FEATURE_MEAN[DROP_N_FEAT] = {4.1226315f, 81886.84f, -0.000504415f, 326.7362f};
static const float FEATURE_INV_STD[DROP_N_FEAT] =
    {0.032043703f, 1.824883e-06f, 0.04735394f, 3.3177873e-06f};

/* ---------------- JSON weight parsing ---------------- */

/*
 * Reads the float array at `obj_key.arr_key` from a model.json string written by
 * export_model_json. The format is well-known, so a tiny scanner suffices (no
 * full JSON parser). Fills up to `max` floats into `out`; returns the count read,
 * or -1 on a structural error. Mirrors json_floats() in main.cpp.
 */
static int
    json_floats(const char* json, const char* obj_key, const char* arr_key, float* out, int max) {
    char obj_marker[40];
    char arr_marker[40];
    snprintf(obj_marker, sizeof(obj_marker), "\"%s\"", obj_key);
    snprintf(arr_marker, sizeof(arr_marker), "\"%s\"", arr_key);

    const char* o = strstr(json, obj_marker);
    if(!o) return -1;
    /* Search for the array key *after* the object key. Because the marker
     * includes the closing quote, "weight" will not match inside "weight_shape". */
    const char* k = strstr(o, arr_marker);
    if(!k) return -1;
    const char* lb = strchr(k, '[');
    if(!lb) return -1;
    const char* rb = strchr(lb, ']');
    if(!rb) return -1;

    int n = 0;
    const char* p = lb + 1;
    while(p < rb && n < max) {
        while(p < rb && (*p == ' ' || *p == ',' || *p == '\n' || *p == '\t' || *p == '\r'))
            p++;
        if(p >= rb) break;
        char* end = NULL;
        float v = strtof(p, &end);
        if(end == p) return -1; /* not a number where one was expected */
        out[n++] = v;
        p = end;
    }
    return n;
}

bool model_parse(DropModel* model, const char* json) {
    if(!model || !json) return false;
    if(json_floats(
           json, "conv1d", "weight", model->conv_w, DROP_CONV_OUT * DROP_N_FEAT * DROP_CONV_K) !=
       DROP_CONV_OUT * DROP_N_FEAT * DROP_CONV_K)
        return false;
    if(json_floats(json, "conv1d", "bias", model->conv_b, DROP_CONV_OUT) != DROP_CONV_OUT)
        return false;
    if(json_floats(json, "dense_0", "weight", model->dense_w, DROP_NUM_CLASSES * DROP_CONV_OUT) !=
       DROP_NUM_CLASSES * DROP_CONV_OUT)
        return false;
    if(json_floats(json, "dense_0", "bias", model->dense_b, DROP_NUM_CLASSES) != DROP_NUM_CLASSES)
        return false;
    return true;
}

/* ---------------- Forward pass ---------------- */

int model_infer(const DropModel* model, const float ring[][6], int head, float out_probs[4]) {
    /* Feature matrix: feat[f][j], f in 0..3, j in 0..248. Static to keep it off
     * the (small) monitor thread stack; only one monitor thread ever runs. */
    static float feat[DROP_N_FEAT * DROP_N_FEAT_VEC];

    for(int t = 1; t < DROP_WINDOW; t++) {
        const float* cur = ring[(head + t) % DROP_WINDOW];
        const float* prev = ring[(head + t - 1) % DROP_WINDOW];
        float a = cur[0] * cur[0] + cur[1] * cur[1] + cur[2] * cur[2];
        float g = cur[3] * cur[3] + cur[4] * cur[4] + cur[5] * cur[5];
        float a_prev = prev[0] * prev[0] + prev[1] * prev[1] + prev[2] * prev[2];
        float g_prev = prev[3] * prev[3] + prev[4] * prev[4] + prev[5] * prev[5];
        float raw[DROP_N_FEAT] = {a, g, a - a_prev, g - g_prev};
        int j = t - 1;
        for(int f = 0; f < DROP_N_FEAT; f++)
            feat[f * DROP_N_FEAT_VEC + j] = (raw[f] - FEATURE_MEAN[f]) * FEATURE_INV_STD[f];
    }

    /* Conv1D -> ReLU -> GlobalMaxPool, folded into one pass: keep only the max
     * post-ReLU response per output channel. */
    float pooled[DROP_CONV_OUT];
    for(int oc = 0; oc < DROP_CONV_OUT; oc++) {
        float best = 0.0f;
        for(int p = 0; p < DROP_CONV_LOUT; p++) {
            float acc = model->conv_b[oc];
            int t0 = p * DROP_CONV_STRIDE;
            for(int ic = 0; ic < DROP_N_FEAT; ic++) {
                const float* wrow = &model->conv_w[((oc * DROP_N_FEAT) + ic) * DROP_CONV_K];
                const float* xrow = &feat[ic * DROP_N_FEAT_VEC + t0];
                for(int k = 0; k < DROP_CONV_K; k++)
                    acc += wrow[k] * xrow[k];
            }
            float r = acc < 0.0f ? 0.0f : acc; /* ReLU */
            if(p == 0 || r > best) best = r; /* GlobalMaxPool */
        }
        pooled[oc] = best;
    }

    /* Dense(8 -> 4) -> logits. */
    float logits[DROP_NUM_CLASSES];
    for(int o = 0; o < DROP_NUM_CLASSES; o++) {
        float acc = model->dense_b[o];
        for(int i = 0; i < DROP_CONV_OUT; i++)
            acc += model->dense_w[o * DROP_CONV_OUT + i] * pooled[i];
        logits[o] = acc;
    }

    /* argmax is invariant under softmax, so take it from the logits. */
    int arg = 0;
    for(int o = 1; o < DROP_NUM_CLASSES; o++)
        if(logits[o] > logits[arg]) arg = o;

    if(out_probs) {
        float maxl = logits[arg];
        float sum = 0.0f;
        for(int o = 0; o < DROP_NUM_CLASSES; o++) {
            out_probs[o] = expf(logits[o] - maxl);
            sum += out_probs[o];
        }
        for(int o = 0; o < DROP_NUM_CLASSES; o++)
            out_probs[o] /= sum;
    }
    return arg;
}

/* ---------------- Firmware-only file loader ---------------- */

#ifndef MODEL_HOST_TEST

#include <furi.h>
#include <storage/storage.h>

#define TAG "drop_model"

bool model_load_json(DropModel* model, const char* path) {
    if(!model || !path) return false;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    bool ok = false;
    char* buf = NULL;

    do {
        if(!storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
            FURI_LOG_E(TAG, "open failed: %s", path);
            break;
        }
        uint64_t size = storage_file_size(file);
        if(size == 0 || size > (256u * 1024u)) { /* model.json is ~6 KB; guard runaways */
            FURI_LOG_E(TAG, "bad size %llu for %s", (unsigned long long)size, path);
            break;
        }
        buf = malloc((size_t)size + 1);
        if(!buf) break;
        uint16_t got = storage_file_read(file, buf, (uint16_t)size);
        /* model.json fits in one read; if a partial read occurs, fail loudly. */
        if(got != (uint16_t)size) {
            FURI_LOG_E(TAG, "short read %u/%llu", got, (unsigned long long)size);
            break;
        }
        buf[size] = '\0';
        ok = model_parse(model, buf);
        if(!ok) FURI_LOG_E(TAG, "parse failed for %s", path);
    } while(0);

    if(buf) free(buf);
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return ok;
}

#endif /* MODEL_HOST_TEST */
