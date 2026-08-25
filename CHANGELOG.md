# Changelog

Tutte le modifiche rilevanti al progetto ESP32 API Caller.

## [0.1.0] - 2026-08-25

### Aggiunto
- **Sprint 1 — Scheletro**: `AppContext`, `SceneManager` + `ViewDispatcher`, menu principale (VariableItemList) e navigazione tra scene stub.
- **Sprint 2 — WiFi via FlipperHTTP**:
  - SDK C FlipperHTTP v2.2.0 vendored in `src/api/flipper_http.c/h` (target ESP32-WROOM)
  - Wrapper `src/wifi/wifi_manager.c/h` (ping, scan, ssid, ip, status, save+connect, disconnect)
  - Scene `Connessione` (Ricerca reti / Reti salvate / Connesso a / Disconnetti), scansione con lista reti, input password, risultato in TextBox
  - **Scan asincrono**: feedback "Ricerca reti... Xs", lista live con aggiornamento automatico (8 s), UI non bloccante
  - **Storico reti salvate** (`src/utils/wifi_history.c/h`): salvataggio automatico di SSID+password su storage (FlipperFormat, max 8 voci), riconnessione senza reinserire la password

### Corretto
- Scan: lista troncata a una sola rete (`furi_string_set_strn` sostituisce invece di appendere)
- Scan: risposta multi-riga della board (attesa della riga JSON con "networks")
- Build: entry point allineato a `application.fam`

### Aggiunto
- **Sprint 3 — Gestione chiamate**:
  - Struttura dati `CallEntry` (URL, protocollo HTTP/HTTPS, metodo, query, headers, body) in `src/api_caller.h`
  - Storico chiamate `src/utils/call_history.c/h` su storage (FlipperFormat, `/data/call_history.txt`, max 16 voci)
  - Scena "Aggiungi chiamata": form con switch protocollo, lista metodi (GET/POST/PUT/DELETE/PATCH/HEAD), campi URL/Query/Headers/Body, salvataggio con normalizzazione dello schema (`http://`/`https://` automatico)
  - Scena "Lista chiamate": submenu delle chiamate salvate, modifica (form pre-compilato) ed eliminazione
  - Back dai campi di testo ritorna al form senza uscire dalla scena


## [0.3.0] - 2026-08-25

### Aggiunto
- **Sprint 4 — Esecuzione e Logging**:
  - `src/api/call_runner.c/h`: invio richieste FlipperHTTP non bloccante (GET/POST/PUT/PATCH/DELETE; HEAD mappato a GET), costruzione URL+query, raccolta risposta body via callback per-riga, timeout 30 s ed errori distinti
  - `src/utils/logger.c/h`: log su `/data/debug.log` con timestamp di uptime e troncamento automatico a 32 KB
  - Scena "Dettaglio chiamata": menu Esegui/Modifica, invio con schermata "Invio richiesta...", risposta (metodo, URL, status code, body) in TextBox, log di SEND/RESULT/ERROR
  - Lista chiamate: pressione sulla voce apre il dettaglio (esecuzione + modifica)

## [Non rilasciato]

- Sprint 5: icone, profilazione, testing hardware
