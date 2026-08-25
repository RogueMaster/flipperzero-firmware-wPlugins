# Changelog

Tutte le modifiche rilevanti al progetto ESP32 API Caller.

## [0.1.1]

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

## [0.1.2]

### Aggiunto
- **Sprint 3 — Gestione chiamate**:
  - Struttura dati `CallEntry` (URL, protocollo HTTP/HTTPS, metodo, query, headers, body) in `src/api_caller.h`
  - Storico chiamate `src/utils/call_history.c/h` su storage (FlipperFormat, `/data/call_history.txt`, max 16 voci)
  - Scena "Aggiungi chiamata": form con switch protocollo, lista metodi (GET/POST/PUT/DELETE/PATCH/HEAD), campi URL/Query/Headers/Body, salvataggio con normalizzazione dello schema (`http://`/`https://` automatico)
  - Scena "Lista chiamate": submenu delle chiamate salvate, modifica (form pre-compilato) ed eliminazione
  - Back dai campi di testo ritorna al form senza uscire dalla scena


## [0.1.3]
### Aggiunto
- **Sprint 4 — Esecuzione e Logging**:
  - `src/api/call_runner.c/h`: invio richieste FlipperHTTP non bloccante (GET/POST/PUT/PATCH/DELETE; HEAD mappato a GET), costruzione URL+query, raccolta risposta body via callback per-riga, timeout 30 s ed errori distinti
  - `src/utils/logger.c/h`: log su `/data/debug.log` con timestamp di uptime e troncamento automatico a 32 KB
  - Scena "Dettaglio chiamata": menu Esegui/Modifica, invio con schermata "Invio richiesta...", risposta (metodo, URL, status code, body) in TextBox, log di SEND/RESULT/ERROR
  - Lista chiamate: pressione sulla voce apre il dettaglio (esecuzione + modifica)

### Corretto
- Blocco su hardware durante l'esecuzione: stato della richiesta rilevato dal callback RX (non piu' da `last_response`, sovrascritto dalle risposte veloci), stack app portato a 4 KB, logger con path risolto una volta sola, marcatori di avanzamento su `debug.log`
- Risposta illeggibile (schermo bianco scorrendo): rimossi i `\r` CRLF inviati dalla board e normalizzati i caratteri di controllo nel body; buffer risposta portato a 32 KB con indicazione "[risposta troncata]" se si supera il limite
- Risposta che sparisce scrollando testi lunghi (limite del TextBox firmware): nuova vista `src/utils/long_text_view.c/h` con wrapping e scroll gestiti localmente (UP/GIU' di una riga, OK pagina), usata per il risultato delle chiamate
- Chiamate salvate non persistevano al riavvio: i campi vuoti (query/headers/body, password WiFi aperte) ora sono salvati con placeholder "-" e ripristinati come vuoti al load

## [Non rilasciato]
- Sprint 5: icone, profilazione, loader, feedback when needed, optimization and fix

### v0.2+
- Import call/list of calls in .json format
- Connection details when "Conneted" was clicked.
- Auto-Connect to first SSID registered available. (Continuous or recurrently background scan - If no saved SSID, not start - After connection stop the background scan)
- Save run in-app with details: Call list> [Call name]> Run list
- Export .log per call
- n8n/activepieces nodes for the interact with Flipper api caller (text, number, bool)
- Schedule call (In-app scheduler - ESP32 scheduler autonomo - Ibrido)
