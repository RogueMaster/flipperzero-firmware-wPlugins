#include "pocsag_tx.h"
#include "pocsag.h"

#include <furi.h>
#include <furi_hal_subghz.h>
#include <cc1101_regs.h>
#include <lib/subghz/devices/cc1101_configs.h>
#include <toolbox/level_duration.h>

const PocsagFreq pocsag_freqs[] = {
    /* SRD europeennes : emission autorisee (10 mW / 500 mW). */
    {434500000, "434.500", false}, /* 434.04-434.79 : 10 mW, 100 %, canal <= 25 kHz */
    {433920000, "433.920", true}, /* 433.05-434.79 : 10 mW, <= 10 % */
    {869525000, "869.525", true}, /* 869.40-869.65 : <= 500 mW, <= 10 % */
    /* Radiomessagerie sous licence FR (Tatoo / e*Message), hors plage CC1101 */
    {466175000, "466.17500", false},
    {466206250, "466.20625", false},
    {466025000, "466.02500", false},
    {466050000, "466.05000 Tatoo", false},
    {466075000, "466.07500", false},
    {466200000, "466.20000", false},
    {466212500, "466.21250", false},
    {466231250, "466.23125", false},
};
const size_t pocsag_freqs_count = COUNT_OF(pocsag_freqs);

/* Déviation = 26 MHz / 2^17 * (8 + M) * 2^E. 0x13 (E=1, M=3) -> 4.36 kHz, le plus proche de 4.5 kHz.
 * Pour essayer 4.76 kHz (récepteur peu sensible), passer à 0x14. */
#define POCSAG_DEVIATN 0x13

typedef struct {
    bool active;
    const uint32_t* cw;
    size_t total_bits; /* préambule + 32 * n */
    size_t k; /* prochain bit */
    uint16_t baud;
    bool invert;
} PocsagTx;

static PocsagTx tx;
static uint8_t preset[128];

/* Copie du preset 2-FSK async du firmware avec la déviation POCSAG. */
static void build_preset(void) {
    const uint8_t* src = subghz_device_cc1101_preset_2fsk_dev2_38khz_async_regs;
    size_t i = 0;
    for(; src[i] && i + 2 < sizeof(preset) - 10; i += 2) {
        preset[i] = src[i];
        preset[i + 1] = src[i] == CC1101_DEVIATN ? POCSAG_DEVIATN : src[i + 1];
    }
    memcpy(&preset[i], &src[i], 10); /* terminateur 0,0 + PATABLE[8] (+10 dBm) */
}

static LevelDuration tx_callback(void* ctx) {
    UNUSED(ctx);
    if(tx.k >= tx.total_bits) return level_duration_reset();

    size_t k = tx.k++;
    bool bit;
    if(k < POCSAG_PREAMBLE_BITS) {
        bit = !(k & 1u); /* 1010... */
    } else {
        k -= POCSAG_PREAMBLE_BITS;
        bit = (tx.cw[k / 32] >> (31 - k % 32)) & 1u;
    }
    /* Accumulateur exact : durée = t(k+1) - t(k), sans dérive malgré la résolution de 1 µs. */
    uint32_t d = (uint32_t)((uint64_t)tx.k * 1000000u / tx.baud -
                            (uint64_t)(tx.k - 1) * 1000000u / tx.baud);
    /* Par défaut, "1" logique = fréquence basse (GDO0 bas). */
    return level_duration_make(bit == tx.invert, d);
}

static const char* tx_err;

const char* pocsag_tx_last_error(void) {
    return tx_err ? tx_err : "Erreur inconnue";
}

/* API bas niveau furi_hal_subghz directement : pas de registre subghz_devices,
 * donc pas de begin()/init() susceptibles d'échouer. La radio est prête dès le boot. */
bool pocsag_tx_start(uint32_t freq, uint16_t baud, bool invert, const uint32_t* cw, size_t n) {
    furi_check(!tx.active);
    tx_err = NULL;
    build_preset();

    if(!furi_hal_subghz_is_frequency_valid(freq)) {
        tx_err = "Frequence hors plage\nmateriel (CC1101)";
        return false;
    }

    furi_hal_subghz_reset();
    furi_hal_subghz_idle();
    furi_hal_subghz_load_custom_preset(preset);
    furi_hal_subghz_flush_tx();
    furi_hal_subghz_set_frequency_and_path(
        freq); /* fixe la regulation selon la region + antenne */

    /* Peuplé avant start_async_tx : le callback peut être appelé dès l'armement du buffer. */
    tx = (PocsagTx){
        .active = true,
        .cw = cw,
        .total_bits = POCSAG_PREAMBLE_BITS + n * 32,
        .baud = baud,
        .invert = invert,
    };

    if(!furi_hal_subghz_start_async_tx(tx_callback, NULL)) {
        tx_err = "Region: TX interdit\nsur cette frequence";
        furi_hal_subghz_idle();
        furi_hal_subghz_sleep();
        tx.active = false;
        return false;
    }
    return true;
}

bool pocsag_tx_is_done(void) {
    return !tx.active || furi_hal_subghz_is_async_tx_complete();
}

void pocsag_tx_stop(void) {
    if(!tx.active) return;
    furi_hal_subghz_stop_async_tx();
    furi_hal_subghz_idle();
    furi_hal_subghz_sleep();
    tx.active = false;
}
