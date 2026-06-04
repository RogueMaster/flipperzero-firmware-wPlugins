// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Cafe com Solda / Cristiano Santana

#pragma once

#include <furi.h>
#include <storage/storage.h>

#define CHAOS_ID_LOG_DIR  "/ext/apps_data/chaos_id"
#define CHAOS_ID_LOG_FILE "/ext/apps_data/chaos_id/scans.csv"

typedef struct {
    const char* freq_label; // ex: "125 kHz (LF)"
    const char* protocol; // ex: "EM4100"
    const char* uid; // ex: "53 00 1D 84 FF"
    const char* data; // ex: "FC: 029 Card: 34047" ou "ATQA: 0004 SAK: 08"
    const char* risk_label; // ex: "TRIVIAL - clonavel"
} ScanLogEntry;

/** Adiciona um scan ao log CSV. Cria diretorio e header automaticamente
 *  na primeira chamada. Timestamp e obtido via furi_hal_rtc. */
bool scan_log_append(Storage* storage, const ScanLogEntry* entry);

/** Le o log e formata pra display (multi-linha, mais recente embaixo).
 *  Retorna false se nao tem log ou esta vazio. */
bool scan_log_read_display(Storage* storage, FuriString* out);
