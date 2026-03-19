/*
 * Suica Scene
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "../../api/suica/suica_drawings.h"
#include "../../metroflip_plugins.h"

#include <lib/nfc/protocols/felica/felica.h>
#include <lib/nfc/protocols/felica/felica_poller.h>
// #include <lib/nfc/protocols/felica/felica_poller_i.h>
#include <lib/bit_lib/bit_lib.h>

#include <applications/services/locale/locale.h>
#include <datetime.h>

// Probably not needed after upstream include this in their suica_i.h

#define TAG                  "Metroflip:Scene:Suica"
#define JAPAN_IC_SYSTEM_CODE 0x0003
#define OCTOPUS_SYSTEM_CODE  0x8008

const char* suica_service_names[] = {
    "Travel History",
    "Taps Log",
};

static void suica_model_initialize(SuicaHistoryViewModel* model, size_t initial_capacity) {
    model->travel_history =
        (uint8_t*)malloc(initial_capacity * FELICA_DATA_BLOCK_SIZE); // Each entry is 16 bytes
    model->size = 0;
    model->capacity = initial_capacity;
    model->entry = 1;
    model->page = 0;
    model->animator_tick = 0;
    model->history.entry_station.name = furi_string_alloc_set("Unknown");
    model->history.entry_station.jr_header = furi_string_alloc_set("0");
    model->history.exit_station.name = furi_string_alloc_set("Unknown");
    model->history.exit_station.jr_header = furi_string_alloc_set("0");
    model->history.entry_line = RailwaysList[SUICA_RAILWAY_NUM];
    model->history.exit_line = RailwaysList[SUICA_RAILWAY_NUM];
}

static void suica_model_initialize_after_load(SuicaHistoryViewModel* model) {
    model->entry = 1;
    model->page = 0;
    model->animator_tick = 0;
    model->history.entry_station.name = furi_string_alloc_set("Unknown");
    model->history.entry_station.jr_header = furi_string_alloc_set("0");
    model->history.exit_station.name = furi_string_alloc_set("Unknown");
    model->history.exit_station.jr_header = furi_string_alloc_set("0");
    model->history.entry_line = RailwaysList[SUICA_RAILWAY_NUM];
    model->history.exit_line = RailwaysList[SUICA_RAILWAY_NUM];
}

static void suica_model_free(SuicaHistoryViewModel* model) {
    if(model->travel_history) free(model->travel_history);
    furi_string_free(model->history.entry_station.name);
    furi_string_free(model->history.entry_station.jr_header);
    furi_string_free(model->history.exit_station.name);
    furi_string_free(model->history.exit_station.jr_header);
    // no need to free RailwaysList — static
}

static void suica_add_entry(SuicaHistoryViewModel* model, const uint8_t* entry) {
    if(model->size <= 0) {
        suica_model_initialize(model, 3);
    }
    // Check if resizing is needed
    if(model->size == model->capacity) {
        size_t new_capacity = model->capacity * 2; // Double the capacity
        uint8_t* new_data =
            (uint8_t*)realloc(model->travel_history, new_capacity * FELICA_DATA_BLOCK_SIZE);
        model->travel_history = new_data;
        model->capacity = new_capacity;
    }

    // Copy the 16-byte entry to the next slot
    for(size_t i = 0; i < FELICA_DATA_BLOCK_SIZE; i++) {
        model->travel_history[(model->size * FELICA_DATA_BLOCK_SIZE) + i] = entry[i];
    }

    model->size++;
}

static void suica_parse_train_code(
    uint8_t line_code,
    uint8_t station_code,
    uint8_t area_code,
    SuicaTrainRideType ride_type,
    SuicaHistoryViewModel* model) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    Stream* stream = file_stream_alloc(storage);

    FuriString* line = furi_string_alloc();
    FuriString* line_code_str = furi_string_alloc();
    FuriString* line_and_station_code_str = furi_string_alloc();
    FuriString* line_candidate = furi_string_alloc_set(SUICA_RAILWAY_UNKNOWN_NAME);
    FuriString* station_candidate = furi_string_alloc_set(SUICA_RAILWAY_UNKNOWN_NAME);
    FuriString* station_num_candidate = furi_string_alloc_set("0");
    FuriString* station_JR_header_candidate = furi_string_alloc_set("0");
    FuriString* line_copy = furi_string_alloc();
    FuriString* file_name = furi_string_alloc();

    furi_string_printf(line_code_str, "0x%02X", line_code);
    furi_string_printf(line_and_station_code_str, "0x%02X,0x%02X", line_code, station_code);

    size_t line_comma_ind = 0;
    size_t station_comma_ind = 0;
    size_t station_num_comma_ind = 0;
    size_t station_JR_header_comma_ind = 0;

    bool station_found = false;

    furi_string_printf(
        file_name, "%sArea%01X/line_0x%02X.txt", SUICA_STATION_LIST_PATH, area_code, line_code);
    if(file_stream_open(stream, furi_string_get_cstr(file_name), FSAM_READ, FSOM_OPEN_EXISTING)) {
        while(stream_read_line(stream, line) && !station_found) {
            // file is in csv format: station_group_id,station_id,station_sub_id,station_name
            // search for the station
            furi_string_replace_all(line, "\r", "");
            furi_string_replace_all(line, "\n", "");
            furi_string_set(line_copy, line); // 0xD5,0x02,Keikyu Main,Shinagawa,1,0

            if(furi_string_start_with(line, line_code_str)) {
                // set line name here
                furi_string_right(line_copy, 10); // Keikyu Main,Shinagawa,1,0
                furi_string_set(line_candidate, line_copy);
                line_comma_ind = furi_string_search_char(line_candidate, ',', 0);
                furi_string_left(line_candidate, line_comma_ind); // Keikyu Main
                // we cut the line and station code in the line line copy
                // and we leave only the line name for the line candidate
                if(furi_string_start_with(line, line_and_station_code_str)) {
                    furi_string_set(station_candidate, line_copy); // Keikyu Main,Shinagawa,1,0
                    furi_string_right(station_candidate, line_comma_ind + 1);
                    station_comma_ind =
                        furi_string_search_char(station_candidate, ',', 0); // Shinagawa,1,0
                    furi_string_left(station_candidate, station_comma_ind); //  Shinagawa
                    station_found = true;
                    break;
                }
            }
        }
    } else {
        FURI_LOG_E(
            TAG, "Failed to open stations list text file: %s", furi_string_get_cstr(file_name));
    }

    furi_string_set(station_num_candidate, line_copy); // Keikyu Main,Shinagawa,1,0
    furi_string_right(station_num_candidate, line_comma_ind + station_comma_ind + 2); // 1,0
    station_num_comma_ind = furi_string_search_char(station_num_candidate, ',', 0);
    furi_string_left(station_num_candidate, station_num_comma_ind); // 1

    furi_string_set(station_JR_header_candidate, line_copy); // Keikyu Main,Shinagawa,1,0
    furi_string_right(
        station_JR_header_candidate,
        line_comma_ind + station_comma_ind + station_num_comma_ind + 3); // 0
    station_JR_header_comma_ind = furi_string_search_char(station_JR_header_candidate, ',', 0);
    furi_string_left(station_JR_header_candidate, station_JR_header_comma_ind); // 0

    switch(ride_type) {
    case SuicaTrainRideEntry:
        furi_string_set(model->history.entry_station.name, "Unknown");
        furi_string_set(model->history.entry_station.jr_header, "0");
        model->history.entry_line = RailwaysList[SUICA_RAILWAY_NUM];
        for(size_t i = 0; i < SUICA_RAILWAY_NUM; i++) {
            if(furi_string_equal_str(line_candidate, RailwaysList[i].long_name)) {
                model->history.entry_line = RailwaysList[i];
                furi_string_set(model->history.entry_station.name, station_candidate);
                model->history.entry_station.station_number =
                    atoi(furi_string_get_cstr(station_num_candidate));
                furi_string_set(
                    model->history.entry_station.jr_header, station_JR_header_candidate);
                break;
            }
        }
        break;
    case SuicaTrainRideExit:
        furi_string_set(model->history.exit_station.name, "Unknown");
        furi_string_set(model->history.exit_station.jr_header, "0");
        model->history.exit_line = RailwaysList[SUICA_RAILWAY_NUM];
        for(size_t i = 0; i < SUICA_RAILWAY_NUM; i++) {
            if(furi_string_equal_str(line_candidate, RailwaysList[i].long_name)) {
                model->history.exit_line = RailwaysList[i];
                furi_string_set(model->history.exit_station.name, station_candidate);
                model->history.exit_station.station_number =
                    atoi(furi_string_get_cstr(station_num_candidate));
                furi_string_set(
                    model->history.exit_station.jr_header, station_JR_header_candidate);
                break;
            }
        }
        break;
    default:
        UNUSED(model);
        break;
    }

    furi_string_free(line);
    furi_string_free(line_copy);
    furi_string_free(line_code_str);
    furi_string_free(line_and_station_code_str);
    furi_string_free(line_candidate);
    furi_string_free(station_candidate);
    furi_string_free(station_num_candidate);
    furi_string_free(station_JR_header_candidate);
    furi_string_free(file_name);

    file_stream_close(stream);
    stream_free(stream);
    furi_record_close(RECORD_STORAGE);
}

static void suica_parse(SuicaHistoryViewModel* my_model) {
    uint8_t current_block[FELICA_DATA_BLOCK_SIZE];
    // Parse the current block/entry
    for(size_t i = 0; i < FELICA_DATA_BLOCK_SIZE; i++) {
        current_block[i] = my_model->travel_history[((my_model->entry - 1) * 16) + i];
    }

    if(((uint8_t)current_block[4] + (uint8_t)current_block[5]) != 0) {
        my_model->history.year = ((uint8_t)current_block[4] & 0xFE) >> 1;
        my_model->history.month = (((uint8_t)current_block[4] & 0x01) << 3) |
                                  (((uint8_t)current_block[5] & 0xE0) >> 5);
        my_model->history.day = (uint8_t)current_block[5] & 0x1F;
    } else {
        my_model->history.year = 0;
        my_model->history.month = 0;
        my_model->history.day = 0;
    }
    my_model->history.balance = ((uint16_t)current_block[11] << 8) | (uint16_t)current_block[10];
    my_model->history.area_code = current_block[15];
    if((uint8_t)current_block[0] >= TERMINAL_FARE_ADJUST_MACHINE &&
       (uint8_t)current_block[0] <= TERMINAL_IN_CAR_MACHINE) {
        // Train rides
        // Will be overwritton is is ticket sale (TERMINAL_TICKET_VENDING_MACHINE)
        my_model->history.history_type = SuicaHistoryTrain;
        uint8_t entry_line = current_block[6];
        uint8_t entry_station = current_block[7];
        uint8_t entry_area = (current_block[15] & 0xC0) >> 6; // 0xC0 = 11000000
        uint8_t exit_line = current_block[8];
        uint8_t exit_station = current_block[9];
        uint8_t exit_area = (current_block[15] & 0x30) >> 4; // 0x30 = 00110000

        suica_parse_train_code(
            entry_line, entry_station, entry_area, SuicaTrainRideEntry, my_model);

        if((uint8_t)current_block[14] != 0x01) {
            suica_parse_train_code(
                exit_line, exit_station, exit_area, SuicaTrainRideExit, my_model);
        }

        if(((uint8_t)current_block[4] + (uint8_t)current_block[5]) != 0) {
            my_model->history.year = ((uint8_t)current_block[4] & 0xFE) >> 1;
            my_model->history.month = (((uint8_t)current_block[4] & 0x01) << 3) |
                                      (((uint8_t)current_block[5] & 0xE0) >> 5);
            my_model->history.day = (uint8_t)current_block[5] & 0x1F;
        }
    }
    switch((uint8_t)current_block[1]) {
    case PROCESSING_NEW_ISSUE:
        my_model->history.history_type = SuicaHistoryHappyBirthday;
        break;
    case PROCESSING_TOP_UP: // is this necessary?
        my_model->history.history_type = SuicaHistoryTopUp;
        // switch case the type of terminals here if necessary
        break;
    default:
        switch((uint8_t)current_block[0]) {
        case TERMINAL_BUS:
            // 6 & 7 bus line code
            // 8 & 9 bus stop code
            my_model->history.history_type = SuicaHistoryBus;
            break;
        case TERMINAL_POS:
        case TERMINAL_VENDING_MACHINE:
            // 6 & 7 are hour and minute
            my_model->history.history_type = ((uint8_t)current_block[0] == TERMINAL_POS) ?
                                                 SuicaHistoryPosAndTaxi :
                                                 SuicaHistoryVendingMachine;
            my_model->history.hour = ((uint8_t)current_block[6] & 0xF8) >> 3;
            my_model->history.minute = (((uint8_t)current_block[6] & 0x07) << 3) |
                                       (((uint8_t)current_block[7] & 0xE0) >> 5);
            my_model->history.shop_code[0] = current_block[8];
            my_model->history.shop_code[1] = current_block[9];
            break;
        case TERMINAL_MOBILE_PHONE:
            break;
        default:
            if((uint8_t)current_block[0] <= TERMINAL_NULL) {
                my_model->history.history_type = SuicaHistoryNull;
            }
            break;
        }
    }
}

static bool suica_model_pack_data(
    SuicaHistoryViewModel* model,
    const FelicaSystem* suica_system,
    FuriString* parsed_data) {
    uint32_t public_block_count = simple_array_get_count(suica_system->public_blocks);
    bool found = false;
    furi_string_printf(parsed_data, "\e#Japan Transit IC\n\n");
    for(uint16_t i = 0; i < public_block_count; i++) {
        FelicaPublicBlock* public_block = simple_array_get(suica_system->public_blocks, i);
        if(public_block->service_code == SERVICE_CODE_HISTORY_IN_LE) {
            suica_add_entry(model, public_block->block.data);
            furi_string_cat_printf(parsed_data, "Log %02X: ", i);
            for(size_t j = 0; j < FELICA_DATA_BLOCK_SIZE; j++) {
                furi_string_cat_printf(parsed_data, "%02X ", public_block->block.data[j]);
            }
            furi_string_cat_printf(parsed_data, "\n");
            found = true;
        }
    }
    return found;
}

static bool suica_help_with_octopus(const FelicaSystem* suica_system, FuriString* parsed_data) {
    bool found = false;
    for(uint16_t i = 0; i < simple_array_get_count(suica_system->public_blocks); i++) {
        FelicaPublicBlock* public_block = simple_array_get(suica_system->public_blocks, i);
        if(public_block->service_code == SERVICE_CODE_OCTOPUS_IN_LE) {
            uint16_t unsigned_balance = ((uint16_t)public_block->block.data[2] << 8) |
                                        (uint16_t)public_block->block.data[3]; // 0x0000..0xFFFF

            int32_t older_balance_cents = (int32_t)unsigned_balance - 350;
            int32_t newer_balance_cents = (int32_t)unsigned_balance - 500;

            uint16_t older_abs_cents =
                (uint16_t)(older_balance_cents < 0 ? -older_balance_cents : older_balance_cents);
            uint16_t newer_abs_cents =
                (uint16_t)(newer_balance_cents < 0 ? -newer_balance_cents : newer_balance_cents);

            uint16_t older_dollars = (uint16_t)(older_abs_cents / 100);
            uint8_t older_cents = (uint8_t)(older_abs_cents % 100);

            uint16_t newer_dollars = (uint16_t)(newer_abs_cents / 100);
            uint8_t newer_cents = (uint8_t)(newer_abs_cents % 100);
            furi_string_printf(parsed_data, "\e#Octopus Card\n");
            furi_string_cat_str(
                parsed_data, "::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::\n");

            furi_string_cat_printf(
                parsed_data, "If this card was issued \nbefore 2017 October 1st:\n");
            furi_string_cat_printf(
                parsed_data,
                "Balance: %sHK$ %d.%02d\n",
                older_balance_cents < 0 ? "-" : "",
                older_dollars,
                older_cents);

            furi_string_cat_str(
                parsed_data, "::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::\n");

            furi_string_cat_printf(
                parsed_data, "If this card was issued \nafter 2017 October 1st:\n");
            furi_string_cat_printf(
                parsed_data,
                "Balance: %sHK$ %d.%02d\n",
                newer_balance_cents < 0 ? "-" : "",
                newer_dollars,
                newer_cents);

            furi_string_cat_str(
                parsed_data, "::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::");

            found = true;
            break; // Octopus only has one public block
        }
    }
    return found;
}

static void suica_parse_detail_callback(GuiButtonType result, InputType type, void* context) {
    Metroflip* app = context;
    UNUSED(result);
    if(type == InputTypeShort) {
        SuicaHistoryViewModel* my_model = view_get_model(app->suica_context->view_history);
        suica_parse(my_model);
        FURI_LOG_I(TAG, "Draw Callback: We have %d entries", my_model->size);
        view_dispatcher_switch_to_view(app->view_dispatcher, MetroflipViewCanvas);
    }
}

static NfcCommand suica_poller_callback(NfcGenericEvent event, void* context) {
    furi_assert(event.protocol == NfcProtocolFelica);
    NfcCommand command = NfcCommandContinue;
    // MetroflipPollerEventType stage = MetroflipPollerEventTypeStart;

    Metroflip* app = context;
    FuriString* parsed_data = furi_string_alloc();
    SuicaHistoryViewModel* model = view_get_model(app->suica_context->view_history);

    Widget* widget = app->widget;

    const FelicaPollerEvent* felica_event = event.event_data;
    FURI_LOG_I(TAG, "Felica event: %d", felica_event->type);
    if(felica_event->type == FelicaPollerEventTypeReady ||
       felica_event->type == FelicaPollerEventTypeIncomplete) {
        dolphin_deed(DolphinDeedNfcRead);

        FURI_LOG_I(TAG, "Read complete");
        view_dispatcher_send_custom_event(app->view_dispatcher, MetroflipCustomEventPollerSuccess);
        command = NfcCommandStop;

        nfc_device_set_data(app->nfc_device, NfcProtocolFelica, nfc_poller_get_data(app->poller));
        const FelicaData* felica_data = nfc_poller_get_data(app->poller);

        metroflip_app_blink_stop(app);

        uint32_t system_count = simple_array_get_count(felica_data->systems);
        bool suica_found = false;
        bool octopus_found = false;
        uint8_t suica_system_index = 0;
        uint8_t octopus_system_index = 0;
        for(uint8_t i = 0; i < system_count; i++) {
            FelicaSystem* system = simple_array_get(felica_data->systems, i);
            if(system->system_code == JAPAN_IC_SYSTEM_CODE) {
                suica_found = true;
                suica_system_index = i;
            } else if(system->system_code == OCTOPUS_SYSTEM_CODE) {
                octopus_found = true;
                octopus_system_index = i;
            }
        }

        do {
            if(suica_found) {
                FelicaSystem* suica_system =
                    simple_array_get(felica_data->systems, suica_system_index);
                suica_model_pack_data(model, suica_system, parsed_data);
                break;
            } else if(octopus_found) {
                FelicaSystem* octopus_system =
                    simple_array_get(felica_data->systems, octopus_system_index);
                suica_help_with_octopus(octopus_system, parsed_data);
                break;
            } else {
                furi_string_printf(
                    parsed_data,
                    "\e#FeliCa\nSorry, unrecorded service code.\nPlease let the developers know and we will add support.");
            }
        } while(false);

        widget_add_text_scroll_element(widget, 0, 0, 128, 64, furi_string_get_cstr(parsed_data));

        if(suica_found) {
            widget_add_button_element(
                widget, GuiButtonTypeCenter, "Parse", suica_parse_detail_callback, app);
        }

        widget_add_button_element(
            widget, GuiButtonTypeRight, "Exit", metroflip_exit_widget_callback, app);
        widget_add_button_element(
            widget, GuiButtonTypeLeft, "Save", metroflip_save_widget_callback, app);

        view_dispatcher_switch_to_view(app->view_dispatcher, MetroflipViewWidget);
    }
    furi_string_free(parsed_data);
    command = NfcCommandStop;
    return command;
}

static bool suica_history_input_callback(InputEvent* event, void* context) {
    Metroflip* app = (Metroflip*)context;
    if(event->type == InputTypeShort || event->type == InputTypeRepeat) {
        switch(event->key) {
        case InputKeyLeft: {
            bool redraw = true;
            with_view_model(
                app->suica_context->view_history,
                SuicaHistoryViewModel * model,
                {
                    if(model->entry > 1) {
                        model->entry--;
                    }
                    suica_parse(model);
                },
                redraw);
            break;
        }
        case InputKeyRight: {
            bool redraw = true;
            with_view_model(
                app->suica_context->view_history,
                SuicaHistoryViewModel * model,
                {
                    if(model->entry < model->size) {
                        model->entry++;
                    }
                    suica_parse(model);
                },
                redraw);
            break;
        }
        case InputKeyUp: {
            bool redraw = true;
            with_view_model(
                app->suica_context->view_history,
                SuicaHistoryViewModel * model,
                {
                    if(model->page > 0) {
                        model->page--;
                    }
                },
                redraw);
            break;
        }
        case InputKeyDown: {
            bool redraw = true;
            with_view_model(
                app->suica_context->view_history,
                SuicaHistoryViewModel * model,
                {
                    if(model->page < HISTORY_VIEW_PAGE_NUM - 1) {
                        model->page++;
                    }
                },
                redraw);
            break;
        }
        default:
            // Handle other keys or do nothing
            break;
        }
    }
    return false;
}

static void suica_on_enter(Metroflip* app) {
    if(app->data_loaded == false) {
        app->suica_context = malloc(sizeof(SuicaContext));
        app->suica_context->view_history = view_alloc();
        view_set_context(app->suica_context->view_history, app);
        view_allocate_model(
            app->suica_context->view_history,
            ViewModelTypeLockFree,
            sizeof(SuicaHistoryViewModel));
    }

    view_set_input_callback(app->suica_context->view_history, suica_history_input_callback);
    view_set_previous_callback(app->suica_context->view_history, suica_navigation_raw_callback);
    view_set_enter_callback(app->suica_context->view_history, suica_view_history_enter_callback);
    view_set_exit_callback(app->suica_context->view_history, suica_view_history_exit_callback);
    view_set_custom_callback(
        app->suica_context->view_history, suica_view_history_custom_event_callback);
    view_set_draw_callback(app->suica_context->view_history, suica_history_draw_callback);

    view_dispatcher_add_view(
        app->view_dispatcher, MetroflipViewCanvas, app->suica_context->view_history);

    if(app->data_loaded == false) {
        popup_set_header(app->popup, "Apply\n card to\nthe back", 68, 30, AlignLeft, AlignTop);
        popup_set_icon(app->popup, 0, 3, &I_RFIDDolphinReceive_97x61);
        view_dispatcher_switch_to_view(app->view_dispatcher, MetroflipViewPopup);

        app->poller = nfc_poller_alloc(app->nfc, NfcProtocolFelica);
        nfc_poller_start(app->poller, suica_poller_callback, app);
        FURI_LOG_I(TAG, "Poller started");

        metroflip_app_blink_start(app);
    } else {
        FURI_LOG_I(TAG, "Loading FeliCa data");
        Storage* storage = furi_record_open(RECORD_STORAGE);
        FlipperFormat* ff = flipper_format_file_alloc(storage);
        if(flipper_format_file_open_existing(ff, app->file_path)) {
            FURI_LOG_I(TAG, "File path: %s", app->file_path);
            FelicaData* felica_data = felica_alloc();
            bool is_stock_nfc_file = felica_load(felica_data, ff, 4);

            SuicaHistoryViewModel* model = view_get_model(app->suica_context->view_history);
            suica_model_initialize_after_load(model);
            Widget* widget = app->widget;
            FuriString* parsed_data = furi_string_alloc();
            bool suica_found = false;
            do {
                if(is_stock_nfc_file) { // loading from stock NFC file (API 87.0+)
                    FURI_LOG_I(TAG, "Loading from stock NFC file");
                    uint32_t system_count = simple_array_get_count(felica_data->systems);
                    bool octopus_found = false;
                    uint8_t suica_system_index = 0;
                    uint8_t octopus_system_index = 0;
                    for(uint8_t i = 0; i < system_count; i++) {
                        FelicaSystem* system = simple_array_get(felica_data->systems, i);
                        if(system->system_code == JAPAN_IC_SYSTEM_CODE) {
                            suica_found = true;
                            suica_system_index = i;
                        } else if(system->system_code == OCTOPUS_SYSTEM_CODE) {
                            octopus_found = true;
                            octopus_system_index = i;
                        }
                    }

                    if(suica_found) {
                        FelicaSystem* suica_system =
                            simple_array_get(felica_data->systems, suica_system_index);
                        suica_model_pack_data(model, suica_system, parsed_data);
                        break;
                    } else if(octopus_found) {
                        FelicaSystem* octopus_system =
                            simple_array_get(felica_data->systems, octopus_system_index);
                        suica_help_with_octopus(octopus_system, parsed_data);
                        break;
                    }
                } else { // loading from legacy saved file (pre API 87.0)
                    suica_found = true;
                    furi_string_printf(parsed_data, "\e#Japan Transit IC\n\n");
                    for(uint8_t i = 0; i < model->size; i++) {
                        furi_string_cat_printf(parsed_data, "Log %02X: ", i);
                        for(size_t j = 0; j < FELICA_DATA_BLOCK_SIZE; j++) {
                            furi_string_cat_printf(
                                parsed_data, "%02X ", model->travel_history[i * 16 + j]);
                        }
                        furi_string_cat_printf(parsed_data, "\n");
                    }
                    break;
                }

                furi_string_printf(
                    parsed_data,
                    "\e#FeliCa\nSorry, unrecorded service code.\nPlease let the developers know and we will add support.");
            } while(false);

            // Text scroll must be added before buttons to prevent overlay
            widget_add_text_scroll_element(
                widget, 0, 0, 128, 64, furi_string_get_cstr(parsed_data));

            widget_add_button_element(
                widget, GuiButtonTypeRight, "Exit", metroflip_exit_widget_callback, app);

            if(suica_found) {
                widget_add_button_element(
                    widget, GuiButtonTypeCenter, "Parse", suica_parse_detail_callback, app);
            }

            // No reason to put a save button here if the data is loaded from an existing file

            view_dispatcher_switch_to_view(app->view_dispatcher, MetroflipViewWidget);
            furi_string_free(parsed_data);
            felica_free(felica_data);
        }
        flipper_format_free(ff);
    }
}

static bool suica_on_event(Metroflip* app, SceneManagerEvent event) {
    bool consumed = false;
    Popup* popup = app->popup;
    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == MetroflipCustomEventCardDetected) {
            popup_set_header(popup, "DON'T\nMOVE", 68, 30, AlignLeft, AlignTop);
            consumed = true;
        } else if(event.event == MetroflipCustomEventCardLost) {
            popup_set_header(popup, "Card \n lost", 68, 30, AlignLeft, AlignTop);
            scene_manager_search_and_switch_to_previous_scene(
                app->scene_manager, MetroflipSceneStart);
            consumed = true;
        } else if(event.event == MetroflipCustomEventWrongCard) {
            popup_set_header(popup, "WRONG \n CARD", 68, 30, AlignLeft, AlignTop);
            scene_manager_search_and_switch_to_previous_scene(
                app->scene_manager, MetroflipSceneStart);
            consumed = true;
        } else if(event.event == MetroflipCustomEventPollerFail) {
            popup_set_header(popup, "Failed", 68, 30, AlignLeft, AlignTop);
            scene_manager_search_and_switch_to_previous_scene(
                app->scene_manager, MetroflipSceneStart);
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        UNUSED(popup);
        scene_manager_search_and_switch_to_previous_scene(app->scene_manager, MetroflipSceneStart);
        consumed = true;
    }
    return consumed;
}

static void suica_on_exit(Metroflip* app) {
    widget_reset(app->widget);
    with_view_model(
        app->suica_context->view_history,
        SuicaHistoryViewModel * model,
        { suica_model_free(model); },
        false);
    view_free_model(app->suica_context->view_history);
    view_dispatcher_remove_view(app->view_dispatcher, MetroflipViewCanvas);
    view_free(app->suica_context->view_history);
    free(app->suica_context);
    if(app->poller && !app->data_loaded) {
        nfc_poller_stop(app->poller);
        nfc_poller_free(app->poller);
    }
}

/* Actual implementation of app<>plugin interface */
static const MetroflipPlugin suica_plugin = {
    .card_name = "Suica",
    .plugin_on_enter = suica_on_enter,
    .plugin_on_event = suica_on_event,
    .plugin_on_exit = suica_on_exit,

};

/* Plugin descriptor to comply with basic plugin specification */
static const FlipperAppPluginDescriptor suica_plugin_descriptor = {
    .appid = METROFLIP_SUPPORTED_CARD_PLUGIN_APP_ID,
    .ep_api_version = METROFLIP_SUPPORTED_CARD_PLUGIN_API_VERSION,
    .entry_point = &suica_plugin,
};

/* Plugin entry point - must return a pointer to const descriptor  */
const FlipperAppPluginDescriptor* suica_plugin_ep(void) {
    return &suica_plugin_descriptor;
}
