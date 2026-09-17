#pragma once
#include "irb_files.h"
#include "irb_scan.h"
#include "irb_cache.h"
#include <furi.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <input/input.h>
#include <stdatomic.h>
typedef enum {
    Home,
    Settings,
    Grid,
    Pair,
    Position,
    ProjectMenu,
    Buttons,
    ButtonMenu,
    SavedMenu,
    Browser,
    Import,
    Keyboard,
    Message,
    Choice,
    Help,
    Scan,
    Navigation,
    Others,
    ScreenCount
} Screen;
typedef enum { BrowseLibrary, BrowseImport, BrowseSaved, BrowseOpen } BrowsePurpose;
typedef enum { TextSave, TextLabel, TextRename, TextDuplicate } TextPurpose;
typedef enum {
    ConfirmNew,
    ConfirmOpen,
    ConfirmEdit,
    ConfirmReplace,
    ConfirmDelete,
    ConfirmLeave
} ChoicePurpose;
typedef enum {
    JobLoad,
    JobOpen,
    JobDraft,
    JobSave,
    JobSend,
    JobScan,
    JobBrowse,
    JobCatalog,
    JobImport,
    JobDelete,
    JobSettings
} JobType;
enum { EventInput = 0x1000, EventDone = 0x2000 };
typedef struct {
    IrbProject project;
    uint32_t counts[IRB_GROUPS];
    Screen screen;
    unsigned focus, slot, position, pair, action, tick, help;
    bool simulate, play, busy, cancelable, committing, upper, navigation_available;
    uint32_t progress;
    char status[48], text[IRB_NAME_SIZE], message[IRB_ERROR_SIZE];
    char default_library[IRB_PATH_SIZE];
    char rows[IRB_PAGE_SIZE][IRB_PATH_SIZE];
    unsigned list_count, list_start, message_pages;
    IrbScan scan;
    bool sending, pausing, checking, send_job;
    uint32_t send_count;
    char folder[IRB_PATH_SIZE];
    bool row_directories[IRB_PAGE_SIZE];
} IrbViewModel;
typedef struct IrbApp IrbApp;
typedef struct {
    IrbApp* app;
    JobType type;
    IrbProject project;
    IrbLibrary library;
    IrbFilePage page;
    IrbFileList files;
    IrbCatalog* catalog;
    IrbScan scan;
    char path[IRB_PATH_SIZE], error[IRB_ERROR_SIZE], old_name[IRB_NAME_SIZE];
    unsigned slot, position;
    uint32_t added, mapped;
    int entry;
    bool restore, saved, replace, rename, both, ok, conflict, repeatable;
    atomic_bool cancel, committing, held;
    atomic_int command;
    unsigned last_percent;
    IrbLoadPhase last_phase;
} IrbJob;
struct IrbApp {
    Storage* storage;
    Gui* gui;
    ViewDispatcher* dispatcher;
    View* view;
    FuriThread* worker;
    IrbJob* job;
    IrbProject project;
    IrbLibrary library;
    IrbCatalog* catalog;
    IrbFilePage page;
    IrbFileList files;
    IrbSignalCache signals;
    Screen screen, return_screen, message_return, job_return, choice_return, keyboard_return;
    unsigned focus, slot, position, pair, action, repeats, tick, help, return_focus;
    unsigned focus_memory[ScreenCount];
    unsigned position_actions[IRB_POSITION_SLOTS];
    Screen button_return;
    bool consume_ok, consume_back, checking;
    InputKey last_direction;
    bool draft, dirty, loaded_saved, draft_current, play, simulate, upper, delete_both;
    BrowsePurpose browse_purpose;
    TextPurpose text_purpose;
    ChoicePurpose choice;
    char loaded_name[IRB_NAME_SIZE], text[IRB_NAME_SIZE], message[IRB_ERROR_SIZE];
    char default_library[IRB_PATH_SIZE], pending_path[IRB_PATH_SIZE];
};
extern const char irb_keys[];
void irb_draw(Canvas* canvas, void* context);
void irb_refresh(IrbApp* app);
bool irb_progress(IrbLoadPhase phase, uint32_t done, uint32_t total, void* context);
int32_t irb_work(void* context);
