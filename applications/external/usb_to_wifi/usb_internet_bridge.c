#include "bridge_session.h"
#include "markets.h"
#include "radio_player.h"

#include <furi.h>
#include <gui/gui.h>
#include <gui/modules/submenu.h>
#include <gui/modules/text_input.h>
#include <gui/modules/widget.h>
#include <gui/view_dispatcher.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TAG                        "UsbInternetBridge"
#define FIB_UI_TICK_MS             100U
#define FIB_MARKET_AUTO_REFRESH_MS 2000U
#define FIB_SAMPLE_URL             "https://api.github.com/zen"
#define FIB_TIME_URL               "https://postman-echo.com/time/now"
#define FIB_NATIONAL_TODAY_URL     "https://nationaltoday.com/today/"
#define FIB_ISS_URL                "https://api.wheretheiss.at/v1/satellites/25544"
#define FIB_RADIO_RESULT_LIMIT     5U
#define FIB_RADIO_COUNTRY_SIZE     48U
#define FIB_RADIO_URL_BASE                                                            \
    "https://all.api.radio-browser.info/json/stations/search?limit=5&hidebroken=true" \
    "&is_https=true&codec=MP3&bitrateMax=64&order=clickcount&reverse=true&"
#define FIB_SEARCH_URL_PREFIX                                                          \
    "https://en.wikipedia.org/w/api.php?action=query&generator=search&gsrlimit=1"      \
    "&prop=extracts&exchars=420&explaintext=1&redirects=1&format=json&formatversion=2" \
    "&gsrsearch="
#define FIB_SEARCH_QUERY_SIZE    64U
#define FIB_WEATHER_QUERY_SIZE   64U
#define FIB_WEATHER_RESULT_LIMIT 3U
#define FIB_GEOCODING_URL_PREFIX \
    "https://geocoding-api.open-meteo.com/v1/search?count=3&language=en&format=json&name="
#define FIB_WEATHER_URL_FORMAT                                                        \
    "https://api.open-meteo.com/v1/forecast?latitude=%s&longitude=%s"                 \
    "&current=temperature_2m,apparent_temperature,relative_humidity_2m,weather_code," \
    "wind_speed_10m&daily=temperature_2m_max,temperature_2m_min,"                     \
    "precipitation_probability_max&timezone=auto&forecast_days=1"

typedef enum {
    FibViewMenu,
    FibViewToolbox,
    FibViewMarkets,
    FibViewStatus,
    FibViewUrlInput,
    FibViewWeatherResults,
    FibViewRadioStations,
} FibView;

typedef enum {
    FibMenuToolbox,
    FibMenuTestConnection,
    FibMenuDownloadSample,
    FibMenuGetDateTime,
    FibMenuCustomUrl,
    FibMenuConnectionInfo,
} FibMenuItem;

typedef enum {
    FibToolboxInformationSearch,
    FibToolboxWeather,
    FibToolboxNationalToday,
    FibToolboxIssLocation,
    FibToolboxInternetRadio,
    FibToolboxMarkets,
} FibToolboxItem;

typedef enum {
    FibCustomEventSessionUpdated = 1U,
} FibCustomEvent;

typedef enum {
    FibRequestModeNormal,
    FibRequestModeMarkets,
    FibRequestModeMarketSearch,
    FibRequestModeWikipedia,
    FibRequestModeWeatherSearch,
    FibRequestModeWeatherForecast,
    FibRequestModeNationalToday,
    FibRequestModeIssLocation,
    FibRequestModeRadioSearch,
    FibRequestModeRadioPlaying,
    FibRequestModeRadioStopped,
} FibRequestMode;

typedef enum {
    FibPendingActionNone,
    FibPendingActionSample,
    FibPendingActionMarkets,
    FibPendingActionDateTime,
    FibPendingActionWikipediaInput,
    FibPendingActionWeatherInput,
    FibPendingActionNationalToday,
    FibPendingActionIssLocation,
    FibPendingActionRadioCountryInput,
    FibPendingActionCustomUrlInput,
} FibPendingAction;

typedef struct {
    char label[48];
    char latitude[16];
    char longitude[16];
} FibWeatherLocation;

typedef struct {
    char label[64];
    char url[256];
    uint16_t bitrate;
} FibRadioStation;

typedef struct {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    Submenu* menu;
    Submenu* toolbox;
    Submenu* markets;
    unsigned market_index;
    Submenu* weather_results;
    Submenu* radio_stations;
    Widget* status_widget;
    TextInput* url_input;
    FuriString* status_text;
    BridgeSession* session;
    RadioPlayer* radio_player;
    BridgeSessionSnapshot snapshot;
    char url_buffer[FIB_MAX_URL_LENGTH + 1U];
    char input_buffer[FIB_MAX_URL_LENGTH + 1U];
    char search_result[FIB_RESPONSE_PREVIEW_SIZE + 1U];
    char market_price[32];
    char market_updated[32];
    char market_symbol[MARKETS_MAX_SYMBOL_LENGTH + 1U];
    bool market_has_price;
    uint32_t market_last_refresh_tick;
    uint32_t market_last_response_id;
    FibView current_view;
    FibView navigation_root;
    bool showing_connection_info;
    FibRequestMode request_mode;
    FibWeatherLocation locations[FIB_WEATHER_RESULT_LIMIT];
    uint8_t location_count;
    bool weather_results_shown;
    FibRadioStation stations[FIB_RADIO_RESULT_LIMIT];
    uint8_t station_count;
    bool radio_results_shown;
    bool request_waiting_for_ready;
    uint32_t radio_last_retry_tick;
    uint32_t radio_last_ui_tick;
    FibPendingAction pending_action;
    bool views_added;
} FibApp;

/* FAP has exactly one UI instance. Module views keep their own context
 * (Widget*, Submenu*, ...), so navigation callbacks must not cast that
 * context to FibApp. */
static FibApp* fib_app_active = NULL;

static void fib_app_url_submitted(void* context);
static void fib_app_menu_selected(void* context, uint32_t index);
static void fib_app_toolbox_selected(void* context, uint32_t index);
static void fib_app_weather_selected(void* context, uint32_t index);
static void fib_app_radio_selected(void* context, uint32_t index);
static void fib_app_continue_pending_action(FibApp* app, FibPendingAction action);
static void fib_app_request_or_wait(FibApp* app, const char* url);
static void fib_app_show_status(FibApp* app, bool connection_info);
static void fib_app_render_status(FibApp* app);

static bool fib_app_radio_body(void* context, const uint8_t* data, size_t length) {
    FibApp* app = context;
    return app && app->radio_player && radio_player_push(app->radio_player, data, length);
}

static void fib_app_stop_radio(FibApp* app) {
    if(!app || (app->request_mode != FibRequestModeRadioPlaying &&
                !radio_player_is_running(app->radio_player)))
        return;
    /* Change mode first so tick/custom callbacks cannot restart the stream
     * while cancellation and decoder shutdown are in progress. */
    if(app->request_mode == FibRequestModeRadioPlaying) {
        app->request_mode = FibRequestModeRadioStopped;
    }
    /* Wake a body callback blocked on a full compressed ring before sending
     * CANCEL. Some stations otherwise leave the USB worker waiting here. */
    radio_player_request_stop(app->radio_player);
    bridge_session_set_body_callback(app->session, NULL, NULL);
    bridge_session_cancel(app->session);
    app->request_waiting_for_ready = false;
    app->pending_action = FibPendingActionNone;
}

static bool fib_app_start_selected_radio(FibApp* app) {
    if(!app) return false;
    radio_player_request_stop(app->radio_player);
    if(!radio_player_start(app->radio_player)) return false;
    app->request_mode = FibRequestModeRadioPlaying;
    app->radio_last_retry_tick = furi_get_tick();
    app->radio_last_ui_tick = 0U;
    bridge_session_set_body_callback(app->session, fib_app_radio_body, app);
    fib_app_request_or_wait(app, app->url_buffer);
    return true;
}

static bool fib_app_percent_encode(const char* input, char* output, size_t output_size) {
    if(!input || !output || output_size < 2U) return false;
    size_t used = 0U;
    static const char hex[] = "0123456789ABCDEF";
    for(size_t index = 0U; input[index] != '\0'; ++index) {
        const uint8_t value = (uint8_t)input[index];
        const bool unreserved = (value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z') ||
                                (value >= '0' && value <= '9') || value == '-' || value == '_' ||
                                value == '.' || value == '~';
        if(unreserved) {
            if(used + 1U >= output_size) return false;
            output[used++] = (char)value;
        } else {
            if(used + 3U >= output_size) return false;
            output[used++] = '%';
            output[used++] = hex[value >> 4U];
            output[used++] = hex[value & 0x0FU];
        }
    }
    output[used] = '\0';
    return used != 0U;
}

static bool
    fib_app_append_ascii(char* output, size_t output_size, size_t* used, const char* text) {
    while(*text) {
        if(*used + 1U >= output_size) return false;
        output[(*used)++] = *text++;
    }
    return true;
}

static void
    fib_app_append_codepoint(uint32_t codepoint, char* output, size_t output_size, size_t* used) {
    const char* replacement = NULL;
    char single[2] = {(char)codepoint, '\0'};

    if(codepoint >= 0x20U && codepoint <= 0x7EU) {
        replacement = single;
    } else if(codepoint == '\n' || codepoint == '\r') {
        replacement = "\n";
    } else if(codepoint == '\t' || codepoint == 0x00A0U) {
        replacement = " ";
    } else if(codepoint >= 0x00C0U && codepoint <= 0x00C5U) {
        replacement = "A";
    } else if(codepoint == 0x00C6U) {
        replacement = "AE";
    } else if(codepoint == 0x00C7U) {
        replacement = "C";
    } else if(codepoint >= 0x00C8U && codepoint <= 0x00CBU) {
        replacement = "E";
    } else if(codepoint >= 0x00CCU && codepoint <= 0x00CFU) {
        replacement = "I";
    } else if(codepoint == 0x00D0U) {
        replacement = "D";
    } else if(codepoint == 0x00D1U) {
        replacement = "N";
    } else if((codepoint >= 0x00D2U && codepoint <= 0x00D6U) || codepoint == 0x00D8U) {
        replacement = "O";
    } else if(codepoint >= 0x00D9U && codepoint <= 0x00DCU) {
        replacement = "U";
    } else if(codepoint == 0x00DDU || codepoint == 0x0178U) {
        replacement = "Y";
    } else if(codepoint == 0x00DEU) {
        replacement = "TH";
    } else if(codepoint == 0x00DFU) {
        replacement = "ss";
    } else if(codepoint >= 0x00E0U && codepoint <= 0x00E5U) {
        replacement = "a";
    } else if(codepoint == 0x00E6U) {
        replacement = "ae";
    } else if(codepoint == 0x00E7U) {
        replacement = "c";
    } else if(codepoint >= 0x00E8U && codepoint <= 0x00EBU) {
        replacement = "e";
    } else if(codepoint >= 0x00ECU && codepoint <= 0x00EFU) {
        replacement = "i";
    } else if(codepoint == 0x00F0U) {
        replacement = "d";
    } else if(codepoint == 0x00F1U) {
        replacement = "n";
    } else if((codepoint >= 0x00F2U && codepoint <= 0x00F6U) || codepoint == 0x00F8U) {
        replacement = "o";
    } else if(codepoint >= 0x00F9U && codepoint <= 0x00FCU) {
        replacement = "u";
    } else if(codepoint == 0x00FDU || codepoint == 0x00FFU) {
        replacement = "y";
    } else if(codepoint == 0x00FEU) {
        replacement = "th";
    } else if(codepoint >= 0x0100U && codepoint <= 0x0105U) {
        replacement = (codepoint & 1U) ? "a" : "A";
    } else if(codepoint >= 0x0106U && codepoint <= 0x010DU) {
        replacement = (codepoint & 1U) ? "c" : "C";
    } else if(codepoint >= 0x010EU && codepoint <= 0x0111U) {
        replacement = (codepoint & 1U) ? "d" : "D";
    } else if(codepoint >= 0x0112U && codepoint <= 0x011BU) {
        replacement = (codepoint & 1U) ? "e" : "E";
    } else if(codepoint >= 0x011CU && codepoint <= 0x0123U) {
        replacement = (codepoint & 1U) ? "g" : "G";
    } else if(codepoint >= 0x0124U && codepoint <= 0x0127U) {
        replacement = (codepoint & 1U) ? "h" : "H";
    } else if(codepoint >= 0x0128U && codepoint <= 0x0131U) {
        replacement = (codepoint == 0x0131U || (codepoint & 1U)) ? "i" : "I";
    } else if(codepoint >= 0x0139U && codepoint <= 0x0142U) {
        replacement = (codepoint & 1U) ? "L" : "l";
    } else if(codepoint >= 0x0143U && codepoint <= 0x014BU) {
        replacement = (codepoint & 1U) ? "N" : "n";
    } else if(codepoint >= 0x014CU && codepoint <= 0x0153U) {
        replacement = (codepoint & 1U) ? "o" : "O";
    } else if(codepoint >= 0x0154U && codepoint <= 0x0159U) {
        replacement = (codepoint & 1U) ? "r" : "R";
    } else if(codepoint >= 0x015AU && codepoint <= 0x0161U) {
        replacement = (codepoint & 1U) ? "s" : "S";
    } else if(codepoint >= 0x0162U && codepoint <= 0x0167U) {
        replacement = (codepoint & 1U) ? "t" : "T";
    } else if(codepoint >= 0x0168U && codepoint <= 0x0173U) {
        replacement = (codepoint & 1U) ? "u" : "U";
    } else if(codepoint >= 0x0179U && codepoint <= 0x017EU) {
        replacement = (codepoint & 1U) ? "Z" : "z";
    } else if(codepoint >= 0x2010U && codepoint <= 0x2015U) {
        replacement = "-";
    } else if(codepoint >= 0x2018U && codepoint <= 0x201BU) {
        replacement = "'";
    } else if(codepoint >= 0x201CU && codepoint <= 0x201FU) {
        replacement = "\"";
    } else if(codepoint == 0x2022U) {
        replacement = "*";
    } else if(codepoint == 0x2026U) {
        replacement = "...";
    } else if(codepoint == 0x00B0U) {
        replacement = " deg";
    } else if(codepoint == 0x00D7U) {
        replacement = "x";
    } else if(codepoint == 0x2122U) {
        replacement = "TM";
    } else if(codepoint == 0x00A9U) {
        replacement = "(c)";
    } else if(codepoint == 0x00AEU) {
        replacement = "(R)";
    } else {
        replacement = " ";
    }

    fib_app_append_ascii(output, output_size, used, replacement);
}

static int8_t fib_app_hex_value(char value) {
    if(value >= '0' && value <= '9') return (int8_t)(value - '0');
    if(value >= 'a' && value <= 'f') return (int8_t)(value - 'a' + 10);
    if(value >= 'A' && value <= 'F') return (int8_t)(value - 'A' + 10);
    return -1;
}

static bool fib_app_parse_hex4(const char* text, uint32_t* value) {
    uint32_t result = 0U;
    for(size_t index = 0U; index < 4U; ++index) {
        const int8_t digit = fib_app_hex_value(text[index]);
        if(digit < 0) return false;
        result = (result << 4U) | (uint32_t)digit;
    }
    *value = result;
    return true;
}

static bool fib_app_extract_search_result(const char* json, char* output, size_t output_size) {
    if(!json || !output || output_size < 2U) return false;
    const char* cursor = strstr(json, "\"extract\":\"");
    if(!cursor) return false;
    cursor += strlen("\"extract\":\"");
    size_t used = 0U;
    while(*cursor && used + 1U < output_size) {
        if(*cursor == '"') break;
        if(*cursor == '\\') {
            ++cursor;
            if(!*cursor) break;
            if(*cursor == 'n' || *cursor == 'r' || *cursor == 't') {
                output[used++] = (*cursor == 't') ? ' ' : '\n';
            } else if(*cursor == 'u' && strlen(cursor + 1U) >= 4U) {
                uint32_t codepoint = 0U;
                if(fib_app_parse_hex4(cursor + 1U, &codepoint)) {
                    cursor += 4U;
                    if(codepoint >= 0xD800U && codepoint <= 0xDBFFU && cursor[1] == '\\' &&
                       cursor[2] == 'u' && strlen(cursor + 3U) >= 4U) {
                        uint32_t low = 0U;
                        if(fib_app_parse_hex4(cursor + 3U, &low) && low >= 0xDC00U &&
                           low <= 0xDFFFU) {
                            codepoint =
                                0x10000U + ((codepoint - 0xD800U) << 10U) + (low - 0xDC00U);
                            cursor += 6U;
                        }
                    }
                    fib_app_append_codepoint(codepoint, output, output_size, &used);
                }
            } else {
                output[used++] = *cursor;
            }
        } else {
            const uint8_t first = (uint8_t)*cursor;
            uint32_t codepoint = first;
            size_t continuation_count = 0U;
            if((first & 0xE0U) == 0xC0U) {
                codepoint = first & 0x1FU;
                continuation_count = 1U;
            } else if((first & 0xF0U) == 0xE0U) {
                codepoint = first & 0x0FU;
                continuation_count = 2U;
            } else if((first & 0xF8U) == 0xF0U) {
                codepoint = first & 0x07U;
                continuation_count = 3U;
            }
            bool valid = true;
            for(size_t index = 0U; index < continuation_count; ++index) {
                const uint8_t next = (uint8_t)cursor[index + 1U];
                if((next & 0xC0U) != 0x80U) {
                    valid = false;
                    break;
                }
                codepoint = (codepoint << 6U) | (next & 0x3FU);
            }
            fib_app_append_codepoint(
                valid ? codepoint : (uint32_t)' ', output, output_size, &used);
            if(valid) cursor += continuation_count;
        }
        ++cursor;
    }
    output[used] = '\0';
    return used != 0U;
}

static bool fib_app_build_search_url(const char* query, char* output, size_t output_size) {
    if(!query || !output) return false;
    const size_t prefix_length = strlen(FIB_SEARCH_URL_PREFIX);
    if(prefix_length + 1U >= output_size) return false;
    memcpy(output, FIB_SEARCH_URL_PREFIX, prefix_length);
    return fib_app_percent_encode(query, output + prefix_length, output_size - prefix_length);
}

static void
    fib_app_ascii_copy(char* output, size_t output_size, const char* input, size_t length) {
    if(output_size == 0U) return;
    size_t used = 0U;
    for(size_t index = 0U; index < length && used + 1U < output_size; ++index) {
        const uint8_t value = (uint8_t)input[index];
        if(value >= 0x20U && value <= 0x7EU) {
            output[used++] = (char)value;
        } else if(value == 0xC4U && index + 1U < length) {
            const uint8_t next = (uint8_t)input[++index];
            output[used++] = (next == 0x9FU || next == 0x9EU) ? 'g' :
                             (next == 0xB1U || next == 0xB0U) ? 'i' :
                                                                '?';
        } else if(value == 0xC5U && index + 1U < length) {
            const uint8_t next = (uint8_t)input[++index];
            output[used++] = (next == 0x9FU || next == 0x9EU) ? 's' : '?';
        } else if(value == 0xC3U && index + 1U < length) {
            const uint8_t next = (uint8_t)input[++index];
            if(next == 0xA7U || next == 0x87U)
                output[used++] = 'c';
            else if(next == 0xB6U || next == 0x96U)
                output[used++] = 'o';
            else if(next == 0xBCU || next == 0x9CU)
                output[used++] = 'u';
            else
                output[used++] = '?';
        } else {
            output[used++] = '?';
        }
    }
    output[used] = '\0';
}

static bool
    fib_app_json_string(const char* object, const char* key, char* output, size_t output_size) {
    const char* value = strstr(object, key);
    if(!value) return false;
    value += strlen(key);
    const char* end = strchr(value, '"');
    if(!end) return false;
    fib_app_ascii_copy(output, output_size, value, (size_t)(end - value));
    return output[0] != '\0';
}

static bool
    fib_app_json_number(const char* object, const char* key, char* output, size_t output_size) {
    const char* value = strstr(object, key);
    if(!value || output_size < 2U) return false;
    value += strlen(key);
    size_t used = 0U;
    while(((*value >= '0' && *value <= '9') || *value == '-' || *value == '.') &&
          used + 1U < output_size) {
        output[used++] = *value++;
    }
    output[used] = '\0';
    return used != 0U;
}

static uint8_t fib_app_parse_locations(FibApp* app, const char* json) {
    const char* cursor = strstr(json, "\"results\":[");
    if(!cursor) return 0U;
    uint8_t count = 0U;
    while(count < FIB_WEATHER_RESULT_LIMIT && (cursor = strchr(cursor, '{')) != NULL) {
        const char* end = strchr(cursor, '}');
        if(!end) break;
        FibWeatherLocation* location = &app->locations[count];
        char name[32] = "";
        char country[8] = "";
        if(!fib_app_json_string(cursor, "\"name\":\"", name, sizeof(name)) ||
           !fib_app_json_number(
               cursor, "\"latitude\":", location->latitude, sizeof(location->latitude)) ||
           !fib_app_json_number(
               cursor, "\"longitude\":", location->longitude, sizeof(location->longitude))) {
            cursor = end + 1U;
            continue;
        }
        fib_app_json_string(cursor, "\"country_code\":\"", country, sizeof(country));
        snprintf(
            location->label,
            sizeof(location->label),
            "%s%s%s",
            name,
            country[0] ? ", " : "",
            country);
        ++count;
        cursor = end + 1U;
    }
    return count;
}

static uint8_t fib_app_parse_radio_stations(FibApp* app, const char* text) {
    if(!text || strncmp(text, "FIBRADIO1\n", 10U) != 0) return 0U;
    const char* line = text + 10U;
    uint8_t count = 0U;
    while(*line && count < FIB_RADIO_RESULT_LIMIT) {
        const char* end = strchr(line, '\n');
        if(!end) end = line + strlen(line);
        const char* tab1 = memchr(line, '\t', (size_t)(end - line));
        const char* tab2 = tab1 ? memchr(tab1 + 1U, '\t', (size_t)(end - tab1 - 1U)) : NULL;
        const char* tab3 = tab2 ? memchr(tab2 + 1U, '\t', (size_t)(end - tab2 - 1U)) : NULL;
        const char* tab4 = tab3 ? memchr(tab3 + 1U, '\t', (size_t)(end - tab3 - 1U)) : NULL;
        if(tab1 && tab2 && tab3 && tab4) {
            FibRadioStation* station = &app->stations[count];
            char name[44], country[18], state[18], bitrate[8];
            fib_app_ascii_copy(name, sizeof(name), line, (size_t)(tab1 - line));
            fib_app_ascii_copy(country, sizeof(country), tab1 + 1U, (size_t)(tab2 - tab1 - 1U));
            fib_app_ascii_copy(state, sizeof(state), tab2 + 1U, (size_t)(tab3 - tab2 - 1U));
            fib_app_ascii_copy(bitrate, sizeof(bitrate), tab3 + 1U, (size_t)(tab4 - tab3 - 1U));
            fib_app_ascii_copy(
                station->url, sizeof(station->url), tab4 + 1U, (size_t)(end - tab4 - 1U));
            station->bitrate = (uint16_t)atoi(bitrate);
            snprintf(
                station->label,
                sizeof(station->label),
                "%s%s%s",
                name,
                country[0] ? " - " : "",
                country);
            if(station->url[0] != '\0') ++count;
        }
        line = *end ? end + 1U : end;
    }
    return count;
}

static uint8_t fib_app_radio_country_fallback(FibApp* app) {
    if(strcmp(app->input_buffer, "TR") != 0 && strcmp(app->input_buffer, "tr") != 0 &&
       strcmp(app->input_buffer, "Turkey") != 0 && strcmp(app->input_buffer, "turkey") != 0 &&
       strcmp(app->input_buffer, "Turkiye") != 0 && strcmp(app->input_buffer, "turkiye") != 0) {
        return 0U;
    }
    static const struct {
        const char* name;
        const char* url;
    } fallback[] = {
        {"Super FM - Turkiye",
         "https://playerservices.streamtheworld.com/api/livestream-redirect/SUPER_FM_SC"},
        {"Metro FM - Turkiye",
         "https://playerservices.streamtheworld.com/api/livestream-redirect/METRO_FM_SC?"},
        {"Joy Turk - Turkiye",
         "https://playerservices.streamtheworld.com/api/livestream-redirect/JOY_TURK_SC?/"},
    };
    for(uint8_t index = 0U; index < COUNT_OF(fallback); ++index) {
        snprintf(
            app->stations[index].label,
            sizeof(app->stations[index].label),
            "%s",
            fallback[index].name);
        snprintf(
            app->stations[index].url, sizeof(app->stations[index].url), "%s", fallback[index].url);
        app->stations[index].bitrate = 64U;
    }
    return COUNT_OF(fallback);
}

static const char* fib_app_weather_description(int code) {
    if(code == 0) return "Clear sky";
    if(code <= 3) return "Partly cloudy";
    if(code == 45 || code == 48) return "Fog";
    if(code >= 51 && code <= 57) return "Drizzle";
    if(code >= 61 && code <= 67) return "Rain";
    if(code >= 71 && code <= 77) return "Snow";
    if(code >= 80 && code <= 82) return "Rain showers";
    if(code >= 85 && code <= 86) return "Snow showers";
    if(code >= 95) return "Thunderstorm";
    return "Unknown";
}

static void fib_app_append_national_today(FuriString* output, const char* text) {
    static const char bold_start[] = "[[B]]";
    static const char bold_end[] = "[[/B]]";
    const char* cursor = text;
    bool first_section = true;
    while(*cursor) {
        if(strncmp(cursor, bold_start, sizeof(bold_start) - 1U) == 0) {
            if(!first_section) furi_string_cat_str(output, "\n\n");
            furi_string_cat_str(output, "\e#");
            cursor += sizeof(bold_start) - 1U;
            first_section = false;
        } else if(strncmp(cursor, bold_end, sizeof(bold_end) - 1U) == 0) {
            furi_string_cat_str(output, "\n");
            cursor += sizeof(bold_end) - 1U;
        } else {
            furi_string_push_back(output, *cursor++);
        }
    }
}

static bool fib_app_weather_value(const char* json, const char* key, char* value, size_t size) {
    return fib_app_json_number(json, key, value, size);
}

static bool fib_app_format_weather(const char* json, char* output, size_t output_size) {
    char temperature[16], apparent[16], humidity[16], wind[16], code[8];
    char high[16], low[16], rain[16];
    const char* current = strstr(json, "\"current\":{");
    const char* daily = strstr(json, "\"daily\":{");
    if(!current || !daily ||
       !fib_app_weather_value(current, "\"temperature_2m\":", temperature, sizeof(temperature)) ||
       !fib_app_weather_value(current, "\"apparent_temperature\":", apparent, sizeof(apparent)) ||
       !fib_app_weather_value(current, "\"relative_humidity_2m\":", humidity, sizeof(humidity)) ||
       !fib_app_weather_value(current, "\"weather_code\":", code, sizeof(code)) ||
       !fib_app_weather_value(current, "\"wind_speed_10m\":", wind, sizeof(wind)) ||
       !fib_app_weather_value(daily, "\"temperature_2m_max\":[", high, sizeof(high)) ||
       !fib_app_weather_value(daily, "\"temperature_2m_min\":[", low, sizeof(low)) ||
       !fib_app_weather_value(daily, "\"precipitation_probability_max\":[", rain, sizeof(rain)))
        return false;
    snprintf(
        output,
        output_size,
        "%s\nTemp: %s C\nFeels: %s C\nHumidity: %s%%\nWind: %s km/h\nHigh/Low: %s/%s C\nRain chance: %s%%",
        fib_app_weather_description(atoi(code)),
        temperature,
        apparent,
        humidity,
        wind,
        high,
        low,
        rain);
    return true;
}

static void fib_app_trim_decimal(char* value, size_t decimal_places) {
    char* decimal = strchr(value, '.');
    if(!decimal) return;
    if(decimal_places == 0U) {
        *decimal = '\0';
        return;
    }
    char* end = decimal + 1U + decimal_places;
    if(*end != '\0') *end = '\0';
}

static bool fib_app_format_iss(const char* json, char* output, size_t output_size) {
    char latitude[24], longitude[24], altitude[24], velocity[24], visibility[24];
    if(!fib_app_json_number(json, "\"latitude\":", latitude, sizeof(latitude)) ||
       !fib_app_json_number(json, "\"longitude\":", longitude, sizeof(longitude)) ||
       !fib_app_json_number(json, "\"altitude\":", altitude, sizeof(altitude)) ||
       !fib_app_json_number(json, "\"velocity\":", velocity, sizeof(velocity)) ||
       !fib_app_json_string(json, "\"visibility\":\"", visibility, sizeof(visibility))) {
        return false;
    }

    fib_app_trim_decimal(latitude, 2U);
    fib_app_trim_decimal(longitude, 2U);
    fib_app_trim_decimal(altitude, 1U);
    fib_app_trim_decimal(velocity, 0U);
    const char latitude_direction = latitude[0] == '-' ? 'S' : 'N';
    const char longitude_direction = longitude[0] == '-' ? 'W' : 'E';
    const char* latitude_value = latitude[0] == '-' ? latitude + 1U : latitude;
    const char* longitude_value = longitude[0] == '-' ? longitude + 1U : longitude;
    const char* light = strcmp(visibility, "daylight") == 0 ? "In daylight" : "In Earth's shadow";

    snprintf(
        output,
        output_size,
        "Latitude: %s %c\nLongitude: %s %c\nAltitude: %s km\nSpeed: %s km/h\n%s",
        latitude_value,
        latitude_direction,
        longitude_value,
        longitude_direction,
        altitude,
        velocity,
        light);
    return true;
}

static const char* fib_state_text(BridgeSessionState state) {
    switch(state) {
    case BridgeSessionStateDisconnected:
        return "Disconnected";
    case BridgeSessionStateWaitingForHelper:
        return "Waiting for host";
    case BridgeSessionStateWaitingForHelloAck:
        return "Connecting to helper";
    case BridgeSessionStateHelperNotFound:
        return "Helper not found";
    case BridgeSessionStatePermissionPending:
        return "Waiting for permission";
    case BridgeSessionStatePermissionDenied:
        return "Permission denied";
    case BridgeSessionStateReady:
        return "Internet access ready";
    case BridgeSessionStatePinging:
        return "Testing connection";
    case BridgeSessionStateSendingRequest:
        return "Sending request";
    case BridgeSessionStateReceivingResponse:
        return "Receiving response";
    case BridgeSessionStateComplete:
        return "Request complete";
    case BridgeSessionStateCancelled:
        return "Request cancelled";
    case BridgeSessionStateTimedOut:
        return "Timed out";
    case BridgeSessionStateError:
        return "Error";
    }
    return "Unknown status";
}

static const char* fib_permission_text(BridgePermission permission) {
    switch(permission) {
    case BridgePermissionPending:
        return "Pending";
    case BridgePermissionDenied:
        return "Denied";
    case BridgePermissionAllowedOnce:
        return "This connection";
    case BridgePermissionAllowedAlways:
        return "Always";
    case BridgePermissionUnknown:
    default:
        return "Unknown";
    }
}

static void fib_app_cancel_button(GuiButtonType button, InputType input_type, void* context) {
    FibApp* app = context;
    if(!app || button != GuiButtonTypeCenter || input_type != InputTypeShort) return;
    if(app->request_mode == FibRequestModeMarkets) {
        if(bridge_session_has_active_request(app->session)) return;
        app->market_last_refresh_tick = furi_get_tick();
        fib_app_request_or_wait(app, app->url_buffer);
        fib_app_render_status(app);
        return;
    }
    if(app->request_mode == FibRequestModeRadioPlaying) {
        fib_app_stop_radio(app);
        fib_app_render_status(app);
        return;
    }
    if(app->request_mode == FibRequestModeRadioStopped) {
        if(fib_app_start_selected_radio(app)) {
            fib_app_render_status(app);
        }
        return;
    }
    bridge_session_cancel(app->session);
}

static void
    fib_app_radio_stations_button(GuiButtonType button, InputType input_type, void* context) {
    FibApp* app = context;
    if(!app || button != GuiButtonTypeLeft || input_type != InputTypeShort) return;
    fib_app_stop_radio(app);
    app->request_mode = FibRequestModeRadioSearch;
    app->current_view = FibViewRadioStations;
    view_dispatcher_switch_to_view(app->view_dispatcher, FibViewRadioStations);
}

static void fib_app_render_status(FibApp* app) {
    bridge_session_get_snapshot(app->session, &app->snapshot);
    furi_string_reset(app->status_text);
    bool market_card_ready = false;

    if(app->request_mode == FibRequestModeMarkets &&
       app->snapshot.state == BridgeSessionStateComplete) {
        bool parsed = false;
        const bool new_response = app->snapshot.active_request_id != app->market_last_response_id;
        if(new_response) {
            app->market_last_response_id = app->snapshot.active_request_id;
            parsed = app->snapshot.http_status == 200U &&
                     (app->market_index == MARKETS_SEARCH_INDEX ? markets_format_coin(
                                                                      app->market_symbol,
                                                                      app->snapshot.preview,
                                                                      app->search_result,
                                                                      sizeof(app->search_result)) :
                                                                  markets_format(
                                                                      app->market_index,
                                                                      app->snapshot.preview,
                                                                      app->search_result,
                                                                      sizeof(app->search_result)));
        }
        if(parsed) {
            const char* price_end = strchr(app->search_result, ' ');
            if(price_end && (size_t)(price_end - app->search_result) < sizeof(app->market_price) &&
               markets_format_updated_label(
                   app->search_result, app->market_updated, sizeof(app->market_updated))) {
                size_t length = (size_t)(price_end - app->search_result);
                memcpy(app->market_price, app->search_result, length);
                app->market_price[length] = '\0';
                app->market_has_price = true;
                app->market_last_refresh_tick = furi_get_tick();
                market_card_ready = true;
            }
        }
        if(!app->market_has_price) {
            const char* title = app->market_index == MARKETS_SEARCH_INDEX ?
                                    app->market_symbol :
                                    markets_name(app->market_index);
            furi_string_cat_printf(app->status_text, "\e#%s\n", title);
            if(app->snapshot.http_status != 200U && app->market_index == MARKETS_SEARCH_INDEX) {
                furi_string_cat_str(
                    app->status_text, "Coin was not found.\nEnter a Binance USDT symbol.");
            } else if(app->snapshot.http_status != 200U) {
                furi_string_cat_printf(
                    app->status_text,
                    "Price service: HTTP %u\nTry again later.",
                    app->snapshot.http_status);
            } else {
                furi_string_cat_str(app->status_text, "Price data unavailable.\nPlease refresh.");
            }
        }
        if(app->market_has_price) market_card_ready = true;
    } else if(app->request_mode == FibRequestModeMarkets && app->market_has_price) {
        market_card_ready = true;
    } else if(
        app->request_mode == FibRequestModeWikipedia &&
        app->snapshot.state == BridgeSessionStateComplete) {
        furi_string_cat_printf(app->status_text, "\e#%s\n", app->input_buffer);
        if(app->snapshot.http_status == 200U &&
           fib_app_extract_search_result(
               app->snapshot.preview, app->search_result, sizeof(app->search_result))) {
            furi_string_cat_str(app->status_text, app->search_result);
        } else if(app->snapshot.http_status == 200U) {
            furi_string_cat_str(app->status_text, "No result found.");
        } else {
            furi_string_cat_printf(
                app->status_text, "Search returned HTTP %u.", app->snapshot.http_status);
        }
    } else if(
        app->request_mode == FibRequestModeWeatherForecast &&
        app->snapshot.state == BridgeSessionStateComplete) {
        furi_string_cat_printf(app->status_text, "\e#Weather\n");
        if(app->snapshot.http_status == 200U &&
           fib_app_format_weather(
               app->snapshot.preview, app->search_result, sizeof(app->search_result))) {
            furi_string_cat_str(app->status_text, app->search_result);
        } else if(app->snapshot.http_status == 200U) {
            furi_string_cat_str(app->status_text, "Weather data was incomplete.");
        } else {
            furi_string_cat_printf(
                app->status_text, "Weather returned HTTP %u.", app->snapshot.http_status);
        }
    } else if(
        app->request_mode == FibRequestModeIssLocation &&
        app->snapshot.state == BridgeSessionStateComplete) {
        furi_string_cat_printf(app->status_text, "\e#ISS Right Now\n");
        if(app->snapshot.http_status == 200U &&
           fib_app_format_iss(
               app->snapshot.preview, app->search_result, sizeof(app->search_result))) {
            furi_string_cat_str(app->status_text, app->search_result);
        } else if(app->snapshot.http_status == 200U) {
            furi_string_cat_str(app->status_text, "ISS location data was incomplete.");
        } else {
            furi_string_cat_printf(
                app->status_text, "ISS service returned HTTP %u.", app->snapshot.http_status);
        }
    } else if(
        app->request_mode == FibRequestModeNationalToday &&
        app->snapshot.state == BridgeSessionStateComplete) {
        furi_string_cat_printf(app->status_text, "\e#National Today\n");
        if(app->snapshot.http_status == 200U && app->snapshot.preview[0] != '\0') {
            fib_app_append_national_today(app->status_text, app->snapshot.preview);
        } else if(app->snapshot.http_status == 200U) {
            furi_string_cat_str(app->status_text, "Today's list could not be read.");
        } else {
            furi_string_cat_printf(
                app->status_text, "National Today returned HTTP %u.", app->snapshot.http_status);
        }
    } else if(
        app->request_mode == FibRequestModeWeatherSearch &&
        app->snapshot.state == BridgeSessionStateComplete && app->weather_results_shown &&
        app->location_count == 0U) {
        furi_string_cat_printf(
            app->status_text,
            "\e#Weather\nNo matching location found.\nTry a longer location name or add a country code.");
    } else if(
        app->request_mode == FibRequestModeRadioSearch &&
        app->snapshot.state == BridgeSessionStateComplete && app->radio_results_shown &&
        app->station_count == 0U) {
        furi_string_cat_str(
            app->status_text,
            "\e#Internet Radio\nNo MP3 stations found.\nTry a country code such as UK, US, DE or JP.");
    } else if(
        app->request_mode == FibRequestModeRadioPlaying ||
        app->request_mode == FibRequestModeRadioStopped) {
        furi_string_cat_str(app->status_text, "\e#Internet Radio\n");
        furi_string_cat_printf(
            app->status_text,
            "%s\n\n%s\nFrames: %lu  Buf: %uK\nGaps: %lu",
            app->search_result,
            app->request_mode == FibRequestModeRadioStopped ? "Stopped" :
            radio_player_decoded_frames(app->radio_player)  ? "Playing on Flipper speaker" :
                                                              "Buffering MP3 audio...",
            (unsigned long)radio_player_decoded_frames(app->radio_player),
            (unsigned)(radio_player_buffered_bytes(app->radio_player) / 1024U),
            (unsigned long)radio_player_underflows(app->radio_player));
    } else if(app->showing_connection_info) {
        furi_string_cat_printf(app->status_text, "\e#Connection Info\n");
        furi_string_cat_printf(
            app->status_text,
            "USB: %s\nHost: %s\nPermission: %s\n",
            app->snapshot.usb_connected ? "Connected" : "Disconnected",
            app->snapshot.helper_present ? "Found" : "Waiting",
            fib_permission_text(app->snapshot.permission));
        furi_string_cat_printf(
            app->status_text,
            "Device: %s\nID suffix: %s\n",
            app->snapshot.device_name,
            app->snapshot.uid_suffix[0] ? app->snapshot.uid_suffix : "None");
        if(app->snapshot.selected_major != 0U) {
            furi_string_cat_printf(
                app->status_text,
                "Protocol: %u.%u\n",
                app->snapshot.selected_major,
                app->snapshot.selected_minor);
        } else {
            furi_string_cat_printf(app->status_text, "Protocol: Waiting\n");
        }
        furi_string_cat_printf(
            app->status_text, "Status: %s", fib_state_text(app->snapshot.state));
    } else {
        furi_string_cat_printf(app->status_text, "\e#%s\n", fib_state_text(app->snapshot.state));
        if(app->snapshot.detail[0] != '\0') {
            furi_string_cat_printf(app->status_text, "%s\n", app->snapshot.detail);
        }
        if(app->snapshot.http_status != 0U) {
            furi_string_cat_printf(
                app->status_text,
                "HTTP %u - %lu bytes%s\n",
                app->snapshot.http_status,
                (unsigned long)app->snapshot.response_bytes,
                app->snapshot.response_truncated ? " (truncated)" : "");
        }
        if(app->snapshot.preview[0] != '\0') {
            furi_string_cat_printf(app->status_text, "---\n%s", app->snapshot.preview);
        } else if(
            app->snapshot.state == BridgeSessionStateWaitingForHelper ||
            app->snapshot.state == BridgeSessionStateDisconnected) {
            furi_string_cat_printf(app->status_text, "Check the USB cable and desktop host.");
        }
    }

    widget_reset(app->status_widget);
    if(market_card_ready) {
        static const char* titles[] = {"BTC / USDT", "ETH / USDT", "Gold", "Brent BZ", "Silver"};
        static const char* units[] = {
            "USDT", "USDT", "USD / troy oz", "USDT / barrel", "USD / troy oz"};
        static const char* sources[] = {"Binance", "Binance", "Gold API", "Binance", "Gold API"};
        const bool searched_coin = app->market_index == MARKETS_SEARCH_INDEX;
        widget_add_string_element(
            app->status_widget,
            3U,
            8U,
            AlignLeft,
            AlignCenter,
            FontSecondary,
            searched_coin ? app->market_symbol : titles[app->market_index]);
        widget_add_string_element(
            app->status_widget,
            125U,
            8U,
            AlignRight,
            AlignCenter,
            FontSecondary,
            searched_coin ? "Binance" : sources[app->market_index]);
        widget_add_line_element(app->status_widget, 2U, 15U, 125U, 15U);
        widget_add_string_element(
            app->status_widget, 64U, 25U, AlignCenter, AlignCenter, FontPrimary, app->market_price);
        widget_add_string_element(
            app->status_widget,
            64U,
            35U,
            AlignCenter,
            AlignCenter,
            FontSecondary,
            searched_coin ? "USDT" : units[app->market_index]);
        widget_add_string_element(
            app->status_widget,
            64U,
            44U,
            AlignCenter,
            AlignCenter,
            FontSecondary,
            app->market_updated);
    } else if(app->request_mode == FibRequestModeWikipedia) {
        widget_add_string_element(
            app->status_widget, 64U, 7U, AlignCenter, AlignCenter, FontPrimary, "Wikipedia");
        const uint8_t text_height = app->snapshot.active_request ? 37U : 49U;
        widget_add_text_scroll_element(
            app->status_widget, 2U, 15U, 124U, text_height, furi_string_get_cstr(app->status_text));
    } else {
        const uint8_t text_height =
            (app->request_mode == FibRequestModeRadioPlaying ||
             app->request_mode == FibRequestModeRadioStopped) ?
                44U :
            (app->snapshot.active_request || app->request_mode == FibRequestModeMarkets) ? 52U :
                                                                                           64U;
        widget_add_text_scroll_element(
            app->status_widget, 0U, 0U, 128U, text_height, furi_string_get_cstr(app->status_text));
    }
    if(app->request_mode == FibRequestModeRadioPlaying ||
       app->request_mode == FibRequestModeRadioStopped) {
        widget_add_button_element(
            app->status_widget, GuiButtonTypeLeft, "Stations", fib_app_radio_stations_button, app);
        widget_add_button_element(
            app->status_widget,
            GuiButtonTypeCenter,
            app->request_mode == FibRequestModeRadioPlaying ? "Stop" : "Play",
            fib_app_cancel_button,
            app);
    } else if(app->request_mode == FibRequestModeMarkets && app->market_has_price) {
        widget_add_button_element(
            app->status_widget, GuiButtonTypeCenter, "Refresh", fib_app_cancel_button, app);
    } else if(app->snapshot.active_request) {
        widget_add_button_element(
            app->status_widget, GuiButtonTypeCenter, "Cancel", fib_app_cancel_button, app);
    } else if(
        app->request_mode == FibRequestModeMarkets &&
        app->pending_action == FibPendingActionNone) {
        widget_add_button_element(
            app->status_widget, GuiButtonTypeCenter, "Refresh", fib_app_cancel_button, app);
    }
}

static void fib_app_show_status(FibApp* app, bool connection_info) {
    app->showing_connection_info = connection_info;
    fib_app_render_status(app);
    app->current_view = FibViewStatus;
    view_dispatcher_switch_to_view(app->view_dispatcher, FibViewStatus);
}

static void fib_app_session_updated(void* context) {
    FibApp* app = context;
    if(app && app->view_dispatcher) {
        view_dispatcher_send_custom_event(app->view_dispatcher, FibCustomEventSessionUpdated);
    }
}

static bool fib_app_internet_ready(const BridgeSessionSnapshot* snapshot) {
    return snapshot->usb_connected && snapshot->helper_present && snapshot->selected_major != 0U &&
           (snapshot->permission == BridgePermissionAllowedOnce ||
            snapshot->permission == BridgePermissionAllowedAlways) &&
           !snapshot->active_request;
}

static bool fib_app_custom_event(void* context, uint32_t event) {
    FibApp* app = context;
    if(!app || event != FibCustomEventSessionUpdated) return false;
    bridge_session_get_snapshot(app->session, &app->snapshot);

    if(app->request_mode == FibRequestModeRadioPlaying &&
       radio_player_is_running(app->radio_player) &&
       app->snapshot.state == BridgeSessionStateComplete && !app->snapshot.active_request) {
        bridge_session_request_radio(app->session, app->url_buffer, 30000U);
        bridge_session_get_snapshot(app->session, &app->snapshot);
    }

    if(app->pending_action != FibPendingActionNone && fib_app_internet_ready(&app->snapshot)) {
        const FibPendingAction action = app->pending_action;
        app->pending_action = FibPendingActionNone;
        fib_app_continue_pending_action(app, action);
        bridge_session_get_snapshot(app->session, &app->snapshot);
    }

    if(app->request_waiting_for_ready && app->snapshot.usb_connected &&
       app->snapshot.helper_present && app->snapshot.selected_major != 0U &&
       (app->snapshot.permission == BridgePermissionAllowedOnce ||
        app->snapshot.permission == BridgePermissionAllowedAlways) &&
       !app->snapshot.active_request) {
        app->request_waiting_for_ready = false;
        bridge_session_request_get(app->session, app->url_buffer, FIB_DEFAULT_REQUEST_TIMEOUT_MS);
        bridge_session_get_snapshot(app->session, &app->snapshot);
    }

    if(app->request_mode == FibRequestModeWeatherSearch &&
       app->snapshot.state == BridgeSessionStateComplete && !app->weather_results_shown) {
        app->weather_results_shown = true;
        app->location_count = 0U;
        if(app->snapshot.http_status == 200U) {
            app->location_count = fib_app_parse_locations(app, app->snapshot.preview);
        }
        if(app->location_count != 0U) {
            submenu_reset(app->weather_results);
            submenu_set_header(app->weather_results, "Select Location");
            for(uint8_t index = 0U; index < app->location_count; ++index) {
                submenu_add_item(
                    app->weather_results,
                    app->locations[index].label,
                    index,
                    fib_app_weather_selected,
                    app);
            }
            app->current_view = FibViewWeatherResults;
            view_dispatcher_switch_to_view(app->view_dispatcher, FibViewWeatherResults);
        } else if(app->current_view == FibViewStatus) {
            fib_app_render_status(app);
        }
    } else if(
        app->request_mode == FibRequestModeRadioSearch &&
        app->snapshot.state == BridgeSessionStateComplete && !app->radio_results_shown) {
        app->radio_results_shown = true;
        app->station_count = app->snapshot.http_status == 200U ?
                                 fib_app_parse_radio_stations(app, app->snapshot.preview) :
                                 0U;
        if(app->station_count == 0U) {
            app->station_count = fib_app_radio_country_fallback(app);
        }
        if(app->station_count != 0U) {
            submenu_reset(app->radio_stations);
            submenu_set_header(app->radio_stations, "Nearby MP3 Stations");
            for(uint8_t index = 0U; index < app->station_count; ++index) {
                submenu_add_item(
                    app->radio_stations,
                    app->stations[index].label,
                    index,
                    fib_app_radio_selected,
                    app);
            }
            app->current_view = FibViewRadioStations;
            view_dispatcher_switch_to_view(app->view_dispatcher, FibViewRadioStations);
        } else if(app->current_view == FibViewStatus) {
            fib_app_render_status(app);
        }
    } else if(app->current_view == FibViewStatus) {
        fib_app_render_status(app);
    }
    return true;
}

static void fib_app_tick(void* context) {
    FibApp* app = context;
    if(!app || !app->session) return;
    bridge_session_tick(app->session);
    const uint32_t now = furi_get_tick();
    if(app->request_mode == FibRequestModeMarkets && app->current_view == FibViewStatus &&
       app->market_has_price &&
       (uint32_t)(now - app->market_last_refresh_tick) >=
           furi_ms_to_ticks(FIB_MARKET_AUTO_REFRESH_MS)) {
        bridge_session_get_snapshot(app->session, &app->snapshot);
        if(fib_app_internet_ready(&app->snapshot)) {
            app->market_last_refresh_tick = now;
            fib_app_request_or_wait(app, app->url_buffer);
        }
    }
    if(app->request_mode != FibRequestModeRadioPlaying ||
       !radio_player_is_running(app->radio_player))
        return;
    bridge_session_get_snapshot(app->session, &app->snapshot);
    if(!app->snapshot.active_request && app->snapshot.usb_connected &&
       app->snapshot.helper_present && app->snapshot.selected_major != 0U &&
       (app->snapshot.permission == BridgePermissionAllowedOnce ||
        app->snapshot.permission == BridgePermissionAllowedAlways) &&
       (uint32_t)(now - app->radio_last_retry_tick) >= furi_ms_to_ticks(500U)) {
        app->radio_last_retry_tick = now;
        bridge_session_request_radio(app->session, app->url_buffer, 30000U);
    }
    if(app->current_view == FibViewStatus &&
       (uint32_t)(now - app->radio_last_ui_tick) >= furi_ms_to_ticks(500U)) {
        app->radio_last_ui_tick = now;
        fib_app_render_status(app);
    }
}

static uint32_t fib_app_back_to_root(void* context) {
    UNUSED(context);
    FibApp* app = fib_app_active;
    FibView destination = FibViewMenu;
    if(app) {
        fib_app_stop_radio(app);
        if(app->request_mode == FibRequestModeRadioStopped) {
            app->request_mode = FibRequestModeNormal;
        }
        app->pending_action = FibPendingActionNone;
        app->request_waiting_for_ready = false;
        destination = app->navigation_root == FibViewToolbox ? FibViewToolbox : FibViewMenu;
        app->current_view = destination;
    }
    return destination;
}

static uint32_t fib_app_back_to_menu(void* context) {
    UNUSED(context);
    FibApp* app = fib_app_active;
    if(app) app->navigation_root = FibViewMenu;
    return fib_app_back_to_root(NULL);
}

static uint32_t fib_app_back_to_toolbox(void* context) {
    UNUSED(context);
    FibApp* app = fib_app_active;
    if(app) app->navigation_root = FibViewToolbox;
    return fib_app_back_to_root(NULL);
}

static uint32_t fib_app_back_from_status(void* context) {
    UNUSED(context);
    FibApp* app = fib_app_active;
    if(app && app->request_mode == FibRequestModeMarkets &&
       app->pending_action != FibPendingActionMarkets) {
        bridge_session_cancel(app->session);
        app->pending_action = FibPendingActionNone;
        app->request_waiting_for_ready = false;
        app->current_view = FibViewMarkets;
        return FibViewMarkets;
    }
    if(app && (app->request_mode == FibRequestModeRadioPlaying ||
               app->request_mode == FibRequestModeRadioStopped)) {
        fib_app_stop_radio(app);
        app->request_mode = FibRequestModeRadioSearch;
        app->current_view = FibViewRadioStations;
        return FibViewRadioStations;
    }
    return fib_app_back_to_root(context);
}

static uint32_t fib_app_back_from_input(void* context) {
    UNUSED(context);
    FibApp* app = fib_app_active;
    if(app && app->request_mode == FibRequestModeMarketSearch) {
        app->request_mode = FibRequestModeMarkets;
        app->current_view = FibViewMarkets;
        return FibViewMarkets;
    }
    return fib_app_back_to_root(context);
}

static void fib_app_status_exited(void* context) {
    UNUSED(context);
    FibApp* app = fib_app_active;
    if(app && (app->request_mode == FibRequestModeRadioPlaying ||
               radio_player_is_running(app->radio_player))) {
        fib_app_stop_radio(app);
    }
}

static uint32_t fib_app_exit(void* context) {
    UNUSED(context);
    return VIEW_NONE;
}

static bool fib_app_url_validator(const char* text, FuriString* error, void* context) {
    UNUSED(context);
    if(!text || strncmp(text, "https://", 8U) != 0) {
        furi_string_set_str(error, "HTTPS only");
        return false;
    }
    const size_t length = strlen(text);
    if(length < 9U || length > FIB_MAX_URL_LENGTH) {
        furi_string_set_str(error, "Invalid URL length");
        return false;
    }
    for(size_t index = 0U; index < length; ++index) {
        const uint8_t value = (uint8_t)text[index];
        if(value < 0x20U || value == 0x7FU) {
            furi_string_set_str(error, "URL has control characters");
            return false;
        }
    }
    return true;
}

static bool fib_app_search_validator(const char* text, FuriString* error, void* context) {
    UNUSED(context);
    const size_t length = text ? strlen(text) : 0U;
    if(length < 2U || length > FIB_SEARCH_QUERY_SIZE) {
        furi_string_set_str(error, "Enter 2-64 characters");
        return false;
    }
    return true;
}

static bool fib_app_market_validator(const char* text, FuriString* error, void* context) {
    UNUSED(context);
    char symbol[MARKETS_MAX_SYMBOL_LENGTH + 1U];
    char url[128];
    if(!markets_build_coin_request(text, symbol, sizeof(symbol), url, sizeof(url))) {
        furi_string_set_str(error, "Use 2-20 letters or numbers");
        return false;
    }
    return true;
}

static void fib_app_request_or_wait(FibApp* app, const char* url) {
    if(!app || !url) return;
    if(url != app->url_buffer) {
        snprintf(app->url_buffer, sizeof(app->url_buffer), "%s", url);
    }

    app->request_waiting_for_ready = false;
    const bool started = app->request_mode == FibRequestModeRadioPlaying ?
                             bridge_session_request_radio(
                                 app->session, app->url_buffer, FIB_DEFAULT_REQUEST_TIMEOUT_MS) :
                             bridge_session_request_get(
                                 app->session, app->url_buffer, FIB_DEFAULT_REQUEST_TIMEOUT_MS);
    if(started) {
        return;
    }

    bridge_session_get_snapshot(app->session, &app->snapshot);
    const bool waiting_for_connection =
        !app->snapshot.active_request &&
        (!app->snapshot.usb_connected || !app->snapshot.helper_present ||
         app->snapshot.selected_major == 0U ||
         (app->snapshot.permission != BridgePermissionAllowedOnce &&
          app->snapshot.permission != BridgePermissionAllowedAlways));
    app->request_waiting_for_ready = waiting_for_connection;
}

static void fib_app_open_search_input(
    FibApp* app,
    FibRequestMode mode,
    const char* header,
    size_t buffer_size) {
    app->request_mode = mode;
    app->input_buffer[0] = '\0';
    text_input_set_header_text(app->url_input, header);
    text_input_set_minimum_length(app->url_input, 2U);
    text_input_set_validator(
        app->url_input,
        mode == FibRequestModeMarketSearch ? fib_app_market_validator : fib_app_search_validator,
        app);
    text_input_set_result_callback(
        app->url_input, fib_app_url_submitted, app, app->input_buffer, buffer_size, true);
    app->current_view = FibViewUrlInput;
    view_dispatcher_switch_to_view(app->view_dispatcher, FibViewUrlInput);
}

static void fib_app_continue_pending_action(FibApp* app, FibPendingAction action) {
    if(!app) return;

    switch(action) {
    case FibPendingActionMarkets:
        app->current_view = FibViewMarkets;
        view_dispatcher_switch_to_view(app->view_dispatcher, FibViewMarkets);
        break;
    case FibPendingActionSample:
        app->request_mode = FibRequestModeNormal;
        fib_app_request_or_wait(app, FIB_SAMPLE_URL);
        fib_app_show_status(app, false);
        break;
    case FibPendingActionDateTime:
        app->request_mode = FibRequestModeNormal;
        fib_app_request_or_wait(app, FIB_TIME_URL);
        fib_app_show_status(app, false);
        break;
    case FibPendingActionWikipediaInput:
        fib_app_open_search_input(
            app, FibRequestModeWikipedia, "Search Wikipedia", FIB_SEARCH_QUERY_SIZE + 1U);
        break;
    case FibPendingActionWeatherInput:
        fib_app_open_search_input(
            app, FibRequestModeWeatherSearch, "City or District", FIB_WEATHER_QUERY_SIZE + 1U);
        break;
    case FibPendingActionNationalToday:
        app->request_mode = FibRequestModeNationalToday;
        fib_app_request_or_wait(app, FIB_NATIONAL_TODAY_URL);
        fib_app_show_status(app, false);
        break;
    case FibPendingActionIssLocation:
        app->request_mode = FibRequestModeIssLocation;
        fib_app_request_or_wait(app, FIB_ISS_URL);
        fib_app_show_status(app, false);
        break;
    case FibPendingActionRadioCountryInput:
        fib_app_open_search_input(
            app, FibRequestModeRadioSearch, "Country Name", FIB_RADIO_COUNTRY_SIZE + 1U);
        break;
    case FibPendingActionCustomUrlInput:
        app->request_mode = FibRequestModeNormal;
        snprintf(app->input_buffer, sizeof(app->input_buffer), "https://");
        text_input_set_header_text(app->url_input, "HTTPS URL");
        text_input_set_minimum_length(app->url_input, 9U);
        text_input_set_validator(app->url_input, fib_app_url_validator, app);
        text_input_set_result_callback(
            app->url_input,
            fib_app_url_submitted,
            app,
            app->input_buffer,
            sizeof(app->input_buffer),
            false);
        app->current_view = FibViewUrlInput;
        view_dispatcher_switch_to_view(app->view_dispatcher, FibViewUrlInput);
        break;
    case FibPendingActionNone:
    default:
        break;
    }
}

static void
    fib_app_start_when_ready(FibApp* app, FibPendingAction action, FibRequestMode waiting_mode) {
    bridge_session_get_snapshot(app->session, &app->snapshot);
    app->request_mode = waiting_mode;
    if(fib_app_internet_ready(&app->snapshot)) {
        fib_app_continue_pending_action(app, action);
    } else {
        app->pending_action = action;
        fib_app_show_status(app, false);
    }
}

static void fib_app_url_submitted(void* context) {
    FibApp* app = context;
    if(!app) return;
    if(app->request_mode == FibRequestModeWikipedia) {
        if(!fib_app_build_search_url(app->input_buffer, app->url_buffer, sizeof(app->url_buffer))) {
            return;
        }
    } else if(app->request_mode == FibRequestModeMarketSearch) {
        if(!markets_build_coin_request(
               app->input_buffer,
               app->market_symbol,
               sizeof(app->market_symbol),
               app->url_buffer,
               sizeof(app->url_buffer)))
            return;
        app->market_index = MARKETS_SEARCH_INDEX;
        app->market_has_price = false;
        app->market_last_refresh_tick = furi_get_tick();
        app->market_last_response_id = 0U;
        app->request_mode = FibRequestModeMarkets;
    } else if(app->request_mode == FibRequestModeWeatherSearch) {
        char encoded[FIB_WEATHER_QUERY_SIZE * 3U + 1U];
        if(!fib_app_percent_encode(app->input_buffer, encoded, sizeof(encoded))) return;
        snprintf(
            app->url_buffer, sizeof(app->url_buffer), "%s%s", FIB_GEOCODING_URL_PREFIX, encoded);
        app->weather_results_shown = false;
    } else if(app->request_mode == FibRequestModeRadioSearch) {
        char encoded[FIB_RADIO_COUNTRY_SIZE * 3U + 1U];
        const char* country = app->input_buffer;
        if(strcmp(country, "UK") == 0 || strcmp(country, "uk") == 0) {
            country = "GB";
        }
        if(strcmp(country, "Turkey") == 0 || strcmp(country, "turkey") == 0) {
            country = "T\xC3\xBCrkiye";
        }
        if(!fib_app_percent_encode(country, encoded, sizeof(encoded))) return;
        const char* field = strlen(country) == 2U ? "countrycode=" : "country=";
        snprintf(
            app->url_buffer, sizeof(app->url_buffer), "%s%s%s", FIB_RADIO_URL_BASE, field, encoded);
        app->radio_results_shown = false;
    } else {
        snprintf(app->url_buffer, sizeof(app->url_buffer), "%s", app->input_buffer);
    }
    fib_app_request_or_wait(app, app->url_buffer);
    fib_app_show_status(app, false);
}

static void fib_app_weather_selected(void* context, uint32_t index) {
    FibApp* app = context;
    if(!app || index >= app->location_count) return;
    const FibWeatherLocation* location = &app->locations[index];
    snprintf(
        app->url_buffer,
        sizeof(app->url_buffer),
        FIB_WEATHER_URL_FORMAT,
        location->latitude,
        location->longitude);
    app->request_mode = FibRequestModeWeatherForecast;
    fib_app_request_or_wait(app, app->url_buffer);
    fib_app_show_status(app, false);
}

static void fib_app_radio_selected(void* context, uint32_t index) {
    FibApp* app = context;
    if(!app || index >= app->station_count) return;
    const FibRadioStation* station = &app->stations[index];
    snprintf(app->url_buffer, sizeof(app->url_buffer), "%s", station->url);
    snprintf(
        app->search_result,
        sizeof(app->search_result),
        "%s\n%d kbps MP3",
        station->label,
        station->bitrate);
    if(!fib_app_start_selected_radio(app)) {
        app->request_mode = FibRequestModeNormal;
        snprintf(
            app->search_result,
            sizeof(app->search_result),
            "Speaker or memory is busy. Close other audio apps and try again.");
        widget_reset(app->status_widget);
        widget_add_text_scroll_element(app->status_widget, 0, 0, 128, 64, app->search_result);
        app->current_view = FibViewStatus;
        view_dispatcher_switch_to_view(app->view_dispatcher, FibViewStatus);
        return;
    }
    fib_app_show_status(app, false);
}

static void fib_app_market_selected(void* context, uint32_t index) {
    FibApp* app = context;
    if(!app || index > MARKETS_SEARCH_INDEX) return;
    if(index == MARKETS_SEARCH_INDEX) {
        fib_app_open_search_input(app, FibRequestModeMarketSearch, "Coin Symbol (e.g. SOL)", 21U);
        return;
    }
    app->market_index = index;
    app->market_has_price = false;
    app->market_last_refresh_tick = furi_get_tick();
    app->market_last_response_id = 0U;
    app->request_mode = FibRequestModeMarkets;
    fib_app_request_or_wait(app, markets_url(index));
    fib_app_show_status(app, false);
}

static void fib_app_menu_selected(void* context, uint32_t index) {
    FibApp* app = context;
    if(!app) return;

    app->navigation_root = FibViewMenu;
    switch(index) {
    case FibMenuToolbox:
        app->navigation_root = FibViewToolbox;
        app->current_view = FibViewToolbox;
        view_dispatcher_switch_to_view(app->view_dispatcher, FibViewToolbox);
        break;
    case FibMenuTestConnection:
        app->request_mode = FibRequestModeNormal;
        bridge_session_ping(app->session);
        fib_app_show_status(app, false);
        break;
    case FibMenuDownloadSample:
        fib_app_start_when_ready(app, FibPendingActionSample, FibRequestModeNormal);
        break;
    case FibMenuGetDateTime:
        fib_app_start_when_ready(app, FibPendingActionDateTime, FibRequestModeNormal);
        break;
    case FibMenuCustomUrl:
        fib_app_start_when_ready(app, FibPendingActionCustomUrlInput, FibRequestModeNormal);
        break;
    case FibMenuConnectionInfo:
        app->request_mode = FibRequestModeNormal;
        fib_app_show_status(app, true);
        break;
    default:
        break;
    }
}

static void fib_app_toolbox_selected(void* context, uint32_t index) {
    FibApp* app = context;
    if(!app) return;

    app->navigation_root = FibViewToolbox;
    switch(index) {
    case FibToolboxInformationSearch:
        fib_app_start_when_ready(app, FibPendingActionWikipediaInput, FibRequestModeWikipedia);
        break;
    case FibToolboxWeather:
        fib_app_start_when_ready(app, FibPendingActionWeatherInput, FibRequestModeWeatherSearch);
        break;
    case FibToolboxNationalToday:
        fib_app_start_when_ready(app, FibPendingActionNationalToday, FibRequestModeNationalToday);
        break;
    case FibToolboxIssLocation:
        fib_app_start_when_ready(app, FibPendingActionIssLocation, FibRequestModeIssLocation);
        break;
    case FibToolboxInternetRadio:
        fib_app_start_when_ready(
            app, FibPendingActionRadioCountryInput, FibRequestModeRadioSearch);
        break;
    case FibToolboxMarkets:
        fib_app_start_when_ready(app, FibPendingActionMarkets, FibRequestModeMarkets);
        break;
    default:
        break;
    }
}

static FibApp* fib_app_alloc(void) {
    FibApp* app = malloc(sizeof(FibApp));
    if(!app) return NULL;
    memset(app, 0, sizeof(*app));
    fib_app_active = app;

    app->gui = furi_record_open(RECORD_GUI);
    app->view_dispatcher = view_dispatcher_alloc();
    app->menu = submenu_alloc();
    app->toolbox = submenu_alloc();
    app->markets = submenu_alloc();
    app->weather_results = submenu_alloc();
    app->radio_stations = submenu_alloc();
    app->status_widget = widget_alloc();
    app->url_input = text_input_alloc();
    app->status_text = furi_string_alloc();
    if(!app->gui || !app->view_dispatcher || !app->menu || !app->toolbox || !app->markets ||
       !app->weather_results || !app->radio_stations || !app->status_widget || !app->url_input ||
       !app->status_text) {
        return app;
    }

    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, fib_app_custom_event);
    view_dispatcher_set_tick_event_callback(app->view_dispatcher, fib_app_tick, FIB_UI_TICK_MS);

    submenu_set_header(app->menu, "USB Internet Bridge");
    submenu_set_header(app->toolbox, "Toolbox");
    submenu_set_header(app->markets, "Markets");
    for(unsigned i = 0; i < MARKETS_COUNT; ++i) {
        submenu_add_item(app->markets, markets_name(i), i, fib_app_market_selected, app);
    }
    submenu_add_item(
        app->markets, "Search Any Coin", MARKETS_SEARCH_INDEX, fib_app_market_selected, app);
    view_set_previous_callback(submenu_get_view(app->markets), fib_app_back_to_toolbox);
    submenu_add_item(
        app->menu, "Test Connection", FibMenuTestConnection, fib_app_menu_selected, app);
    submenu_add_item(
        app->menu, "Get Sample Text", FibMenuDownloadSample, fib_app_menu_selected, app);
    submenu_add_item(
        app->menu, "Get Date and Time", FibMenuGetDateTime, fib_app_menu_selected, app);
    submenu_add_item(app->menu, "Toolbox", FibMenuToolbox, fib_app_menu_selected, app);
    submenu_add_item(
        app->menu, "Custom URL Request", FibMenuCustomUrl, fib_app_menu_selected, app);
    submenu_add_item(
        app->menu, "Connection Info", FibMenuConnectionInfo, fib_app_menu_selected, app);
    submenu_add_item(
        app->toolbox,
        "Search Wikipedia",
        FibToolboxInformationSearch,
        fib_app_toolbox_selected,
        app);
    submenu_add_item(app->toolbox, "Weather", FibToolboxWeather, fib_app_toolbox_selected, app);
    submenu_add_item(
        app->toolbox, "National Today", FibToolboxNationalToday, fib_app_toolbox_selected, app);
    submenu_add_item(
        app->toolbox, "Where is the ISS?", FibToolboxIssLocation, fib_app_toolbox_selected, app);
    submenu_add_item(
        app->toolbox, "Internet Radio", FibToolboxInternetRadio, fib_app_toolbox_selected, app);
    submenu_add_item(app->toolbox, "Markets", FibToolboxMarkets, fib_app_toolbox_selected, app);
    view_set_previous_callback(submenu_get_view(app->menu), fib_app_exit);
    view_set_previous_callback(submenu_get_view(app->toolbox), fib_app_back_to_menu);
    view_set_previous_callback(submenu_get_view(app->weather_results), fib_app_back_to_toolbox);
    view_set_previous_callback(submenu_get_view(app->radio_stations), fib_app_back_to_toolbox);
    view_set_previous_callback(widget_get_view(app->status_widget), fib_app_back_from_status);
    view_set_exit_callback(widget_get_view(app->status_widget), fib_app_status_exited);

    snprintf(app->input_buffer, sizeof(app->input_buffer), "https://");
    text_input_set_header_text(app->url_input, "HTTPS URL");
    text_input_set_minimum_length(app->url_input, 9U);
    text_input_set_validator(app->url_input, fib_app_url_validator, app);
    text_input_set_result_callback(
        app->url_input,
        fib_app_url_submitted,
        app,
        app->input_buffer,
        sizeof(app->input_buffer),
        false);
    view_set_previous_callback(text_input_get_view(app->url_input), fib_app_back_from_input);

    view_dispatcher_add_view(app->view_dispatcher, FibViewMenu, submenu_get_view(app->menu));
    view_dispatcher_add_view(app->view_dispatcher, FibViewToolbox, submenu_get_view(app->toolbox));
    view_dispatcher_add_view(app->view_dispatcher, FibViewMarkets, submenu_get_view(app->markets));
    view_dispatcher_add_view(
        app->view_dispatcher, FibViewStatus, widget_get_view(app->status_widget));
    view_dispatcher_add_view(
        app->view_dispatcher, FibViewUrlInput, text_input_get_view(app->url_input));
    view_dispatcher_add_view(
        app->view_dispatcher, FibViewWeatherResults, submenu_get_view(app->weather_results));
    view_dispatcher_add_view(
        app->view_dispatcher, FibViewRadioStations, submenu_get_view(app->radio_stations));
    app->views_added = true;

    app->session = bridge_session_alloc(fib_app_session_updated, app);
    if(!app->session) return app;
    app->radio_player = radio_player_alloc();
    if(!app->radio_player) return app;
    bridge_session_start(app->session);
    app->current_view = FibViewMenu;
    app->navigation_root = FibViewMenu;
    return app;
}

static bool fib_app_is_complete(const FibApp* app) {
    return app && app->gui && app->view_dispatcher && app->menu && app->toolbox && app->markets &&
           app->status_widget && app->weather_results && app->radio_stations && app->url_input &&
           app->status_text && app->session && app->radio_player;
}

static void fib_app_free(FibApp* app) {
    if(!app) return;
    if(fib_app_active == app) fib_app_active = NULL;
    if(app->session) bridge_session_set_body_callback(app->session, NULL, NULL);
    if(app->radio_player) radio_player_free(app->radio_player);
    if(app->session) bridge_session_free(app->session);
    if(app->view_dispatcher) {
        if(app->views_added) {
            view_dispatcher_remove_view(app->view_dispatcher, FibViewMenu);
            view_dispatcher_remove_view(app->view_dispatcher, FibViewToolbox);
            view_dispatcher_remove_view(app->view_dispatcher, FibViewMarkets);
            view_dispatcher_remove_view(app->view_dispatcher, FibViewStatus);
            view_dispatcher_remove_view(app->view_dispatcher, FibViewUrlInput);
            view_dispatcher_remove_view(app->view_dispatcher, FibViewWeatherResults);
            view_dispatcher_remove_view(app->view_dispatcher, FibViewRadioStations);
        }
        view_dispatcher_free(app->view_dispatcher);
    }
    if(app->url_input) text_input_free(app->url_input);
    if(app->weather_results) submenu_free(app->weather_results);
    if(app->radio_stations) submenu_free(app->radio_stations);
    if(app->status_widget) widget_free(app->status_widget);
    if(app->menu) submenu_free(app->menu);
    if(app->toolbox) submenu_free(app->toolbox);
    if(app->markets) submenu_free(app->markets);
    if(app->status_text) furi_string_free(app->status_text);
    if(app->gui) furi_record_close(RECORD_GUI);
    free(app);
}

int32_t usb_internet_bridge_main(void* context) {
    UNUSED(context);
    FibApp* app = fib_app_alloc();
    if(!fib_app_is_complete(app)) {
        FURI_LOG_E(TAG, "Application allocation failed");
        fib_app_free(app);
        return -1;
    }

    view_dispatcher_switch_to_view(app->view_dispatcher, FibViewMenu);
    view_dispatcher_run(app->view_dispatcher);
    fib_app_free(app);
    return 0;
}
