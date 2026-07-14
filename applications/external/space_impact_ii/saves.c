#include "saves.h"
#include <storage/storage.h>

#define SAVING_DIRECTORY  "/ext/apps/Games"
#define LEVEL_FILENAME    SAVING_DIRECTORY "/SpaceImpactIILevel.save"
#define TOPSCORE_FILENAME SAVING_DIRECTORY "/SpaceImpactIIScore.save"

/** Beolvassa a mentett szintet, ha az el lett mentve **/
void ReadSavedLevel(Uint8* Level) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    if(storage_file_open(file, LEVEL_FILENAME, FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_read(file, Level, sizeof(*Level));
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

/** Beolvassa a mentett legjobb pontszámokat a paraméterben kapott tömbbe, ha el lettek mentve **/
void ReadTopScore(unsigned int* Arr) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    if(storage_file_open(file, TOPSCORE_FILENAME, FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_read(file, Arr, SCORE_COUNT * sizeof(*Arr));
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

/** A bemenetként kapott szint számát menti el **/
void SaveLevel(Uint8 Level) {
    Storage* storage = furi_record_open(RECORD_STORAGE);

    if(storage_common_stat(storage, SAVING_DIRECTORY, NULL) == FSE_NOT_EXIST) {
        if(!storage_simply_mkdir(storage, SAVING_DIRECTORY)) {
            furi_record_close(RECORD_STORAGE);
            return;
        }
    }

    File* file = storage_file_alloc(storage);
    if(storage_file_open(file, LEVEL_FILENAME, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_write(file, &Level, sizeof(Level));
    }
    storage_file_close(file);
    storage_file_free(file);

    furi_record_close(RECORD_STORAGE);
}

/** A bemenetként kapott 10 elemû tömbbe úgy illeszti be a második paraméterben kapott elemet, hogy az csökkenõ sorrendû maradjon, majd kiírja fájlba **/
void PlaceTopScore(unsigned int* Arr, Uint16 Entry) {
    unsigned int *Start = Arr, *End = Arr + SCORE_COUNT;
    while(Arr != End) { /* A tömb elsõ olyan eleméntek keresése, ami az újnál kisebb */
        if(*Arr < Entry) {
            int j;
            for(j = 9; j >= Arr - Start; j--) /* A tömb hátralévõ részének továbbléptetése */
                Start[j] = Start[j - 1];
            *Arr = Entry; /* Az így keletkezett helyre mehet az új érték */
            Arr = End; /* Kilépés a ciklusból */
        } else
            ++Arr; /* Ha még nincs elég hátul az új pont beszúrásához, menjen tovább */
    }

    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(storage_common_stat(storage, SAVING_DIRECTORY, NULL) == FSE_NOT_EXIST) {
        if(!storage_simply_mkdir(storage, SAVING_DIRECTORY)) {
            furi_record_close(RECORD_STORAGE);
            return;
        }
    }

    File* file = storage_file_alloc(storage);
    if(storage_file_open(file, TOPSCORE_FILENAME, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_write(file, Start, SCORE_COUNT * sizeof(*Start));
    }
    storage_file_close(file);
    storage_file_free(file);

    furi_record_close(RECORD_STORAGE);
}
