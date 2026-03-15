#include "navigo.h"
#include "navigo_lists.h"
#include "../../../metroflip_i.h"
#include "../../metroflip/metroflip_api.h"

const char* get_navigo_service_provider(int provider) {
    switch(provider) {
    case NAVIGO_PROVIDER_SNCF:
        return "SNCF";
    case NAVIGO_PROVIDER_RATP:
        return "RATP";
    case 4:
    case 10:
        return "IDF Mobilites";
    case NAVIGO_PROVIDER_ORA:
        return "ORA";
    case NAVIGO_PROVIDER_VEOLIA_CSO:
        return "CSO (VEOLIA)";
    case NAVIGO_PROVIDER_VEOLIA_RBUS:
        return "R'Bus (VEOLIA)";
    case NAVIGO_PROVIDER_PHEBUS:
        return "Phebus";
    case NAVIGO_PROVIDER_RATP_VEOLIA_NANTERRE:
        return "RATP (Veolia Transport Nanterre)";
    default: {
        char* provider_str = malloc(6 * sizeof(char));
        snprintf(provider_str, 6, "%d", provider);
        return provider_str;
    }
    }
}

const char* get_navigo_tariff(int tariff) {
    switch(tariff) {
    case 0x0000:
        return "Navigo Mois";
    case 0x0001:
        return "Navigo Semaine";
    case 0x0002:
        return "Navigo Annuel";
    case 0x0003:
        return "Navigo Jour";
    case 0x0004:
        return "Imagine R Junior";
    case 0x0005:
        return "Imagine R Etudiant";
    case 0x000D:
        return "Navigo Jeunes Week-end";
    case 0x0015:
        return "Paris-Visite"; // Theoric
    case 0x1000:
        return "Navigo Liberte+";
    case 0x4000:
        return "Navigo Mois 75%%";
    case 0x4001:
        return "Navigo Semaine 75%%";
    case 0x4015:
        return "Paris-Visite (Enfant)"; // Theoric
    case 0x5000:
        return "Tickets T+";
    case 0x5004:
        return "Tickets OrlyBus"; // Theoric
    case 0x5005:
        return "Tickets RoissyBus"; // Theoric
    case 0x5006:
        return "Bus-Tram"; // Theoric
    case 0x5008:
        return "Metro-Train-RER"; // Theoric
    case 0x500b:
        return "Paris <> Aeroports"; // Theoric
    case 0x5010:
        return "Tickets T+ (Reduit)"; // Theoric
    case 0x5016:
        return "Bus-Tram (Reduit)"; // Theoric
    case 0x5018:
        return "Metro-Train-RER (Reduit)"; // Theoric
    case 0x501b:
        return "Paris <> Aeroports (Reduit)"; // Theoric
    case 0x8003:
        return "Navigo Solidarite Gratuit";
    default: {
        char* tariff_str = malloc(6 * sizeof(char));
        snprintf(tariff_str, 6, "%d", tariff);
        return tariff_str;
    }
    }
}

const char* get_zones(int* zones) {
    if(zones[0] && zones[4]) {
        return "All Zones (1-5)";
    } else if(zones[0] && zones[3]) {
        return "Zones 1-4";
    } else if(zones[0] && zones[2]) {
        return "Zones 1-3";
    } else if(zones[0] && zones[1]) {
        return "Zones 1-2";
    } else if(zones[0]) {
        return "Zone 1";
    } else if(zones[1] && zones[4]) {
        return "Zones 2-5";
    } else if(zones[1] && zones[3]) {
        return "Zones 2-4";
    } else if(zones[1] && zones[2]) {
        return "Zones 2-3";
    } else if(zones[1]) {
        return "Zone 2";
    } else if(zones[2] && zones[4]) {
        return "Zones 3-5";
    } else if(zones[2] && zones[3]) {
        return "Zones 3-4";
    } else if(zones[2]) {
        return "Zone 3";
    } else if(zones[3] && zones[4]) {
        return "Zones 4-5";
    } else if(zones[3]) {
        return "Zone 4";
    } else if(zones[4]) {
        return "Zone 5";
    } else {
        return "Unknown";
    }
}

char* get_token(char* psrc, const char* delimit, void* psave) {
    static char sret[512];
    register char* ptr = psave;
    memset(sret, 0, sizeof(sret));

    if(psrc != NULL) strcpy(ptr, psrc);
    if(ptr == NULL) return NULL;

    int i = 0, nlength = strlen(ptr);
    for(i = 0; i < nlength; i++) {
        if(ptr[i] == delimit[0]) break;
        if(ptr[i] == delimit[1]) {
            ptr = NULL;
            break;
        }
        sret[i] = ptr[i];
    }
    if(ptr != NULL) strcpy(ptr, &ptr[i + 1]);

    return sret;
}

char* get_navigo_station(
    int station_group_id,
    int station_id,
    int station_sub_id,
    int transport_type) {
    switch(transport_type) {
    case COMMUTER_TRAIN: {
        if(station_group_id < 77 && station_id < 19) {
            char* file_path = malloc(256 * sizeof(char));
            if(!file_path) {
                return "Unknown";
            }
            snprintf(
                file_path,
                256,
                APP_ASSETS_PATH("navigo/stations/train/stations_%d.txt"),
                station_group_id);
            const char* sncf_stations_path = file_path;
            Storage* storage = furi_record_open(RECORD_STORAGE);

            Stream* stream = file_stream_alloc(storage);
            FuriString* line = furi_string_alloc();

            char* found_station_name = NULL;

            if(file_stream_open(stream, sncf_stations_path, FSAM_READ, FSOM_OPEN_EXISTING)) {
                while(stream_read_line(stream, line)) {
                    // file is in csv format: station_id,station_sub_id,station_name
                    // search for the station
                    furi_string_replace_all(line, "\r", "");
                    furi_string_replace_all(line, "\n", "");
                    const char* string_line = furi_string_get_cstr(line);
                    char* string_line_copy = strdup(string_line);
                    if(!string_line_copy) {
                        return "Unknown";
                    }
                    int line_station_id = atoi(get_token(string_line_copy, ",", string_line_copy));
                    int line_station_sub_id =
                        atoi(get_token(string_line_copy, ",", string_line_copy));
                    if(line_station_id == station_id && line_station_sub_id == station_sub_id) {
                        found_station_name =
                            strdup(get_token(string_line_copy, ",", string_line_copy));
                        free(string_line_copy);
                        break;
                    }
                    free(string_line_copy);
                }
            } else {
                FURI_LOG_E("Metroflip:Scene:Calypso", "Failed to open train stations file");
            }

            furi_string_free(line);
            file_stream_close(stream);
            stream_free(stream);
            free(file_path);

            if(found_station_name) {
                return found_station_name;
            }
        }
        // cast station_group_id-station_id-station_sub_id to a string
        char* station = malloc(12 * sizeof(char));
        if(!station) {
            return "Unknown";
        }
        snprintf(station, 10, "%d-%d-%d", station_group_id, station_id, station_sub_id);
        return station;
    }
    case TRAM: {
        char* file_path = malloc(256 * sizeof(char));
        if(!file_path) {
            return "Unknown";
        }
        snprintf(
            file_path,
            256,
            APP_ASSETS_PATH("navigo/stations/tram/stations_%d.txt"),
            station_group_id);
        const char* sncf_stations_path = file_path;
        Storage* storage = furi_record_open(RECORD_STORAGE);

        Stream* stream = file_stream_alloc(storage);
        FuriString* line = furi_string_alloc();

        char* found_station_name = NULL;

        if(file_stream_open(stream, sncf_stations_path, FSAM_READ, FSOM_OPEN_EXISTING)) {
            while(stream_read_line(stream, line)) {
                // file is in csv format: station_id,station_sub_id,station_name
                // search for the station
                furi_string_replace_all(line, "\r", "");
                furi_string_replace_all(line, "\n", "");
                const char* string_line = furi_string_get_cstr(line);
                char* string_line_copy = strdup(string_line);
                if(!string_line_copy) {
                    return "Unknown";
                }
                int line_station_id = atoi(get_token(string_line_copy, ",", string_line_copy));
                int line_station_sub_id = atoi(get_token(string_line_copy, ",", string_line_copy));
                if(line_station_id == station_id && line_station_sub_id == station_sub_id) {
                    found_station_name =
                        strdup(get_token(string_line_copy, ",", string_line_copy));
                    free(string_line_copy);
                    break;
                }
                free(string_line_copy);
            }
        } else {
            FURI_LOG_E("Metroflip:Scene:Calypso", "Failed to open tram stations file");
        }

        furi_string_free(line);
        file_stream_close(stream);
        stream_free(stream);
        free(file_path);

        if(found_station_name) {
            return found_station_name;
        }

        // cast station_group_id-station_id-station_sub_id to a string
        char* station = malloc(12 * sizeof(char));
        if(!station) {
            return "Unknown";
        }
        if(station_sub_id != 0) {
            snprintf(station, 10, "%d-%d-%d", station_group_id, station_id, station_sub_id);
        } else if(station_id != 0) {
            snprintf(station, 10, "%d-%d", station_group_id, station_id);
        } else {
            snprintf(station, 10, "%d", station_group_id);
        }
        return station;
    }
    case METRO: {
        if(station_group_id < 32 && station_id < 16) {
            char* file_path = malloc(256 * sizeof(char));
            if(!file_path) {
                return "Unknown";
            }
            snprintf(
                file_path,
                256,
                APP_ASSETS_PATH("navigo/stations/metro/stations_%d.txt"),
                station_group_id);
            const char* ratp_stations_path = file_path;
            Storage* storage = furi_record_open(RECORD_STORAGE);

            Stream* stream = file_stream_alloc(storage);
            FuriString* line = furi_string_alloc();

            char* found_station_name = NULL;

            if(file_stream_open(stream, ratp_stations_path, FSAM_READ, FSOM_OPEN_EXISTING)) {
                while(stream_read_line(stream, line)) {
                    // file is in csv format: station_id,station_name
                    // search for the station
                    furi_string_replace_all(line, "\r", "");
                    furi_string_replace_all(line, "\n", "");
                    const char* string_line = furi_string_get_cstr(line);
                    char* string_line_copy = strdup(string_line);
                    if(!string_line_copy) {
                        return "Unknown";
                    }
                    int line_station_id = atoi(get_token(string_line_copy, ",", string_line_copy));
                    if(line_station_id == station_id) {
                        found_station_name =
                            strdup(get_token(string_line_copy, ",", string_line_copy));
                        free(string_line_copy);
                        break;
                    }
                    free(string_line_copy);
                }
            } else {
                FURI_LOG_E("Metroflip:Scene:Calypso", "Failed to open metro stations file");
            }

            furi_string_free(line);
            file_stream_close(stream);
            stream_free(stream);
            free(file_path);

            if(found_station_name) {
                return found_station_name;
            }
        }
        // cast station_group_id-station_id to a string
        char* station = malloc(12 * sizeof(char));
        if(!station) {
            return "Unknown";
        }
        if(station_sub_id != 0) {
            snprintf(station, 10, "%d-%d-%d", station_group_id, station_id, station_sub_id);
        } else if(station_id != 0) {
            snprintf(station, 10, "%d-%d", station_group_id, station_id);
        } else {
            snprintf(station, 10, "%d", station_group_id);
        }
        return station;
    }
    default: {
        // cast station_group_id-station_id to a string
        char* station = malloc(12 * sizeof(char));
        if(!station) {
            return "Unknown";
        }
        if(station_sub_id != 0) {
            snprintf(station, 10, "%d-%d-%d", station_group_id, station_id, station_sub_id);
        } else if(station_id != 0) {
            snprintf(station, 10, "%d-%d", station_group_id, station_id);
        } else {
            snprintf(station, 10, "%d", station_group_id);
        }
        return station;
    }
    }
}

char* get_navigo_train_sector(int station_group_id) {
    // group id is in format XY where X is the sector
    const char* station_name = NAVIGO_SNCF_SECTORS_LIST[station_group_id / 10];
    return strdup(station_name);
}

const char* get_navigo_tram_line(int route_number) {
    switch(route_number) {
    case 1:
    case 13:
        return "T3a";
    case 9:
        return "T9";
    case 16:
        return "T6";
    case 18:
        return "T8";
    default: {
        char* line = malloc(5 * sizeof(char));
        if(!line) {
            return "Unknown";
        }
        snprintf(line, 5, "?%d?", route_number);
        return line;
    }
    }
}

void show_navigo_event_info(
    NavigoCardEvent* event,
    NavigoCardContract* contracts,
    FuriString* parsed_data) {
    if(event->used_contract == 0) {
        furi_string_cat_printf(parsed_data, "No event data\n");
        return;
    }
    int navigo_station_type = event->transport_type;
    char* station = get_navigo_station(
        event->station_group_id, event->station_id, event->station_sub_id, navigo_station_type);
    char* sector = NULL;
    if(navigo_station_type == COMMUTER_TRAIN || navigo_station_type == TRAM) {
        sector = get_navigo_train_sector(event->station_group_id);
    } else {
        sector = get_navigo_station(event->station_group_id, 0, 0, navigo_station_type);
    }

    if(event->transport_type == URBAN_BUS || event->transport_type == INTERURBAN_BUS ||
       event->transport_type == METRO || event->transport_type == TRAM) {
        if(event->route_number_available) {
            if(event->transport_type == METRO && event->route_number == 103) {
                furi_string_cat_printf(
                    parsed_data,
                    "%s 3 bis\n%s\n",
                    get_intercode_string_transport_type(event->transport_type),
                    get_intercode_string_transition_type(event->transition));
            } else if(event->transport_type == TRAM) {
                furi_string_cat_printf(
                    parsed_data,
                    "%s %s\n%s\n",
                    get_intercode_string_transport_type(event->transport_type),
                    get_navigo_tram_line(event->route_number),
                    get_intercode_string_transition_type(event->transition));
            } else {
                furi_string_cat_printf(
                    parsed_data,
                    "%s %d\n%s\n",
                    get_intercode_string_transport_type(event->transport_type),
                    event->route_number,
                    get_intercode_string_transition_type(event->transition));
            }
        } else {
            furi_string_cat_printf(
                parsed_data,
                "%s\n%s\n",
                get_intercode_string_transport_type(event->transport_type),
                get_intercode_string_transition_type(event->transition));
        }
        furi_string_cat_printf(
            parsed_data,
            "Transporter: %s\n",
            get_navigo_service_provider(event->service_provider));
        furi_string_cat_printf(parsed_data, "Station: %s\nSector: %s\n", station, sector);
        if(event->location_gate_available) {
            furi_string_cat_printf(parsed_data, "Gate: %d\n", event->location_gate);
        }
        if(event->device_available) {
            if(event->transport_type == URBAN_BUS || event->transport_type == INTERURBAN_BUS) {
                const char* side = event->side == 0 ? "right" : "left";
                furi_string_cat_printf(parsed_data, "Door: %d\nSide: %s\n", event->door, side);
            } else {
                furi_string_cat_printf(parsed_data, "Device: %d\n", event->device);
            }
        }
        if(event->mission_available) {
            furi_string_cat_printf(parsed_data, "Mission: %d\n", event->mission);
        }
        if(event->vehicle_id_available) {
            furi_string_cat_printf(parsed_data, "Vehicle: %d\n", event->vehicle_id);
        }
        if(event->used_contract_available) {
            furi_string_cat_printf(
                parsed_data,
                "Contract: %d - %s\n",
                event->used_contract,
                get_navigo_tariff(contracts[event->used_contract - 1].tariff));
        }
        locale_format_datetime_cat(parsed_data, &event->date, true);
        furi_string_cat_printf(parsed_data, "\n");
    } else if(event->transport_type == COMMUTER_TRAIN) {
        if(event->route_number_available) {
            furi_string_cat_printf(
                parsed_data,
                "RER %c\n%s\n",
                (65 + event->route_number - 16),
                get_intercode_string_transition_type(event->transition));
        } else {
            furi_string_cat_printf(
                parsed_data,
                "%s\n%s\n",
                get_intercode_string_transport_type(event->transport_type),
                get_intercode_string_transition_type(event->transition));
        }
        furi_string_cat_printf(
            parsed_data,
            "Transporter: %s\n",
            get_navigo_service_provider(event->service_provider));
        furi_string_cat_printf(parsed_data, "Station: %s\nSector: %s\n", station, sector);
        if(event->location_gate_available) {
            furi_string_cat_printf(parsed_data, "Gate: %d\n", event->location_gate);
        }
        if(event->device_available) {
            if(event->service_provider == NAVIGO_PROVIDER_SNCF) {
                furi_string_cat_printf(parsed_data, "Device: %d\n", event->device & 0xFF);
            } else {
                furi_string_cat_printf(parsed_data, "Device: %d\n", event->device);
            }
        }
        if(event->mission_available) {
            furi_string_cat_printf(parsed_data, "Mission: %d\n", event->mission);
        }
        if(event->vehicle_id_available) {
            furi_string_cat_printf(parsed_data, "Vehicle: %d\n", event->vehicle_id);
        }
        if(event->used_contract_available) {
            furi_string_cat_printf(
                parsed_data,
                "Contract: %d - %s\n",
                event->used_contract,
                get_navigo_tariff(contracts[event->used_contract - 1].tariff));
        }
        locale_format_datetime_cat(parsed_data, &event->date, true);
        furi_string_cat_printf(parsed_data, "\n");
    } else {
        furi_string_cat_printf(
            parsed_data,
            "%s - %s\n",
            get_intercode_string_transport_type(event->transport_type),
            get_intercode_string_transition_type(event->transition));
        furi_string_cat_printf(
            parsed_data,
            "Transporter: %s\n",
            get_navigo_service_provider(event->service_provider));
        furi_string_cat_printf(parsed_data, "Station: %s\nSector: %s\n", station, sector);
        if(event->location_gate_available) {
            furi_string_cat_printf(parsed_data, "Gate: %d\n", event->location_gate);
        }
        if(event->device_available) {
            furi_string_cat_printf(parsed_data, "Device: %d\n", event->device);
        }
        if(event->mission_available) {
            furi_string_cat_printf(parsed_data, "Mission: %d\n", event->mission);
        }
        if(event->vehicle_id_available) {
            furi_string_cat_printf(parsed_data, "Vehicle: %d\n", event->vehicle_id);
        }
        if(event->used_contract_available) {
            furi_string_cat_printf(
                parsed_data,
                "Contract: %d - %s\n",
                event->used_contract,
                get_navigo_tariff(contracts[event->used_contract - 1].tariff));
        }
        locale_format_datetime_cat(parsed_data, &event->date, true);
        furi_string_cat_printf(parsed_data, "\n");
    }

    free(station);
    free(sector);
}

void show_navigo_special_event_info(NavigoCardSpecialEvent* event, FuriString* parsed_data) {
    int navigo_station_type = event->transport_type;
    char* station = get_navigo_station(
        event->station_group_id, event->station_id, event->station_sub_id, navigo_station_type);
    char* sector = NULL;
    if(navigo_station_type == COMMUTER_TRAIN || navigo_station_type == TRAM) {
        sector = get_navigo_train_sector(event->station_group_id);
    } else {
        sector = get_navigo_station(event->station_group_id, 0, 0, navigo_station_type);
    }

    if(event->transport_type == URBAN_BUS || event->transport_type == INTERURBAN_BUS ||
       event->transport_type == METRO || event->transport_type == TRAM) {
        if(event->route_number_available) {
            if(event->transport_type == METRO && event->route_number == 103) {
                furi_string_cat_printf(
                    parsed_data,
                    "%s 3 bis\n%s\n",
                    get_intercode_string_transport_type(event->transport_type),
                    get_intercode_string_transition_type(event->transition));
            } else if(event->transport_type == TRAM) {
                furi_string_cat_printf(
                    parsed_data,
                    "%s %s\n%s\n",
                    get_intercode_string_transport_type(event->transport_type),
                    get_navigo_tram_line(event->route_number),
                    get_intercode_string_transition_type(event->transition));
            } else {
                furi_string_cat_printf(
                    parsed_data,
                    "%s %d\n%s\n",
                    get_intercode_string_transport_type(event->transport_type),
                    event->route_number,
                    get_intercode_string_transition_type(event->transition));
            }
        } else {
            furi_string_cat_printf(
                parsed_data,
                "%s\n%s\n",
                get_intercode_string_transport_type(event->transport_type),
                get_intercode_string_transition_type(event->transition));
        }
        furi_string_cat_printf(
            parsed_data, "Result: %s\n", get_intercode_string_event_result(event->result));
        furi_string_cat_printf(
            parsed_data,
            "Transporter: %s\n",
            get_navigo_service_provider(event->service_provider));
        furi_string_cat_printf(parsed_data, "Station: %s\nSector: %s\n", station, sector);
        if(event->device_available) {
            furi_string_cat_printf(parsed_data, "Device: %d\n", event->device);
        }
        locale_format_datetime_cat(parsed_data, &event->date, true);
        furi_string_cat_printf(parsed_data, "\n");
    } else if(event->transport_type == COMMUTER_TRAIN) {
        if(event->route_number_available) {
            furi_string_cat_printf(
                parsed_data,
                "RER %c\n%s\n",
                (65 + event->route_number - 16),
                get_intercode_string_transition_type(event->transition));
        } else {
            furi_string_cat_printf(
                parsed_data,
                "%s\n%s\n",
                get_intercode_string_transport_type(event->transport_type),
                get_intercode_string_transition_type(event->transition));
        }
        furi_string_cat_printf(
            parsed_data, "Result: %s\n", get_intercode_string_event_result(event->result));
        furi_string_cat_printf(
            parsed_data,
            "Transporter: %s\n",
            get_navigo_service_provider(event->service_provider));
        furi_string_cat_printf(parsed_data, "Station: %s\nSector: %s\n", station, sector);
        if(event->device_available) {
            if(event->service_provider == NAVIGO_PROVIDER_SNCF) {
                furi_string_cat_printf(parsed_data, "Device: %d\n", event->device & 0xFF);
            } else {
                furi_string_cat_printf(parsed_data, "Device: %d\n", event->device);
            }
        }
        locale_format_datetime_cat(parsed_data, &event->date, true);
        furi_string_cat_printf(parsed_data, "\n");
    } else {
        furi_string_cat_printf(
            parsed_data,
            "%s - %s\n",
            get_intercode_string_transport_type(event->transport_type),
            get_intercode_string_transition_type(event->transition));
        furi_string_cat_printf(
            parsed_data, "Result: %s\n", get_intercode_string_event_result(event->result));
        furi_string_cat_printf(
            parsed_data,
            "Transporter: %s\n",
            get_navigo_service_provider(event->service_provider));
        furi_string_cat_printf(parsed_data, "Station: %s\nSector: %s\n", station, sector);
        if(event->device_available) {
            furi_string_cat_printf(parsed_data, "Device: %d\n", event->device);
        }
        locale_format_datetime_cat(parsed_data, &event->date, true);
        furi_string_cat_printf(parsed_data, "\n");
    }

    free(station);
    free(sector);
}

void show_navigo_contract_info(NavigoCardContract* contract, FuriString* parsed_data) {
    // Core type and ticket info
    furi_string_cat_printf(parsed_data, "Type: %s\n", get_navigo_tariff(contract->tariff));
    if(contract->counter_present) {
        furi_string_cat_printf(parsed_data, "Remaining Tickets: %d\n", contract->counter.count);
        furi_string_cat_printf(parsed_data, "Last load: %d\n", contract->counter.last_load);
    }

    // Validity period
    furi_string_cat_printf(parsed_data, "Valid from: ");
    locale_format_datetime_cat(parsed_data, &contract->start_date, false);
    furi_string_cat_printf(parsed_data, "\n");
    if(contract->end_date_available) {
        furi_string_cat_printf(parsed_data, "to: ");
        locale_format_datetime_cat(parsed_data, &contract->end_date, false);
        furi_string_cat_printf(parsed_data, "\n");
    }

    // Serial number (if available)
    if(contract->serial_number_available) {
        furi_string_cat_printf(parsed_data, "TCN Number: %d\n", contract->serial_number);
    }
    if(contract->price_amount_available) {
        furi_string_cat_printf(parsed_data, "Amount: %.2f EUR\n", contract->price_amount);
    }
    if(contract->pay_method_available) {
        furi_string_cat_printf(
            parsed_data,
            "Payment Method: %s\n",
            get_intercode_string_pay_method(contract->pay_method));
    }
    if(contract->zones_available) {
        furi_string_cat_printf(parsed_data, "%s\n", get_zones(contract->zones));
    }
    furi_string_cat_printf(parsed_data, "Sold on: ");
    locale_format_datetime_cat(parsed_data, &contract->sale_date, false);
    furi_string_cat_printf(parsed_data, "\n");
    furi_string_cat_printf(
        parsed_data, "Sales Agent: %s\n", get_navigo_service_provider(contract->sale_agent));
    furi_string_cat_printf(parsed_data, "Sales Terminal: %d\n", contract->sale_device);
    furi_string_cat_printf(
        parsed_data, "Status: %s\n", get_intercode_string_contract_status(contract->status));
    furi_string_cat_printf(parsed_data, "Authenticity Code: %d\n", contract->authenticator);
}

void show_navigo_environment_info(
    NavigoCardEnv* environment,
    NavigoCardHolder* holder,
    FuriString* parsed_data) {
    furi_string_cat_printf(
        parsed_data, "Card status: %s\n", get_intercode_string_holder_type(holder->card_status));
    if(is_intercode_string_holder_linked(holder->card_status)) {
        furi_string_cat_printf(parsed_data, "Linked to an organization\n");
    }
    furi_string_cat_printf(
        parsed_data,
        "App Version: %s - v%d\n",
        get_intercode_string_version(environment->app_version),
        get_intercode_string_subversion(environment->app_version));
    furi_string_cat_printf(
        parsed_data, "Country: %s\n", get_country_string(environment->country_num));
    furi_string_cat_printf(
        parsed_data,
        "Network: %s\n",
        get_network_string(guess_card_type(environment->country_num, environment->network_num)));
    furi_string_cat_printf(parsed_data, "End of validity:\n");
    locale_format_datetime_cat(parsed_data, &environment->end_dt, false);
    furi_string_cat_printf(parsed_data, "\n");
}
