# Building the app from source

1) Clone the repo and cd into it.

2) Create a python venv and activate it (optional)

```
python3 -m venv .venv --prompt="fuji-general"
source .venv/bin/activate
```

3) Install [ufbt](https://github.com/flipperdevices/flipperzero-ufbt) (Check the ufbt page in case installation instructions changed)

```
python3 -m pip install --upgrade ufbt
```

4) Setup VS code dev envirionment (optional)

```
ufbt vscode_dist
```

5) Build the app

```
ufbt
```


If flipper is connected via usb, you can upload and launch it using this command

```
ufbt launch
```
