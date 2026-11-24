/**
 * Encodeur POCSAG (CCIR Rec. 584) — aucune dépendance au firmware,
 * compilable et testable sur PC (voir test/test_pocsag.c).
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define POCSAG_SYNC          0x7CD215D8u
#define POCSAG_IDLE          0x7A89C197u
#define POCSAG_PREAMBLE_BITS 576u
#define POCSAG_RIC_MAX       0x1FFFFFu /* 21 bits */
#define POCSAG_MSG_MAX       80u
/* 80 car. alpha = 28 mots + adresse + 14 idles max -> 3 lots, marge à 4 lots (4 x 17) */
#define POCSAG_MAX_WORDS     68u

typedef enum {
    PocsagNumeric,
    PocsagAlpha,
} PocsagType;

/** Complète un mot de code : calcule le BCH(31,21) (g = 0x769) sur les bits 31..11
 *  et la parité paire (bit 0). Les bits 10..0 en entrée sont ignorés. */
uint32_t pocsag_bch(uint32_t cw);

/** true si le caractère est encodable dans le type donné. */
bool pocsag_char_valid(PocsagType type, char c);

/** Encode un message complet : SYNC + lots de 16 mots, terminé par au moins un IDLE.
 *  Le préambule n'est pas inclus (généré à l'émission).
 *  @param func bits de fonction (0..3) ; usage courant : 0 numérique, 3 alpha
 *  @return nombre de mots écrits dans out, 0 si paramètre invalide ou buffer trop petit */
size_t pocsag_encode(
    uint32_t ric,
    uint8_t func,
    PocsagType type,
    const char* msg,
    uint32_t* out,
    size_t max);
