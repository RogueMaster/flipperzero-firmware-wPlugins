FliPort records labeled IMU windows from an ICM42688P breakout on the
external SPI/GPIO header. Four screens, one per label: Drop, Idle,
Walking, Fidget. Drop captures a fixed 300-sample event window
triggered by an on-screen "READY → DROP!" countdown; the other modes
record continuously until you back out. CSV files are written to
/ext/apps_data/drop_detect/LABEL/TIMESTAMP.csv and are intended
for offline classifier training (the matching Conv1D / dataset
tooling lives in the same repo).

Requires an ICM42688P module wired to the external header
(CS = PC3, INT = PB2, on the standard SPI bus). Without the IMU the
app starts but capture will fail.
