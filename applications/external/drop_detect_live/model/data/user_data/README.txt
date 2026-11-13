Drop your own IMU recordings here to personalize the model to your motion patterns.

Layout (matches drop_detect/):
    user_data/
        idle/      *.csv     <- your stationary recordings
        walking/   *.csv     <- your walks
        fidget/    *.csv     <- your handling / fidgeting
        drop/      *.csv     <- your drop captures

CSV format must match what the Flipper data-collection app writes (same as
files in drop_detect/): one header row "order, ax, ay, az, gx, gy, gz" then
one row per IMU sample. Sample rate 100 Hz, window length 250 rows (2.5 s).

You can add data for some classes only (e.g. just walking/ if you only want
to personalize gait). Empty class folders are silently skipped.

Once files are in place:
    python tuning.py                   # mix with original train, overwrite model.json
    python tuning.py --keep_old        # write to model_tuned.json instead
    python tuning.py --epochs 8 --lr 0.005   # more aggressive

The script prints BEFORE / AFTER accuracy on TEST.bin so you can see whether
your fine-tune helped or hurt.
