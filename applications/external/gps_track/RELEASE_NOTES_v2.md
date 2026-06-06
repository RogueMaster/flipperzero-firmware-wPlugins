## GPS Track v2.0

### What's new

**Area measurement** — new tool in Settings. Walk around any area and get its size in m² in real time. The app averages GPS readings to reduce jitter and intelligently simplifies the polygon to keep accuracy while storing only up to 30 key vertices. Polygon is saved to `area.csv` on exit.

**Better LED feedback** — the main screen now shows GPS acquisition status via LED: red means no signal, blue×2 means signal but no coordinates yet, green means ready to navigate.

**Settings reworked** — navigate with ↑↓, change values with ←→, OK saves and exits, Back cancels all changes. Baud rate and other settings are now correctly applied on every app launch.

**POI improvements** — selected POI is no longer lost when switching between NAV and other screens. Saving a POI now works immediately with OK; ←→ opens the name editor if you want to rename.

---

### Upgrading from v1.0

All data files are compatible. Copy your existing `poi.csv`, `track.csv`, and `settings.csv` — they will work with v2.0 without changes.

---

### Which version to use?

**Use v1.0** if you are setting up a new GPS module and need to diagnose the serial connection. The NMEA dump tool (Settings → NMEA dump) captures raw output and tells you whether the baud rate is correct.

**Use v2.0** for everything else.

---

### Assets

| File | Description |
|------|-------------|
| `gps_track_v2.fap` | Ready-to-use app for Unleashed firmware |
| `gps_track_v1.fap` | Previous version with NMEA diagnostics |
| `demo_data.zip` | Sample POI (30 Russian cities) and track |
| Source code (zip) | Build from source with ufbt |
