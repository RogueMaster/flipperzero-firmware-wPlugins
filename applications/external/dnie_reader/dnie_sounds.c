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

#include "dnie_mem.h"
#include "dnie_plugin.h"

typedef DnieNote Note;

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

typedef struct {
    FuriThread* thread;
    const Note* notes;
    size_t n;
    DnieNote* owned; // copia del himno en el heap (el plugin ya esta descargado)
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
    free(player.owned);
    player.owned = NULL;
}

#define PLAYER_STACK 1024

static bool sound_muted(void) {
    return furi_hal_rtc_is_flag_set(FuriHalRtcFlagStealthMode) || volume_level == 0;
}

// owned: notas en el heap que pasan a ser del reproductor (se liberan al parar)
static bool play(const Note* notes, size_t n, DnieNote* owned) {
    dnie_sound_stop();
    // Modo silencioso del Flipper: sin musica (las luces y la vibracion siguen)
    if(sound_muted() || !dnie_mem_ok(PLAYER_STACK + 512)) {
        free(owned);
        return sound_muted();
    }
    player.notes = notes;
    player.n = n;
    player.owned = owned;
    player.stop = false;
    player.thread = furi_thread_alloc_ex("DnieMusic", PLAYER_STACK, player_thread, NULL);
    furi_thread_start(player.thread);
    return true;
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

bool dnie_sound_original(NotificationApp* n, const char* issuer) {
    notification_message(n, &seq_ok);
    dnie_sound_stop(); // libera las notas anteriores antes de cargar el plugin
    if(sound_muted()) return true;

    DniePlugin plugin;
    if(dnie_plugin_open(&plugin, DniePluginHimnos) != DniePluginOk) return false;
    const DnieHimnosApi* api = plugin.api;
    const DnieNote* notes = NULL;
    size_t count = api->find(issuer, &notes);
    // El himno se copia al heap: asi el plugin no sigue cargado mientras suena
    DnieNote* copy = count ? dnie_malloc(count * sizeof(DnieNote)) : NULL;
    if(copy) memcpy(copy, notes, count * sizeof(DnieNote));
    dnie_plugin_close(&plugin);

    if(!count) return play(FANFARE, COUNT_OF(FANFARE), NULL);
    return copy ? play(copy, count, copy) : false;
}

void dnie_sound_preview(void) {
    play(FANFARE, COUNT_OF(FANFARE), NULL);
}

void dnie_sound_fake(NotificationApp* n) {
    notification_message(n, &seq_fake);
    play(FAKE, COUNT_OF(FAKE), NULL);
}
