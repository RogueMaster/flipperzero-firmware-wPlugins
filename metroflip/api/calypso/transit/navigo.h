#include "../calypso_util.h"
#include "../cards/intercode.h"
#include "navigo_i.h"
#include <datetime.h>
#include <stdbool.h>
#include <furi.h>
#include <storage/storage.h>
#include <toolbox/stream/stream.h>
#include <toolbox/stream/file_stream.h>

#ifndef NAVIGO_H
#define NAVIGO_H

const char* get_navigo_type(int type);

char* get_navigo_station(
    int station_group_id,
    int station_id,
    int station_sub_id,
    int service_provider);

const char* get_navigo_sncf_train_line(int station_group_id);

const char* get_navigo_sncf_station(int station_group_id, int station_id);

const char* get_navigo_tram_line(int route_number);

typedef enum {
    NAVIGO_PROVIDER_SNCF = 2,
    NAVIGO_PROVIDER_RATP = 3,
    NAVIGO_PROVIDER_IDFM = 4,
    NAVIGO_PROVIDER_ORA = 8,
    NAVIGO_PROVIDER_VEOLIA_CSO = 115,
    NAVIGO_PROVIDER_VEOLIA_RBUS = 116,
    NAVIGO_PROVIDER_PHEBUS = 156,
    NAVIGO_PROVIDER_RATP_VEOLIA_NANTERRE = 175
} NAVIGO_SERVICE_PROVIDER;

#endif // NAVIGO_H
