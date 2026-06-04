// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Cafe com Solda / Cristiano Santana

#pragma once

#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/submenu.h>
#include <gui/modules/widget.h>
#include <gui/modules/popup.h>

#include <lib/lfrfid/lfrfid_worker.h>
#include <lib/lfrfid/protocols/lfrfid_protocols.h>
#include <toolbox/protocols/protocol_dict.h>

#include <nfc/nfc.h>
#include <nfc/nfc_scanner.h>
#include <nfc/nfc_device.h>
#include <nfc/protocols/nfc_protocol.h>
#include <nfc/protocols/mf_classic/mf_classic.h>

#include <storage/storage.h>

#include "database/cards_db.h"

#define CHAOSID_MAX_SECTORS 40 // espelha MF_CLASSIC_TOTAL_SECTORS_MAX

typedef enum {
    ChaosIdSceneSplash,
    ChaosIdSceneScanning,
    ChaosIdSceneResult,
    ChaosIdSceneHistory,
    ChaosIdSceneAbout,
    ChaosIdSceneAttack,
    ChaosIdSceneCount,
} ChaosIdScene;

typedef enum {
    ChaosIdViewMenu,
    ChaosIdViewScanning,
    ChaosIdViewWidget,
} ChaosIdView;

typedef enum {
    ChaosIdCustomEventStartScan = 100,
    ChaosIdCustomEventShowAbout,
    ChaosIdCustomEventShowHistory,
    ChaosIdCustomEventPhaseLfDone,
    ChaosIdCustomEventPhaseHfDone,
    ChaosIdCustomEventHfDetected,
    ChaosIdCustomEventCardFound,
    ChaosIdCustomEventScanFailed,
    // v0.5
    ChaosIdCustomEventInvestigate,
    ChaosIdCustomEventAttackProgress,
    ChaosIdCustomEventAttackComplete,
} ChaosIdCustomEvent;

// Estado do ataque de dicionario contra MIFARE Classic (v0.5)
typedef struct {
    MfClassicType type;
    uint8_t total_sectors;

    // Progresso atual
    uint8_t current_sector;
    MfClassicKeyType current_key_type;
    size_t current_key_idx;

    // Resultados acumulados
    bool sector_a_found[CHAOSID_MAX_SECTORS];
    bool sector_b_found[CHAOSID_MAX_SECTORS];
    MfClassicKey found_key_a[CHAOSID_MAX_SECTORS];
    MfClassicKey found_key_b[CHAOSID_MAX_SECTORS];
    uint8_t keys_found;
    uint8_t sectors_fully_cracked;

    // Controle de thread
    FuriThread* thread;
    bool stop;
    bool failed;
    bool complete;
} AttackContext;

typedef struct {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;

    Submenu* submenu;
    Widget* widget;
    Popup* popup;

    Storage* storage;

    FuriTimer* scan_timer;
    uint8_t scan_phase;

    // LF worker (app-lifetime)
    LFRFIDWorker* lf_worker;
    ProtocolDict* lf_dict;
    bool lf_done;

    // HF scanner (app-lifetime nfc, per-scan scanner)
    Nfc* nfc;
    NfcScanner* nfc_scanner;
    bool hf_done;
    NfcProtocol detected_protocol;

    const CardProfile* last_result;
    char uid_buffer[48];
    char card_data_buffer[48];

    AttackContext attack;
} ChaosIdApp;
