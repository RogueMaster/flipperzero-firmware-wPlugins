# Training and workbench

This folder is the PC side: it turns the motion recordings collected on the Flipper into a trained model, and it's where you experiment with the model. None of it runs on the device.

## What the pieces are

- `scripts/data.py` builds the dataset. It reads the CSV recordings, cuts them into 2.5-second windows, turns each window into a few simple features, and writes `TRAIN.bin` and `TEST.bin`. It splits by whole recording, not by window, so the test score can't be inflated by near-duplicate windows.
- `src/main.cpp` is the trainer. It builds the network, trains it, prints accuracy per class, and saves the weights to `data/model.json`. The network layers and the math behind them are written from scratch here, no ML framework.
- `src/app.cpp` is `flip-keeper`, a small Windows desktop app with a UI for the same steps: build the dataset, train, run predictions on a CSV, and fine-tune. A prebuilt `flip-keeper.exe` is included.
- `scripts/tuning.py` fine-tunes an existing model on your own recordings (see `data/user_data/README.txt`).

## The model in one paragraph

Four features per timestep go into a 1-D convolution, a ReLU, a global max-pool, a small dense layer, and a softmax over the four classes. That's about 268 weights total. It's deliberately tiny so the same forward pass can run on the Flipper. The full write-up is in [../MODEL_CARD.md](../MODEL_CARD.md).

## Using the desktop app

```powershell
cd model
.\flip-keeper.exe
```

Go through the tabs left to right: build the dataset, train, then point Predict at any CSV under `data/user_data/` to see what the model says. To rebuild the app you need Visual Studio 2022; run `.\build_app.bat`.

## Using the scripts instead

```powershell
cd model\scripts
python data.py            # build TRAIN.bin and TEST.bin
python tuning.py          # fine-tune model.json on your own recordings
```

## Output

The trained weights land in `data/model.json`. That's the file the Flipper detector loads. After retraining, copy it over to `flipper-zero/drop-dect/assets/model.json`.
