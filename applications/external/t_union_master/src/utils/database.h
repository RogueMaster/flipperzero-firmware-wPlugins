#pragma once

#include <furi.h>
#include <storage/storage.h>
#include "../protocol/t_union_msg.h"
#include "t_union_msgext.h"

void tum_db_get_card_type_name(TunionCardType card_type, FuriString* card_type_name);
void tum_db_get_line_type_name(LineType line_type, FuriString* line_type_name);

bool tum_db_query_card_name(Storage* storage, const char* card_number, FuriString* card_name);
bool tum_db_query_city_name(Storage* storage, const char* city_id, FuriString* city_name);
bool tum_db_query_line_label(
    Storage* storage,
    const char* city_id,
    const char* station_id,
    FuriString* line_label);
bool tum_db_query_line_name(
    Storage* storage,
    const char* city_id,
    const char* line_label,
    FuriString* line_name,
    LineType* line_type);
bool tum_db_query_station_name(
    Storage* storage,
    const char* city_id,
    const char* line_label,
    const char* station_id,
    FuriString* station_name);
