#ifndef FLIP_WEATHER_PARSE_H
#define FLIP_WEATHER_PARSE_H
#include <flip_weather.h>
extern bool sent_get_request;
extern bool get_request_success;
extern bool got_ip_address;
extern bool geo_information_processed;
extern bool weather_information_processed;

// Add edits by Derek Jamison
typedef enum DataState DataState;
enum DataState
{
    DataStateInitial,
    DataStateRequested,
    DataStateReceived,
    DataStateParsed,
    DataStateParseError,
    DataStateError,
};

typedef enum FlipWeatherCustomEvent FlipWeatherCustomEvent;
enum FlipWeatherCustomEvent
{
    FlipWeatherCustomEventProcess,
};

typedef struct DataLoaderModel DataLoaderModel;
typedef bool (*DataLoaderFetch)(DataLoaderModel *model);
typedef char *(*DataLoaderParser)(DataLoaderModel *model);
struct DataLoaderModel
{
    char *title;
    char *data_text;
    DataState data_state;
    DataLoaderFetch fetcher;
    DataLoaderParser parser;
    void *parser_context;
    size_t request_index;
    size_t request_count;
    ViewNavigationCallback back_callback;
    FuriTimer *timer;
};

bool send_geo_location_request();
char *process_geo_location(DataLoaderModel *model);
bool process_geo_location_2();
char *process_weather(DataLoaderModel *model);
bool send_geo_weather_request(DataLoaderModel *model);

#endif