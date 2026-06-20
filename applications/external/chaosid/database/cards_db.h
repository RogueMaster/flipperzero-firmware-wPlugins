// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Cafe com Solda / Cristiano Santana
//
// This file is part of ChaosID.
// ChaosID is free software: you can redistribute it and/or modify it under
// the terms of the GNU General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option)
// any later version. See <https://www.gnu.org/licenses/gpl-3.0.html>.

#pragma once

#include <stdint.h>
#include <stddef.h>

typedef enum {
    CardRiskTrivial, // clona em segundos com qualquer Flipper / Proxmark
    CardRiskBroken, // crypto broken, requires attack but feasible
    CardRiskHardened, // considerado seguro hoje (se bem implementado)
    CardRiskUnknown,
} CardRisk;

typedef enum {
    CardFreqLF, // 125 kHz
    CardFreqHF, // 13.56 MHz
} CardFrequency;

typedef struct {
    const char* protocol; // "MIFARE Classic 1K"
    CardFrequency frequency;
    const char* typical_use; // "Transit, corporate badge"
    const char* crypto; // "Crypto1 (proprietary, broken)"
    const char* attack_vector; // "Nested / Darkside / MFKey32"
    const char* year_broken; // "2008" ou "-"
    CardRisk risk;
    const char* notes; // observacoes extras
} CardProfile;

extern const CardProfile cards_db[];
extern const size_t cards_db_size;

const CardProfile* cards_db_pick_random_for_freq(CardFrequency freq);

/** Mapeia o nome de protocolo retornado por protocol_dict_get_name() do worker
 *  LFRFID para a entrada correspondente no cards_db. Retorna NULL se o
 *  protocolo nao estiver mapeado.
 */
const CardProfile* cards_db_find_by_lfrfid_name(const char* name);

/** Mapeia o nome de protocolo retornado por nfc_device_get_protocol_name()
 *  para a entrada correspondente no cards_db. Faz match por substring
 *  (case-sensitive) pra cobrir variacoes tipo "Mifare Classic" e
 *  "MIFARE Classic 1K". Retorna NULL se nao mapeado.
 */
const CardProfile* cards_db_find_by_nfc_name(const char* name);

const char* card_risk_label(CardRisk risk);
const char* card_frequency_label(CardFrequency freq);
