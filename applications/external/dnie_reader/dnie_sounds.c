#include "dnie_sounds.h"

#include <notification/notification_messages.h>

DnieVerdict dnie_verdict(const DnieAuth* a) {
    if(dnie_verdict_reason(a)) return DnieVerdictFake;
    if(dnie_auth_all_ok(a)) return DnieVerdictOriginal;
    if(dnie_auth_data_ok(a)) return DnieVerdictGenuine;
    return DnieVerdictUnknown;
}

const char* dnie_verdict_reason(const DnieAuth* a) {
    if(dnie_auth_integrity_failed(a)) return "Data tampered:\nhashes don't match";
    if(a->pa.result == DniePaBadSignature) return "SOD signature\nis fake";
    if(a->csca.result == DnieCscaBadSignature) return "Certificate not\nissued by Police";
    if(a->ca.result == DnieCaFailed) return "Chip lacks the\nkey: possible CLONE";
    return NULL;
}

// ---- Melodias ----
// Se tocan con un hilo propio directamente en el altavoz (no por la cola de notificaciones):
// asi se pueden cortar al instante al salir y no bloquean la pantalla.

#include <furi_hal_speaker.h>
#include <furi_hal_rtc.h>
#include <storage/storage.h>
#include <string.h>

typedef struct {
    float freq; // 0 = silencio
    uint16_t ms;
} Note;

// Marcha Real (Espinosa, 1761, dominio publico). Melodia de la partitura para piano de
// Wikimedia Commons ("Marcha Real de España.pdf"), en Do mayor: frase A (con la 2a casilla)
// y frase B, sin repeticiones. Negra = 400 ms.
#define Q   400
#define G4_ 392.00f
#define A4_ 440.00f
#define B4_ 493.88f
#define C5_ 523.25f
#define D5_ 587.33f
#define E5_ 659.25f
#define F5_ 698.46f
#define G5_ 783.99f
static const Note ANTHEM_ES[] = {
    // A
    {C5_, Q},
    {G4_, Q},
    {E5_, Q},
    {C5_, Q / 2},
    {G5_, Q / 2},
    {F5_, Q / 2},
    {E5_, Q / 2},
    {D5_, Q / 2},
    {C5_, Q / 2},
    {C5_, Q / 2},
    {B4_, Q / 2},
    {A4_, Q / 2},
    {G4_, Q / 2},
    {C5_, Q},
    {D5_, Q},
    {E5_, Q},
    {0, Q / 2},
    {G5_, Q / 2},
    {F5_, Q / 2},
    {E5_, Q / 2},
    {D5_, Q / 2},
    {C5_, Q / 2},
    {G5_, Q * 2},
    // B
    {G5_, Q},
    {E5_, Q * 3 / 4},
    {G5_, Q / 4},
    {F5_, Q},
    {D5_, Q * 3 / 4},
    {F5_, Q / 4},
    {E5_, Q},
    {C5_, Q * 3 / 4},
    {E5_, Q / 4},
    {D5_, Q / 2},
    {G4_, Q / 2},
    {A4_, Q / 2},
    {B4_, Q / 2},
    {C5_, Q},
    {D5_, Q},
    {E5_, Q * 3 / 4},
    {F5_, Q / 4},
    {G5_, Q * 3 / 4},
    {F5_, Q / 4},
    {E5_, Q},
    {D5_, Q},
    {C5_, Q * 2},
};

#include "dnie_anthems.inc"

// Pais sin himno en la app: fanfarria corta
static const Note FANFARE[] = {
    {523.25f, 150},
    {659.25f, 150},
    {783.99f, 150},
    {1046.50f, 450},
    {783.99f, 150},
    {1046.50f, 600},
};

// "Wah wah wah waaah": cromatico descendente y grave
static const Note FAKE[] = {
    {196.00f, 300},
    {185.00f, 300},
    {174.61f, 300},
    {164.81f, 1000},
};

#define ANTHEM(cc, code) {cc, ANTHEM_##code, sizeof(ANTHEM_##code) / sizeof(Note)}
static const struct {
    const char* issuer; // emisor de la MRZ (3 letras; Alemania es "D")
    const Note* notes;
    size_t n;
} ANTHEMS[] = {
    ANTHEM("ESP", ES),
    ANTHEM("USA", US),
    ANTHEM("FRA", FR),
    ANTHEM("GBR", GB),
    ANTHEM("CAN", CA),
    ANTHEM("JPN", JP),
    ANTHEM("CHN", CN),
    ANTHEM("IDN", ID),
    ANTHEM("D", DE),
    ANTHEM("DEU", DE),
    ANTHEM("ITA", IT),
    ANTHEM("NLD", NL),
};

typedef struct {
    FuriThread* thread;
    const Note* notes;
    size_t n;
    volatile bool stop;
} Player;

static Player player;

// Volumen del himno: 0 = apagado ... 4 = 100 %. Se guarda en la SD.
#define VOLUME_PATH EXT_PATH("apps_data/dnie_reader/volume")
static uint8_t volume_level = 4;
static const float VOLUMES[] = {0.0f, 0.25f, 0.5f, 0.75f, 1.0f};

uint8_t dnie_sound_volume(void) {
    return volume_level;
}

void dnie_sound_volume_load(void) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* f = storage_file_alloc(storage);
    uint8_t v;
    if(storage_file_open(f, VOLUME_PATH, FSAM_READ, FSOM_OPEN_EXISTING) &&
       storage_file_read(f, &v, 1) == 1 && v <= 4)
        volume_level = v;
    storage_file_close(f);
    storage_file_free(f);
    furi_record_close(RECORD_STORAGE);
}

void dnie_sound_volume_set(uint8_t level) {
    if(level > 4) level = 4;
    volume_level = level;
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, EXT_PATH("apps_data"));
    storage_simply_mkdir(storage, EXT_PATH("apps_data/dnie_reader"));
    File* f = storage_file_alloc(storage);
    if(storage_file_open(f, VOLUME_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS))
        storage_file_write(f, &level, 1);
    storage_file_close(f);
    storage_file_free(f);
    furi_record_close(RECORD_STORAGE);
}

static int32_t player_thread(void* ctx) {
    UNUSED(ctx);
    if(!furi_hal_speaker_acquire(1000)) return 0;
    for(size_t i = 0; i < player.n && !player.stop; i++) {
        const Note* nt = &player.notes[i];
        if(nt->freq > 0) furi_hal_speaker_start(nt->freq, VOLUMES[volume_level]);
        // Esperar en trozos cortos para poder cortar enseguida
        uint32_t left = nt->ms > 30 ? nt->ms - 30 : nt->ms;
        while(left && !player.stop) {
            uint32_t d = left > 20 ? 20 : left;
            furi_delay_ms(d);
            left -= d;
        }
        furi_hal_speaker_stop();
        if(!player.stop) furi_delay_ms(30); // separa notas repetidas
    }
    furi_hal_speaker_stop();
    furi_hal_speaker_release();
    return 0;
}

void dnie_sound_stop(void) {
    if(!player.thread) return;
    player.stop = true;
    furi_thread_join(player.thread);
    furi_thread_free(player.thread);
    player.thread = NULL;
}

static void play(const Note* notes, size_t n) {
    dnie_sound_stop();
    // Modo silencioso del Flipper: sin musica (las luces y la vibracion siguen)
    if(furi_hal_rtc_is_flag_set(FuriHalRtcFlagStealthMode) || volume_level == 0) return;
    player.notes = notes;
    player.n = n;
    player.stop = false;
    player.thread = furi_thread_alloc_ex("DnieMusic", 1024, player_thread, NULL);
    furi_thread_start(player.thread);
}

static const NotificationSequence seq_ok = {
    &message_display_backlight_on,
    &message_green_255,
    &message_vibro_on,
    &message_delay_100,
    &message_vibro_off,
    &message_delay_1000,
    &message_delay_1000,
    NULL,
};

static const NotificationSequence seq_fake = {
    &message_display_backlight_on,
    &message_red_255,
    &message_vibro_on,
    &message_delay_1000,
    &message_vibro_off,
    &message_delay_1000,
    NULL,
};

void dnie_sound_original(NotificationApp* n, const char* issuer) {
    notification_message(n, &seq_ok);
    for(size_t i = 0; i < sizeof(ANTHEMS) / sizeof(ANTHEMS[0]); i++) {
        if(issuer && strcmp(issuer, ANTHEMS[i].issuer) == 0) {
            play(ANTHEMS[i].notes, ANTHEMS[i].n);
            return;
        }
    }
    play(FANFARE, sizeof(FANFARE) / sizeof(FANFARE[0]));
}

void dnie_sound_preview(void) {
    play(FANFARE, sizeof(FANFARE) / sizeof(FANFARE[0]));
}

void dnie_sound_fake(NotificationApp* n) {
    notification_message(n, &seq_fake);
    play(FAKE, sizeof(FAKE) / sizeof(FAKE[0]));
}
