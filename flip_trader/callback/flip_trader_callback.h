#ifndef FLIP_TRADER_CALLBACK_H
#define FLIP_TRADER_CALLBACK_H

#define MAX_TOKENS 32 // Adjust based on expected JSON size (25)
#include <flip_trader.h>
#include <flip_storage/flip_trader_storage.h>

// hold the price of the asset
extern char asset_price[64];
extern bool sent_get_request;
extern bool get_request_success;
extern bool request_processed;

void callback_submenu_choices(void *context, uint32_t index);
void text_updated_ssid(void *context);

void text_updated_password(void *context);

uint32_t callback_to_submenu(void *context);

uint32_t callback_to_wifi_settings(void *context);
uint32_t callback_to_assets_submenu(void *context);
void settings_item_selected(void *context, uint32_t index);

// Add edits by Derek Jamison
typedef enum AssetState AssetState;
enum AssetState
{
    AssetStateInitial,
    AssetStateRequested,
    AssetStateReceived,
    AssetStateParsed,
    AssetStateParseError,
    AssetStateError,
};

typedef enum FlipTraderCustomEvent FlipTraderCustomEvent;
enum FlipTraderCustomEvent
{
    FlipTraderCustomEventProcess,
};

typedef struct AssetLoaderModel AssetLoaderModel;
typedef bool (*AssetLoaderFetch)(AssetLoaderModel *model);
typedef char *(*AssetLoaderParser)(AssetLoaderModel *model);
struct AssetLoaderModel
{
    char *title;
    char *asset_text;
    AssetState asset_state;
    AssetLoaderFetch fetcher;
    AssetLoaderParser parser;
    void *parser_context;
    size_t request_index;
    size_t request_count;
    ViewNavigationCallback back_callback;
    FuriTimer *timer;
};

void flip_trader_generic_switch_to_view(FlipTraderApp *app, char *title, AssetLoaderFetch fetcher, AssetLoaderParser parser, size_t request_count, ViewNavigationCallback back, uint32_t view_id);

void flip_trader_loader_draw_callback(Canvas *canvas, void *model);

void flip_trader_loader_init(View *view);

void flip_trader_loader_free_model(View *view);

bool flip_trader_custom_event_callback(void *context, uint32_t index);
#endif // FLIP_TRADER_CALLBACK_H