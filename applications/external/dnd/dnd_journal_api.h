#pragma once
#include "dnd_plugin_loader.h"
#define DND_JOURNAL_API_ID          "dnd_journal"
#define DND_JOURNAL_API_VERSION     1U
#define DND_JOURNAL_HUB_PATH        "/ext/apps_data/dndolphins/plugins/dnd_journal.fal"
#define DND_JOURNAL_STANDALONE_PATH "/ext/apps_data/dndjournal/plugins/dnd_journal.fal"
#define DND_JOURNAL_DATA_ROOT       "/ext/apps_data/dndjournal"
typedef struct {
    uint32_t size;
    DndPluginUiResult (*run)(
        ViewDispatcher* dispatcher,
        Storage* storage,
        uint32_t profile,
        bool have_profile,
        bool loading_on_return);
} DndJournalApi;
