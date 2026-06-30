#ifndef WEB_CRAWLER_CALLBACK_H
#define WEB_CRAWLER_CALLBACK_H
#include "web_crawler.h"
#include <flip_storage/web_crawler_storage.h>

void web_crawler_http_method_change(VariableItem *item);
uint32_t web_crawler_back_to_main_callback(void *context);
void free_all(WebCrawlerApp *app);

/**
 * @brief      Navigation callback to handle exiting from other views to the submenu.
 * @param      context   The context - WebCrawlerApp object.
 * @return     WebCrawlerViewSubmenu
 */
uint32_t web_crawler_back_to_configure_callback(void *context);

uint32_t web_crawler_back_to_wifi_callback(void *context);

uint32_t web_crawler_back_to_request_callback(void *context);

/**
 * @brief      Navigation callback to handle exiting the app from the main submenu.
 * @param      context   The context - unused
 * @return     VIEW_NONE to exit the app
 */
uint32_t web_crawler_exit_app_callback(void *context);

/**
 * @brief      Handle submenu item selection.
 * @param      context   The context - WebCrawlerApp object.
 * @param      index     The WebCrawlerSubmenuIndex item that was clicked.
 */
void web_crawler_submenu_callback(void *context, uint32_t index);

/**
 * @brief      Configuration enter callback to handle different items.
 * @param      context   The context - WebCrawlerApp object.
 * @param      index     The index of the item that was clicked.
 */
void web_crawler_wifi_enter_callback(void *context, uint32_t index);

/**
 * @brief      Configuration enter callback to handle different items.
 * @param      context   The context - WebCrawlerApp object.
 * @param      index     The index of the item that was clicked.
 */
void web_crawler_file_enter_callback(void *context, uint32_t index);

/**
 * @brief      Configuration enter callback to handle different items.
 * @param      context   The context - WebCrawlerApp object.
 * @param      index     The index of the item that was clicked.
 */
void web_crawler_request_enter_callback(void *context, uint32_t index);

/**
 * @brief      Callback for when the user finishes entering the URL.
 * @param      context   The context - WebCrawlerApp object.
 */
void web_crawler_set_path_updated(void *context);

/**
 * @brief      Callback for when the user finishes entering the headers
 * @param      context   The context - WebCrawlerApp object.
 */
void web_crawler_set_headers_updated(void *context);

/**
 * @brief      Callback for when the user finishes entering the payload.
 * @param      context   The context - WebCrawlerApp object.
 */
void web_crawler_set_payload_updated(void *context);

/**
 * @brief      Callback for when the user finishes entering the SSID.
 * @param      context   The context - WebCrawlerApp object.
 */
void web_crawler_set_ssid_updated(void *context);

/**
 * @brief      Callback for when the user finishes entering the Password.
 * @param      context   The context - WebCrawlerApp object.
 */
void web_crawler_set_password_update(void *context);

/**
 * @brief      Callback for when the user finishes entering the File Type.
 * @param      context   The context - WebCrawlerApp object.
 */
void web_crawler_set_file_type_update(void *context);

/**
 * @brief      Callback for when the user finishes entering the File Rename.
 * @param      context   The context - WebCrawlerApp object.
 */
void web_crawler_set_file_rename_update(void *context);

/**
 * @brief      Handler for File Delete configuration item click.
 * @param      context  The context - WebCrawlerApp object.
 * @param      index    The index of the item that was clicked.
 */
void web_crawler_setting_item_file_delete_clicked(void *context, uint32_t index);

void web_crawler_setting_item_file_read_clicked(void *context, uint32_t index);

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

typedef enum WebCrawlerCustomEvent WebCrawlerCustomEvent;
enum WebCrawlerCustomEvent
{
    WebCrawlerCustomEventProcess,
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

void web_crawler_generic_switch_to_view(WebCrawlerApp *app, char *title, DataLoaderFetch fetcher, DataLoaderParser parser, size_t request_count, ViewNavigationCallback back, uint32_t view_id);

void web_crawler_loader_draw_callback(Canvas *canvas, void *model);

void web_crawler_loader_init(View *view);

void web_crawler_loader_free_model(View *view);

bool web_crawler_custom_event_callback(void *context, uint32_t index);
#endif