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
    [StrOverview] =
        {"Overview", "Panoramica", "Resumen", "Apercu", "Ubersicht"},
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
    [StrReadingIButton] =
        {"Reading iButton",
         "Lettura iButton",
         "Leyendo iButton",
         "Lecture iButton",
         "iButton lesen"},
    [StrReadingBadge] =
        {"Reading badge", "Lettura badge", "Leyendo tarjeta", "Lecture badge", "Ausweis lesen"},
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
    [StrBackup] = {"Backup", "Backup", "Copia seg.", "Sauvegarde", "Backup"},
    [StrBackupDone] =
        {"Backup saved", "Backup salvato", "Copia guardada", "Sauvegarde OK", "Backup fertig"},
    [StrRestore] =
        {"Restore", "Ripristina", "Restaurar", "Restaurer", "Wiederherstellen"},
    [StrRestoreDone] =
        {"Restored", "Ripristinato", "Restaurado", "Restaure", "Wiederhergestellt"},
    [StrNoBackups] =
        {"No backups", "Nessun backup", "Sin copias", "Aucune sauvegarde", "Keine Backups"},
    [StrRestoreConfirm] =
        {"Restore this backup?\nReplaces current data.",
         "Ripristinare?\nSostituisce i dati.",
         "Restaurar?\nReemplaza los datos.",
         "Restaurer?\nRemplace les donnees.",
         "Wiederherstellen?\nErsetzt die Daten."},
    // ---- Settings (label words; value appended with a literal format) ----
    [StrReader] = {"Reader", "Lettore", "Lector", "Lecteur", "Leser"},
    [StrSound] = {"Sound", "Suono", "Sonido", "Son", "Ton"},
    [StrVibro] = {"Vibro", "Vibro", "Vibra", "Vibro", "Vibro"},
    [StrLed] = {"LED", "LED", "LED", "LED", "LED"},
    [StrLanguage] = {"Language", "Lingua", "Idioma", "Langue", "Sprache"},
    [StrSetPin] = {"Set PIN", "Imposta PIN", "Definir PIN", "Definir PIN", "PIN setzen"},
    [StrChangePin] = {"Change PIN", "Cambia PIN", "Cambiar PIN", "Changer PIN", "PIN aendern"},
    [StrDisablePin] = {"Disable PIN", "Disattiva PIN", "Quitar PIN", "Desactiver PIN", "PIN aus"},
    [StrExitApp] = {"Exit", "Esci", "Salir", "Quitter", "Beenden"},
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
    [StrPinSaved] =
        {"PIN saved", "PIN salvato", "PIN guardado", "PIN enregistre", "PIN gespeichert"},
    [StrPinDisabled] =
        {"PIN disabled",
         "PIN disattivato",
         "PIN desactivado",
         "PIN desactive",
         "PIN deaktiviert"},
    // ---- Onboarding ----
    [StrOnbText] =
        {"Protect the app with\nan arrow PIN - needed\nto exit Work mode.",
         "Proteggi l'app con un\nPIN a frecce: serve per\nuscire dal Work mode.",
         "Protege la app con un\nPIN de flechas: hace falta\npara salir del Work mode.",
         "Protegez l'app avec un\nPIN a fleches: necessaire\npour quitter le Work mode.",
         "Schuetze die App mit\neinem Pfeil-PIN: noetig fuer\nden Arbeitsmodus-Ausgang."},
    [StrSkip] = {"Skip", "Salta", "Omitir", "Passer", "Ueberspringen"},
    // ---- About (how-it-works block; copyright appended by the scene) ----
    [StrAboutText] =
        {"Staff Time Clock\n\nNFC/RFID/iButton time clock\nfor teams. Tap a badge to\nclock in or out.\n\ngithub.com/vladpereverzyev/\nflipper-staff-time-clock\n",
         "Staff Time Clock\n\nTimbratrice NFC/RFID/iButton\nper il personale. Avvicina\nil badge per timbrare.\n\ngithub.com/vladpereverzyev/\nflipper-staff-time-clock\n",
         "Staff Time Clock\n\nFichaje NFC/RFID/iButton\npara equipos. Acerca la\ntarjeta para fichar.\n\ngithub.com/vladpereverzyev/\nflipper-staff-time-clock\n",
         "Staff Time Clock\n\nPointeuse NFC/RFID/iButton\npour equipes. Scannez le\nbadge pour pointer.\n\ngithub.com/vladpereverzyev/\nflipper-staff-time-clock\n",
         "Staff Time Clock\n\nNFC/RFID/iButton-Zeiterfassung\nfuers Team. Ausweis zum\nStempeln halten.\n\ngithub.com/vladpereverzyev/\nflipper-staff-time-clock\n"},
    // ---- Extra (v2.1) ----
    [StrThisMonth] = {"This month", "Mese", "Este mes", "Ce mois", "Dieser Monat"},
    [StrUndoLast] =
        {"Undo last punch",
         "Annulla ultima",
         "Deshacer ultimo",
         "Annuler dernier",
         "Letzte zurueck"},
    [StrUndone] = {"Undone", "Annullata", "Deshecho", "Annule", "Rueckgaengig"},
    [StrClockNotSet] =
        {"Set the Flipper clock",
         "Imposta l'orologio",
         "Ajusta el reloj",
         "Regle l'horloge",
         "Uhr einstellen"},
    // ---- Extra (v2.3) ----
    [StrAddIn] = {"Add IN", "Aggiungi IN", "Anadir IN", "Ajouter IN", "IN hinzu"},
    [StrAddOut] = {"Add OUT", "Aggiungi OUT", "Anadir OUT", "Ajouter OUT", "OUT hinzu"},
    [StrPunchAdded] =
        {"Punch added", "Timbratura aggiunta", "Fichaje anadido", "Pointage ajoute", "Stempel hinzu"},
    [StrExportMonth] =
        {"Export month", "Esporta mese", "Exportar mes", "Exporter mois", "Monat export"},
    [StrTarget] = {"Target", "Obiettivo", "Objetivo", "Objectif", "Ziel"},
    [StrOvertime] =
        {"Overtime", "Straordinario", "Horas extra", "Heures sup", "Ueberstunden"},
    // ---- Extra (v2.4) ----
    [StrDateWrongText] =
        {"The date looks wrong.\nPunches may be logged\nwith the wrong date.",
         "La data sembra errata.\nLe timbrature potrebbero\navere la data sbagliata.",
         "La fecha parece mal.\nLos fichajes pueden\ntener fecha equivocada.",
         "La date semble fausse.\nLes pointages peuvent\navoir la mauvaise date.",
         "Das Datum scheint falsch.\nStempel koennten das\nfalsche Datum haben."},
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
