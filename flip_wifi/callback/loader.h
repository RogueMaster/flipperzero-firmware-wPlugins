#pragma once
#include <flip_wifi.h>

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
    FlipperHTTP *fhttp;
};
bool loader_view_alloc(void *context);
void loader_view_free(void *context);
void loader_switch_to_view(FlipWiFiApp *app, char *title, DataLoaderFetch fetcher, DataLoaderParser parser, size_t request_count, ViewNavigationCallback back, uint32_t view_id);
void loader_draw_callback(Canvas *canvas, void *model);
void loader_init(View *view);
void loader_widget_set_text(char *message, Widget **widget);
void loader_free_model(View *view);
bool loader_custom_event_callback(void *context, uint32_t index);
