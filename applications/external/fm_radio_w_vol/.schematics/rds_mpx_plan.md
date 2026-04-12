# RDS MPX Plan

## Cel

Odbiór i dekodowanie RDS z sygnalu MPX z TEA5767 na jednym wolnym wejsciu ADC Flippera (PA4)
bez konfliktu z I2C i liniami sterujacymi PAM8406.

Procesor docelowy: STM32WB55RGV6TR (Cortex-M4 z DSP/FPU).

Zasoby CPU wykorzystane w implementacji:
- ADC z DMA do ciaglego zrzutu probek (228 kS/s)
- DSP i FPU rdzenia Cortex-M4 do filtracji i obrobki sygnalu
- przetwarzanie blokowe (1024 probki) zamiast probka po probce w przerwaniu

Status: **zaimplementowane i dzialajace**. Dekoder odczytuje PI, PS, RT, PTY z nadajnikow FM.

---

## Wybrany pin

Wejscie ADC:
- pin 4 Flippera
- PA4 / ADC1_IN9
- nazwa sygnalu na PCB: RDS_MPX_ADC

Powod wyboru:
- nie koliduje z I2C na PC0/PC1
- nie koliduje z liniami PAM_MUTE, PAM_SHDN, PAM_MODE_ABD na PB3, PB2, PC3
- potwierdzony kanal ADC w STM32WB55RGV6TR

---

## Analog front-end (MCP6001)

### Cel sekcji

Wzmocnic skladowa RDS (57 kHz) z wyjscia MPXO TEA5767 przed podaniem na ADC PA4 Flippera.
Bez wzmacniacza sygnal RDS to okolo 6.7 mV peak, co daje zaledwie 8 LSB na 12-bitowym ADC.
Po wzmocnieniu x6 z filtrem HP uzyskujemy okolo 41 LSB (+14 dB poprawa kwantyzacji).

Pelna dokumentacja obwodu, weryfikacja parametrow MCP6001 i analiza clippingu:
-> [pcb_v1_1_design_notes.md](pcb_v1_1_design_notes.md), sekcja "RDS Analog Front-End z MCP6001"

### Parametry wyjscia MPXO z datasheetu TEA5767

- DC bias VMPXO: 680 do 950 mV, typowo 815 mV
- AC output (mono, delta_f = 22.5 kHz): 60 do 90 mV, typowo 75 mV
- Output resistance Ro: max 500 Ohm
- Sink current Isink: max 30 uA
- Skladowa RDS na MPXO: okolo 6.7 mV peak (dewiacja RDS +/- 2 kHz vs testowa 22.5 kHz = 8.9%)

### Obwod projektowy

Wzmacniacz nieodwracajacy MCP6001 (SOT-23-5) zasilany z 3.3V_TEA:

| Ref  | Wartosc    | Funkcja                                    |
|------|------------|--------------------------------------------|
| C1   | 1 uF       | coupling cap — zdejmuje DC 815 mV z TEA    |
| C2   | 2.2 nF     | HPF cap — odcina audio, przepuszcza RDS    |
| R1   | 2.2 kOhm   | HPF shunt + sciezka DC bias               |
| Rb1  | 100 kOhm   | bias divider gorny (3.3V → Vbias)         |
| Rb2  | 100 kOhm   | bias divider dolny (Vbias → GND)          |
| Cb   | 100 nF     | AC ground na nodzie bias                   |
| Rf   | 10 kOhm    | feedback resistor                          |
| Rg   | 2 kOhm     | gain set resistor                          |
| C4   | 100 nF     | bypass VDD MCP6001                         |

Kluczowe parametry obwodu:
- HPF: fc = 1 / (2*pi*C2*R1) = 33 kHz → odcina audio i pilota, przepuszcza 57 kHz z -1.3 dB
- Wzmocnienie: Av = 1 + Rf/Rg = 1 + 10k/2k = **x6**
- DC na wyjsciu: Vbias = 3.3V / 2 = **1.65 V** (srodek zakresu ADC)
- Worst case output (800 mV MPX): 2.44 V p-p wokol 1.65 V → od 0.43 V do 2.87 V → **nie clipuje**

### Weryfikacja MCP6001

Potwierdzone z datasheetu (DS20001733L):
- GBW = 1 MHz → f_-3dB = 167 kHz → gain na 57 kHz = 5.68x (strata 5.3%)
- Slew rate = 0.6 V/us → worst case potrzeba 0.44 V/us (73% SR) → **przechodzi**
- Phase margin: 90° przy G=+1, stabilny przy G=+6
- VCM = 1.65 V < crossover 2.2 V → pracujemy na jednym stopniu wejsciowym
- CL: ~15 pF (ADC + trasa) << 100 pF limit

---

## Sygnal MPX

Sygnal MPX zawiera:
- audio L+R w pasmie do okolo 15 kHz
- pilot 19 kHz
- stereo L-R wokol 38 kHz (DSB-SC)
- RDS na podnosnej 57 kHz (= 3 x pilot)

Wniosek: tor analogowy i probkowanie nie moga ucinac pasma ponizej 57 kHz.
HPF fc=33 kHz w obwodzie MCP6001 tlumi audio ale przepuszcza 57 kHz.

---

## Probkowanie ADC

### Sample rate: 228 kS/s

Wybor uzasadniony:
- f_s / f_RDS = 228000 / 57000 = **4 probki na okres nosnej** → upraszcza demodulacje
- f_s / R_s = 228000 / 1187.5 = **192 probki na symbol** → duzy zapas
- calkowite relacje do nosnej i bitrate eliminuja blad numeryczny
- wyraznie powyzej minimum Nyquist dla 57 kHz

### Bufor DMA: 2048 probek / bloki po 1024

- 2048 x uint16_t = 4096 B bufora kolowego DMA
- przerwanie na half-transfer (1024 probek) i full-transfer (2048 probek)
- blok obrobki = 1024 probki = ~4.49 ms przy 228 kS/s
- ~5.33 symbolu RDS na blok (dekoder trzyma stan miedzy blokami)

### Implementacja (RDSAcquisition)

Plik: `RDS/RDSAcquisition.h`, `RDS/RDSAcquisition.c`

Timer:
- TIM1, zegar 64 MHz
- AutoReload = 280 (divider 281) → actual rate = 227757 Hz (+0.1% blad)
- Update trigger → ADC TRGO

ADC:
- Scale: FuriHalAdcScale2048 (10-bit, zakres 0-2048)
- Sampling time: 12.5 cykli
- Trigger: TIM1 TRGO
- DMA: REG_DMA_TRANSFER_UNLIMITED (circular)

DMA:
- DMA1 Channel 1
- Circular, periph-to-memory
- 16-bit halfword
- Przerwania: HT (half-transfer) + TC (transfer complete)

Przetwarzanie:
- Timer-driven callback co 2 ms
- Backpressure: pending > 24 blocks → max 3 blocks/tick, > 12 → max 2, else 1
- Pending limit: 24 blokow

---

## Architektura dekodera (wzorzec SAA6588)

SAA6588 jest ureferencyjaczem architektury dekodera RDS. Nasz dekoder przejmuje koncepcje:

| Blok SAA6588 | Nasza implementacja |
|---|---|
| Selection of RDS from MPX | MCP6001 HPF + cyfrowy I/Q mixer + 3-pole LPF |
| 57 kHz carrier regeneration | NCO 57 kHz (fast path: direct demux przy 228k) |
| Demodulation | I/Q mieszanie, LPF, DBPSK differential decode |
| Symbol integration | Integrator po 192 probkach z timing recovery |
| Block detection | Sliding syndrome search (SEARCH) / krok co 26 bitow (SYNC) |
| Error correction | Burst 1-5 bit, tablica 120 wpisow, syndrome-based |
| Synchronization | SEARCH → PRE_SYNC(3) → SYNC → LOST |
| Flywheel | Limit 20 blokow uncorrectable |
| Bit slip correction | ±1 bit retry w stanie SYNC |
| Signal quality | pilot_level, rds_band_level, lock_quality — wszystko Q8/Q16 |
| Data output | Circular event queue (8 slotow), publication on change |

Warstwy naszego dekodera:

1. **RDSAcquisition** — ADC + DMA + Timer, produkuje bloki 1024 probek uint16_t
2. **RDSDsp** — demodulacja 57 kHz, LPF, timing recovery, DBPSK → strumien bitow
3. **RDSCore** — syndrome search, korekcja, sync state machine, parser grup → zdarzenia
4. **radio.c** — konsumuje zdarzenia, wyswietla PI/PS/RT w UI

---

## DSP Pipeline (RDSDsp)

Plik: `RDS/RDSDsp.h`, `RDS/RDSDsp.c`

Caly pipeline jest integer-only (Q8/Q16/Q32). FPU nie jest uzywany w hot loop.

### Przetwarzanie per-sample (228k razy na sekunde):

**1. Usuniecie DC:**
```
centered = sample - adc_midpoint
centered_q8 = centered << 8
dc_estimate_q8 += (centered_q8 - dc_estimate_q8) >> 6   (EMA, alpha=1/64)
hp = centered_q8 - dc_estimate_q8
```

**2. Demodulacja 57 kHz:**
- Fast path (228 kHz): bezposredni demux na 4 fazy (cos/sin lookup przez sample_mod4)
- Generic path: NCO z carrier_phase_q32, lookup tables rds_carrier_cos_q8[16] / rds_carrier_sin_q8[16]
- mixed_i = (hp * cos) >> 8, mixed_q = (-hp * sin) >> 8

**3. Trzystopniowa kaskada IIR LPF (alpha=1/8 kazdy stopien):**
```
s1 += (mixed - s1) >> 3
s2 += (s1 - s2) >> 3
s3 += (s2 - s3) >> 3
```
- -3 dB przy ~2500 Hz, 18 dB/oktawe rolloff
- Odrzuca stereo L-R (4+ kHz baseband) i 38 kHz leakage

**4. Integracja po symbolu:**
```
integrator_i += lpf_state3_i
integrator_q += lpf_state3_q
symbol_phase_q16 += step
```

**5. Timing recovery (early-late):**
- Okno krawedzi: 1/8 okresu symbolu na kazdej stronie
- early_energy i late_energy akumulowane z |I|+|Q|
- Bled: late - early → timing_adjust_q16 (clamped ±6.25% okresu)
- Noise gate: jesli |error| < (avg_vector_mag >> 4), step = 0
- cached_symbol_period_q16 uaktualniany kazdym symbolem

**6. Decyzja bitowa (DBPSK):**
```
dot = prev_I * curr_I + prev_Q * curr_Q   (int64_t)
bit = (dot < 0) ? 1 : 0
```

**7. Metryki sygnalu (wszystko Q8/Q16, EMA):**

| Metryka | Format | EMA shift | Co mierzy |
|---|---|---|---|
| pilot_level_q8 | Q8 | 8 | amplituda 19 kHz |
| rds_band_level_q8 | Q8 | 8 | |I|+|Q| po LPF |
| avg_abs_hp_q8 | Q8 | 8 | amplituda HP |
| dc_estimate_q8 | Q8 | 6 | offset DC |
| symbol_confidence_avg_q16 | Q16 | 7 | jakosc decyzji (0-65535) |
| block_confidence_avg_q16 | Q16 | 5 | jakosc bloku |

### Stale DSP

| Parametr | Wartosc | Format |
|---|---|---|
| RDS_BITRATE_Q16 | 0x04A38000 | Q16 = 1187.5 bps |
| RDS_CARRIER_HZ | 57000 | — |
| RDS_PILOT_HZ | 19000 | — |
| Samples/symbol @228k | 192 (191.36 Q16) | Q16 |
| Timing adjust limit | ±1/16 symbolu | ±6.25% |

---

## Decoder Core (RDSCore)

Plik: `RDS/RDSCore.h`, `RDS/RDSCore.c`

### Maszyna stanow

```
SEARCH ──(valid block A/B/C/D)──> PRE_SYNC
                                    │
                     (3 kolejne valid/corrected w sekwencji)
                                    │
                                    v
                                  SYNC
                                    │
                      (flywheel_errors > 20)
                                    │
                                    v
                                  LOST ──> SEARCH
```

#### SEARCH
- Sliding window bit po bicie, test syndromu dla wszystkich 5 typow blokow
- Przejscie do PRE_SYNC: **tylko** gdy blok jest **Valid** (bez korekcji)
- Jesli wiele typow pasuje → ambiguous → Invalid, pozostajemy w SEARCH

#### PRE_SYNC
- Wymagane **3 kolejne** bloki valid lub corrected w prawidlowej sekwencji (A→B→C/C'→D→A)
- presync_consecutive++, advance expected_next_block
- Jesli blok nie pasuje lub uncorrectable → restart do SEARCH

#### SYNC
- Dekodowanie co 26 bitow (nie sliding)
- Valid/corrected → flywheel_errors = 0, handle block
- Uncorrectable → flywheel_errors++ → bit slip retry (±1 bit)
- C i C' sa wymienne na pozycji trzeciego bloku

#### LOST
- Emit SyncLost
- Natychmiast restart do SEARCH

### Kluczowe parametry synchronizacji

| Parametr | Wartosc | Uzasadnienie |
|---|---|---|
| RDS_PRESYNC_REQUIRED | **3** | Wiecej niz SAA6588 (2), mniej false positives |
| RDS_DEFAULT_FLYWHEEL_LIMIT | **20** | Tolerancja na krótkie zaklocenia |
| SEARCH acceptance | **Valid only** | Corrected w SEARCH daje za duzo false positives |
| Bit slip retry | ±1 bit | Tylko w SYNC, przy failed expected block |

### Sekwencja blokow i offset words

Blok RDS = 26 bitow: 16 bitow danych + 10 bitow checkword.
Cztery bloki tworza grupe 104-bit: A → B → C (lub C') → D.

Generator polynomial: g(x) = x^10 + x^8 + x^7 + x^5 + x^4 + x^3 + 1 (binarnie: 10110111001 = 0x5B9)

Stale offset words:
- A = 0x0FC
- B = 0x198
- C = 0x168
- C' = 0x350
- D = 0x1B4

### Obliczanie syndromu

Szybka metoda tablicowa (sliced, 4 LUT):
- 3 x 256-entry uint16_t dla bitow [7:0], [15:8], [23:16]
- 1 x 4-entry uint16_t dla bitow [25:24]
- Wynik: XOR wszystkich czesciowych syndromow
- Tablice generowane raz przy init (rds_core_build_syndrome_tables)

### Korekcja bledow

Burst errors o dlugosci 1 do 5 bitow:
- 120 wpisow w tablicy korekcji (26 + 25 + 24 + 23 + 22)
- Lookup: error_syndrome = syndrome XOR expected_offset
- Jesli error_syndrome jest w tablicy → popraw bity, zweryfikuj ponownie
- Per syndrome zapamietany wpis z minimalnym burst_length (lepsze dopasowanie)

Procedura:
1. Oblicz syndrome odebranego 26-bit slowa
2. Jezeli syndrome == expected_offset → **Valid**
3. Oblicz error_syndrome = syndrome XOR expected_offset
4. Lookup w tablicy korekcji
5. Jesli znaleziony: popraw bity, przelicz syndrome → jesli OK → **Corrected**
6. W przeciwnym razie → **Uncorrectable**

### Bit-slip correction

- Aktywna tylko w stanie SYNC
- Jesli blok nie pasuje do oczekiwanego typu:
  - Proba z oknem przesuniętym o 1 bit (bit_history >> 1)
  - Jesli shifted window dekoduje poprawnie i pasuje do expected → accept
  - Inkrementacja bit_slip_repairs
  - Reset bit_phase

### Skladanie PS (grupy 0A/0B)

```
segment = block_b & 0x03   (0-3)
ps_candidate[segment*2 : segment*2+1] = block_d high/low bytes
```

**Wyswietlanie inkrementalne**: kazdy segment kopiowany do ps[] natychmiast po odebraniu.
PsUpdated emitowane po kazdym segmencie, nie dopiero po zebraniu wszystkich 4.
ps_segment_mask = bitmask 0x0F, resetowany po zebraniu wszystkich.

### Skladanie RT (grupy 2A/2B)

- RT A/B flag: zmiana → czyszczenie bufora
- V1.0 (2A): 4 znaki na segment, 16 segmentow, max 64 znaki
- V2.0 (2B): 2 znaki na segment, 8 segmentow, max 32 znaki
- RtUpdated emitowane po zebraniu wszystkich segmentow i wykryciu zmiany

---

## System zdarzen

Circular event queue, 8 slotow, FIFO z odrzucaniem najstarszych przy overflow.

Typy zdarzen:

| Zdarzenie | Kiedy emitowane |
|---|---|
| DecoderStarted | start modulu |
| PilotDetected | pilot_level_q8 > RDS_PILOT_LEVEL_MIN_Q8 (5120 = 20.0) |
| RdsCarrierDetected | rds_band_level_q8 > RDS_BAND_LEVEL_MIN_Q8 (5120) |
| SyncAcquired | presync_consecutive osiagnal 3 |
| SyncLost | flywheel_errors > 20 |
| PiUpdated | zmiana PI |
| PsUpdated | nowy segment PS (inkrementalnie) |
| RtUpdated | kompletny nowy RadioText |
| PtyUpdated | zmiana PTY |
| BlockStatsUpdated | aktualizacja statystyk blokow |

Struktura zdarzenia:
- type, tick_ms, pi, ps[9], rt[65], pty, sync_state, total/valid/corrected/uncorrectable blocks

Zasada: radio.c konsumuje zdarzenia przez rds_core_pop_event() i aktualizuje UI.

---

## Flaga kompilacji ENABLE_RDS

W radio.c zdefiniowana flaga:
```c
#define ENABLE_RDS 1   // 0 = wylacza caly kod RDS
```

Gdy ENABLE_RDS=0:
- brak linkowania modulow RDS (includes, init, cleanup)
- brak pozycji RDS w menu konfiguracji
- brak pola PS/RT na ekranie
- kompiluje sie czysto (sprawdzone)

Gdy ENABLE_RDS=1:
- pelna funkcjonalnosc RDS
- opcja RDS On/Off w menu konfiguracji
- wyswietlanie PS na ekranie

---

## Statystyki i diagnostyka

### Metryki RDSAcquisition

- configured_sample_rate_hz (228000)
- measured_sample_rate_hz (z pomiaru timingowego)
- adc_midpoint (poziom DC)
- dma_half_events, dma_full_events (liczniki przerwan)
- delivered_blocks, dropped_blocks (backpressure)
- pending_blocks, pending_peak_blocks
- adc_overrun_count

### Metryki RDSCore

- total_blocks, valid_blocks, corrected_blocks, uncorrectable_blocks
- sync_losses
- bit_slip_repairs
- sync_state (aktualny stan maszyny)
- pilot_detected, rds_carrier_detected (bool)

### Metryki RDSDsp

- symbol_count (calkowita liczba zdemodulowanych symboli)
- block_confidence_avg_q16 (srednia jakosc bloku, Q16 0-65535)
- symbol_confidence_avg_q16 (srednia jakosc decyzji, Q16)
- pilot_level_q8, rds_band_level_q8, avg_abs_hp_q8

---

## Porownanie z SAA6588 — szczegolowe odniesienia

SAA6588 jako wzorzec architektury — elementy przejete koncepcyjnie:

1. **Pasmowe wydzielenie 57 kHz** — u nas: MCP6001 HPF + cyfrowy mixer + 3-pole IIR LPF
2. **Regeneracja nosnej** — u nas: NCO carrier_phase_q32, fast path przy 228k (direct demux)
3. **Integracja po symbolu** — u nas: integrator I/Q z 192 probkami na symbol
4. **Detekcja roznicowa bitow** — u nas: dot product prev/curr, DBPSK
5. **Syndrome-based block detection** — u nas: sliced LUT (4 tablice), 0x5B9 polynomial
6. **Korekcja burst errors** — u nas: tablica 120 wpisow, burst 1-5 bit
7. **Synchronizacja po sekwencji** — u nas: 3 kolejne bloki (wiecej niz SAA6588: 2)
8. **Flywheel** — u nas: limit 20 (szerzej niz typowe 8-12, lepsza tolerancja na krótkie zaklocenia)
9. **Bit-slip correction** — u nas: ±1 bit retry w SYNC
10. **Metryki jakosci** — u nas: pilot_level, rds_band, confidence — wszystko Q8/Q16 integer

Elementy, ktorych NIE kopiujemy z SAA6588:
- analogowy switched-capacitor band-pass filter (u nas: cyfrowy)
- comparator sprzetowy (u nas: software decision)
- presync=2 (u nas: 3, mniej false positives)
- szczegoly I2C/DAVN rejestrowe

---

## Wymagania PCB

Na PCB v1.1 przewidziane:
- polaczenie MPXO z TEA5767 pin 25 do sekcji RDS MCP6001
- wyjscie MCP6001 do PA4 (Flipper pin 4) oznaczone jako RDS_MPX_ADC
- footprint MCP6001 (SOT-23-5) z rezystorami wzmocnienia (Rf, Rg)
- footprint HPF (C2, R1) i bias divider (Rb1, Rb2, Cb)
- coupling cap C1 = 1 uF
- bypass C4 = 100 nF przy VDD MCP6001
- zasilanie MCP6001: 3.3V_TEA (z LDO TEA)

---

## Proponowany format plikow debug dump

Lokalizacja: `/ext/apps_data/fmradio_controller_pt2257/rds_debug/`

Pliki jednej sesji:

### session_meta.txt
```
app_version=
utc_or_tick=
station_freq_10khz=
sample_rate_hz=228000
adc_bits=10
adc_storage=u16le
adc_pin=PA4
adc_channel=ADC1_IN9
input_mode=mcp6001_x6
dc_bias_mv=1650
gain_nominal=6
dma_samples=2048
block_samples=1024
```

### mpx_adc_u16le.raw
- surowe probki ADC, little-endian uint16_t
- wartosc 0 do 2048 (10-bit ADC)
- tylko tryb diagnostyczny, ograniczony czas (kilka sekund)

### rds_events.csv
Kolumny: tick_ms, event, pi_hex, ps, pty, block_ok, block_err, sync_state, pilot_detected, rds57_detected

---

## Historia dokumentu

Ten dokument zostal pierwotnie stworzony jako plan implementacji RDS.
Po pelnej implementacji zaktualizowany do stanu zgodnego z kodem zrodlowym (RDS/RDSDsp.c, RDS/RDSCore.c, RDS/RDSAcquisition.c).

Wazna zmiana: wczesne pomiary z modulu RRD-102BC sugerowaly "wariant startowy" bez kondensatora odsprzegajacego i bez dzielnika bias. Te wnioski okazaly sie bledne — w finalnym projekcie uzywamy C1=1uF, dzielnika Rb1/Rb2=100k i wzmocnienia x6 na MCP6001. Stare wnioski pomiarowe zostaly usuniete z dokumentu.