#include "transit/navigo_i.h"
#include "transit/ravkav_i.h"
#include "transit/opus_i.h"
#include <furi.h>

#ifndef CALYPSO_I_H
#define CALYPSO_I_H
#define CALYPSO_MAX_FILE_COUNT (10U)
#define CALYPSO_MAX_FILE_SIZE (30U)

#include <lib/flipper_format/flipper_format.h>

typedef enum {
    CALYPSO_CARD_MOBIB,
    CALYPSO_CARD_OPUS,
    CALYPSO_CARD_VIVA,
    CALYPSO_CARD_PASSPASS,
    CALYPSO_CARD_TAM,
    CALYPSO_CARD_TRANSPOLE,
    CALYPSO_CARD_OURA,
    CALYPSO_CARD_NAVIGO,
    CALYPSO_CARD_KORRIGO,
    CALYPSO_CARD_TISSEO,
    CALYPSO_CARD_ENVIBUS,
    CALYPSO_CARD_GIRONDE,
    CALYPSO_CARD_RAVKAV,
    CALYPSO_CARD_UNKNOWN
} CALYPSO_CARD_TYPE;

/*
typedef struct {
    uint8_t data[30]; // max bytes per file 30 
} CalypsoFile;

typedef struct {
    uint16_t aid; 
    CalypsoFile files[10]; // max files per app up to 10
} CalypsoApp;
*/
typedef struct {
    NavigoCardData* navigo;
    OpusCardData* opus;
    RavKavCardData* ravkav;

    CALYPSO_CARD_TYPE card_type;
    unsigned int card_number;

    int contracts_count;
    int events_count;
    int special_events_count;

    int country_num;
    int network_num;
    //CalypsoApp apps[10]; // max apps to 10
    //size_t app_count;
} CalypsoCardData;

typedef struct {
    CalypsoCardData* card;
    int page_id;
    // mutex
    FuriMutex* mutex;
} CalypsoContext;

#endif // CALYPSO_I_H
