#include <math.h>
#include <string.h>
#include "audio.h"
#include "config.h"

#define PIx2 6.283185307179586476925286766559 /* Pi * 2 */

/** Hang létrehozása két frekvenciából **/
void FillStream(
    Sint16* stream,
    Sint32 freq_distort,
    Sint32 freq_sine,
    Sint32 playrem,
    Sint32 partrem) {
    Sint32 i;
    for(i = 0; i < (playrem < partrem ? playrem : partrem);
        i++) /* Hátralévő minták: vagy az effekt végéig menjen, vagy a callback végéig */
        stream[i] +=
            (sin((playrem - i) *
                 i / /* A varázslat (*i): felhúzza a frekvenciát minden előállításban */
                 /* Megjegyzendő, hogy a varázslat igazából csak egy teljesen véletlen "mi lenne, ha..." próbálkozás eredménye.
               Aztán véletlenül pont azt a hangot adta ki, mint az eredeti játék. Ki gondolta volna? */
                 // TODO: double calculation here?
                 SAMPLE_RATE * PIx2 *
                 freq_distort) + /* Szinuszhullám előállítása: sin(idő*2*pi*frekvencia) */
             sin((playrem - i) / SAMPLE_RATE * PIx2 * freq_sine)) *
            VOLUME; /* Ugyanígy a másik komponenst is, csak varázslat nélkül */
}

/** Hangmixer: amelyik flag épp be van állítva, azt rámixeli a kimenetre **/
void AudioCallback(void* flags, Uint8* stream8, int len) {
    Sint16* stream = (Sint16*)
        stream8; /* Kimenet castolása a megfelelő típusba, mert alapvetően az SDL nem ilyen formátumot használ callback-re */
    static Sint32 btnrem = 0, shotrem = 0, bwpnrem = 0,
                  deathrem = 0; /* Hátralévő minták mindenféle hangból */
    memset(stream8, 0, len); /* Hang ürítése */
    len /= sizeof(Sint16); /* A hosszt bájtokban adta volna meg */
    if(*(Sint32*)flags & SOUND_MENUBTN) /* Ha van menüben lenyomott gomb */
        btnrem = 2 * SAMPLE_RATE / FRAMERATE; /* 2 képkocka idejére adja ki */
    if(*(Sint32*)flags & SOUND_SHOT) /* Ha van lövéshang */
        shotrem =
            3 * SAMPLE_RATE /
            FRAMERATE; /* 3 képkocka idejére adja ki (ne folyjon egymásba két lövés hangja, akármekkora is a képfrissítés) */
    if(*(Sint32*)flags & SOUND_BONUSWPN) /* Ha van bónuszfegyver-hang */
        bwpnrem = 4 * SAMPLE_RATE / FRAMERATE; /* 4 képkocka idejére adja ki */
    if(*(Sint32*)flags & SOUND_DEATH) /* Ha van halálhang */
        deathrem = 6 * SAMPLE_RATE / FRAMERATE; /* 6 képkocka idejére adja ki */
    *((Sint32*)flags) = 0; /* Flagek nullázása, ne maradjon meg a következő callback-re */
/* A hangok egymást kioltják egy prioritási sor szerint */
#ifdef LEGACY_AUDIO /* Ha az eredeti hangleképezés be van állítva */
    if(btnrem > 0) /* A gombnyomáshang a legfontosabb */
        FillStream(
            stream,
            0,
            BUTTON_FREQ,
            btnrem,
            len); /* A gombnyomáshanghoz csak a szinuszos leképezés kell */
    else if(deathrem > 0) /* A halál hangja a játékon belül a legfontosabb */
        FillStream(stream, DEATH_FREQ_DISTORT, DEATH_FREQ_CONTINOUS, deathrem, len);
    else if(bwpnrem > 0) /* Közepes prioritást kap a bónuszfegyverek hangja */
        FillStream(stream, BONUS_FREQ_DISTORT, BONUS_FREQ_CONTINOUS, bwpnrem, len);
    else if(shotrem > 0) /* A lövéshang a legkevésbé fontos */
        FillStream(stream, SHOT_FREQ_DISTORT, SHOT_FREQ_CONTINOUS, shotrem, len);
#else
    FillStream(
        stream,
        0,
        BUTTON_FREQ,
        btnrem,
        len); /* Gombnyomáshang lejátszása (a ha van feltételt a hossz fedezi ezeknél) */
    FillStream(
        stream, SHOT_FREQ_DISTORT, SHOT_FREQ_CONTINOUS, shotrem, len); /* Lövéshang lejátszása */
    FillStream(
        stream,
        BONUS_FREQ_DISTORT,
        BONUS_FREQ_CONTINOUS,
        bwpnrem,
        len); /* Bónuszfegyver-hang lejátszása */
    FillStream(
        stream, DEATH_FREQ_DISTORT, DEATH_FREQ_CONTINOUS, deathrem, len); /* Halálhang lejátszása */
#endif
    /* Minden hang hátralévő idejének csökkentése, ha nem szólalt meg, akkor is */
    /* Igen, így alulcsordulhat, de ahhoz >12 óra kell, inkább spóroljunk a bájtokkal, minthogy egy ennyire kis valószínűségű eseményre készüljünk */
    btnrem -= len;
    shotrem -= len;
    bwpnrem -= len;
    deathrem -= len;
}
