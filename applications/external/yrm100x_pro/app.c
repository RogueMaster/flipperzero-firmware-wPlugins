#include "app.h"
#include <furi/core/memmgr.h>
#include "structures.h"

#undef FURI_LOG_D
#undef FURI_LOG_I
#define FURI_LOG_D(...) ((void)0)
#define FURI_LOG_I(...) ((void)0)

#define uhf_reader_log_heap(...) ((void)0)

#define UHF_WORKER_ALLOC_ATTEMPTS       4U
#define UHF_WORKER_ALLOC_RETRY_DELAY_MS 100U
#define UHF_STARTUP_SETTLE_DELAY_MS     200U

static UHFReaderApp* uhf_main_menu_app = NULL;

/* Secondary screens are session UI. Reclaim them after returning to Main. */
static void uhf_reader_release_secondary_views(UHFReaderApp* App) {
    if(!App) return;

    UHFTagWrapper* wrapper = App->YRM100XWorker ? App->YRM100XWorker->uhf_tag_wrapper : NULL;
    if(App->YRM100XWorker) uhf_worker_stop(App->YRM100XWorker);
    if(App->Timer) {
        furi_timer_stop(App->Timer);
        furi_timer_free(App->Timer);
        App->Timer = NULL;
    }
    if(wrapper) {
        uhf_tag_wrapper_reset_list(wrapper);
        uhf_tag_wrapper_set_tag(wrapper, NULL);
    }

    if(App->ViewDelete) view_delete_free(App);
    if(App->ViewDeleteSuccess) view_delete_success_free(App);
    if(App->WidgetAbout) view_about_free(App);
    if(App->ViewEpc) view_epc_free(App);
    if(App->ViewEpcInfo) view_epc_info_free(App);
    if(App->ViewBankMem) view_bank_mem_free(App);
    if(App->ViewWrite) view_write_free(App);
    if(App->SubmenuSaved) view_saved_free(App);
    if(App->SubmenuTagActions) view_tag_actions_free(App);
    if(App->VariableItemListLock) view_lock_free(App);
    if(App->SubmenuKillActions) view_kill_free(App);
    if(App->ViewUmiTest) view_umi_test_free(App);
    if(App->ViewBankSize) view_bank_size_free(App);
    if(App->ViewRewritable) view_rewritable_free(App);
    if(App->SubmenuBankTools) view_bank_tools_free(App);
    if(App->ViewClone) view_clone_free(App);
    if(App->VariableItemListClone) view_clone_banks_free(App);

    App->SaveSourceView = NULL;
    App->SaveReturnView = UHFReaderViewRead;
    App->NumberOfEpcsToRead = 0U;
    App->CurEpcIndex = 0U;
}

void uhf_reader_main_menu_enter(void* context) {
    UNUSED(context);
    UHFReaderApp* App = uhf_main_menu_app;
    if(!App || !App->Submenu) return;

    uhf_reader_release_secondary_views(App);

    const size_t free_bytes = memmgr_get_free_heap();
    const unsigned long free_tenths_kib = (unsigned long)((free_bytes * 10U) / 1024U);
    snprintf(
        App->MainMenuHeader,
        sizeof(App->MainMenuHeader),
        "YRM100X_PRO    %lu.%luk",
        free_tenths_kib / 10U,
        free_tenths_kib % 10U);
    submenu_set_header(App->Submenu, App->MainMenuHeader);
}

bool uhf_reader_ensure_worker(UHFReaderApp* App) {
    furi_assert(App);

    if(uhf_worker_is_ready(App->YRM100XWorker)) return true;

    if(App->YRM100XWorker) {
        UHF_W("APP", "discarding incomplete worker before retry");
        if(App->YRM100XWorker->uhf_tag_wrapper) {
            uhf_tag_wrapper_free(App->YRM100XWorker->uhf_tag_wrapper);
            App->YRM100XWorker->uhf_tag_wrapper = NULL;
        }
        uhf_worker_free(App->YRM100XWorker);
        App->YRM100XWorker = NULL;
    }

    for(uint8_t attempt = 0U; attempt < UHF_WORKER_ALLOC_ATTEMPTS; attempt++) {
        UHF_I(
            "APP",
            "worker alloc attempt=%u/%u",
            (unsigned int)(attempt + 1U),
            (unsigned int)UHF_WORKER_ALLOC_ATTEMPTS);
        uhf_debug_flush();

        UHFWorker* worker = uhf_worker_alloc();
        if(worker) {
            UHFTagWrapper* wrapper = uhf_tag_wrapper_alloc();
            if(wrapper) {
                worker->uhf_tag_wrapper = wrapper;
                worker->DefaultAP = bytes_to_uint32(App->ApTempBuffer, 4);
                worker->FullDumpMaxAttempts = App->SettingFullDumpAttempts;
                worker->CloneMaxAttempts = App->SettingCloneAttempts;
                worker->ReadProfile = App->SettingTagProfileIndex;
                worker->UmiMarkerRegistry = &App->UmiMarkerRegistry;
                App->YRM100XWorker = worker;

                UHF_I(
                    "APP",
                    "worker ready worker=%p module=%p uart=%p handle=%p",
                    (void*)worker,
                    (void*)worker->module,
                    (void*)worker->module->uart,
                    (void*)worker->module->uart->handle);
                uhf_debug_flush();
                return true;
            }

            UHF_E("APP", "tag wrapper allocation failed");
            uhf_worker_free(worker);
        }

        UHF_W(
            "APP",
            "worker/UART unavailable attempt=%u/%u",
            (unsigned int)(attempt + 1U),
            (unsigned int)UHF_WORKER_ALLOC_ATTEMPTS);
        uhf_debug_flush();

        if(attempt + 1U < UHF_WORKER_ALLOC_ATTEMPTS) {
            furi_delay_ms(UHF_WORKER_ALLOC_RETRY_DELAY_MS);
        }
    }

    UHF_E("APP", "worker/UART unavailable; continuing disconnected");
    uhf_debug_flush();
    return false;
}

bool uhf_reader_require_antenna(UHFReaderApp* App, uint32_t return_view) {
    if(App && App->ReaderConnected && uhf_worker_is_ready(App->YRM100XWorker)) {
        return true;
    }

    if(App) {
        App->ReaderConnected = false;
        App->Setting1Index = 0U;
        if(App->ConnectionItem) {
            variable_item_set_current_value_index(App->ConnectionItem, 0U);
            variable_item_set_current_value_text(App->ConnectionItem, App->Setting1Names[0]);
        }
        uhf_reader_show_antenna_not_connected(App, return_view);
    }
    return false;
}

/*
 * Heavy secondary views are created on first use instead of at application
 * startup. This keeps enough heap headroom when firmware USB/CDC services are
 * active.
 */
void uhf_reader_ensure_epc_views(UHFReaderApp* App) {
    furi_assert(App);

    if(!App->ViewEpc) {
        UHF_I("MEM", "LAZY alloc EPC dump BEGIN");
        view_epc_alloc(App);
        uhf_reader_log_heap("lazy_epc_dump");
    }

    if(!App->ViewBankMem) {
        UHF_I("MEM", "LAZY alloc BankMem BEGIN");
        view_bank_mem_alloc(App);
        uhf_reader_log_heap("lazy_bank_mem");
    }
}

void uhf_reader_ensure_epc_info_view(UHFReaderApp* App) {
    furi_assert(App);

    if(!App->ViewEpcInfo) {
        UHF_I("MEM", "LAZY alloc EPC info BEGIN");
        view_epc_info_alloc(App);
        uhf_reader_log_heap("lazy_epc_info");
    }
}

void uhf_reader_ensure_write_view(UHFReaderApp* App) {
    furi_assert(App);

    if(!App->ViewWrite) {
        UHF_I("MEM", "LAZY alloc Write BEGIN");
        view_write_alloc(App);
        uhf_reader_log_heap("lazy_write");
    }
}

void uhf_reader_ensure_kill_view(UHFReaderApp* App) {
    furi_assert(App);

    if(!App->SubmenuKillActions) {
        UHF_I("MEM", "LAZY alloc Kill BEGIN");
        view_kill_alloc(App);
        uhf_reader_log_heap("lazy_kill");
    }
}

void uhf_reader_ensure_delete_views(UHFReaderApp* App) {
    furi_assert(App);

    if(!App->ViewDelete) {
        UHF_I("MEM", "LAZY alloc Delete BEGIN");
        view_delete_alloc(App);
        uhf_reader_log_heap("lazy_delete");
    }

    if(!App->ViewDeleteSuccess) {
        UHF_I("MEM", "LAZY alloc DeleteSuccess BEGIN");
        view_delete_success_alloc(App);
        uhf_reader_log_heap("lazy_delete_success");
    }
}

void uhf_reader_ensure_saved_view(UHFReaderApp* App) {
    furi_assert(App);

    if(!App->SubmenuSaved) {
        UHF_I("MEM", "LAZY alloc Saved BEGIN");
        view_saved_menu_alloc(App);
        uhf_reader_log_heap("lazy_saved");
    }
}

void uhf_reader_ensure_tag_actions_view(UHFReaderApp* App) {
    furi_assert(App);

    if(!App->SubmenuTagActions) {
        UHF_I("MEM", "LAZY alloc TagActions BEGIN");
        view_tag_actions_alloc(App);
        uhf_reader_log_heap("lazy_tag_actions");
    }
}

void uhf_reader_ensure_lock_view(UHFReaderApp* App) {
    furi_assert(App);

    if(!App->VariableItemListLock) {
        UHF_I("MEM", "LAZY alloc Lock BEGIN");
        view_lock_alloc(App);
        uhf_reader_log_heap("lazy_lock");
    }
}

void uhf_reader_ensure_clone_views(UHFReaderApp* App) {
    furi_assert(App);

    if(!App->VariableItemListClone) {
        UHF_I("MEM", "LAZY alloc CloneBanks BEGIN");
        view_clone_banks_alloc(App);
        uhf_reader_log_heap("lazy_clone_banks");
    }

    if(!App->ViewClone) {
        UHF_I("MEM", "LAZY alloc Clone BEGIN");
        view_clone_alloc(App);
        uhf_reader_log_heap("lazy_clone");
    }
}

void uhf_reader_ensure_umi_test_view(UHFReaderApp* App) {
    furi_assert(App);

    if(!App->ViewUmiTest) {
        UHF_I("MEM", "LAZY alloc UMI Test BEGIN");
        view_umi_test_alloc(App);
        uhf_reader_log_heap("lazy_umi_test");
    }
}

void uhf_reader_ensure_bank_size_view(UHFReaderApp* App) {
    furi_assert(App);

    if(!App->ViewBankSize) {
        UHF_I("MEM", "LAZY alloc BankSize BEGIN free=%lu", (unsigned long)memmgr_get_free_heap());
        view_bank_size_alloc(App);
        uhf_reader_log_heap("lazy_bank_size");
    }
}

void uhf_reader_ensure_bank_tools_view(UHFReaderApp* App) {
    furi_assert(App);

    if(!App->SubmenuBankTools) {
        UHF_I("MEM", "LAZY alloc BankTools BEGIN");
        view_bank_tools_alloc(App);
        uhf_reader_log_heap("lazy_bank_tools");
    }
}

void uhf_reader_ensure_rewritable_views(UHFReaderApp* App) {
    furi_assert(App);

    if(!App->ViewRewritable) {
        UHF_I("MEM", "LAZY alloc Rewritable BEGIN");
        view_rewritable_alloc(App);
        uhf_reader_log_heap("lazy_rewritable");
    }
}

void uhf_reader_ensure_about_view(UHFReaderApp* App) {
    furi_assert(App);

    if(!App->WidgetAbout) {
        UHF_I("MEM", "LAZY alloc About BEGIN");
        view_about_alloc(App);
        uhf_reader_log_heap("lazy_about");
    }
}

/**
 * @brief      Callback for exiting the application.
 * @details    This function is called when user press back button.  We return VIEW_NONE to
 *            indicate that we want to exit the application.
 * @param      context  The context - unused
 * @return     next view id
*/
uint32_t uhf_reader_navigation_exit_callback(void* context) {
    UNUSED(context);
    return VIEW_NONE;
}

/**
 * @brief      Handle submenu item selection.
 * @details    This function is called when user selects an item from the submenu.
 * @param      context  The context - UHFReaderApp object.
 * @param      index     The UHFReaderSubmenuIndex item that was clicked.
*/
void uhf_reader_submenu_callback(void* context, uint32_t index) {
    UHFReaderApp* App = (UHFReaderApp*)context;
    UHF_I(
        "UI",
        "main menu index=%lu free_heap=%lu",
        (unsigned long)index,
        (unsigned long)memmgr_get_free_heap());
    /* The startup/About Antenna view is no longer active while Main handles input. */
    if(App->ViewAntennaInfo) view_antenna_info_free(App);

    switch(index) {
    case UHFReaderSubmenuIndexRead:
    case UHFReaderSubmenuIndexReadFullMulti:
    case UHFReaderSubmenuIndexReadForever:
    case UHFReaderSubmenuIndexReadSingle:
    case UHFReaderSubmenuIndexReadFullSingle:
    case UHFReaderSubmenuIndexBankTools:
    case UHFReaderSubmenuIndexTestUmi:
        if(!uhf_reader_require_antenna(App, UHFReaderViewSubmenu)) return;
        break;
    default:
        break;
    }

    switch(index) {
    case UHFReaderSubmenuIndexRead:
        App->SingleReadMode = false;
        App->FullSingleReadMode = false;
        App->FullMultiReadMode = false;
        App->ForeverReadMode = false;
        view_dispatcher_switch_to_view(App->ViewDispatcher, UHFReaderViewRead);
        break;
    case UHFReaderSubmenuIndexReadFullMulti:
        App->SingleReadMode = false;
        App->FullSingleReadMode = false;
        App->FullMultiReadMode = true;
        App->ForeverReadMode = false;
        view_dispatcher_switch_to_view(App->ViewDispatcher, UHFReaderViewRead);
        break;
    case UHFReaderSubmenuIndexReadForever:
        App->SingleReadMode = false;
        App->FullSingleReadMode = false;
        App->FullMultiReadMode = false;
        App->ForeverReadMode = true;
        view_dispatcher_switch_to_view(App->ViewDispatcher, UHFReaderViewRead);
        break;
    case UHFReaderSubmenuIndexReadSingle:
        App->SingleReadMode = true;
        App->FullSingleReadMode = false;
        App->FullMultiReadMode = false;
        App->ForeverReadMode = false;
        view_dispatcher_switch_to_view(App->ViewDispatcher, UHFReaderViewRead);
        break;
    case UHFReaderSubmenuIndexReadFullSingle:
        App->SingleReadMode = false;
        App->FullSingleReadMode = true;
        App->FullMultiReadMode = false;
        App->ForeverReadMode = false;
        view_dispatcher_switch_to_view(App->ViewDispatcher, UHFReaderViewRead);
        break;
    case UHFReaderSubmenuIndexSaved:
        uhf_reader_ensure_saved_view(App);
        view_dispatcher_switch_to_view(App->ViewDispatcher, UHFReaderViewSaved);
        break;
    case UHFReaderSubmenuIndexBankTools:
        uhf_reader_ensure_bank_tools_view(App);
        view_dispatcher_switch_to_view(App->ViewDispatcher, UHFReaderViewBankTools);
        break;
    case UHFReaderSubmenuIndexTestUmi:
        uhf_reader_ensure_umi_test_view(App);
        view_dispatcher_switch_to_view(App->ViewDispatcher, UHFReaderViewUmiTest);
        break;
    case UHFReaderSubmenuIndexConfig:
        view_dispatcher_switch_to_view(App->ViewDispatcher, UHFReaderViewConfigure);
        break;
    case UHFReaderSubmenuIndexAbout:
        uhf_reader_ensure_about_view(App);
        view_dispatcher_switch_to_view(App->ViewDispatcher, UHFReaderViewAbout);
        break;
    default:
        break;
    }
}

/**
 * @brief      Allocates Main Menu
 * @details    Allocates the main submenu with the read, saved, config, and about submenu items
 * @param      app  The UHFReaderApp object.
*/
void main_menu_alloc(UHFReaderApp* App) {
    App->Submenu = submenu_alloc();
    uhf_main_menu_app = App;
    snprintf(App->MainMenuHeader, sizeof(App->MainMenuHeader), "YRM100X_PRO");
    submenu_set_header(App->Submenu, App->MainMenuHeader);
    submenu_add_item(
        App->Submenu, "Read (Multi)", UHFReaderSubmenuIndexRead, uhf_reader_submenu_callback, App);
    submenu_add_item(
        App->Submenu,
        "Read Full(Multi)",
        UHFReaderSubmenuIndexReadFullMulti,
        uhf_reader_submenu_callback,
        App);
    submenu_add_item(
        App->Submenu,
        "Read Forever",
        UHFReaderSubmenuIndexReadForever,
        uhf_reader_submenu_callback,
        App);
    submenu_add_item(
        App->Submenu,
        "Read (Single)",
        UHFReaderSubmenuIndexReadSingle,
        uhf_reader_submenu_callback,
        App);
    submenu_add_item(
        App->Submenu,
        "Read Full (Single)",
        UHFReaderSubmenuIndexReadFullSingle,
        uhf_reader_submenu_callback,
        App);
    submenu_add_item(
        App->Submenu,
        "Get Info Bank on Tag",
        UHFReaderSubmenuIndexBankTools,
        uhf_reader_submenu_callback,
        App);
    submenu_add_item(
        App->Submenu,
        "Saved Dumps (0)",
        UHFReaderSubmenuIndexSaved,
        uhf_reader_submenu_callback,
        App);
    submenu_add_item(
        App->Submenu,
        "Test UMI Auto",
        UHFReaderSubmenuIndexTestUmi,
        uhf_reader_submenu_callback,
        App);
    submenu_add_item(
        App->Submenu, "Configure", UHFReaderSubmenuIndexConfig, uhf_reader_submenu_callback, App);
    submenu_add_item(
        App->Submenu, "About", UHFReaderSubmenuIndexAbout, uhf_reader_submenu_callback, App);
    view_set_previous_callback(
        submenu_get_view(App->Submenu), uhf_reader_navigation_exit_callback);
    view_set_enter_callback(submenu_get_view(App->Submenu), uhf_reader_main_menu_enter);
    view_dispatcher_add_view(
        App->ViewDispatcher, UHFReaderViewSubmenu, submenu_get_view(App->Submenu));
    view_dispatcher_switch_to_view(App->ViewDispatcher, UHFReaderViewSubmenu);
}

/**
 * @brief      Allocate the UHF RFID Reader Application.
 * @details    This function allocates all resources for the UHF RFID Reader Application.
 * @return     UHFReaderApp object.
*/
static UHFReaderApp* uhf_reader_app_alloc() {
    //Allocating storage for the saved_epcs and index file
    UHFReaderApp* App = (UHFReaderApp*)calloc(1, sizeof(UHFReaderApp));
    if(!App) {
        FURI_LOG_E(TAG, "Application state allocation failed");
        return NULL;
    }
    Storage* Storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* File = flipper_format_file_alloc(Storage);
    FlipperFormat* IndexFile = flipper_format_file_alloc(Storage);
    App->TagStorage = Storage;
    App->EpcFile = File;
    App->EpcIndexFile = IndexFile;

    uhf_umi_marker_registry_load(Storage, &App->UmiMarkerRegistry);
    uhf_reader_log_heap("alloc_begin");

    // YRM100X-only build: no legacy transport receive buffers.
    // EpcToSave is a borrowed pointer while the save callback is running.
    App->EpcToSave = NULL;
    App->NumberOfEpcsToRead = 0;
    App->MultiDumpInProgress = false;
    App->LastMultiBeepCount = 0;
    App->LastFullMultiEpcBeepCount = 0;
    App->LastFullMultiDumpBeepCount = 0;

    //Initializing the indices for each array and the file name
    App->CurEpcIndex = 26;

    //Creating the initial GUI
    Gui* Gui = furi_record_open(RECORD_GUI);
    App->ViewDispatcher = view_dispatcher_alloc();
    //view_dispatcher_enable_queue(App->ViewDispatcher);
    view_dispatcher_attach_to_gui(App->ViewDispatcher, Gui, ViewDispatcherTypeFullscreen);
    view_dispatcher_set_event_callback_context(App->ViewDispatcher, App);

    //Allocating the different views, menus, and widgets for the app
    main_menu_alloc(App);
    uhf_reader_log_heap("main_menu");

    view_config_alloc(App);
    uhf_reader_log_heap("config");

    view_read_alloc(App);
    uhf_reader_log_heap("read");

    /*
     * Recount now so the main-menu label is correct, but do not retain one
     * heap allocation per saved item. Saved/actions/lock/clone/tool views are
     * created only when the user opens them.
     */
    uhf_saved_recount_and_repair_index(App);
    uhf_reader_log_heap("startup_core_complete");

    App->Notifications = furi_record_open(RECORD_NOTIFICATION);

#ifdef BACKLIGHT_ON
    notification_message(App->Notifications, &sequence_display_backlight_enforce_on);
#endif
    // YRM100X is the only reader backend in this build.
    App->UHFModuleType = YRM100X_MODULE;
    bool worker_ready = uhf_reader_ensure_worker(App);
    uhf_reader_log_heap("worker_wrapper");

    /*
     * YRM100X_PRO assumes the reader is normally wired before the app opens.
     * Try to connect immediately and reflect the actual result in Configure
     * and all read/write view models.
     */
    UHF_I(
        "APP",
        "auto_connect BEGIN worker_ready=%d lazy_ptrs write=%p epc=%p info=%p bank=%p kill=%p delete=%p",
        worker_ready ? 1 : 0,
        (void*)App->ViewWrite,
        (void*)App->ViewEpc,
        (void*)App->ViewEpcInfo,
        (void*)App->ViewBankMem,
        (void*)App->SubmenuKillActions,
        (void*)App->ViewDelete);
    uhf_debug_flush();

    bool connected = uhf_reader_auto_connect(App);
    UHF_I(
        "APP",
        "auto_connect END result=%d reader_connected=%d power=%u region=%u session=%u target=%u",
        connected ? 1 : 0,
        App->ReaderConnected ? 1 : 0,
        (unsigned int)App->SettingPowerIndex,
        (unsigned int)App->SettingRegionIndex,
        (unsigned int)App->SettingSessionIndex,
        (unsigned int)App->SettingTargetIndex);
    if(connected) {
        uhf_reader_show_antenna_info(App, true);
    } else {
        uhf_reader_show_antenna_not_connected(App, UHFReaderViewSubmenu);
    }
    uhf_reader_log_heap("app_alloc_complete");

    return App;
}

/**
 * @brief      Free the UHFReaderApp application.
 * @details    This function frees the UHF RFID Reader application resources.
 * @param      app  The UHFReaderApp application object.
*/
static void uhf_reader_app_free(UHFReaderApp* App) {
    UHF_I("MEM", "app_free BEGIN free_heap=%lu", (unsigned long)memmgr_get_free_heap());
    UHF_I("APP", "app_free BEGIN");

#ifdef BACKLIGHT_ON
    notification_message(App->Notifications, &sequence_display_backlight_enforce_auto);
#endif

    /*
     * All normal view-exit callbacks free their timers, but App->Timer is shared
     * by several views. If an abnormal navigation path leaves one alive, reclaim
     * it here before any view/context is destroyed.
     */
    if(App->Timer) {
        UHF_W("APP", "app_free found live Timer -> stop/free");
        furi_timer_stop(App->Timer);
        furi_timer_free(App->Timer);
        App->Timer = NULL;
    }

    uhf_reader_release_secondary_views(App);

    if(App->YRM100XWorker) {
        UHF_I("APP", "app_free worker stop BEGIN");
        uhf_debug_flush();
        uhf_worker_stop(App->YRM100XWorker);
        UHF_I("APP", "app_free worker stop END");

        if(App->YRM100XWorker->uhf_tag_wrapper) {
            uhf_tag_wrapper_free(App->YRM100XWorker->uhf_tag_wrapper);
            App->YRM100XWorker->uhf_tag_wrapper = NULL;
        }

        uhf_worker_free(App->YRM100XWorker);
        App->YRM100XWorker = NULL;
    }

    view_read_free(App);
    view_config_free(App);

    if(App->ViewAntennaInfo) view_antenna_info_free(App);

    UHF_I("APP", "app_free all child views freed");

    view_dispatcher_remove_view(App->ViewDispatcher, UHFReaderViewSubmenu);
    submenu_free(App->Submenu);
    App->Submenu = NULL;
    uhf_main_menu_app = NULL;

    view_dispatcher_free(App->ViewDispatcher);
    App->ViewDispatcher = NULL;

    if(App->EpcName) {
        furi_string_free(App->EpcName);
        App->EpcName = NULL;
    }
    if(App->EpcDelete) {
        furi_string_free(App->EpcDelete);
        App->EpcDelete = NULL;
    }
    if(App->EpcNameDelete) {
        furi_string_free(App->EpcNameDelete);
        App->EpcNameDelete = NULL;
    }
    if(App->EpcToWrite) {
        furi_string_free(App->EpcToWrite);
        App->EpcToWrite = NULL;
    }

    if(App->EpcFile) {
        flipper_format_file_close(App->EpcFile);
        flipper_format_free(App->EpcFile);
        App->EpcFile = NULL;
    }
    if(App->EpcIndexFile) {
        flipper_format_file_close(App->EpcIndexFile);
        flipper_format_free(App->EpcIndexFile);
        App->EpcIndexFile = NULL;
    }

    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_STORAGE);
    furi_record_close(RECORD_GUI);

    free(App);
}

/**
 * @brief      Main function for UHF RFID Reader application.
 * @details    This function is the entry point for the UHF RFID Reader application.
 * @param      _p  Input parameter - unused
 * @return     0 - Success
*/
int32_t main_uhf_reader_app(void* _p) {
    UNUSED(_p);

    Expansion* expansion = furi_record_open(RECORD_EXPANSION);
    expansion_disable(expansion);

    /*
     * GPIO 5V can be supplied by either Flipper's OTG boost or external USB
     * VBUS. Never force the charger IC into OTG boost while USB-C VBUS is
     * already present.
     */
    const float usb_vbus = furi_hal_power_get_usb_voltage();
    const bool usb_present = usb_vbus >= 4.0f;
    const bool otg_was_enabled = furi_hal_power_is_otg_enabled();
    bool PowerOn = false;

    FURI_LOG_I(
        TAG,
        "POWER startup USB=%.2fV present=%d otg_before=%d heap=%lu",
        (double)usb_vbus,
        usb_present ? 1 : 0,
        otg_was_enabled ? 1 : 0,
        (unsigned long)memmgr_get_free_heap());

    if(!usb_present && !otg_was_enabled) {
        PowerOn = furi_hal_power_enable_otg();

        FURI_LOG_I(TAG, "POWER OTG enable requested result=%d", PowerOn ? 1 : 0);
    } else if(usb_present) {
        FURI_LOG_I(TAG, "POWER USB VBUS present -> skip OTG boost");
    }

    /*
     * Lazy alpha37 startup reaches USART about 170 ms earlier than alpha36.
     * Give the expansion service time to release USART and the externally
     * powered reader time to finish booting before acquiring the transport.
     */
    furi_delay_ms(UHF_STARTUP_SETTLE_DELAY_MS);

    UHFReaderApp* App = uhf_reader_app_alloc();
    if(App) {
        UHF_I("APP", "dispatcher RUN");
        uhf_debug_flush();
        view_dispatcher_run(App->ViewDispatcher);
        UHF_I("APP", "dispatcher EXIT");
        uhf_debug_flush();

        // Release worker/UART first while the reader is still powered.
        uhf_reader_app_free(App);
    }

    if(PowerOn) {
        furi_hal_power_disable_otg();
    }

    expansion_enable(expansion);
    furi_record_close(RECORD_EXPANSION);

    // Same settling idea used by Flipper's own leak tests.
    furi_delay_ms(250);

    return 0;
}
