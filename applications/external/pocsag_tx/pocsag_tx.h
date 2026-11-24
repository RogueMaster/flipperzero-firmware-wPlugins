/** Émission POCSAG en 2-FSK via le CC1101 interne (mode asynchrone, TIM2/DMA, 1 µs). */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint32_t hz;
    const char* label;
    bool duty_10pct; /* bande limitée à 10 % de duty cycle (ERC/REC 70-03) */
} PocsagFreq;

/* Liste blanche, codée en dur : bandes SRD européennes uniquement. */
extern const PocsagFreq pocsag_freqs[];
extern const size_t pocsag_freqs_count;

/** Démarre l'émission (préambule + mots). Les mots doivent rester valides jusqu'à pocsag_tx_stop().
 *  @return false si la fréquence est refusée (matériel ou région) */
bool pocsag_tx_start(uint32_t freq, uint16_t baud, bool invert, const uint32_t* cw, size_t n);
/** Détail du dernier échec de pocsag_tx_start() (chaîne affichable). */
const char* pocsag_tx_last_error(void);
bool pocsag_tx_is_done(void);
/** Arrête (ou annule) l'émission et libère la radio. Sans effet si rien n'est en cours. */
void pocsag_tx_stop(void);
