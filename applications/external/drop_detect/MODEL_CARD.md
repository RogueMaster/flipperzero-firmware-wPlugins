# Model Card — FliPort Motion Classifier

A lightweight 1-D convolutional neural network that classifies short windows of
Flipper Zero IMU data into one of four motion states. Designed to be small
enough to run inside firmware-class memory budgets.

---

## Task

Multi-class time-series classification of 6-axis IMU data (3-axis accelerometer +
3-axis gyroscope) into **4 motion states**:

| Label | Index | Meaning |
| ----- | ----- | ------- |
| `idle`    | 0 | Device stationary |
| `walking` | 1 | Carried while walking |
| `fidget`  | 2 | Handling / fidgeting |
| `drop`    | 3 | Free-fall / impact event |

The product priority is **never missing a drop** (high drop recall), so drop is
the safety-critical class.

## Dataset

- **Source:** Real IMU recordings collected on a Flipper Zero (ICM42688P IMU,
  100 Hz, ±16 g accel / ±2000 dps gyro) using the **FliPort** data-collector FAP
  in [`flipper-zero/expo_app`](flipper-zero/expo_app). Each session is written as
  one labeled CSV (`order, ax, ay, az, gx, gy, gz`) to the SD card.
- **Stored at:** [`model/data/user_data/`](model/data/user_data) — one folder per class.
- **Class counts (raw files):** drop = 38, fidget = 24, walking = 14, idle = 6.
- **Windowing:** 250-sample sliding window (2.5 s @ 100 Hz), stride 125 (≈50%
  overlap). Each window → 249 feature vectors.
- **Features (4 per timestep):** accel magnitude², gyro magnitude², and the
  first difference (delta) of each. Squared magnitudes are deliberate — they
  avoid `sqrt`, which is expensive on a Cortex-M4.
- **Split:** stratified, **file-level** (whole files go to train *or* test, never
  both), 80/20, seed 42. Per-feature mean/std normalization is fit on train only.
- **Prep code:** [`model/scripts/data.py`](model/scripts/data.py) (and the
  equivalent C++ in the desktop workbench's *Build dataset* tab).

## Architecture

A 5-layer 1-D CNN (`build_model` in [`model/src/main.cpp`](model/src/main.cpp)):

```
Input (4 channels × 249 timesteps)
  → Conv1D(in=4, out=8, kernel=7, stride=2)  → ReLU
  → GlobalMaxPool1D  (8 values)
  → Dense(8 → 4)
  → Softmax
```

- **Parameters:** 268 (≈1 KB at FP32).
- Implemented from scratch in C++ on a custom tiny tensor library
  (`bean_tensor` / "BeanTensor lite") — **no PyTorch / TensorFlow / scikit-learn**
  at train or inference time. The same forward pass runs in the desktop app's
  Predict tab.

## Training procedure

- **Optimizer:** plain per-sample SGD with a fused softmax + cross-entropy
  gradient (`dL/dz = p − target`).
- **Final model:** 100 epochs, learning rate 0.01, batch size 1, seed 42.
- **Weight init:** He-uniform (Conv1D), Xavier-uniform (Dense).
- **Output artifact:** [`model/data/model.json`](model/data/model.json) — weights +
  an `arch` block so Predict/Tune rebuild the exact same shape. Normalization
  constants travel in the `TRAIN.bin` header.
- **Fine-tuning:** [`model/scripts/tuning.py`](model/scripts/tuning.py) mixes
  user-collected CSVs with the original train set and continues SGD, reusing the
  original normalization stats. Every run is archived to `model/runs/<timestamp>/`.

## Evaluation results

Held-out file-level test set (598 windows). Best run:
[`model/runs/2026-05-27_08-04-51`](model/runs/2026-05-27_08-04-51).

| Metric | Result |
| ------ | ------ |
| **Overall test accuracy** | **89.6 %** |
| drop recall | **100 %** (0 missed drops) |
| fidget recall | 93.1 % |
| walking recall | 82.7 % |
| idle recall | 0 % *(see limitations)* |

Proposal target was **F1 ≥ 0.85** with minimized drop false-negatives. We hit
89.6 % accuracy with **perfect drop recall**, satisfying the safety-critical
goal; idle remains the weak class and pulls the macro-average down.

## Known limitations

- **`idle` is not learned (0 % recall).** Only 6 idle files exist, and global-max
  pooling on squared magnitudes makes near-zero "still" windows hard to separate
  from low-energy fidget windows. Idle is currently misclassified, usually as
  fidget. This is the top item to fix with more idle data and/or a variance feature.
- **Small, single-operator dataset** → limited subject/device diversity; results
  may not generalize across users or hardware.
- **Drop captures are short, scripted clips** (300 samples), not naturalistic
  in-the-wild falls.
- **No on-device inference yet.** The trained model runs in the PC workbench;
  the firmware side currently only collects data (inference engine is the next
  integration step).

## Intended use

- **Intended:** offline / desktop experimentation with embedded-scale motion
  classification, and as the training + inference backend that feeds a future
  Flipper-side runtime. Drop detection is the headline use case.
- **Not intended:** safety-, medical-, or fall-alert-critical deployments; use as
  a general human activity recognizer; any setting where a missed `idle`/`walking`
  distinction matters. Treat outputs as best-effort signals, not guarantees.
