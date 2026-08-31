#include "dndolphins.h"
#include "dndadventure_campaign_packs.h"
#include "dndadventure_campaigns.h"
#include "dnd_fs.h"
#include "dnd_handoff.h"
#include "dnd_profile_ref.h"
#include "dndolphins_rules.h"
#include "dndolphins_items.h"
#include "dndolphins_storage.h"

#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <input/input.h>
#include <storage/storage.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TAG "DndAdventure"
#define ADVENTURE_MAX_CHOICES 4U
#define ADVENTURE_READ_BUFFER 256U
#define ADVENTURE_LINE_LEN 512U
#define ADVENTURE_STATUS_LEN 40U
#define ADVENTURE_JOURNAL_PATH_LEN 160U

typedef struct {
    char label[POCKET_D20_NAME_LEN];
    int8_t skill;
    uint8_t dc;
    char success_scene[POCKET_D20_SHORT_LEN];
    char failure_scene[POCKET_D20_SHORT_LEN];
    char reward_item[POCKET_D20_NAME_LEN];
    char milestone[POCKET_D20_NAME_LEN];
    uint8_t quest_flag;
    uint8_t achievement;
} DndAdventureChoice;

typedef struct {
    char id[POCKET_D20_SHORT_LEN];
    char title[POCKET_D20_NAME_LEN];
    char body[POCKET_D20_DETAIL_LEN];
    char sprite[POCKET_D20_SHORT_LEN];
    uint8_t choice_count;
    DndAdventureChoice choices[ADVENTURE_MAX_CHOICES];
} DndAdventureScene;

typedef struct {
    File* file;
    uint8_t buffer[ADVENTURE_READ_BUFFER];
    uint16_t position;
    uint16_t count;
} DndAdventureReader;

typedef enum {
    DndAdventureScreenCampaigns,
    DndAdventureScreenAdventure,
    DndAdventureScreenResult,
    DndAdventureScreenDiagnostics,
    DndAdventureScreenPacks,
    DndAdventureScreenPackPreview,
} DndAdventureScreen;

typedef struct {
    Gui* gui;
    Storage* storage;
    ViewDispatcher* dispatcher;
    View* view;

    PocketSaveData character_data;
    uint32_t profile;
    uint8_t character_loaded;
    uint8_t character_dirty;
    uint8_t return_to_dnd;

    DndAdventureScreen screen;
    uint16_t selection;
    uint16_t scroll;
    char status[ADVENTURE_STATUS_LEN];

    uint16_t campaign_count;
    char campaign_rows[5][27];
    PocketCampaignSummary active_campaign;
    PocketCampaignProgress progress;
    uint8_t active_campaign_valid;
    DndAdventureScene* scene;
    uint8_t adventure_started;

    PocketCampaignDiagnostics diagnostics;
    uint16_t pack_count;
    char pack_rows[5][27];
    PocketCampaignPackSummary inbox_preview;
    uint8_t inbox_preview_valid;

    int16_t last_total;
    int8_t last_modifier;
    int8_t last_skill;
    uint8_t last_natural;
    uint8_t last_dc;
    uint8_t last_passed;
} DndAdventureApp;

static void adventure_copy(char* destination, size_t size, const char* source) {
    if(!destination || !size) return;
    strncpy(destination, source ? source : "", size - 1U);
    destination[size - 1U] = '\0';
}

static void adventure_set_status(DndAdventureApp* app, const char* status) {
    adventure_copy(app->status, sizeof(app->status), status);
}

static bool adventure_parse_u32(const char* text, uint32_t maximum, uint32_t* output) {
    if(!text || !text[0] || !output) return false;
    uint32_t value = 0U;
    for(const char* p = text; *p; ++p) {
        if(*p < '0' || *p > '9') return false;
        uint32_t digit = (uint32_t)(*p - '0');
        if(value > maximum / 10U || (value == maximum / 10U && digit > maximum % 10U))
            return false;
        value = value * 10U + digit;
    }
    *output = value;
    return true;
}

static bool adventure_parse_i8(const char* text, int8_t* output) {
    if(!text || !text[0] || !output) return false;
    bool negative = *text == '-';
    if(negative) ++text;
    uint32_t value = 0U;
    if(!adventure_parse_u32(text, negative ? 128U : 127U, &value)) return false;
    *output = negative ? (int8_t)(-(int16_t)value) : (int8_t)value;
    return true;
}

static void adventure_reader_init(DndAdventureReader* reader, File* file) {
    memset(reader, 0, sizeof(*reader));
    reader->file = file;
}

static bool adventure_read_line(DndAdventureReader* reader, char* line, size_t size) {
    size_t used = 0U;
    bool consumed = false;
    while(true) {
        if(reader->position >= reader->count) {
            reader->count =
                (uint16_t)storage_file_read(reader->file, reader->buffer, sizeof(reader->buffer));
            reader->position = 0U;
            if(!reader->count) break;
        }
        char value = (char)reader->buffer[reader->position++];
        consumed = true;
        if(value == '\r') continue;
        if(value == '\n') break;
        if(used + 1U < size) line[used++] = value;
    }
    line[used] = '\0';
    return consumed;
}

static uint8_t adventure_split(char* line, char** fields, uint8_t capacity) {
    uint8_t count = 0U;
    char* cursor = line;
    while(count < capacity) {
        fields[count++] = cursor;
        char* separator = strchr(cursor, '|');
        if(!separator) break;
        *separator = '\0';
        cursor = separator + 1U;
    }
    return count;
}

static void adventure_scene_free(DndAdventureApp* app) {
    free(app->scene);
    app->scene = NULL;
}

static void adventure_process_scene_line(
    DndAdventureScene* scene,
    const char* target,
    char* line,
    bool* found) {
    if(!line[0] || line[0] == '#') return;
    char* fields[11];
    uint8_t count = adventure_split(line, fields, 11U);
    if(count == 5U && !strcmp(fields[0], "S") && !strcmp(fields[1], target)) {
        adventure_copy(scene->id, sizeof(scene->id), fields[1]);
        adventure_copy(scene->title, sizeof(scene->title), fields[2]);
        adventure_copy(scene->body, sizeof(scene->body), fields[3]);
        adventure_copy(scene->sprite, sizeof(scene->sprite), fields[4]);
        *found = true;
    } else if(
        count == 11U && !strcmp(fields[0], "C") && !strcmp(fields[1], target) &&
        scene->choice_count < ADVENTURE_MAX_CHOICES) {
        int8_t skill = 0;
        uint32_t dc = 0U, quest = 0U, achievement = 0U;
        if(!adventure_parse_i8(fields[3], &skill) ||
           !adventure_parse_u32(fields[4], UINT8_MAX, &dc) ||
           !adventure_parse_u32(fields[9], UINT8_MAX, &quest) ||
           !adventure_parse_u32(fields[10], UINT8_MAX, &achievement))
            return;
        DndAdventureChoice* choice = &scene->choices[scene->choice_count++];
        memset(choice, 0, sizeof(*choice));
        adventure_copy(choice->label, sizeof(choice->label), fields[2]);
        choice->skill = skill;
        choice->dc = (uint8_t)dc;
        adventure_copy(choice->success_scene, sizeof(choice->success_scene), fields[5]);
        adventure_copy(choice->failure_scene, sizeof(choice->failure_scene), fields[6]);
        adventure_copy(choice->reward_item, sizeof(choice->reward_item), fields[7]);
        adventure_copy(choice->milestone, sizeof(choice->milestone), fields[8]);
        choice->quest_flag = (uint8_t)quest;
        choice->achievement = (uint8_t)achievement;
    }
}

static bool adventure_load_scene(DndAdventureApp* app) {
    adventure_scene_free(app);
    if(!app->active_campaign_valid || !app->progress.scene[0]) return false;
    DndAdventureScene* scene = calloc(1U, sizeof(DndAdventureScene));
    if(!scene) {
        adventure_set_status(app, "Adventure memory low");
        return false;
    }
    char path[POCKET_D20_LONG_PATH_LEN];
    if(!pocket_campaign_scene_path(app->storage, &app->active_campaign, path, sizeof(path))) {
        free(scene);
        adventure_set_status(app, "Campaign scene file missing");
        return false;
    }
    File* file = storage_file_alloc(app->storage);
    if(!file) {
        free(scene);
        adventure_set_status(app, "Adventure memory low");
        return false;
    }
    if(!storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_free(file);
        free(scene);
        adventure_set_status(app, "Campaign scene file missing");
        return false;
    }
    DndAdventureReader reader;
    adventure_reader_init(&reader, file);
    char line[ADVENTURE_LINE_LEN];
    bool found = false;
    while(adventure_read_line(&reader, line, sizeof(line)))
        adventure_process_scene_line(scene, app->progress.scene, line, &found);
    bool io_ok = storage_file_get_error(file) == FSE_OK;
    storage_file_close(file);
    storage_file_free(file);
    if(!io_ok || !found || !scene->choice_count) {
        free(scene);
        adventure_set_status(app, "Scene invalid");
        return false;
    }
    app->scene = scene;
    return true;
}

static bool adventure_save_progress(DndAdventureApp* app) {
    if(!app->active_campaign_valid || !app->character_loaded) return true;
    return pocket_campaign_progress_save(
        app->storage, app->profile, &app->active_campaign, &app->progress);
}

static bool adventure_save_character(DndAdventureApp* app) {
    if(!app->character_dirty) return true;
    if(!app->character_loaded) return false;
    if(!pocket_d20_storage_save_profile_updated(app->storage, app->profile, &app->character_data)) return false;
    app->character_dirty = 0U;
    return true;
}

static bool adventure_resolve_active(DndAdventureApp* app) {
    if(app->active_campaign_valid) return true;
    char active_id[POCKET_CAMPAIGN_ID_LEN];
    PocketCampaignSummary campaign;
    bool loaded = app->character_loaded &&
                  pocket_campaign_active_load(
                      app->storage, app->profile, active_id, sizeof(active_id));
    bool found = loaded && active_id[0] && pocket_campaign_find(app->storage, active_id, &campaign);
    if(!found) found = pocket_campaign_at(app->storage, 0U, &campaign);
    if(!found) return false;
    app->active_campaign = campaign;
    app->active_campaign_valid = 1U;
    if(!app->character_loaded) {
        memset(&app->progress, 0, sizeof(app->progress));
        adventure_copy(app->progress.campaign, sizeof(app->progress.campaign), campaign.id);
        adventure_copy(app->progress.scene, sizeof(app->progress.scene), campaign.entry_scene);
        return true;
    }
    return pocket_campaign_progress_load(app->storage, app->profile, &campaign, &app->progress);
}

static bool adventure_select_campaign(DndAdventureApp* app, uint16_t index) {
    PocketCampaignSummary next;
    if(!pocket_campaign_at(app->storage, index, &next)) {
        adventure_set_status(app, "Campaign record invalid");
        return false;
    }
    if(next.pack_version != POCKET_CAMPAIGN_PACK_VERSION ||
       next.minimum_app > POCKET_CAMPAIGN_APP_VERSION ||
       (next.maximum_app && next.maximum_app < POCKET_CAMPAIGN_APP_VERSION)) {
        adventure_set_status(app, "Campaign incompatible");
        return false;
    }
    if(app->active_campaign_valid && !adventure_save_progress(app)) {
        adventure_set_status(app, "Progress save failed");
        return false;
    }
    app->active_campaign = next;
    app->active_campaign_valid = 1U;
    if(app->character_loaded) {
        if(!pocket_campaign_progress_load(app->storage, app->profile, &next, &app->progress))
            return false;
    } else {
        memset(&app->progress, 0, sizeof(app->progress));
        adventure_copy(app->progress.campaign, sizeof(app->progress.campaign), next.id);
        adventure_copy(app->progress.scene, sizeof(app->progress.scene), next.entry_scene);
    }
    if(!adventure_load_scene(app)) return false;
    app->screen = DndAdventureScreenAdventure;
    app->selection = 0U;
    app->scroll = 0U;
    app->adventure_started = 0U;
    app->status[0] = '\0';
    return true;
}

static void adventure_reward_item(DndAdventureApp* app, const char* name) {
    if(!app->character_loaded || !name || !name[0] || !strcmp(name, "-")) return;
    if(!pocket_d20_items_grant_reward(
           app->storage,
           app->profile,
           &app->character_data.character,
           name,
           "Adventure reward"))
        adventure_set_status(app, "Item reward save failed");
}

static bool adventure_journal_write_string(File* file, const char* key, const char* value) {
    size_t key_length = strlen(key);
    if(storage_file_write(file, key, key_length) != key_length ||
       storage_file_write(file, "=", 1U) != 1U)
        return false;
    static const char digits[] = "0123456789ABCDEF";
    char chunk[64];
    size_t used = 0U;
    for(size_t i = 0U; value && value[i]; ++i) {
        uint8_t byte = (uint8_t)value[i];
        bool escape = byte == '%' || byte == '\n' || byte == '\r' || byte < 0x20U;
        size_t needed = escape ? 3U : 1U;
        if(used + needed > sizeof(chunk)) {
            if(storage_file_write(file, chunk, used) != used) return false;
            used = 0U;
        }
        if(escape) {
            chunk[used++] = '%';
            chunk[used++] = digits[byte >> 4U];
            chunk[used++] = digits[byte & 0x0FU];
        } else {
            chunk[used++] = (char)byte;
        }
    }
    if(used && storage_file_write(file, chunk, used) != used) return false;
    return storage_file_write(file, "\n", 1U) == 1U;
}

static bool adventure_writef(File* file, const char* format, ...) {
    char line[160];
    va_list args;
    va_start(args, format);
    int length = vsnprintf(line, sizeof(line), format, args);
    va_end(args);
    return length > 0 && (size_t)length < sizeof(line) &&
           storage_file_write(file, line, (size_t)length) == (size_t)length;
}

static bool adventure_write_milestone_journal(DndAdventureApp* app, const char* milestone) {
    if(!milestone || !milestone[0] || !strcmp(milestone, "-")) return true;
    if(!app->character_loaded) return true;
    char directory[ADVENTURE_JOURNAL_PATH_LEN];
    int directory_length = snprintf(
        directory,
        sizeof(directory),
        POCKET_D20_JOURNAL_DATA_ROOT "/ch_%lu",
        (unsigned long)app->profile);
    if(directory_length <= 0 || (size_t)directory_length >= sizeof(directory)) return false;
    storage_common_mkdir(app->storage, POCKET_D20_JOURNAL_DATA_ROOT);
    storage_common_mkdir(app->storage, directory);
    DateTime now;
    furi_hal_rtc_get_datetime(&now);
    for(uint8_t suffix = 0U; suffix < 100U; ++suffix) {
        char path[ADVENTURE_JOURNAL_PATH_LEN];
        int length = snprintf(
            path,
            sizeof(path),
            "%s/ch_%lu_%04u%02u%02u_%02u%02u%02u_%02u.txt",
            directory,
            (unsigned long)app->profile,
            (unsigned int)now.year,
            (unsigned int)now.month,
            (unsigned int)now.day,
            (unsigned int)now.hour,
            (unsigned int)now.minute,
            (unsigned int)now.second,
            (unsigned int)suffix);
        if(length <= 0 || (size_t)length >= sizeof(path)) return false;
        if(storage_file_exists(app->storage, path)) continue;
        File* file = storage_file_alloc(app->storage);
        if(!file) return false;
        char body[POCKET_D20_DETAIL_LEN];
        snprintf(
            body,
            sizeof(body),
            "Reached in %s.",
            app->active_campaign.name[0] ? app->active_campaign.name : "Adventure");
        bool ok = storage_file_open(file, path, FSAM_WRITE, FSOM_CREATE_ALWAYS) &&
                  adventure_writef(file, "PocketD20Journal=1\n") &&
                  adventure_writef(file, "CharacterId=%lu\n", (unsigned long)app->profile) &&
                  adventure_journal_write_string(file, "Title", milestone) &&
                  adventure_journal_write_string(file, "Body", body) &&
                  adventure_writef(file, "Category=3\n") &&
                  adventure_writef(file, "Completed=0\n") &&
                  adventure_writef(file, "LevelGranted=0\n") &&
                  adventure_writef(file, "ClassIndex=0\n") &&
                  adventure_writef(file, "End=OK\n") && storage_file_sync(file);
        storage_file_close(file);
        storage_file_free(file);
        if(!ok) storage_common_remove(app->storage, path);
        return ok;
    }
    return false;
}

static bool adventure_apply_choice(DndAdventureApp* app, const DndAdventureChoice* choice) {
    uint8_t natural = 0U;
    int8_t modifier = 0;
    bool passed = true;
    if(choice->skill >= 0 && (uint8_t)choice->skill < POCKET_D20_SKILL_COUNT) {
        if(!app->character_loaded) {
            adventure_set_status(app, "Character not available");
            return false;
        }
        natural = (uint8_t)pocket_d20_roll_dice(1U, 20U);
        modifier = pocket_d20_skill_modifier(&app->character_data.character, (uint8_t)choice->skill);
        app->last_natural = natural;
        app->last_modifier = modifier;
        app->last_skill = choice->skill;
        app->last_dc = choice->dc;
        app->last_total = (int16_t)natural + modifier;
        app->last_passed = app->last_total >= choice->dc;
        passed = app->last_passed;
    }

    uint32_t previous_quest_flags = app->progress.quest_flags;
    uint32_t previous_achievements = app->progress.achievements;
    char previous_scene[POCKET_D20_SHORT_LEN];
    adventure_copy(previous_scene, sizeof(previous_scene), app->progress.scene);

    bool quest_guard = choice->quest_flag < 32U;
    bool achievement_guard = choice->achievement < 32U;
    bool quest_new = quest_guard && !(app->progress.quest_flags & (1UL << choice->quest_flag));
    bool achievement_new = achievement_guard &&
                           !(app->progress.achievements & (1UL << choice->achievement));
    bool first_grant = !(quest_guard || achievement_guard) || quest_new || achievement_new;
    bool grant_pending = passed && first_grant;
    bool milestone_present = choice->milestone[0] && strcmp(choice->milestone, "-");
    bool milestone_guarded = quest_guard || achievement_guard;

    if(grant_pending) {
        if(quest_guard) app->progress.quest_flags |= 1UL << choice->quest_flag;
        if(achievement_guard) app->progress.achievements |= 1UL << choice->achievement;
    }

    const char* next = passed ? choice->success_scene : choice->failure_scene;
    if(next[0] && strcmp(next, "-")) adventure_copy(app->progress.scene, sizeof(app->progress.scene), next);
    if(!adventure_load_scene(app)) {
        app->progress.quest_flags = previous_quest_flags;
        app->progress.achievements = previous_achievements;
        adventure_copy(app->progress.scene, sizeof(app->progress.scene), previous_scene);
        adventure_load_scene(app);
        adventure_set_status(app, "Next scene missing");
        return false;
    }
    app->selection = 0U;
    app->scroll = 0U;
    bool progress_saved = adventure_save_progress(app);
    if(!progress_saved) {
        app->progress.quest_flags = previous_quest_flags;
        app->progress.achievements = previous_achievements;
        adventure_copy(app->progress.scene, sizeof(app->progress.scene), previous_scene);
        adventure_load_scene(app);
        adventure_set_status(app, "Progress save failed");
        return false;
    } else if(grant_pending) {
        adventure_reward_item(app, choice->reward_item);
        if(milestone_present && !milestone_guarded) {
            adventure_set_status(app, "Milestone requires flag/achievement");
        } else if(milestone_present && !adventure_write_milestone_journal(app, choice->milestone)) {
            adventure_set_status(app, "Milestone set; Journal write failed");
        }
        if(!adventure_save_character(app))
            adventure_set_status(app, "Character reward save failed");
    }
    if(choice->skill >= 0) app->screen = DndAdventureScreenResult;
    else if(!app->status[0]) adventure_set_status(app, "Choice applied");
    return true;
}

static void adventure_draw_header(Canvas* canvas, const char* title, const char* status) {
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_box(canvas, 0, 0, 128, 10);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 8, title);
    if(status && status[0]) {
        uint16_t width = canvas_string_width(canvas, status);
        if(width < 58U) canvas_draw_str(canvas, 126U - width, 8, status);
    }
    canvas_set_color(canvas, ColorBlack);
}

static void adventure_draw_row(Canvas* canvas, uint8_t row, bool selected, const char* text) {
    uint8_t y = (uint8_t)(11U + row * 10U);
    if(selected) {
        canvas_draw_box(canvas, 0, y, 128, 10);
        canvas_set_color(canvas, ColorWhite);
    }
    canvas_set_font(canvas, FontSecondary);
    char shown[27];
    adventure_copy(shown, sizeof(shown), text);
    canvas_draw_str(canvas, 2, (uint8_t)(y + 8U), shown);
    if(selected) canvas_set_color(canvas, ColorBlack);
}

static void adventure_draw_sprite(Canvas* canvas, const DndAdventureScene* scene) {
    canvas_draw_frame(canvas, 1, 13, 27, 27);
    if(!strcmp(scene->sprite, "dolphin")) {
        canvas_draw_line(canvas, 5, 28, 12, 22);
        canvas_draw_line(canvas, 12, 22, 22, 24);
        canvas_draw_line(canvas, 22, 24, 15, 30);
        canvas_draw_line(canvas, 15, 30, 7, 31);
        canvas_draw_line(canvas, 7, 31, 5, 28);
        canvas_draw_dot(canvas, 19, 25);
    } else if(!strcmp(scene->sprite, "pearl")) {
        canvas_draw_circle(canvas, 14, 26, 8);
        canvas_draw_circle(canvas, 14, 26, 5);
        canvas_draw_dot(canvas, 12, 23);
    } else {
        canvas_draw_line(canvas, 3, 31, 8, 27);
        canvas_draw_line(canvas, 8, 27, 13, 31);
        canvas_draw_line(canvas, 13, 31, 18, 27);
        canvas_draw_line(canvas, 18, 27, 25, 31);
        canvas_draw_line(canvas, 3, 35, 8, 32);
        canvas_draw_line(canvas, 8, 32, 13, 35);
        canvas_draw_line(canvas, 13, 35, 18, 32);
        canvas_draw_line(canvas, 18, 32, 25, 35);
    }
}

static void adventure_prepare_campaign_rows(DndAdventureApp* app) {
    uint16_t rows = app->campaign_count + 2U;
    for(uint8_t visible = 0U; visible < 5U; ++visible) {
        char* row = app->campaign_rows[visible];
        row[0] = '\0';
        uint16_t index = app->scroll + visible;
        if(index >= rows) continue;
        if(index < app->campaign_count) {
            PocketCampaignSummary campaign;
            if(pocket_campaign_at(app->storage, index, &campaign)) {
                const char* label = campaign.name[0] ? campaign.name : campaign.id;
                snprintf(
                    row,
                    27U,
                    "%c %.24s",
                    app->active_campaign_valid && !strcmp(campaign.id, app->progress.campaign) ? '*' : ' ',
                    label);
            } else {
                snprintf(row, 27U, "  Campaign %u", (unsigned)(index + 1U));
            }
        } else if(index == app->campaign_count) {
            adventure_copy(row, 27U, "Campaign Diagnostics");
        } else {
            adventure_copy(row, 27U, "Installed Pack Controls");
        }
    }
}

static void adventure_draw_campaigns(Canvas* canvas, DndAdventureApp* app) {
    adventure_draw_header(canvas, "DNDAdventure", app->status);
    uint16_t rows = app->campaign_count + 2U;
    for(uint8_t visible = 0U; visible < 5U; ++visible) {
        uint16_t index = app->scroll + visible;
        if(index >= rows) break;
        adventure_draw_row(
            canvas,
            visible,
            index == app->selection,
            app->campaign_rows[visible][0] ? app->campaign_rows[visible] : "Campaign");
    }
}

static void adventure_draw_scene(Canvas* canvas, DndAdventureApp* app) {
    if(!app->scene) {
        adventure_draw_header(canvas, "Adventure", "No scene");
        return;
    }
    char title[48];
    snprintf(title, sizeof(title), "Adventure: %.24s", app->scene->title);
    adventure_draw_header(canvas, title, app->status);
    adventure_draw_sprite(canvas, app->scene);
    char line1[24], line2[24], line3[24];
    snprintf(line1, sizeof(line1), "%.23s", app->scene->body);
    snprintf(line2, sizeof(line2), "%.23s", strlen(app->scene->body) > 23U ? app->scene->body + 23U : "");
    snprintf(line3, sizeof(line3), "%.23s", strlen(app->scene->body) > 46U ? app->scene->body + 46U : "");
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 31, 19, line1);
    canvas_draw_str(canvas, 31, 28, line2);
    canvas_draw_str(canvas, 31, 37, line3);
    if(!app->adventure_started) {
        adventure_draw_row(canvas, 4U, true, "Start Adventure");
        return;
    }
    uint8_t visible_count = app->scene->choice_count > 2U ? 2U : app->scene->choice_count;
    for(uint8_t row = 0U; row < visible_count; ++row) {
        uint8_t index = (uint8_t)(app->scroll + row);
        if(index >= app->scene->choice_count) break;
        adventure_draw_row(canvas, (uint8_t)(3U + row), index == app->selection, app->scene->choices[index].label);
    }
}

static void adventure_draw_result(Canvas* canvas, DndAdventureApp* app) {
    adventure_draw_header(canvas, "Adventure Roll Result", NULL);
    char row[48];
    if(app->last_skill >= 0 && (uint8_t)app->last_skill < POCKET_D20_SKILL_COUNT)
        snprintf(row, sizeof(row), "%s check", pocket_d20_skill_names[(uint8_t)app->last_skill]);
    else
        adventure_copy(row, sizeof(row), "Check");
    adventure_draw_row(canvas, 0U, false, row);
    snprintf(row, sizeof(row), "d20 %u %+d", app->last_natural, app->last_modifier);
    adventure_draw_row(canvas, 1U, false, row);
    snprintf(row, sizeof(row), "Total %d vs DC %u", app->last_total, app->last_dc);
    adventure_draw_row(canvas, 2U, false, row);
    adventure_draw_row(canvas, 3U, false, app->last_passed ? "PASS" : "FAIL");
    adventure_draw_row(canvas, 4U, true, "OK: Continue");
}

static void adventure_draw_diagnostics(Canvas* canvas, DndAdventureApp* app) {
    adventure_draw_header(canvas, "Campaign Diagnostics", app->status);
    char rows[9][40];
    snprintf(rows[0], sizeof(rows[0]), "Manifests: %u", app->diagnostics.records);
    snprintf(rows[1], sizeof(rows[1]), "Incompatible: %u", app->diagnostics.incompatible);
    snprintf(rows[2], sizeof(rows[2]), "Missing files: %u", app->diagnostics.missing_scene_files);
    snprintf(rows[3], sizeof(rows[3]), "Duplicate packs: %u", app->diagnostics.duplicate_campaign_ids);
    snprintf(rows[4], sizeof(rows[4]), "Duplicate scenes: %u", app->diagnostics.duplicate_scene_ids);
    snprintf(rows[5], sizeof(rows[5]), "Missing entries: %u", app->diagnostics.missing_entry_scenes);
    snprintf(rows[6], sizeof(rows[6]), "Broken links: %u", app->diagnostics.broken_links);
    snprintf(rows[7], sizeof(rows[7]), "ID: %.24s", app->diagnostics.problem_id[0] ? app->diagnostics.problem_id : "none");
    snprintf(rows[8], sizeof(rows[8]), "Issue: %.23s", app->diagnostics.problem[0] ? app->diagnostics.problem : "none");
    for(uint8_t visible = 0U; visible < 5U; ++visible) {
        uint16_t index = app->scroll + visible;
        if(index >= 9U) break;
        adventure_draw_row(canvas, visible, index == app->selection, rows[index]);
    }
}

static void adventure_prepare_pack_rows(DndAdventureApp* app) {
    for(uint8_t visible = 0U; visible < 5U; ++visible) {
        app->pack_rows[visible][0] = '\0';
        uint16_t index = app->scroll + visible;
        if(index > app->pack_count) break;
        if(index == app->pack_count) {
            adventure_copy(app->pack_rows[visible], sizeof(app->pack_rows[visible]), "Install Campaign Inbox");
        } else {
            PocketCampaignPackSummary pack;
            if(pocket_campaign_pack_at(app->storage, index, &pack))
                snprintf(
                    app->pack_rows[visible],
                    sizeof(app->pack_rows[visible]),
                    "%c %.23s",
                    pack.enabled ? '*' : ' ',
                    pack.name);
            else
                adventure_copy(app->pack_rows[visible], sizeof(app->pack_rows[visible]), "Pack unavailable");
        }
    }
}

static void adventure_draw_pack_preview(Canvas* canvas, DndAdventureApp* app) {
    adventure_draw_header(canvas, "Campaign Inbox Preview", app->status);
    char row[48];
    if(app->inbox_preview_valid) {
        snprintf(row, sizeof(row), "Name: %.20s", app->inbox_preview.name);
        adventure_draw_row(canvas, 0U, false, row);
        snprintf(row, sizeof(row), "ID: %.22s", app->inbox_preview.id);
        adventure_draw_row(canvas, 1U, false, row);
        adventure_draw_row(canvas, 3U, true, "Hold OK: install");
        adventure_draw_row(canvas, 4U, false, "Back: cancel");
    } else {
        adventure_draw_row(canvas, 1U, false, "Inbox pack invalid");
        adventure_draw_row(canvas, 4U, false, "Back: cancel");
    }
}

static void adventure_draw_packs(Canvas* canvas, DndAdventureApp* app) {
    adventure_draw_header(canvas, "Campaign Pack Controls", app->status);
    uint16_t rows = app->pack_count + 1U;
    for(uint8_t visible = 0U; visible < 5U; ++visible) {
        uint16_t index = app->scroll + visible;
        if(index >= rows) break;
        adventure_draw_row(canvas, visible, index == app->selection, app->pack_rows[visible]);
    }
}

static void adventure_draw(Canvas* canvas, void* model) {
    DndAdventureApp* app = *(DndAdventureApp**)model;
    canvas_clear(canvas);
    switch(app->screen) {
    case DndAdventureScreenCampaigns:
        adventure_draw_campaigns(canvas, app);
        break;
    case DndAdventureScreenAdventure:
        adventure_draw_scene(canvas, app);
        break;
    case DndAdventureScreenResult:
        adventure_draw_result(canvas, app);
        break;
    case DndAdventureScreenDiagnostics:
        adventure_draw_diagnostics(canvas, app);
        break;
    case DndAdventureScreenPacks:
        adventure_draw_packs(canvas, app);
        break;
    case DndAdventureScreenPackPreview:
        adventure_draw_pack_preview(canvas, app);
        break;
    }
}

static void adventure_refresh(DndAdventureApp* app) {
    view_commit_model(app->view, true);
}

static void adventure_move(DndAdventureApp* app, uint16_t count, int8_t delta, uint8_t visible) {
    if(!count) return;
    int32_t next = (int32_t)app->selection + delta;
    if(next < 0) next = (int32_t)count - 1;
    if(next >= count) next = 0;
    app->selection = (uint16_t)next;
    if(app->selection < app->scroll) app->scroll = app->selection;
    if(app->selection >= app->scroll + visible) app->scroll = app->selection - visible + 1U;
}

static void adventure_return_to_dnd(DndAdventureApp* app) {
    app->return_to_dnd = 1U;
    /* Scene/cache allocations are not needed for the return handoff. Release
       them before the dispatcher exits; adventure_app_free() remains idempotent
       and performs the complete teardown before Loader is opened. */
    adventure_scene_free(app);
    pocket_campaign_cache_reset();
    view_dispatcher_stop(app->dispatcher);
}

static void adventure_back(DndAdventureApp* app) {
    if(app->screen == DndAdventureScreenCampaigns) {
        adventure_return_to_dnd(app);
    } else if(app->screen == DndAdventureScreenAdventure || app->screen == DndAdventureScreenResult) {
        adventure_save_progress(app);
        adventure_save_character(app);
        app->screen = DndAdventureScreenCampaigns;
        app->selection = 0U;
        app->scroll = 0U;
        app->status[0] = '\0';
        adventure_prepare_campaign_rows(app);
    } else if(app->screen == DndAdventureScreenPackPreview) {
        app->screen = DndAdventureScreenPacks;
        app->selection = app->pack_count;
        app->scroll = app->pack_count > 4U ? app->pack_count - 4U : 0U;
        app->status[0] = '\0';
        adventure_prepare_pack_rows(app);
    } else {
        app->screen = DndAdventureScreenCampaigns;
        app->selection = 0U;
        app->scroll = 0U;
        app->status[0] = '\0';
        adventure_prepare_campaign_rows(app);
    }
}

static bool adventure_input(InputEvent* event, void* context) {
    DndAdventureApp* app = context;
    if(event->type == InputTypeLong && event->key == InputKeyBack) {
        adventure_return_to_dnd(app);
        return true;
    }
    if(event->type == InputTypeShort && event->key == InputKeyBack) {
        adventure_back(app);
        adventure_refresh(app);
        return true;
    }

    bool move = event->type == InputTypeShort || event->type == InputTypeRepeat;
    if(app->screen == DndAdventureScreenCampaigns) {
        uint16_t rows = app->campaign_count + 2U;
        if(move && event->key == InputKeyUp) {
            adventure_move(app, rows, -1, 5U);
            adventure_prepare_campaign_rows(app);
        } else if(move && event->key == InputKeyDown) {
            adventure_move(app, rows, 1, 5U);
            adventure_prepare_campaign_rows(app);
        } else if(event->type == InputTypeShort && event->key == InputKeyOk) {
            if(app->selection < app->campaign_count) {
                adventure_select_campaign(app, app->selection);
            } else if(app->selection == app->campaign_count) {
                pocket_campaign_diagnose(app->storage, &app->diagnostics);
                app->screen = DndAdventureScreenDiagnostics;
                app->selection = 0U;
                app->scroll = 0U;
            } else {
                app->pack_count = pocket_campaign_pack_count(app->storage);
                app->screen = DndAdventureScreenPacks;
                app->selection = 0U;
                app->scroll = 0U;
                adventure_prepare_pack_rows(app);
            }
        }
    } else if(app->screen == DndAdventureScreenAdventure) {
        if(!app->scene) {
            adventure_set_status(app, "No scene");
        } else if(!app->adventure_started) {
            if(event->type == InputTypeShort && event->key == InputKeyOk) {
                app->adventure_started = 1U;
                app->selection = 0U;
                app->scroll = 0U;
                app->status[0] = '\0';
            }
        } else if(move && event->key == InputKeyUp) {
            adventure_move(app, app->scene->choice_count, -1, 2U);
        } else if(move && event->key == InputKeyDown) {
            adventure_move(app, app->scene->choice_count, 1, 2U);
        } else if(event->type == InputTypeLong && event->key == InputKeyOk) {
            adventure_copy(app->progress.checkpoint, sizeof(app->progress.checkpoint), app->progress.scene);
            adventure_set_status(app, adventure_save_progress(app) ? "Checkpoint saved [X]" : "Checkpoint save failed");
        } else if(event->type == InputTypeLong && event->key == InputKeyLeft) {
            adventure_copy(app->progress.scene, sizeof(app->progress.scene), app->progress.checkpoint);
            if(adventure_load_scene(app)) {
                app->selection = 0U;
                app->scroll = 0U;
                adventure_save_progress(app);
                adventure_set_status(app, "Checkpoint loaded");
            }
        } else if(event->type == InputTypeLong && event->key == InputKeyRight) {
            adventure_copy(app->progress.scene, sizeof(app->progress.scene), app->active_campaign.entry_scene);
            if(adventure_load_scene(app)) {
                app->selection = 0U;
                app->scroll = 0U;
                adventure_save_progress(app);
                adventure_set_status(app, "Adventure restarted");
            }
        } else if(event->type == InputTypeShort && event->key == InputKeyOk &&
                  app->selection < app->scene->choice_count) {
            DndAdventureChoice choice = app->scene->choices[app->selection];
            app->status[0] = '\0';
            adventure_apply_choice(app, &choice);
        }
    } else if(app->screen == DndAdventureScreenResult) {
        if(event->type == InputTypeShort && event->key == InputKeyOk) {
            app->screen = DndAdventureScreenAdventure;
            app->status[0] = '\0';
        }
    } else if(app->screen == DndAdventureScreenDiagnostics) {
        if(move && event->key == InputKeyUp)
            adventure_move(app, 9U, -1, 5U);
        else if(move && event->key == InputKeyDown)
            adventure_move(app, 9U, 1, 5U);
        else if(event->type == InputTypeShort && event->key == InputKeyOk) {
            pocket_campaign_diagnose(app->storage, &app->diagnostics);
            adventure_set_status(
                app,
                app->diagnostics.incompatible || app->diagnostics.missing_scene_files ||
                        app->diagnostics.duplicate_campaign_ids || app->diagnostics.duplicate_scene_ids ||
                        app->diagnostics.missing_entry_scenes || app->diagnostics.broken_links ?
                    "Pack needs attention" :
                    "Campaign packs OK");
        }
    } else if(app->screen == DndAdventureScreenPacks) {
        uint16_t rows = app->pack_count + 1U;
        if(move && event->key == InputKeyUp) {
            adventure_move(app, rows, -1, 5U);
            adventure_prepare_pack_rows(app);
        } else if(move && event->key == InputKeyDown) {
            adventure_move(app, rows, 1, 5U);
            adventure_prepare_pack_rows(app);
        }
        else if(event->type == InputTypeLong && event->key == InputKeyOk && app->selection < app->pack_count) {
            PocketCampaignPackSummary pack;
            if(pocket_campaign_pack_at(app->storage, app->selection, &pack)) {
                bool changed = pocket_campaign_pack_set_enabled(app->storage, pack.id, !pack.enabled);
                adventure_set_status(
                    app,
                    changed ? (pack.enabled ? "Pack marked inactive" : "Pack marked active") :
                              "Pack update failed");
                if(changed) {
                    pocket_campaign_cache_reset();
                    app->campaign_count = pocket_campaign_count(app->storage);
                    app->pack_count = pocket_campaign_pack_count(app->storage);
                    app->active_campaign_valid = 0U;
                    adventure_scene_free(app);
                    app->selection = 0U;
                    app->scroll = 0U;
                    adventure_prepare_campaign_rows(app);
                    adventure_prepare_pack_rows(app);
                }
            }
        } else if(event->type == InputTypeShort && event->key == InputKeyOk && app->selection == app->pack_count) {
            app->inbox_preview_valid = pocket_campaign_pack_preview_inbox(
                                           app->storage,
                                           &app->inbox_preview,
                                           app->status,
                                           sizeof(app->status)) ?
                                           1U :
                                           0U;
            app->screen = DndAdventureScreenPackPreview;
            app->selection = 0U;
            app->scroll = 0U;
        }
    } else if(app->screen == DndAdventureScreenPackPreview) {
        if(event->type == InputTypeLong && event->key == InputKeyOk && app->inbox_preview_valid) {
            bool changed = pocket_campaign_pack_install_inbox(app->storage, app->status, sizeof(app->status));
            if(changed) {
                pocket_campaign_cache_reset();
                app->campaign_count = pocket_campaign_count(app->storage);
                app->pack_count = pocket_campaign_pack_count(app->storage);
                app->active_campaign_valid = 0U;
                adventure_scene_free(app);
                app->screen = DndAdventureScreenPacks;
                app->selection = 0U;
                app->scroll = 0U;
                adventure_prepare_campaign_rows(app);
                adventure_prepare_pack_rows(app);
            }
        }
    }
    adventure_refresh(app);
    return true;
}

static bool adventure_navigation(void* context) {
    DndAdventureApp* app = context;
    adventure_back(app);
    adventure_refresh(app);
    return true;
}

static bool adventure_load_character(DndAdventureApp* app, const char* args) {
    bool have_profile = false;
    if(args && args[0]) {
        uint32_t profile = 0U;
        char profile_path[POCKET_D20_PATH_LEN];
        if(adventure_parse_u32(args, UINT32_MAX, &profile) &&
           dnd_profile_ref_path(app->storage, profile, profile_path, sizeof(profile_path))) {
            app->profile = profile;
            have_profile = true;
        }
    }
    if(!have_profile && dnd_profile_ref_active(app->storage, &app->profile))
        have_profile = true;
    if(!have_profile) app->profile = 0U;
    bool recovered = false;
    app->character_loaded = pocket_d20_storage_load_profile(
                                app->storage, app->profile, &app->character_data, &recovered) ?
                                1U :
                                0U;
    if(app->character_loaded && recovered)
        app->character_loaded = pocket_d20_storage_restore_backup(
                                    app->storage, app->profile, &app->character_data) ?
                                    1U :
                                    0U;
    return app->character_loaded != 0U;
}

static DndAdventureApp* adventure_app_alloc(const char* args) {
    DndAdventureApp* app = calloc(1U, sizeof(DndAdventureApp));
    if(!app) return NULL;
    app->gui = furi_record_open(RECORD_GUI);
    app->storage = furi_record_open(RECORD_STORAGE);
    if(!app->gui || !app->storage) goto fail;

    /* Reserve the fixed dispatcher/view before campaign indexing and scene
       storage allocate variable-sized buffers. */
    app->dispatcher = view_dispatcher_alloc();
    app->view = view_alloc();
    if(!app->dispatcher || !app->view) goto fail;
    view_dispatcher_set_event_callback_context(app->dispatcher, app);
    view_dispatcher_set_navigation_event_callback(app->dispatcher, adventure_navigation);
    view_allocate_model(app->view, ViewModelTypeLockFree, sizeof(DndAdventureApp*));
    DndAdventureApp** model = view_get_model(app->view);
    if(!model) goto fail;
    *model = app;
    view_commit_model(app->view, false);
    view_set_context(app->view, app);
    view_set_draw_callback(app->view, adventure_draw);
    view_set_input_callback(app->view, adventure_input);

    adventure_load_character(app, args);
    if(!pocket_campaign_pack_ensure_enabled(app->storage))
        adventure_set_status(app, "Pack index rebuild failed");
    app->campaign_count = pocket_campaign_count(app->storage);
    app->pack_count = pocket_campaign_pack_count(app->storage);
    if(!app->campaign_count && !app->status[0]) adventure_set_status(app, "No campaign manifests");
    adventure_resolve_active(app);
    adventure_prepare_campaign_rows(app);

    app->screen = DndAdventureScreenCampaigns;
    view_dispatcher_add_view(app->dispatcher, 0U, app->view);
    view_dispatcher_attach_to_gui(app->dispatcher, app->gui, ViewDispatcherTypeFullscreen);
    return app;

fail:
    adventure_scene_free(app);
    pocket_campaign_cache_reset();
    pocket_d20_data_clear(&app->character_data);
    if(app->view) view_free(app->view);
    if(app->dispatcher) view_dispatcher_free(app->dispatcher);
    if(app->storage) furi_record_close(RECORD_STORAGE);
    if(app->gui) furi_record_close(RECORD_GUI);
    free(app);
    return NULL;
}

static void adventure_app_free(DndAdventureApp* app) {
    if(!app) return;
    adventure_save_progress(app);
    adventure_save_character(app);
    adventure_scene_free(app);
    pocket_campaign_cache_reset();
    if(app->dispatcher && app->view) view_dispatcher_remove_view(app->dispatcher, 0U);
    if(app->view) view_free(app->view);
    if(app->dispatcher) view_dispatcher_free(app->dispatcher);
    pocket_d20_data_clear(&app->character_data);
    if(app->storage) furi_record_close(RECORD_STORAGE);
    if(app->gui) furi_record_close(RECORD_GUI);
    free(app);
}

int32_t dndadventure_app(void* context) {
    DndAdventureApp* app = adventure_app_alloc(context);
    if(!app) return -1;
    view_dispatcher_switch_to_view(app->dispatcher, 0U);
    view_dispatcher_run(app->dispatcher);
    bool return_to_dnd = app->return_to_dnd;
    adventure_app_free(app);
    if(return_to_dnd) {
        if(!dnd_handoff_launch(DNDOLPHINS_FAP_PATH, NULL)) return -1;
    }
    return 0;
}
