# Time Clock - Flipper Zero

[![Build & Release](https://github.com/vladpereverzyev/flipper-timeclock/actions/workflows/build.yml/badge.svg)](https://github.com/vladpereverzyev/flipper-timeclock/actions/workflows/build.yml)
[![Latest release](https://img.shields.io/github/v/release/vladpereverzyev/flipper-timeclock?cacheSeconds=300)](https://github.com/vladpereverzyev/flipper-timeclock/releases)
[![Downloads](https://img.shields.io/github/downloads/vladpereverzyev/flipper-timeclock/total?cacheSeconds=300)](https://github.com/vladpereverzyev/flipper-timeclock/releases)
[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](LICENSE)
[![Languages](https://img.shields.io/badge/lang-EN%20%7C%20IT%20%7C%20ES%20%7C%20FR%20%7C%20DE-blue)](#)

[English](README.md) | **Italiano** | [Espanol](README.es.md) | [Francais](README.fr.md) | [Deutsch](README.de.md)

App di **timbratura del personale** per il [Flipper Zero](https://flipperzero.one/).
Serve a registrare i tuoi collaboratori e le loro entrate/uscite: assegna a ogni
persona un badge - **NFC**, **RFID** o **iButton** - lo avvicini, e ogni
timbratura viene salvata con data e ora sulla microSD come foglio CSV apribile in
Excel. Funziona in totale autonomia, senza telefono ne PC.

Il lettore e **completamente automatico**: NFC, RFID e iButton vengono letti tutti
insieme, non devi mai scegliere la tecnologia. Nella stessa azienda una persona
puo avere un badge NFC, un'altra un portachiavi RFID e un'altra un iButton, e
funzionano tutti.

**Va bene anche una tessera che la persona ha gia.** Poiche l'app **legge solo
l'UID** e non scrive nulla sulla tessera, un badge gia usato con un'altra azienda
(tessera di accesso ufficio, portachiavi della palestra, tessera dei trasporti...)
puo essere registrato e usato qui senza essere modificato o sovrascritto in alcun
modo - il sistema memorizza semplicemente il suo UID insieme agli altri.

> **Solo identificazione - solo uso autorizzato.** L'app legge l'UID del badge per
> distinguere una persona dall'altra. **Non** scrive ne emula i badge e **non**
> aggira alcun sistema di autenticazione; registrare qui una tessera non ha effetto
> su dove altro viene usata. Usa solo con persone e badge che sei autorizzato a
> gestire. Vedi [SECURITY.md](SECURITY.md).

## Schermate

Mockup in stile Flipper delle schermate principali (128x64):

<table>
  <tr>
    <td align="center"><img src="docs/img/menu.svg" width="240" alt="Menu principale"><br><b>Menu principale</b><br>Timbra, Work mode, Badge, Storico...</td>
    <td align="center"><img src="docs/img/work-clock.svg" width="240" alt="Work mode"><br><b>Work mode</b><br>Orologio, timbri col chip</td>
  </tr>
  <tr>
    <td align="center"><img src="docs/img/greeting.svg" width="240" alt="Saluto"><br><b>Passa il badge</b><br>Benvenuto / Arrivederci col nome</td>
    <td align="center"><img src="docs/img/today.svg" width="240" alt="Riepilogo giornaliero"><br><b>Oggi</b><br>Timbrature + ore totali</td>
  </tr>
  <tr>
    <td align="center"><img src="docs/img/pin.svg" width="240" alt="Blocco PIN"><br><b>Blocco PIN</b><br>Protegge l'uscita dall'app</td>
    <td align="center"><img src="docs/img/settings.svg" width="240" alt="Impostazioni"><br><b>Impostazioni</b><br>Suono / Vibro / LED, PIN</td>
  </tr>
</table>

## Funzioni

- **Timbra** avvicinando un badge - i badge noti sono riconosciuti tramite UID.
- **Lettore multi-tecnologia automatico**: **NFC** (13.56 MHz), **RFID LF**
  (125 kHz) e **iButton** (chiavi Dallas 1-Wire) vengono letti tutti insieme.
  **Nessuna impostazione del lettore** - vince il primo che rileva il badge,
  quindi NFC, RFID e iButton convivono nella stessa installazione.
- **Funziona con tessere esistenti**: leggendo solo l'UID (mai scrivendo), una
  tessera gia usata altrove - anche di un'altra azienda - puo essere registrata e
  usata senza alterarla.
- **Registra un collaboratore** la prima volta che passi il suo badge, con un
  nome. Ogni persona e legata a quel chip (il suo UID): ogni timbratura fa
  riferimento a quel chip.
- **Gestisci i collaboratori (badge)**: rinomina, **sostituisci il chip** se
  perso (mantiene nome e storico, cambia solo il chip), vedi lo storico della
  persona, elimina (lo storico resta).
- **IN/OUT automatico**: avvicinando un badge si alterna da solo (prima IN, poi
  OUT, poi IN...) - nessuna scelta manuale, la timbratura e istantanea.
- **Feedback alla timbratura**: **suono**, **vibrazione** e **LED** distinti per
  IN e OUT (tono ascendente + 1 vibrazione + verde per IN; tono discendente + 2
  vibrazioni + blu per OUT). Ognuno disattivabile in Impostazioni (di default
  attivi).
- **Storico** con filtri (tutti / oggi / settimana, o per collaboratore dai
  Badge), riepilogo **Oggi** (prima entrata, ultima uscita, ore e pause) e
  **Settimana** (ore per giorno + totale).
- **Salvataggio su microSD** in CSV, piu **export JSON**, **CSV datato**, un
  **Backup** (copia datata di badge + storico) e **Ripristino** (ricarica badge +
  storico da un backup).
- **Modalita protetta (PIN)**: codice opzionale a **sequenza di frecce** (Su /
  Giu / Sinistra / Destra - veloce) che blocca l'uscita dall'app; proposto al
  primo avvio o piu tardi in Impostazioni.
- **Lingue**: Inglese, Italiano, Spagnolo, Francese, Tedesco - selezionabili in
  Impostazioni (il firmware ufficiale non espone una lingua di sistema).

Vedi la [Roadmap](#roadmap) per le idee v1.1 / v2.0.

### Un chip per persona (e chip perso)

Ogni collaboratore e identificato dall'**UID** del chip: assegna un chip a testa
e tienilo come riferimento - tutte le sue timbrature puntano a quel chip. Se
qualcuno **perde il chip**, apri **Badge -> (persona) -> Sostituisci chip** e
passa un nuovo chip vergine: nome e timbrature passate restano, cambia solo il
chip di riferimento.

## File dei dati

Tutto e salvato sulla microSD in `/ext/apps_data/timeclock/`:

| File          | Contenuto                                                       |
|---------------|-----------------------------------------------------------------|
| `badges.csv`  | Badge registrati: `uid,name,tech,created,last_used,last_event` (`tech`: `NFC`/`RFID`/`iBTN`) |
| `punches.csv` | Storico timbrature: `date,time,name,uid,type` (`IN`/`OUT`)      |
| `config.txt`  | Impostazioni + **hash** del PIN e salt (mai il PIN in chiaro)   |
| `export.json` | Export JSON dello storico (*Export -> Export JSON*)             |
| `punches-YYYY-MM-DD.csv` | Snapshot CSV datato (*Export -> Export CSV*)         |
| `backup/`     | Copie datate di badge + storico (*Export -> Backup*)            |

`punches.csv` e il foglio presenze interno: ogni entrata/uscita di ogni
collaboratore, per giorno e ora. Si apre in Excel, LibreOffice, Google Sheets,
ecc.

Esempio di `punches.csv`:

```csv
date,time,name,uid,type
2026-09-12,08:02,Mario,04A1B2C3D4,IN
2026-09-12,12:31,Mario,04A1B2C3D4,OUT
```

## Compilazione e installazione

E un'app esterna (FAP) per il **firmware ufficiale**. Si compila con
[`ufbt`](https://github.com/flipperdevices/flipperzero-ufbt):

```bash
python3 -m pip install --upgrade ufbt
```

Dalla cartella del progetto (quella con `application.fam`):

```bash
ufbt
```

Installa e avvia su un Flipper collegato:

```bash
ufbt launch
```

Il `.fap` compilato finisce in `dist/`. Puoi anche copiarlo in
`SD Card/apps/Tools/` con qFlipper e avviarlo da **Apps -> Tools -> Time Clock**.

> **Nota firmware.** Il layer radio e in `timeclock_reader.c` (NFC col poller
> ISO14443-3A - MIFARE Classic/Ultralight, NTAG, DESFire - il worker LF RFID a
> 125 kHz e il worker iButton per le chiavi Dallas 1-Wire, avviati insieme). E la
> parte piu sensibile ai cambi di API del firmware; se un simbolo cambia, la
> correzione e in quel solo file.

## Compatibilita

Time Clock funziona sul firmware **ufficiale** del Flipper Zero e sui fork piu
diffusi. Un FAP e compilato per l'API di uno specifico firmware, quindi ogni
Release fornisce **un `.fap` per firmware** - scarica quello adatto al tuo:

| Firmware    | File della release          |
|-------------|-----------------------------|
| Ufficiale   | `timeclock-official.fap`    |
| Momentum    | `timeclock-momentum.fap`    |
| Unleashed   | `timeclock-unleashed.fap`   |
| RogueMaster | `timeclock-roguemaster.fap` |

RogueMaster e basato sull'SDK Unleashed (compatibile a livello binario). Per un
firmware non elencato, compila dai sorgenti con `ufbt` (vedi sopra): il codice
usa API standard ed e scritto per essere portabile.

## Modalita protetta e PIN - cosa puo e non puo fare

Il PIN e una veloce **sequenza di 4 frecce** (es. Su, Su, Sinistra, Destra). Ti
viene proposto al primo avvio, o in qualsiasi momento da *Impostazioni -> Imposta
PIN*. Quando e attivo, l'app parte bloccata e **Back non esce piu dall'app**;
l'unica via software e *Impostazioni -> Esci* (o Work mode -> Back), che chiede la
sequenza. E salvato solo come **hash con salt**, mai in chiaro.

**Limiti onesti (per scelta):**

- Nessuna app puo impedire uno spegnimento **hardware** o un force-quit a livello
  firmware (es. `Sinistra` + `Back` per riavviare, o togliere l'alimentazione).
  La modalita protetta copre solo le azioni controllabili via software.
- L'hash del PIN (FNV-1a) evita di salvarlo in chiaro e protegge l'interfaccia,
  ma **non** e una difesa forte contro chi ha accesso fisico alla microSD e prova
  offline tutte le brevi sequenze.
- Non c'e **nessun bypass nascosto**. Eliminando `config.txt` dalla SD si azzerano
  le impostazioni (e il PIN).

## Struttura del progetto

```
timeclock/
|-- application.fam            # manifest dell'app
|-- timeclock.h / .c           # ciclo di vita, entry point, helper condivisi
|-- timeclock_storage.h / .c   # persistenza microSD + modello dati
|-- timeclock_pin.h / .c       # hash del PIN con salt
|-- views/
|   `-- pin_view.h / .c        # vista custom per il PIN
`-- scenes/
    |-- timeclock_scene*.{h,c} # scene manager (X-macro)
    `-- timeclock_scene_*.c    # un file per schermata
```

## Roadmap

- **v1.1 / v1.2** - riepilogo settimanale, calcolo pause e filtri storico (fatti).
- **v2.0** - backup e ripristino (fatti); poi: sync Bluetooth, app companion, import.

## Contribuire

I contributi sono benvenuti - vedi [CONTRIBUTING.md](CONTRIBUTING.md) e il
[Codice di Condotta](CODE_OF_CONDUCT.md).

## Sostieni

Se Time Clock ti e utile, puoi sostenere lo sviluppo:

[![Sponsor su GitHub](https://img.shields.io/badge/Sponsor-GitHub-ea4aaa?logo=githubsponsors&logoColor=white)](https://github.com/sponsors/vladpereverzyev)
[![Offri un caffe su Ko-fi](https://img.shields.io/badge/Ko--fi-Offri%20un%20caffe-ff5e5b?logo=ko-fi&logoColor=white)](https://ko-fi.com/vladpereverzyev)

## Licenza

Copyright © 2026 Vladyslav Pereverzyev.

Distribuito sotto **GNU General Public License v3.0 o successiva** - vedi
[LICENSE](LICENSE). I sorgenti riportano l'header
`SPDX-License-Identifier: GPL-3.0-or-later`.
