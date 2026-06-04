// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Cafe com Solda / Cristiano Santana
//
// This file is part of ChaosID.
// ChaosID is free software: you can redistribute it and/or modify it under
// the terms of the GNU General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option)
// any later version. See <https://www.gnu.org/licenses/gpl-3.0.html>.

#include "cards_db.h"
#include <furi_hal.h>
#include <string.h>

const CardProfile cards_db[] = {
    // ===== LF (125 kHz) =====
    {
        .protocol = "EM4100",
        .frequency = CardFreqLF,
        .typical_use = "Controle de acesso predial basico",
        .crypto = "Nenhuma (UID em claro)",
        .attack_vector = "Clone direto p/ T5577",
        .year_broken = "N/A",
        .risk = CardRiskTrivial,
        .notes = "Read-only de fabrica. UID de 40 bits. Onipresente em condominios.",
    },
    {
        .protocol = "HID Prox (H10301)",
        .frequency = CardFreqLF,
        .typical_use = "Cracha corporativo (legado)",
        .crypto = "Nenhuma (formato proprietario)",
        .attack_vector = "Clone p/ T5577, brute force facility code",
        .year_broken = "2007 (DEFCON)",
        .risk = CardRiskTrivial,
        .notes = "26 bits = 8 facility + 16 card. Sem autenticacao.",
    },
    {
        .protocol = "Indala",
        .frequency = CardFreqLF,
        .typical_use = "Acesso corporativo legado",
        .crypto = "Nenhuma (PSK1 modulation)",
        .attack_vector = "Clone direto, sniff a distancia",
        .year_broken = "N/A",
        .risk = CardRiskTrivial,
        .notes = "Variantes 26, 27 e 64 bits. Comum em sistemas antigos.",
    },
    {
        .protocol = "AWID",
        .frequency = CardFreqLF,
        .typical_use = "Cracha corporativo",
        .crypto = "Nenhuma",
        .attack_vector = "Clone p/ T5577",
        .year_broken = "N/A",
        .risk = CardRiskTrivial,
        .notes = "Formatos 26 e 50 bits. Sem rolling code.",
    },
    {
        .protocol = "T5577",
        .frequency = CardFreqLF,
        .typical_use = "Cartao em branco / emulador",
        .crypto = "Password opcional (32 bits)",
        .attack_vector = "Brute force de password",
        .year_broken = "N/A",
        .risk = CardRiskTrivial,
        .notes = "O 'cassete virgem' do mundo LF. Emula EM, HID, Indala, AWID.",
    },

    // ===== HF (13.56 MHz) =====
    {
        .protocol = "MIFARE Classic 1K",
        .frequency = CardFreqHF,
        .typical_use = "Transporte publico, cracha, hotel",
        .crypto = "Crypto1 (NXP, proprietaria)",
        .attack_vector = "Nested / Darkside / MFKey32 / Hardnested",
        .year_broken = "2008 (Nohl & Plotz)",
        .risk = CardRiskBroken,
        .notes = "16 setores x 4 blocos. Bilhete unico SP/RJ, VEM Recife.",
    },
    {
        .protocol = "MIFARE Classic 4K",
        .frequency = CardFreqHF,
        .typical_use = "Cracha corporativo de alta capacidade",
        .crypto = "Crypto1",
        .attack_vector = "Nested / Hardnested",
        .year_broken = "2008",
        .risk = CardRiskBroken,
        .notes = "40 setores. Mesma quebra do 1K, dump demora mais.",
    },
    {
        .protocol = "MIFARE Ultralight",
        .frequency = CardFreqHF,
        .typical_use = "Tickets descartaveis, eventos",
        .crypto = "Nenhuma",
        .attack_vector = "Leitura/escrita direta",
        .year_broken = "N/A",
        .risk = CardRiskTrivial,
        .notes = "64 bytes EEPROM. Sem autenticacao. Counter monotonico opcional.",
    },
    {
        .protocol = "MIFARE Ultralight C",
        .frequency = CardFreqHF,
        .typical_use = "Tickets de maior valor",
        .crypto = "3DES (chave de 16 bytes)",
        .attack_vector = "Brute force se chave default; sniff handshake",
        .year_broken = "Parcial (chaves default)",
        .risk = CardRiskBroken,
        .notes = "192 bytes. Autenticacao opcional, frequentemente desativada.",
    },
    {
        .protocol = "NTAG213/215/216",
        .frequency = CardFreqHF,
        .typical_use = "NFC tags, marketing, Amiibo",
        .crypto = "Password 32 bits opcional",
        .attack_vector = "Brute force password, leitura direta sem pwd",
        .year_broken = "N/A",
        .risk = CardRiskTrivial,
        .notes = "144/504/888 bytes. Amiibo usa NTAG215.",
    },
    {
        .protocol = "MIFARE DESFire EV1",
        .frequency = CardFreqHF,
        .typical_use = "Transporte, cracha governamental",
        .crypto = "AES-128 / 3DES",
        .attack_vector = "Side channel (lab); chaves fracas",
        .year_broken = "Side-channel apenas",
        .risk = CardRiskHardened,
        .notes = "App-based, ate 28 apps. Se mal configurado, vira MIFARE Classic.",
    },
    {
        .protocol = "MIFARE DESFire EV2/EV3",
        .frequency = CardFreqHF,
        .typical_use = "Pagamento, transporte, ID nacional",
        .crypto = "AES-128, Secure Messaging",
        .attack_vector = "Nenhum publico viavel",
        .year_broken = "-",
        .risk = CardRiskHardened,
        .notes = "Estado da arte em MIFARE. EV3 adiciona transaction MAC.",
    },
    {
        .protocol = "iCLASS Standard",
        .frequency = CardFreqHF,
        .typical_use = "Cracha HID corporativo",
        .crypto = "3DES com chave mestra HID",
        .attack_vector = "Chave mestra vazou (Meriac 2010)",
        .year_broken = "2010",
        .risk = CardRiskBroken,
        .notes = "Reader keys publicas. iCLASS SE/SR sao mais seguros.",
    },
    {
        .protocol = "FeliCa (Sony)",
        .frequency = CardFreqHF,
        .typical_use = "Transporte JP (Suica, Pasmo), pgto",
        .crypto = "Proprietaria (DES-based)",
        .attack_vector = "Nenhum publico relevante",
        .year_broken = "-",
        .risk = CardRiskHardened,
        .notes = "Dominante no Japao. Flipper le UID e service codes.",
    },
    {
        .protocol = "ISO15693 (Vicinity)",
        .frequency = CardFreqHF,
        .typical_use = "Bibliotecas, controle de estoque",
        .crypto = "Geralmente nenhuma",
        .attack_vector = "Leitura/escrita direta",
        .year_broken = "N/A",
        .risk = CardRiskTrivial,
        .notes = "Alcance maior (~1m). ICODE SLIX e variante comum.",
    },
};

const size_t cards_db_size = sizeof(cards_db) / sizeof(cards_db[0]);

const CardProfile* cards_db_pick_random_for_freq(CardFrequency freq) {
    // POC: sorteia uma entrada da frequencia indicada.
    // TODO: substituir por match real do protocolo detectado pelos workers.
    size_t matches[cards_db_size];
    size_t count = 0;
    for(size_t i = 0; i < cards_db_size; i++) {
        if(cards_db[i].frequency == freq) {
            matches[count++] = i;
        }
    }
    if(count == 0) return NULL;
    uint32_t r = furi_hal_random_get() % count;
    return &cards_db[matches[r]];
}

/* Mapeia nomes de protocolo emitidos pelo lfrfid_worker (firmware Momentum)
 * para entradas do nosso DB. Alguns nomes do firmware podem cair na mesma
 * entrada do DB (ex: HIDProx e H10301 sao ambos HID Prox 26-bit pra nos). */
const CardProfile* cards_db_find_by_lfrfid_name(const char* name) {
    if(!name) return NULL;

    static const struct {
        const char* fw_name;
        const char* db_protocol;
    } name_map[] = {
        {"EM4100", "EM4100"},
        {"EM410032", "EM4100"},
        {"EM410016", "EM4100"},
        {"H10301", "HID Prox (H10301)"},
        {"HIDProx", "HID Prox (H10301)"},
        {"HIDExt", "HID Prox (H10301)"},
        {"Indala26", "Indala"},
        {"AWID", "AWID"},
    };
    const size_t map_size = sizeof(name_map) / sizeof(name_map[0]);

    const char* db_protocol = NULL;
    for(size_t i = 0; i < map_size; i++) {
        if(strcmp(name, name_map[i].fw_name) == 0) {
            db_protocol = name_map[i].db_protocol;
            break;
        }
    }
    if(!db_protocol) return NULL;

    for(size_t i = 0; i < cards_db_size; i++) {
        if(strcmp(cards_db[i].protocol, db_protocol) == 0) {
            return &cards_db[i];
        }
    }
    return NULL;
}

/* Mapeia nome de protocolo emitido pelo nfc_device_get_protocol_name()
 * para entradas do DB. Match por substring (com strstr) pra cobrir variacoes
 * de nomenclatura entre firmwares. Ordem importa: o primeiro match vence,
 * entao subtipos mais especificos vem antes dos genericos. */
const CardProfile* cards_db_find_by_nfc_name(const char* name) {
    if(!name) return NULL;

    static const struct {
        const char* fw_substring;
        const char* db_protocol;
    } name_map[] = {
        // Mais especifico primeiro
        {"DESFire", "MIFARE DESFire EV1"},
        {"Ultralight C", "MIFARE Ultralight C"},
        {"Ultralight", "MIFARE Ultralight"},
        {"NTAG", "NTAG213/215/216"},
        {"Mifare Plus", "MIFARE Classic 1K"}, // Plus em SL1 e' Classic
        {"Mifare Classic", "MIFARE Classic 1K"}, // default 1K, sem refinamento
        {"FeliCa", "FeliCa (Sony)"},
        {"ISO15693", "ISO15693 (Vicinity)"},
        {"iClass", "iCLASS Standard"},
        // Fallback generico no fim - varios subtipos sao baseados em 14443-3A
        {"ISO14443-3a", "MIFARE Classic 1K"},
        {"ISO14443-3A", "MIFARE Classic 1K"},
    };
    const size_t map_size = sizeof(name_map) / sizeof(name_map[0]);

    for(size_t i = 0; i < map_size; i++) {
        if(strstr(name, name_map[i].fw_substring) != NULL) {
            for(size_t j = 0; j < cards_db_size; j++) {
                if(strcmp(cards_db[j].protocol, name_map[i].db_protocol) == 0) {
                    return &cards_db[j];
                }
            }
        }
    }
    return NULL;
}

const char* card_risk_label(CardRisk risk) {
    switch(risk) {
    case CardRiskTrivial:
        return "TRIVIAL - clonavel";
    case CardRiskBroken:
        return "QUEBRADA - viavel";
    case CardRiskHardened:
        return "ENDURECIDA - ok";
    default:
        return "DESCONHECIDO";
    }
}

const char* card_frequency_label(CardFrequency freq) {
    switch(freq) {
    case CardFreqLF:
        return "125 kHz (LF)";
    case CardFreqHF:
        return "13.56 MHz (HF)";
    default:
        return "?";
    }
}
