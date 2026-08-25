# FLIPPER API CALLER 

A client that lets you easily send and schedule API call requests. A .json import is included.

## IDEA
- Connessione
    - On/off
    - Ricerca rete
        - [Lista Nome reti]
            - Password
    - Connesso a
        - Dimentica
- Aggiungi chiamata
    - Richiesta(tipo di richiesta se https o...)
    - Tipo di richiesta(tutti i tipi)
    - header(tutti i tipi)
    - body(tutti i tipi)
    - query (tutti i tipi)
    - (inserisci tu altro se necessario)
        - salva in Lista Chiamate
- Lista Chiamate
    - [Nome chiamata]
        - call (e mostra il log a schermo flipper)

## STRUCTURE

flipper_api_caller/
├── application.fam          # Application Manifest
├── README.md                # Doc
├── CHANGELOG.md             # Changelog
├── .gitignore
├── src/
│   ├── api_caller.c        # Entry point (SceneManager+ViewDispatcher)
│   ├── api_caller.h        # Header with AppContext
│   ├── scenes/
│   │   ├── scene_main.c/h           # Scena principale (menu impostazioni)
│   │   ├── scene_wifi.c/h           # Scena connessione (menu WiFi) - FATTA
│   │   ├── scene_wifi_scan.c/h      # Scena ricerca reti Wi-Fi - FATTA (scan asincrono)
│   │   ├── scene_wifi_saved.c/h     # Scena reti salvate (storico) - FATTA
│   │   ├── scene_wifi_connect.c/h   # Scena connessione (SSID/password) - FATTA
│   │   ├── scene_call_add.c/h       # Scena aggiungi chiamata API - stub
│   │   ├── scene_call_list.c/h      # Scena lista chiamate salvate - stub
│   │   └── scene_call_detail.c/h    # Scena dettaglio chiamata con log - stub
│   ├── api/
│   │   ├── flipper_http.c/h         # SDK C FlipperHTTP (vendored) - FATTO
│   │   ├── api_request.c/h          # Logica per costruire e inviare richieste
│   │   └── api_storage.c/h          # Salvataggio/caricamento chiamate salvate
│   ├── wifi/
│   │   └── wifi_manager.c/h         # Gestione Wi-Fi via FlipperHTTP - FATTO
│   └── utils/
│       ├── wifi_history.c/h         # Storico reti salvate (SSID+password) - FATTO
│       ├── logger.c/h               # Sistema di logging con debug.log
│       └── json_parser.c/h          # Parsing JSON per risposte API
└── dist/                     # Output compilato (.fap)

**Cosa manca nella struttura** - manca tutta la parte che gestisce lo scheduling e l'import delle liste in .json

## FASI PROGETTUALI (NON COMPLETO)

### Sprint 1: Scheletro e Navigazione - DONE

- Setup progetto con uFBT
- Implementare application.fam
- Creare AppContext e ViewDispatcher
- Implementare scena principale con VariableItemList
- Navigazione tra scene (submenu)

### Sprint 2: Connessione Wi-Fi - DONE

- Integrazione SDK C FlipperHTTP (vendored in src/api) su ESP32-WROOM (firmware v2.2.0)
- Scena "Ricerca rete" con scan AP asincrono (feedback + lista con aggiornamento automatico)
- Scena "Connessione" con input SSID/password (TextInput)
- Storico reti salvate con password (riconnessione rapida)
- Test con ESP32 flashato

### Sprint 3: Gestione Chiamate - DONE

- Implementare struttura dati per chiamate salvate
- Scena "Aggiungi chiamata" con tutti i campi:
- Tipo richiesta (HTTP/HTTPS) - switch
- Metodo (GET/POST/PUT/DELETE/PATCH/HEAD) - lista
- Headers (input text)
- Body (input text multiline)
- Query parameters (input text)
- Salvataggio su storage (SD card)
- Scena "Lista Chiamate" con submenu per ogni chiamata salvata

### Sprint 4: Esecuzione e Logging

- Integrazione con FlipperHTTP per invio richieste
- Visualizzazione risposta in TextBox
- Sistema di logging su debug.log
- Gestione errori e timeout

**Bug da fixare**: Nella risposta non è possibile scorrere in basso e leggerela

## Sprint 5: Ottimizzazione e UI
- Icone e assets
- Profilazione stack size con top e free
- Testing and optimizations
- Documentazione e pubblicazione

## Risorse Ufficiali e Community

- Documentazione ufficiale: developer.flipper.net
- FAM (App Manifests): documentazione completa
    https://developer.flipper.net/
- Template progetto:
    https://github.com/gemisis/flipper_zero_template
- FlipperHTTP C API: DeepWiki
    https://deepwiki.com/jblanked/FlipperHTTP
- Wi-Fi App reference: Butwm/FlipperZero-Wifi-App
    https://github.com/Butwm/FlipperZero-Wifi-App