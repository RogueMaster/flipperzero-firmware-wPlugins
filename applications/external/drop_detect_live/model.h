/*
 * model.h — on-device port of the FliPort drop-detection CNN.
 *
 * Architecture (must match model/src/main.cpp build_model):
 *   Input {4 channels, 249 timesteps}
 *     -> Conv1D(in=4, out=8, kernel=7, stride=2) -> ReLU
 *     -> GlobalMaxPool1D (8 values)
 *     -> Dense(8 -> 4)
 *     -> Softmax  => P(idle, walking, fidget, drop)
 *
 * Weights are loaded at runtime from model.json (keys "conv1d" and "dense_0").
 * The per-feature normalization constants are NOT in model.json — they live in
 * TRAIN.bin's header — so they are baked into model.c (see FEATURE_MEAN /
 * FEATURE_INV_STD there). If the feature pipeline in model/scripts/data.py
 * changes, re-bake those constants from data.py's printed FEATURE_* arrays.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DROP_WINDOW      250 /* raw IMU samples per window (2.5 s @ 100 Hz) */
#define DROP_N_FEAT_VEC  249 /* feature vectors per window (WINDOW - 1) */
#define DROP_N_FEAT      4 /* features per timestep / conv input channels */
#define DROP_CONV_OUT    8
#define DROP_CONV_K      7
#define DROP_CONV_STRIDE 2
#define DROP_CONV_LOUT   122 /* (249 - 7) / 2 + 1 */
#define DROP_NUM_CLASSES 4

/* Class indices — must match model/scripts/data.py label_map. */
#define DROP_CLASS_IDLE    0
#define DROP_CLASS_WALKING 1
#define DROP_CLASS_FIDGET  2
#define DROP_CLASS_DROP    3

/* Trained weights, laid out exactly as model.json stores them (row-major). */
typedef struct {
    float conv_w[DROP_CONV_OUT * DROP_N_FEAT * DROP_CONV_K]; /* [8][4][7] = 224 */
    float conv_b[DROP_CONV_OUT]; /* 8  */
    float dense_w[DROP_NUM_CLASSES * DROP_CONV_OUT]; /* [4][8] = 32 */
    float dense_b[DROP_NUM_CLASSES]; /* 4  */
} DropModel;

/*
 * Parse weights from an in-memory, NUL-terminated model.json buffer.
 * Returns true only if all four arrays were found with the exact expected counts.
 * Pure C (no firmware deps) so it can be unit-tested on a host.
 */
bool model_parse(DropModel* model, const char* json);

/*
 * Firmware-only: read model.json from the SD card into memory and parse it.
 * Not compiled when MODEL_HOST_TEST is defined (host harness reads the file
 * itself and calls model_parse).
 */
bool model_load_json(DropModel* model, const char* path);

/*
 * Run a full forward pass over a 250-sample ring buffer of *scaled* IMU samples
 * (accel in g, gyro in dps — same units the trainer saw).
 *   ring[i] = { ax, ay, az, gx, gy, gz }
 *   chronological sample i (0..249) is ring[(head + i) % 250]
 * Writes the 4 class probabilities to out_probs (may be NULL).
 * Returns the argmax class index (0..3). Never fails.
 */
int model_infer(const DropModel* model, const float ring[][6], int head, float out_probs[4]);

#ifdef __cplusplus
}
#endif
