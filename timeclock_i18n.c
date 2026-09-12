// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Vladyslav Pereverzyev

#include "timeclock_i18n.h"

// Order EN, IT, ES, FR, DE - must match TcLang. Strings are ASCII only (the
// Flipper font has no accented glyphs), so accents are dropped on purpose.

static TcLang g_lang = TcLangEn;

static const char* const S[TcStrCount][TcLangCount] = {
    // ---- Menu ----
    [StrPunch] = {"Punch", "Timbra", "Fichar", "Pointer", "Stempeln"},
    [StrWorkMode] = {"Work mode", "Modalita lavoro", "Modo trabajo", "Mode travail", "Arbeitsmodus"},
    [StrBadges] = {"Badges", "Badge", "Tarjetas", "Badges", "Ausweise"},
    [StrHistory] = {"History", "Storico", "Historial", "Historique", "Verlauf"},
    [StrToday] = {"Today", "Oggi", "Hoy", "Aujourd'hui", "Heute"},
    [StrThisWeek] = {"This week", "Settimana", "Esta semana", "Cette semaine", "Diese Woche"},
    [StrExport] = {"Export", "Esporta", "Exportar", "Exporter", "Export"},
    [StrSettings] = {"Settings", "Impostazioni", "Ajustes", "Reglages", "Einstellungen"},
    [StrAbout] = {"About", "Info", "Acerca de", "A propos", "Info"},
    // ---- Common ----
    [StrOn] = {"On", "On", "On", "On", "Ein"},
    [StrOff] = {"Off", "Off", "Off", "Off", "Aus"},
    [StrYes] = {"Yes", "Si", "Si", "Oui", "Ja"},
    [StrNo] = {"No", "No", "No", "Non", "Nein"},
    [StrSaved] = {"Saved", "Salvato", "Guardado", "Enregistre", "Gespeichert"},
    [StrDone] = {"Done", "Fatto", "Hecho", "Termine", "Fertig"},
    // ---- Scan ----
    [StrReadingNfc] = {"Reading NFC", "Lettura NFC", "Leyendo NFC", "Lecture NFC", "NFC lesen"},
    [StrReadingRfid] =
        {"Reading RFID", "Lettura RFID", "Leyendo RFID", "Lecture RFID", "RFID lesen"},
    [StrNewBadge] = {"New badge", "Nuovo badge", "Nueva tarjeta", "Nouveau badge", "Neuer Ausweis"},
    [StrNewChip] = {"New chip", "Nuovo chip", "Nuevo chip", "Nouvelle puce", "Neuer Chip"},
    [StrHoldBadge] =
        {"Hold the badge\nnear the Flipper",
         "Avvicina il badge\nal Flipper",
         "Acerca la tarjeta\nal Flipper",
         "Approchez le badge\ndu Flipper",
         "Ausweis an den\nFlipper halten"},
    [StrTapRegister] =
        {"Tap the chip\nto register",
         "Passa il chip\nper registrare",
         "Pasa el chip\npara registrar",
         "Scannez la puce\npour enregistrer",
         "Chip zum\nRegistrieren"},
    [StrTapNewChip] =
        {"Tap the new chip\nfor this person",
         "Passa il nuovo chip\nper la persona",
         "Pasa el nuevo chip\npara la persona",
         "Scannez la puce\npour la personne",
         "Neuen Chip fuer\ndie Person"},
    [StrUnknownBadge] =
        {"Unknown badge", "Badge sconosciuto", "Desconocida", "Badge inconnu", "Unbekannt"},
    [StrNotRegistered] =
        {"Badge not\nregistered",
         "Badge non\nregistrato",
         "Tarjeta no\nregistrada",
         "Badge non\nenregistre",
         "Ausweis nicht\nregistriert"},
    [StrBadge] = {"Badge", "Badge", "Tarjeta", "Badge", "Ausweis"},
    [StrAlreadyReg] =
        {"Already yours:", "Gia registrato:", "Ya registrado:", "Deja enregistre:", "Schon da:"},
    [StrChip] = {"Chip", "Chip", "Chip", "Puce", "Chip"},
    [StrChipSet] =
        {"New chip set for",
         "Nuovo chip per",
         "Nuevo chip para",
         "Nouvelle puce pour",
         "Neuer Chip fuer"},
    [StrChipUsed] =
        {"Chip already used by",
         "Chip gia usato da",
         "Chip ya usado por",
         "Puce deja utilisee",
         "Chip schon benutzt"},
    // ---- Work ----
    [StrRegisterFirst] =
        {"Register a\ncollaborator first",
         "Registra prima\nun collaboratore",
         "Registra antes\nun colaborador",
         "Enregistrez\nune personne",
         "Erst Person\nregistrieren"},
    [StrSetPinFirst] =
        {"Set a PIN first\n(Settings)",
         "Imposta un PIN\n(Impostazioni)",
         "Define un PIN\n(Ajustes)",
         "Definir un PIN\n(Reglages)",
         "Erst PIN setzen\n(Einstellungen)"},
    [StrPinToExit] = {"PIN to exit", "PIN per uscire", "PIN para salir", "PIN pour sortir", "PIN zum Ende"},
    [StrWelcome] = {"Welcome", "Benvenuto", "Bienvenido", "Bienvenue", "Willkommen"},
    [StrGoodbye] = {"Goodbye", "Arrivederci", "Adios", "Au revoir", "Tschuess"},
    // ---- Badge management ----
    [StrBadgeName] = {"Badge name", "Nome badge", "Nombre", "Nom", "Name"},
    [StrNewBadgeItem] =
        {"+ New badge", "+ Nuovo badge", "+ Nueva tarjeta", "+ Nouveau badge", "+ Neuer Ausweis"},
    [StrRename] = {"Rename", "Rinomina", "Renombrar", "Renommer", "Umbenennen"},
    [StrReplaceChip] =
        {"Replace chip", "Sostituisci chip", "Cambiar chip", "Remplacer puce", "Chip ersetzen"},
    [StrViewHistory] =
        {"View history", "Vedi storico", "Ver historial", "Voir historique", "Verlauf zeigen"},
    [StrDeleteBadge] =
        {"Delete badge", "Elimina badge", "Borrar tarjeta", "Supprimer badge", "Ausweis loeschen"},
    [StrDeleteQ] =
        {"Delete badge?",
         "Eliminare badge?",
         "Borrar tarjeta?",
         "Supprimer badge?",
         "Ausweis loeschen?"},
    // ---- History filter ----
    [StrAll] = {"All", "Tutti", "Todos", "Tous", "Alle"},
    // ---- Today / Week ----
    [StrFirstIn] =
        {"First in", "Prima entrata", "Primera entrada", "Premiere entree", "Erster IN"},
    [StrLastOut] =
        {"Last out", "Ultima uscita", "Ultima salida", "Derniere sortie", "Letzter OUT"},
    [StrTotal] = {"Total", "Totale", "Total", "Total", "Gesamt"},
    [StrBreak] = {"Break", "Pausa", "Pausa", "Pause", "Pause"},
    [StrNoPunches] =
        {"No punches yet.",
         "Nessuna timbratura.",
         "Sin fichajes.",
         "Aucun pointage.",
         "Keine Stempel."},
    [StrWeekTotal] =
        {"Week total", "Totale settimana", "Total semana", "Total semaine", "Wochensumme"},
    // ---- Export ----
    [StrExportCsv] =
        {"Export CSV", "Esporta CSV", "Exportar CSV", "Exporter CSV", "CSV export"},
    [StrExportJson] =
        {"Export JSON", "Esporta JSON", "Exportar JSON", "Exporter JSON", "JSON export"},
    [StrClearHistory] =
        {"Clear history",
         "Cancella storico",
         "Borrar historial",
         "Effacer historique",
         "Verlauf loeschen"},
    [StrClearConfirm] =
        {"Clear all history?\nThis cannot be undone.",
         "Cancellare tutto?\nNon reversibile.",
         "Borrar todo?\nSin vuelta atras.",
         "Tout effacer?\nIrreversible.",
         "Alles loeschen?\nUnwiderruflich."},
    [StrHistoryCleared] =
        {"History cleared",
         "Storico cancellato",
         "Historial borrado",
         "Historique efface",
         "Verlauf geloescht"},
    [StrNothingExport] =
        {"Nothing to export",
         "Niente da esportare",
         "Nada que exportar",
         "Rien a exporter",
         "Nichts zu exportieren"},
    // ---- Settings (label words; value appended with a literal format) ----
    [StrReader] = {"Reader", "Lettore", "Lector", "Lecteur", "Leser"},
    [StrSound] = {"Sound", "Suono", "Sonido", "Son", "Ton"},
    [StrVibro] = {"Vibro", "Vibro", "Vibra", "Vibro", "Vibro"},
    [StrLed] = {"LED", "LED", "LED", "LED", "LED"},
    [StrLanguage] = {"Language", "Lingua", "Idioma", "Langue", "Sprache"},
    [StrSetPin] = {"Set PIN", "Imposta PIN", "Definir PIN", "Definir PIN", "PIN setzen"},
    [StrChangePin] = {"Change PIN", "Cambia PIN", "Cambiar PIN", "Changer PIN", "PIN aendern"},
    [StrDisablePin] = {"Disable PIN", "Disattiva PIN", "Quitar PIN", "Desactiver PIN", "PIN aus"},
    [StrExitApp] = {"Exit app", "Esci dall'app", "Salir", "Quitter", "App beenden"},
    // ---- PIN ----
    [StrEnterPin] =
        {"Enter PIN", "Inserisci PIN", "Introduce PIN", "Entrez le PIN", "PIN eingeben"},
    [StrConfirmPin] =
        {"Confirm PIN", "Conferma PIN", "Confirmar PIN", "Confirmer PIN", "PIN bestaetigen"},
    [StrCurrentPin] =
        {"Current PIN", "PIN attuale", "PIN actual", "PIN actuel", "Aktueller PIN"},
    [StrMismatch] =
        {"Mismatch, retry", "Non combacia", "No coincide", "Non concordant", "Stimmt nicht"},
    [StrWrongPin] = {"Wrong PIN", "PIN errato", "PIN incorrecto", "PIN incorrect", "Falscher PIN"},
    [StrAttempts] = {"Attempts", "Tentativi", "Intentos", "Essais", "Versuche"},
    [StrPinHint] =
        {"Arrows  |  OK=clear",
         "Frecce | OK=azzera",
         "Flechas | OK=borrar",
         "Fleches | OK=effacer",
         "Pfeile | OK=leeren"},
    // ---- Onboarding ----
    [StrOnbText] =
        {"Set an arrow PIN to protect\nthe app: nobody leaves Work\nmode without it.\nAlso later in Settings.",
         "Imposta un PIN a frecce per\nproteggere l'app: senza, non\nsi esce dal Work mode.\nAnche dopo in Impostazioni.",
         "Define un PIN de flechas\npara proteger la app: sin el\nno se sale del Work mode.\nTambien en Ajustes.",
         "Definir un PIN a fleches\npour proteger l'app: sans, on\nne quitte pas le Work mode.\nAussi dans Reglages.",
         "Pfeil-PIN zum Schutz der\nApp: ohne ihn kein Verlassen\ndes Arbeitsmodus.\nAuch spaeter in Einstellungen."},
    [StrSkip] = {"Skip", "Salta", "Omitir", "Passer", "Ueberspringen"},
    // ---- About (how-it-works block; copyright appended by the scene) ----
    [StrAboutText] =
        {"Time Clock\nStaff time-clock.\n\n- Badges: register a person on\n  a blank chip (name only).\n- Punch: tap the chip. Logs IN,\n  then OUT, then IN...\n- Work mode: kiosk clock; exit\n  needs the arrow PIN.\n- Today / This week: worked\n  time and breaks.\n- Export CSV/JSON on the SD.\n- Lost chip: Badges > Replace.\n\nOnly the chip UID is read - no\nwriting, no emulation.\n",
         "Time Clock\nTimbrature del personale.\n\n- Badge: registra una persona\n  su un chip vergine (nome).\n- Timbra: passa il chip. Segna\n  IN, poi OUT, poi IN...\n- Work mode: orologio; per\n  uscire serve il PIN a frecce.\n- Oggi / Settimana: ore e pause.\n- Esporta CSV/JSON su SD.\n- Chip perso: Badge > Sostituisci.\n\nSi legge solo l'UID del chip -\nniente scrittura o emulazione.\n",
         "Time Clock\nControl de fichajes.\n\n- Tarjetas: registra a alguien\n  en un chip vacio (nombre).\n- Fichar: pasa el chip. Marca\n  IN, luego OUT, luego IN...\n- Work mode: reloj; para salir\n  hace falta el PIN de flechas.\n- Hoy / Semana: horas y pausas.\n- Exportar CSV/JSON en la SD.\n- Chip perdido: Tarjetas > Cambiar.\n\nSolo se lee el UID del chip -\nsin escritura ni emulacion.\n",
         "Time Clock\nPointage du personnel.\n\n- Badges: enregistrer une\n  personne sur une puce (nom).\n- Pointer: scannez la puce. IN,\n  puis OUT, puis IN...\n- Work mode: horloge; sortir\n  demande le PIN a fleches.\n- Aujourd'hui / Semaine: heures\n  et pauses.\n- Export CSV/JSON sur la SD.\n- Puce perdue: Badges > Remplacer.\n\nSeul l'UID de la puce est lu -\naucune ecriture ni emulation.\n",
         "Time Clock\nZeiterfassung.\n\n- Ausweise: Person auf einem\n  leeren Chip anlegen (Name).\n- Stempeln: Chip scannen. IN,\n  dann OUT, dann IN...\n- Arbeitsmodus: Uhr; Beenden\n  braucht den Pfeil-PIN.\n- Heute / Woche: Zeit und Pausen.\n- CSV/JSON auf SD exportieren.\n- Chip verloren: Ausweise > Ersetzen.\n\nNur die Chip-UID wird gelesen -\nkein Schreiben, keine Emulation.\n"},
    // ---- Weekday abbreviations (Mon..Sun) ----
    [StrDowMon] = {"Mon", "Lun", "Lun", "Lun", "Mo"},
    [StrDowTue] = {"Tue", "Mar", "Mar", "Mar", "Di"},
    [StrDowWed] = {"Wed", "Mer", "Mie", "Mer", "Mi"},
    [StrDowThu] = {"Thu", "Gio", "Jue", "Jeu", "Do"},
    [StrDowFri] = {"Fri", "Ven", "Vie", "Ven", "Fr"},
    [StrDowSat] = {"Sat", "Sab", "Sab", "Sam", "Sa"},
    [StrDowSun] = {"Sun", "Dom", "Dom", "Dim", "So"},
};

void tc_lang_set(TcLang lang) {
    if(lang < TcLangCount) g_lang = lang;
}

TcLang tc_lang_get(void) {
    return g_lang;
}

const char* tc_lang_name(TcLang lang) {
    static const char* const names[TcLangCount] =
        {"English", "Italiano", "Espanol", "Francais", "Deutsch"};
    if(lang >= TcLangCount) lang = TcLangEn;
    return names[lang];
}

const char* tc_str(TcStr key) {
    if(key >= TcStrCount) return "";
    const char* s = S[key][g_lang];
    if(!s) s = S[key][TcLangEn]; // fall back to English if a cell is empty
    return s ? s : "";
}
