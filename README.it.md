# Time Clock - Flipper Zero

[![Build & Release](https://github.com/vladpereverzyev/flipper-timeclock/actions/workflows/build.yml/badge.svg)](https://github.com/vladpereverzyev/flipper-timeclock/actions/workflows/build.yml)
[![Latest release](https://img.shields.io/github/v/release/vladpereverzyev/flipper-timeclock?cacheSeconds=300)](https://github.com/vladpereverzyev/flipper-timeclock/releases)
[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](LICENSE)

[English](README.md) | **Italiano** | [Espanol](README.es.md) | [Francais](README.fr.md) | [Deutsch](README.de.md)

App di **timbratura del personale** per il [Flipper Zero](https://flipperzero.one/).
Assegna a ogni collaboratore un **chip vergine** NFC o RFID, lo avvicini, e la
timbratura (entrata/uscita) viene registrata con data e ora sulla microSD come
foglio CSV. Funziona in autonomia, senza telefono ne PC.

> **Solo uso autorizzato.** L'app legge **solo l'UID** del chip per riconoscerlo.
> Non emula e non scrive i chip, e non aggira nessun sistema di accesso. Usa solo
> chip e sistemi per cui hai l'autorizzazione.

## Funzioni

- **Timbra**: avvicini il chip e l'app registra da sola, alternando **IN** e
  **OUT** (prima volta IN, poi OUT, poi IN...). Nessuna scelta manuale.
- **Lettore** scegliibile in Impostazioni: **NFC** (13.56 MHz) o **RFID LF**
  (125 kHz).
- **Registrazione** di un collaboratore su un chip vergine (solo il nome). Ogni
  persona e legata al suo chip (UID); chip perso: *Badge > Sostituisci chip*.
- **Work mode**: orologio a schermo intero, saluto Benvenuto/Arrivederci; per
  uscire serve il **PIN a frecce**.
- **Storico** con filtri, riepilogo **Oggi** (entrata/uscita/ore/pause) e
  **Settimana**.
- **Export** CSV/JSON, **CSV datato**, **Backup** e **Ripristino** su microSD.
- **Feedback** suono/vibrazione/LED distinti per IN e OUT (disattivabili).
- **PIN** opzionale (sequenza di frecce) proposto al primo avvio.
- **Lingue**: Inglese, Italiano, Spagnolo, Francese, Tedesco.

## Compilazione e installazione

App esterna (FAP) per il **firmware ufficiale**, compilata con
[`ufbt`](https://github.com/flipperdevices/flipperzero-ufbt):

```bash
python3 -m pip install --upgrade ufbt
ufbt            # compila (output in dist/)
ufbt launch     # installa e avvia sul Flipper collegato
```

Oppure scarica dalla [Release](https://github.com/vladpereverzyev/flipper-timeclock/releases)
il `.fap` giusto per il tuo firmware e copialo in `apps/Tools/`.

| Firmware    | File                        |
|-------------|-----------------------------|
| Ufficiale   | `timeclock-official.fap`    |
| Momentum    | `timeclock-momentum.fap`    |
| Unleashed   | `timeclock-unleashed.fap`   |
| RogueMaster | `timeclock-roguemaster.fap` |

## Licenza

Copyright (C) 2026 Vladyslav Pereverzyev. Licenza **GNU GPL-3.0-or-later** - vedi
[LICENSE](LICENSE). La documentazione completa e nel [README in inglese](README.md).
