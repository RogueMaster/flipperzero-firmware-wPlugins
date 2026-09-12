// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Vladyslav Pereverzyev

#pragma once

// =============================================================================
// Minimal in-app localization. English is the default; the language is chosen
// in Settings (the official firmware exposes no reliable "system language").
// Universal tokens (IN, OUT, NFC, RFID, CSV, JSON, time values) are not
// translated. Use tc_str(key) to fetch the current language's string.
// =============================================================================

#include <furi.h>

typedef enum {
    TcLangEn,
    TcLangIt,
    TcLangEs,
    TcLangFr,
    TcLangDe,
    TcLangCount,
} TcLang;

typedef enum {
    // Menu
    StrPunch,
    StrWorkMode,
    StrBadges,
    StrHistory,
    StrToday,
    StrThisWeek,
    StrExport,
    StrSettings,
    StrAbout,
    // Common
    StrOn,
    StrOff,
    StrYes,
    StrNo,
    StrSaved,
    StrDone,
    // Scan
    StrReadingNfc,
    StrReadingRfid,
    StrNewBadge,
    StrNewChip,
    StrHoldBadge,
    StrTapRegister,
    StrTapNewChip,
    StrUnknownBadge,
    StrNotRegistered,
    StrBadge,
    StrAlreadyReg, // "Already yours:"
    StrChip,
    StrChipSet, // "New chip set for" (name appended by caller)
    StrChipUsed, // "Chip already used"
    // Work
    StrRegisterFirst,
    StrSetPinFirst,
    StrPinToExit,
    StrWelcome,
    StrGoodbye,
    // Badge management
    StrBadgeName,
    StrNewBadgeItem, // "+ New badge"
    StrRename,
    StrReplaceChip,
    StrViewHistory,
    StrDeleteBadge,
    StrDeleteQ, // "Delete badge?"
    // History filter
    StrAll,
    // Today / Week
    StrFirstIn,
    StrLastOut,
    StrTotal,
    StrBreak,
    StrNoPunches,
    StrWeekTotal,
    // Export
    StrExportCsv,
    StrExportJson,
    StrClearHistory,
    StrClearConfirm,
    StrHistoryCleared,
    StrNothingExport,
    StrBackup,
    StrBackupDone,
    StrRestore,
    StrRestoreDone,
    StrNoBackups,
    StrRestoreConfirm,
    // Settings
    StrReader,
    StrSound,
    StrVibro,
    StrLed,
    StrLanguage,
    StrSetPin,
    StrChangePin,
    StrDisablePin,
    StrExitApp,
    // PIN
    StrEnterPin,
    StrConfirmPin,
    StrCurrentPin,
    StrMismatch,
    StrWrongPin, // followed by " x/y" appended by caller
    StrAttempts, // "Attempts" + " x/y"
    StrPinHint,
    // Onboarding
    StrOnbText,
    StrSkip,
    // About
    StrAboutText,
    // Weekday abbreviations (Monday..Sunday)
    StrDowMon,
    StrDowTue,
    StrDowWed,
    StrDowThu,
    StrDowFri,
    StrDowSat,
    StrDowSun,

    TcStrCount,
} TcStr;

// Set / get the active language (clamped to a valid value).
void tc_lang_set(TcLang lang);
TcLang tc_lang_get(void);

// Human-readable language name (in that language), e.g. "English", "Italiano".
const char* tc_lang_name(TcLang lang);

// Current-language string for a key.
const char* tc_str(TcStr key);
