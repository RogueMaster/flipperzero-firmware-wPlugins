#ifndef FLIP_LIBRARY_CALLBACK_H
#define FLIP_LIBRARY_CALLBACK_H
#include <flip_library.h>
#include <flip_storage/flip_library_storage.h>

#define MAX_TOKENS 512 // Adjust based on expected JSON size

typedef enum FactState FactState;
enum FactState
{
    FactStateInitial,
    FactStateRequested,
    FactStateReceived,
    FactStateParsed,
    FactStateParseError,
    FactStateError,
};

typedef enum FlipLibraryCustomEvent FlipLibraryCustomEvent;
enum FlipLibraryCustomEvent
{
    FlipLibraryCustomEventProcess,
};

typedef struct FactLoaderModel FactLoaderModel;
typedef bool (*FactLoaderFetch)(FactLoaderModel *model);
typedef char *(*FactLoaderParser)(FactLoaderModel *model);
struct FactLoaderModel
{
    char *title;
    char *fact_text;
    FactState fact_state;
    FactLoaderFetch fetcher;
    FactLoaderParser parser;
    void *parser_context;
    size_t request_index;
    size_t request_count;
    ViewNavigationCallback back_callback;
    FuriTimer *timer;
};

extern uint32_t random_facts_index;
extern bool sent_random_fact_request;
extern bool random_fact_request_success;
extern bool random_fact_request_success_all;
extern char *random_fact;

void flip_library_generic_switch_to_view(FlipLibraryApp *app, char *title, FactLoaderFetch fetcher, FactLoaderParser parser, size_t request_count, ViewNavigationCallback back, uint32_t view_id);

void flip_library_loader_draw_callback(Canvas *canvas, void *model);

void flip_library_loader_init(View *view);

void flip_library_loader_free_model(View *view);

bool flip_library_custom_event_callback(void *context, uint32_t index);

void callback_submenu_choices(void *context, uint32_t index);

void text_updated_ssid(void *context);

void text_updated_password(void *context);

void temperature_unit_change(VariableItem *item);

void text_updated_query(void *context);

uint32_t callback_to_submenu(void *context);

uint32_t callback_to_wifi_settings(void *context);

uint32_t callback_to_random_facts(void *context);

void settings_item_selected(void *context, uint32_t index);

/**
 * @brief Navigation callback for exiting the application
 * @param context The context - unused
 * @return next view id (VIEW_NONE to exit the app)
 */
uint32_t callback_exit_app(void *context);
uint32_t callback_to_submenu_predict(void *context);
uint32_t callback_to_submenu_library(void *context);

#endif // FLIP_LIBRARY_CALLBACK_H
