# 🚀 Szybki Start - Flight Monitor

## Instalacja w 3 krokach:

### 1️⃣ Flashuj aplikację na Flipper

```bash
cd c:\Users\mosfet\Documents\flipper\flipperzero-firmware
./fbt launch_app APPSRC=applications_user/flight_monitor
```

Lub ręcznie:
- Skopiuj `build/f7-firmware-D/.extapps/flight_monitor.fap`
- Do `/ext/apps/Bluetooth/` na Flipper Zero

---

### 2️⃣ Zainstaluj biblioteki Python

```bash
pip install requests bleak
```

---

### 3️⃣ Uruchom!

#### A) TRYB TESTOWY (bez gry):

```bash
cd applications_user/flight_monitor
python test_flight.py
```

✅ Zobaczysz symulację startu samolotu

---

#### B) TRYB GRY (War Thunder):

1. Włącz War Thunder
2. Uruchom serwer:

```bash
python flight_server.py
```

3. Wejdź do gry i zacznij lot!

---

## 📱 Na Flipper Zero:

1. **Apps → Bluetooth → Flight Monitor**
2. Poczekaj na połączenie
3. Gotowe! 🎮

---

## ⚠️ Troubleshooting

| Problem | Rozwiązanie |
|---------|-------------|
| "Nie znaleziono Flipper" | Uruchom aplikację na Flipperze najpierw |
| "Nie można połączyć z grą" | Sprawdź: http://localhost:8111/state |
| Dane się nie zmieniają | Musisz być w locie, nie w hangarze |
| "Connection Lost" | Zbliż Flipper do PC |

---

## 📊 Co będziesz widział:

```
┌─────────────────────┐
│ ALT:1234m  P:12     │  <- Wysokość, Pitch
│ SPD:456km/h R:-5    │  <- Prędkość, Roll
│ V/S:15.3m/s         │  <- Prędkość wznoszenia
│ THR:85% ████████▒▒  │  <- Przepustnica (pasek)
│ FLP:30% ███▒▒▒▒▒▒▒  │  <- Klapy (pasek)
│ GEAR:DOWN           │  <- Podwozie
└─────────────────────┘
```

---

**Have fun!** 🛩️

Pytania? Zobacz pełny [README.md](README.md)
