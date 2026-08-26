#pragma once

#include <furi.h>

typedef struct AppContext AppContext;

/** Supported UI languages. */
typedef enum {
    LocLangEn,
    LocLangIt,
    LocLangCount,
} LocLang;

/**
 * Stable string identifiers.
 *
 * Every UI string is referenced by one of these keys; the translation tables
 * in locale.c must keep the exact same order. Adding a language means adding
 * one table (no scene changes needed).
 */
typedef enum {
    // Main menu
    LocKeyMainConnection,
    LocKeyMainAddCall,
    LocKeyMainCallList,

    // WiFi menu / connect
    LocKeyWifiScanNetworks,
    LocKeyWifiSavedNetworks,
    LocKeyWifiConnectedTo,
    LocKeyWifiDisconnect,
    LocKeyWifiDisconnected,
    LocKeyWifiPasswordHeader,
    LocKeyWifiConnectedOk,
    LocKeyWifiConnectFailed,

    // WiFi scan
    LocKeyScanNoResults,
    LocKeyScanInProgress,
    LocKeyScanRetry,
    LocKeyScanRefresh,
    LocKeyScanFoundHeader,

    // Saved networks
    LocKeyNoSavedNetworks,

    // Call form
    LocKeyFormUrl,
    LocKeyFormProtocol,
    LocKeyFormMethod,
    LocKeyFormQuery,
    LocKeyFormHeaders,
    LocKeyFormBody,
    LocKeyFormSave,
    LocKeyFormDelete,
    LocKeyInputUrlHeader,
    LocKeyInputQueryHeader,
    LocKeyInputHeadersHeader,
    LocKeyInputBodyHeader,

    // Call list / detail
    LocKeyCallListEmpty,
    LocKeyCallListHeader,
    LocKeyDetailRun,
    LocKeyDetailEdit,
    LocKeyDetailSending,
    LocKeyDetailSendFailed,
    LocKeyDetailError,
    LocKeyDetailResultFmt,
    LocKeyDetailEmptyResponse,
    LocKeyDetailTruncated,
    LocKeyDetailProgressFmt,

    // Request runner errors (shown in the UI)
    LocKeyRunnerNoBoard,
    LocKeyRunnerSendFailed,
    LocKeyRunnerBoardError,
    LocKeyRunnerTimeoutNoReply,
    LocKeyRunnerTimeoutPartial,

    LocKeyCount,
} LocKey;

/** Load the language from the settings file (default: Italian). */
void locale_init(AppContext* app);

/** Return the string for the key in the current language. */
const char* locale_get(const AppContext* app, LocKey key);

/** Switch the language and persist it to the settings file. */
void locale_set(AppContext* app, LocLang lang);
