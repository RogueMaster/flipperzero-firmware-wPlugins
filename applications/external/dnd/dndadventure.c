#include "dnd_data.h"
#include "dndadventure_campaign_packs.h"
#include "dndadventure_campaigns.h"
#include "dnd_fs.h"
#include "dnd_profile_handoff.h"
#include "dnd_profile_projection.h"
#include "dnd_rules.h"
#include "dndadventure_item_reward.h"
#include "dnd_storage.h"

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
#define ADVENTURE_PREVIEW_WIDTH 22U
#define ADVENTURE_FULL_TEXT_WIDTH 26U

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
    DndAdventureScreenFullText,
    DndAdventureScreenRestartConfirm,
} DndAdventureScreen;

typedef struct {
    Gui* gui;
    Storage* storage;
    ViewDispatcher* dispatcher;
    View* view;

    DndAdventureProfileProjection character;
    uint32_t profile;
    uint8_t character_loaded;
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
    uint8_t full_text_offset;

    int16_t last_total;
    int8_t last_modifier;
    int8_t last_skill;
    uint8_t last_natural;
    uint8_t last_dc;
    uint8_t last_passed;
} DndAdventureApp;

static void dndadventure_copy(char* destination, size_t size, const char* source) {
    if(!destination || !size) return;
    strncpy(destination, source ? source : "", size - 1U);
    destination[size - 1U] = '\0';
}

static void dndadventure_set_status(DndAdventureApp* app, const char* status) {
    dndadventure_copy(app->status, sizeof(app->status), status);
}

static uint8_t dndadventure_character_level(const DndAdventureProfileProjection* character) {
    uint16_t total = 0U;
    if(character) {
        for(uint8_t i = 0U; i < character->class_count && i < POCKET_D20_MAX_CLASSES; ++i)
            total += character->class_levels[i];
    }
    if(total < 1U) return 1U;
    return total > 20U ? 20U : (uint8_t)total;
}

static int8_t dndadventure_skill_modifier(
    const DndAdventureProfileProjection* character,
    uint8_t skill) {
    if(!character || skill >= POCKET_D20_SKILL_COUNT) return 0;
    uint8_t ability = dnd_rules_core_skill_abilities[skill];
    int16_t total = dnd_rules_core_ability_modifier(character->ability_scores[ability]) +
                    character->skill_misc[skill];
    uint8_t pb = (uint8_t)(2U + (dndadventure_character_level(character) - 1U) / 4U);
    if(character->skill_proficiency[skill] == PocketProficiencyProficient) total += pb;
    else if(character->skill_proficiency[skill] == PocketProficiencyExpertise) total += pb * 2U;
    if(total < -128) total = -128;
    if(total > 127) total = 127;
    return (int8_t)total;
}

static bool dndadventure_parse_u32(const char* text, uint32_t maximum, uint32_t* output) {
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

static bool dndadventure_parse_i8(const char* text, int8_t* output) {
    if(!text || !text[0] || !output) return false;
    bool negative = *text == '-';
    if(negative) ++text;
    uint32_t value = 0U;
    if(!dndadventure_parse_u32(text, negative ? 128U : 127U, &value)) return false;
    *output = negative ? (int8_t)(-(int16_t)value) : (int8_t)value;
    return true;
}

static void dndadventure_reader_init(DndAdventureReader* reader, File* file) {
    memset(reader, 0, sizeof(*reader));
    reader->file = file;
}

static bool dndadventure_read_line(DndAdventureReader* reader, char* line, size_t size) {
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

static uint8_t dndadventure_split(char* line, char** fields, uint8_t capacity) {
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

static void dndadventure_scene_free(DndAdventureApp* app) {
    free(app->scene);
    app->scene = NULL;
}

static void dndadventure_process_scene_line(
    DndAdventureScene* scene,
    const char* target,
    char* line,
    bool* found) {
    if(!line[0] || line[0] == '#') return;
    char* fields[11];
    uint8_t count = dndadventure_split(line, fields, 11U);
    if(count == 5U && !strcmp(fields[0], "S") && !strcmp(fields[1], target)) {
        dndadventure_copy(scene->id, sizeof(scene->id), fields[1]);
        dndadventure_copy(scene->title, sizeof(scene->title), fields[2]);
        dndadventure_copy(scene->body, sizeof(scene->body), fields[3]);
        dndadventure_copy(scene->sprite, sizeof(scene->sprite), fields[4]);
        *found = true;
    } else if(
        count == 11U && !strcmp(fields[0], "C") && !strcmp(fields[1], target) &&
        scene->choice_count < ADVENTURE_MAX_CHOICES) {
        int8_t skill = 0;
        uint32_t dc = 0U, quest = 0U, achievement = 0U;
        if(!dndadventure_parse_i8(fields[3], &skill) ||
           !dndadventure_parse_u32(fields[4], UINT8_MAX, &dc) ||
           !dndadventure_parse_u32(fields[9], UINT8_MAX, &quest) ||
           !dndadventure_parse_u32(fields[10], UINT8_MAX, &achievement))
            return;
        DndAdventureChoice* choice = &scene->choices[scene->choice_count++];
        memset(choice, 0, sizeof(*choice));
        dndadventure_copy(choice->label, sizeof(choice->label), fields[2]);
        choice->skill = skill;
        choice->dc = (uint8_t)dc;
        dndadventure_copy(choice->success_scene, sizeof(choice->success_scene), fields[5]);
        dndadventure_copy(choice->failure_scene, sizeof(choice->failure_scene), fields[6]);
        dndadventure_copy(choice->reward_item, sizeof(choice->reward_item), fields[7]);
        dndadventure_copy(choice->milestone, sizeof(choice->milestone), fields[8]);
        choice->quest_flag = (uint8_t)quest;
        choice->achievement = (uint8_t)achievement;
    }
}

static bool dndadventure_load_scene(DndAdventureApp* app) {
    dndadventure_scene_free(app);
    if(!app->active_campaign_valid || !app->progress.scene[0]) return false;
    DndAdventureScene* scene = calloc(1U, sizeof(DndAdventureScene));
    if(!scene) {
        dndadventure_set_status(app, "Adventure memory low");
        return false;
    }
    char path[POCKET_D20_LONG_PATH_LEN];
    if(!dndadventure_campaigns_scene_path(app->storage, &app->active_campaign, path, sizeof(path))) {
        free(scene);
        dndadventure_set_status(app, "Campaign scene file missing");
        return false;
    }
    File* file = storage_file_alloc(app->storage);
    if(!file) {
        free(scene);
        dndadventure_set_status(app, "Adventure memory low");
        return false;
    }
    if(!storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_free(file);
        free(scene);
        dndadventure_set_status(app, "Campaign scene file missing");
        return false;
    }
    DndAdventureReader reader;
    dndadventure_reader_init(&reader, file);
    char line[ADVENTURE_LINE_LEN];
    bool found = false;
    while(dndadventure_read_line(&reader, line, sizeof(line)))
        dndadventure_process_scene_line(scene, app->progress.scene, line, &found);
    bool io_ok = storage_file_get_error(file) == FSE_OK;
    storage_file_close(file);
    storage_file_free(file);
    if(!io_ok || !found || !scene->choice_count) {
        free(scene);
        dndadventure_set_status(app, "Scene invalid");
        return false;
    }
    app->scene = scene;
    return true;
}

static bool dndadventure_save_progress(DndAdventureApp* app) {
    if(!app->active_campaign_valid || !app->character_loaded) return true;
    return dndadventure_campaigns_progress_save(
        app->storage, app->profile, &app->active_campaign, &app->progress);
}

static bool dndadventure_resolve_active(DndAdventureApp* app) {
    if(app->active_campaign_valid) return true;
    char active_id[POCKET_CAMPAIGN_ID_LEN];
    PocketCampaignSummary campaign;
    bool loaded = app->character_loaded &&
                  dndadventure_campaigns_active_load(
                      app->storage, app->profile, active_id, sizeof(active_id));
    bool found = loaded && active_id[0] && dndadventure_campaigns_find(app->storage, active_id, &campaign);
    if(!found) found = dndadventure_campaigns_at(app->storage, 0U, &campaign);
    if(!found) return false;
    app->active_campaign = campaign;
    app->active_campaign_valid = 1U;
    if(!app->character_loaded) {
        memset(&app->progress, 0, sizeof(app->progress));
        dndadventure_copy(app->progress.campaign, sizeof(app->progress.campaign), campaign.id);
        dndadventure_copy(app->progress.scene, sizeof(app->progress.scene), campaign.entry_scene);
        return true;
    }
    return dndadventure_campaigns_progress_load(app->storage, app->profile, &campaign, &app->progress);
}

static bool dndadventure_select_campaign(DndAdventureApp* app, uint16_t index) {
    PocketCampaignSummary next;
    if(!dndadventure_campaigns_at(app->storage, index, &next)) {
        dndadventure_set_status(app, "Campaign record invalid");
        return false;
    }
    if(next.pack_version != POCKET_CAMPAIGN_PACK_VERSION ||
       next.minimum_app > POCKET_CAMPAIGN_APP_VERSION ||
       (next.maximum_app && next.maximum_app < POCKET_CAMPAIGN_APP_VERSION)) {
        dndadventure_set_status(app, "Campaign incompatible");
        return false;
    }
    if(app->active_campaign_valid && !dndadventure_save_progress(app)) {
        dndadventure_set_status(app, "Progress save failed");
        return false;
    }
    app->active_campaign = next;
    app->active_campaign_valid = 1U;
    if(app->character_loaded) {
        if(!dndadventure_campaigns_progress_load(app->storage, app->profile, &next, &app->progress))
            return false;
    } else {
        memset(&app->progress, 0, sizeof(app->progress));
        dndadventure_copy(app->progress.campaign, sizeof(app->progress.campaign), next.id);
        dndadventure_copy(app->progress.scene, sizeof(app->progress.scene), next.entry_scene);
    }
    if(!dndadventure_load_scene(app)) return false;
    app->screen = DndAdventureScreenAdventure;
    app->selection = 0U;
    app->scroll = 0U;
    app->adventure_started = 0U;
    app->status[0] = '\0';
    return true;
}

static void dndadventure_reward_item(DndAdventureApp* app, const char* name) {
    if(!app->character_loaded || !name || !name[0] || !strcmp(name, "-")) return;
    if(!dndadventure_item_reward_grant_reward(
           app->storage,
           app->profile,
           &app->character,
           name,
           "Adventure reward"))
        dndadventure_set_status(app, "Item reward save failed");
}

static bool dndadventure_journal_write_string(File* file, const char* key, const char* value) {
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

static bool dndadventure_writef(File* file, const char* format, ...) {
    char line[160];
    va_list args;
    va_start(args, format);
    int length = vsnprintf(line, sizeof(line), format, args);
    va_end(args);
    return length > 0 && (size_t)length < sizeof(line) &&
           storage_file_write(file, line, (size_t)length) == (size_t)length;
}

static bool dndadventure_write_milestone_journal(DndAdventureApp* app, const char* milestone) {
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
            "Milestone reached in %s. Use Continue active Adventure from this entry to resume.",
            app->active_campaign.name[0] ? app->active_campaign.name : "Adventure");
        bool ok = storage_file_open(file, path, FSAM_WRITE, FSOM_CREATE_ALWAYS) &&
                  dndadventure_writef(file, "PocketD20Journal=1\n") &&
                  dndadventure_writef(file, "CharacterId=%lu\n", (unsigned long)app->profile) &&
                  dndadventure_journal_write_string(file, "Title", milestone) &&
                  dndadventure_journal_write_string(file, "Body", body) &&
                  dndadventure_writef(file, "Category=3\n") &&
                  dndadventure_writef(file, "Completed=0\n") &&
                  dndadventure_writef(file, "LevelGranted=0\n") &&
                  dndadventure_writef(file, "ClassIndex=0\n") &&
                  dndadventure_writef(file, "End=OK\n") && storage_file_sync(file);
        storage_file_close(file);
        storage_file_free(file);
        if(!ok) storage_common_remove(app->storage, path);
        return ok;
    }
    return false;
}

static bool dndadventure_apply_choice(DndAdventureApp* app, const DndAdventureChoice* choice) {
    uint8_t natural = 0U;
    int8_t modifier = 0;
    bool passed = true;
    if(choice->skill >= 0 && (uint8_t)choice->skill < POCKET_D20_SKILL_COUNT) {
        if(!app->character_loaded) {
            dndadventure_set_status(app, "Character not available");
            return false;
        }
        natural = (uint8_t)dnd_rules_core_roll_dice(1U, 20U);
        modifier = dndadventure_skill_modifier(&app->character, (uint8_t)choice->skill);
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
    dndadventure_copy(previous_scene, sizeof(previous_scene), app->progress.scene);

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
    if(next[0] && strcmp(next, "-")) dndadventure_copy(app->progress.scene, sizeof(app->progress.scene), next);
    if(!dndadventure_load_scene(app)) {
        app->progress.quest_flags = previous_quest_flags;
        app->progress.achievements = previous_achievements;
        dndadventure_copy(app->progress.scene, sizeof(app->progress.scene), previous_scene);
        dndadventure_load_scene(app);
        dndadventure_set_status(app, "Next scene missing");
        return false;
    }
    app->selection = 0U;
    app->scroll = 0U;
    bool progress_saved = dndadventure_save_progress(app);
    if(!progress_saved) {
        app->progress.quest_flags = previous_quest_flags;
        app->progress.achievements = previous_achievements;
        dndadventure_copy(app->progress.scene, sizeof(app->progress.scene), previous_scene);
        dndadventure_load_scene(app);
        dndadventure_set_status(app, "Progress save failed");
        return false;
    } else if(grant_pending) {
        dndadventure_reward_item(app, choice->reward_item);
        if(milestone_present && !milestone_guarded) {
            dndadventure_set_status(app, "Milestone requires flag/achievement");
        } else if(milestone_present && !dndadventure_write_milestone_journal(app, choice->milestone)) {
            dndadventure_set_status(app, "Milestone set; Journal write failed");
        }
    }
    if(choice->skill >= 0) app->screen = DndAdventureScreenResult;
    else if(!app->status[0]) dndadventure_set_status(app, "Choice applied");
    return true;
}

static void dndadventure_draw_header(Canvas* canvas, DndAdventureApp* app, const char* title, const char* status) {
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_box(canvas, 0, 0, 128, 10);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 8, title);
    if(status && status[0]) {
        uint16_t width = canvas_string_width(canvas, status);
        if(width < 58U) canvas_draw_str(canvas, 126U - width, 8, status);
    }
    if(app && app->screen == DndAdventureScreenCampaigns && app->profile != UINT32_MAX) {
        char profile_id[16];
        snprintf(profile_id, sizeof(profile_id), "[%lu]", (unsigned long)app->profile);
        uint16_t id_width = canvas_string_width(canvas, profile_id);
        uint8_t id_x = id_width < 125U ? (uint8_t)(126U - id_width) : 1U;
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_box(canvas, id_x > 1U ? (uint8_t)(id_x - 1U) : 0U, 0,
                        (uint8_t)(id_width + 2U), 10);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_str(canvas, id_x, 8, profile_id);
    }
    canvas_set_color(canvas, ColorBlack);
}

static void dndadventure_draw_row(Canvas* canvas, uint8_t row, bool selected, const char* text) {
    uint8_t y = (uint8_t)(11U + row * 10U);
    if(selected) {
        canvas_draw_box(canvas, 0, y, 128, 10);
        canvas_set_color(canvas, ColorWhite);
    }
    canvas_set_font(canvas, FontSecondary);
    char shown[27];
    dndadventure_copy(shown, sizeof(shown), text);
    canvas_draw_str(canvas, 2, (uint8_t)(y + 8U), shown);
    if(selected) canvas_set_color(canvas, ColorBlack);
}

static void dndadventure_draw_sprite(Canvas* canvas, const DndAdventureScene* scene) {
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

static void dndadventure_prepare_campaign_rows(DndAdventureApp* app) {
    uint16_t rows = app->campaign_count + 1U;
    for(uint8_t visible = 0U; visible < 5U; ++visible) {
        char* row = app->campaign_rows[visible];
        row[0] = '\0';
        uint16_t index = app->scroll + visible;
        if(index >= rows) continue;
        if(index < app->campaign_count) {
            PocketCampaignSummary campaign;
            if(dndadventure_campaigns_at(app->storage, index, &campaign)) {
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
        } else {
            dndadventure_copy(row, 27U, "Restart Current Adventure");
        }
    }
}

static void dndadventure_draw_campaigns(Canvas* canvas, DndAdventureApp* app) {
    dndadventure_draw_header(canvas, app, "DNDAdventure", app->status);
    uint16_t rows = app->campaign_count + 1U;
    for(uint8_t visible = 0U; visible < 5U; ++visible) {
        uint16_t index = app->scroll + visible;
        if(index >= rows) break;
        dndadventure_draw_row(
            canvas,
            visible,
            index == app->selection,
            app->campaign_rows[visible][0] ? app->campaign_rows[visible] : "Campaign");
    }
}

static void dndadventure_draw_scene(Canvas* canvas, DndAdventureApp* app) {
    if(!app->scene) {
        dndadventure_draw_header(canvas, app, "Adventure", "No scene");
        return;
    }
    char title[48];
    snprintf(title, sizeof(title), "Adventure: %.24s", app->scene->title);
    dndadventure_draw_header(canvas, app, title, app->status);
    dndadventure_draw_sprite(canvas, app->scene);
    char line1[ADVENTURE_PREVIEW_WIDTH + 1U];
    char line2[ADVENTURE_PREVIEW_WIDTH + 1U];
    char line3[ADVENTURE_PREVIEW_WIDTH + 1U];
    snprintf(line1, sizeof(line1), "%.*s", (int)ADVENTURE_PREVIEW_WIDTH, app->scene->body);
    snprintf(
        line2,
        sizeof(line2),
        "%.*s",
        (int)ADVENTURE_PREVIEW_WIDTH,
        strlen(app->scene->body) > ADVENTURE_PREVIEW_WIDTH ?
            app->scene->body + ADVENTURE_PREVIEW_WIDTH :
            "");
    snprintf(
        line3,
        sizeof(line3),
        "%.*s",
        (int)ADVENTURE_PREVIEW_WIDTH,
        strlen(app->scene->body) > ADVENTURE_PREVIEW_WIDTH * 2U ?
            app->scene->body + ADVENTURE_PREVIEW_WIDTH * 2U :
            "");
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 31, 19, line1);
    canvas_draw_str(canvas, 31, 28, line2);
    canvas_draw_str(canvas, 31, 37, line3);
    if(!app->adventure_started) {
        dndadventure_draw_row(canvas, 4U, true, "Start Adventure");
        return;
    }
    uint8_t visible_count = app->scene->choice_count > 2U ? 2U : app->scene->choice_count;
    for(uint8_t row = 0U; row < visible_count; ++row) {
        uint8_t index = (uint8_t)(app->scroll + row);
        if(index >= app->scene->choice_count) break;
        dndadventure_draw_row(canvas, (uint8_t)(3U + row), index == app->selection, app->scene->choices[index].label);
    }
}

static void dndadventure_draw_result(Canvas* canvas, DndAdventureApp* app) {
    dndadventure_draw_header(canvas, app, "Adventure Roll Result", NULL);
    char row[48];
    if(app->last_skill >= 0 && (uint8_t)app->last_skill < POCKET_D20_SKILL_COUNT)
        snprintf(row, sizeof(row), "%s check", dnd_rules_core_skill_names[(uint8_t)app->last_skill]);
    else
        dndadventure_copy(row, sizeof(row), "Check");
    dndadventure_draw_row(canvas, 0U, false, row);
    snprintf(row, sizeof(row), "d20 %u %+d", app->last_natural, app->last_modifier);
    dndadventure_draw_row(canvas, 1U, false, row);
    snprintf(row, sizeof(row), "Total %d vs DC %u", app->last_total, app->last_dc);
    dndadventure_draw_row(canvas, 2U, false, row);
    dndadventure_draw_row(canvas, 3U, false, app->last_passed ? "PASS" : "FAIL");
    dndadventure_draw_row(canvas, 4U, true, "OK: Continue");
}

static const char* dndadventure_next_full_text_line(
    const char* cursor, char* output, size_t output_size) {
    if(!cursor || !output || output_size < 2U) return NULL;
    while(*cursor == ' ' || *cursor == '\r' || *cursor == '\n') ++cursor;
    if(!*cursor) {
        output[0] = '\0';
        return cursor;
    }
    size_t length = 0U;
    size_t last_space = SIZE_MAX;
    while(cursor[length] && cursor[length] != '\r' && cursor[length] != '\n' &&
          length < ADVENTURE_FULL_TEXT_WIDTH) {
        if(cursor[length] == ' ') last_space = length;
        ++length;
    }
    if(length == ADVENTURE_FULL_TEXT_WIDTH && cursor[length] && cursor[length] != ' ' &&
       cursor[length] != '\r' && cursor[length] != '\n' && last_space != SIZE_MAX)
        length = last_space;
    if(length >= output_size) length = output_size - 1U;
    memcpy(output, cursor, length);
    while(length && output[length - 1U] == ' ') --length;
    output[length] = '\0';
    cursor += length;
    while(*cursor == ' ' || *cursor == '\r' || *cursor == '\n') ++cursor;
    return cursor;
}

static uint8_t dndadventure_full_text_line_count(const char* text) {
    uint8_t count = 0U;
    char line[ADVENTURE_FULL_TEXT_WIDTH + 1U];
    const char* cursor = text;
    while(cursor && *cursor && count < UINT8_MAX) {
        const char* next = dndadventure_next_full_text_line(cursor, line, sizeof(line));
        if(!next || next == cursor) break;
        ++count;
        cursor = next;
    }
    return count ? count : 1U;
}

static void dndadventure_draw_full_text(Canvas* canvas, DndAdventureApp* app) {
    if(!app->scene) {
        dndadventure_draw_header(canvas, app, "Adventure Text", "No scene");
        return;
    }
    uint8_t line_count = dndadventure_full_text_line_count(app->scene->body);
    uint8_t last_line = (uint8_t)(app->full_text_offset + 5U);
    if(last_line > line_count) last_line = line_count;
    char position[16];
    snprintf(
        position,
        sizeof(position),
        "%u-%u/%u",
        (unsigned)(app->full_text_offset + 1U),
        (unsigned)last_line,
        (unsigned)line_count);
    dndadventure_draw_header(canvas, app, app->scene->title, position);
    canvas_draw_frame(canvas, 0, 10, 128, 54);
    canvas_set_font(canvas, FontSecondary);
    const char* cursor = app->scene->body;
    char line[ADVENTURE_FULL_TEXT_WIDTH + 1U];
    for(uint8_t skip = 0U; skip < app->full_text_offset && cursor && *cursor; ++skip)
        cursor = dndadventure_next_full_text_line(cursor, line, sizeof(line));
    for(uint8_t row = 0U; row < 5U && cursor && *cursor; ++row) {
        cursor = dndadventure_next_full_text_line(cursor, line, sizeof(line));
        canvas_draw_str(canvas, 4, (uint8_t)(20U + row * 10U), line);
    }
}

static void dndadventure_draw_restart_confirm(Canvas* canvas, DndAdventureApp* app) {
    dndadventure_draw_header(canvas, app, "Restart Adventure?", "Resets progress");
    dndadventure_draw_row(canvas, 0U, app->selection == 0U, "Restart Adventure");
    dndadventure_draw_row(canvas, 1U, app->selection == 1U, "Cancel");
}

static void dndadventure_draw(Canvas* canvas, void* model) {
    DndAdventureApp* app = *(DndAdventureApp**)model;
    canvas_clear(canvas);
    switch(app->screen) {
    case DndAdventureScreenCampaigns:
        dndadventure_draw_campaigns(canvas, app);
        break;
    case DndAdventureScreenAdventure:
        dndadventure_draw_scene(canvas, app);
        break;
    case DndAdventureScreenResult:
        dndadventure_draw_result(canvas, app);
        break;
    case DndAdventureScreenFullText:
        dndadventure_draw_full_text(canvas, app);
        break;
    case DndAdventureScreenRestartConfirm:
        dndadventure_draw_restart_confirm(canvas, app);
        break;
    }
}

static void dndadventure_refresh(DndAdventureApp* app) {
    view_commit_model(app->view, true);
}

static void dndadventure_move(DndAdventureApp* app, uint16_t count, int8_t delta, uint8_t visible) {
    if(!count) return;
    int32_t next = (int32_t)app->selection + delta;
    if(next < 0) next = (int32_t)count - 1;
    if(next >= count) next = 0;
    app->selection = (uint16_t)next;
    if(app->selection < app->scroll) app->scroll = app->selection;
    if(app->selection >= app->scroll + visible) app->scroll = app->selection - visible + 1U;
}

static bool dndadventure_restart_current(DndAdventureApp* app) {
    if(!app || !app->active_campaign_valid || !app->character_loaded) return false;
    PocketCampaignProgress restarted;
    memset(&restarted, 0, sizeof(restarted));
    dndadventure_copy(
        restarted.campaign, sizeof(restarted.campaign), app->active_campaign.id);
    dndadventure_copy(
        restarted.scene, sizeof(restarted.scene), app->active_campaign.entry_scene);
    dndadventure_copy(
        restarted.checkpoint, sizeof(restarted.checkpoint), app->active_campaign.entry_scene);
    PocketCampaignProgress previous = app->progress;
    app->progress = restarted;
    if(!dndadventure_load_scene(app) || !dndadventure_save_progress(app)) {
        app->progress = previous;
        (void)dndadventure_load_scene(app);
        return false;
    }
    app->adventure_started = 0U;
    app->selection = 0U;
    app->scroll = 0U;
    app->full_text_offset = 0U;
    return true;
}

static void dndadventure_return_to_dnd(DndAdventureApp* app) {
    app->return_to_dnd = 1U;
    /* Scene/cache allocations are not needed for the return handoff. Release
       them before the dispatcher exits; dndadventure_app_free() remains idempotent
       and performs the complete teardown before Loader is opened. */
    dndadventure_scene_free(app);
    dndadventure_campaigns_cache_reset();
    view_dispatcher_stop(app->dispatcher);
}

static void dndadventure_back(DndAdventureApp* app) {
    if(app->screen == DndAdventureScreenCampaigns) {
        dndadventure_return_to_dnd(app);
    } else if(app->screen == DndAdventureScreenFullText) {
        app->screen = DndAdventureScreenAdventure;
        app->full_text_offset = 0U;
        app->status[0] = '\0';
    } else if(app->screen == DndAdventureScreenRestartConfirm) {
        app->screen = DndAdventureScreenCampaigns;
        app->selection = app->campaign_count;
        app->scroll = app->selection > 4U ? app->selection - 4U : 0U;
        app->status[0] = '\0';
        dndadventure_prepare_campaign_rows(app);
    } else if(app->screen == DndAdventureScreenAdventure || app->screen == DndAdventureScreenResult) {
        dndadventure_save_progress(app);
        app->screen = DndAdventureScreenCampaigns;
        app->selection = 0U;
        app->scroll = 0U;
        app->status[0] = '\0';
        dndadventure_prepare_campaign_rows(app);
    } else {
        app->screen = DndAdventureScreenCampaigns;
        app->selection = 0U;
        app->scroll = 0U;
        app->status[0] = '\0';
        dndadventure_prepare_campaign_rows(app);
    }
}

static bool dndadventure_input(InputEvent* event, void* context) {
    DndAdventureApp* app = context;
    if(event->type == InputTypeLong && event->key == InputKeyBack) {
        app->return_to_dnd = 0U;
        view_dispatcher_stop(app->dispatcher);
        return true;
    }
    if(event->type == InputTypeShort && event->key == InputKeyBack) {
        dndadventure_back(app);
        dndadventure_refresh(app);
        return true;
    }

    bool move = event->type == InputTypeShort || event->type == InputTypeRepeat;
    if(app->screen == DndAdventureScreenCampaigns) {
        uint16_t rows = app->campaign_count + 1U;
        if(move && event->key == InputKeyUp) {
            dndadventure_move(app, rows, -1, 5U);
            dndadventure_prepare_campaign_rows(app);
        } else if(move && event->key == InputKeyDown) {
            dndadventure_move(app, rows, 1, 5U);
            dndadventure_prepare_campaign_rows(app);
        } else if(event->type == InputTypeShort && event->key == InputKeyOk) {
            if(app->selection < app->campaign_count) {
                dndadventure_select_campaign(app, app->selection);
            } else if(app->active_campaign_valid && app->character_loaded) {
                app->screen = DndAdventureScreenRestartConfirm;
                app->selection = 1U;
                app->scroll = 0U;
                app->status[0] = '\0';
            } else {
                dndadventure_set_status(app, "No active adventure");
            }
        }
    } else if(app->screen == DndAdventureScreenAdventure) {
        if(!app->scene) {
            dndadventure_set_status(app, "No scene");
        } else if(!app->adventure_started) {
            if(event->type == InputTypeShort && event->key == InputKeyOk) {
                app->adventure_started = 1U;
                app->selection = 0U;
                app->scroll = 0U;
                app->status[0] = '\0';
            }
        } else if(move && event->key == InputKeyUp) {
            dndadventure_move(app, app->scene->choice_count, -1, 2U);
        } else if(move && event->key == InputKeyDown) {
            dndadventure_move(app, app->scene->choice_count, 1, 2U);
        } else if(event->type == InputTypeLong && event->key == InputKeyOk) {
            app->full_text_offset = 0U;
            app->screen = DndAdventureScreenFullText;
            app->status[0] = '\0';
        } else if(event->type == InputTypeLong && event->key == InputKeyLeft) {
            if(!app->progress.checkpoint[0]) {
                dndadventure_set_status(app, "No checkpoint saved");
            } else {
                char previous_scene[POCKET_D20_SHORT_LEN];
                dndadventure_copy(previous_scene, sizeof(previous_scene), app->progress.scene);
                dndadventure_copy(
                    app->progress.scene, sizeof(app->progress.scene), app->progress.checkpoint);
                if(dndadventure_load_scene(app)) {
                    app->selection = 0U;
                    app->scroll = 0U;
                    dndadventure_save_progress(app);
                    dndadventure_set_status(app, "Checkpoint loaded");
                } else {
                    dndadventure_copy(
                        app->progress.scene, sizeof(app->progress.scene), previous_scene);
                    dndadventure_set_status(app, "Checkpoint unavailable");
                }
            }
        } else if(event->type == InputTypeLong && event->key == InputKeyRight) {
            dndadventure_copy(
                app->progress.checkpoint, sizeof(app->progress.checkpoint), app->progress.scene);
            dndadventure_set_status(
                app,
                dndadventure_save_progress(app) ?
                    "Checkpoint saved [X]" :
                    "Checkpoint save failed");
        } else if(event->type == InputTypeShort && event->key == InputKeyOk &&
                  app->selection < app->scene->choice_count) {
            DndAdventureChoice choice = app->scene->choices[app->selection];
            app->status[0] = '\0';
            dndadventure_apply_choice(app, &choice);
        }
    } else if(app->screen == DndAdventureScreenResult) {
        if(event->type == InputTypeShort && event->key == InputKeyOk) {
            app->screen = DndAdventureScreenAdventure;
            app->status[0] = '\0';
        }
    } else if(app->screen == DndAdventureScreenFullText) {
        uint8_t line_count = app->scene ? dndadventure_full_text_line_count(app->scene->body) : 1U;
        uint8_t maximum = line_count > 5U ? (uint8_t)(line_count - 5U) : 0U;
        if(move && event->key == InputKeyUp) {
            if(app->full_text_offset) --app->full_text_offset;
        } else if(move && event->key == InputKeyDown) {
            if(app->full_text_offset < maximum) ++app->full_text_offset;
        } else if(move && event->key == InputKeyLeft) {
            app->full_text_offset = app->full_text_offset > 5U ?
                                        (uint8_t)(app->full_text_offset - 5U) :
                                        0U;
        } else if(move && event->key == InputKeyRight) {
            uint16_t next = (uint16_t)app->full_text_offset + 5U;
            app->full_text_offset = next < maximum ? (uint8_t)next : maximum;
        } else if(event->type == InputTypeShort && event->key == InputKeyOk) {
            app->screen = DndAdventureScreenAdventure;
            app->full_text_offset = 0U;
        }
    } else if(app->screen == DndAdventureScreenRestartConfirm) {
        if(move && (event->key == InputKeyUp || event->key == InputKeyDown))
            app->selection = app->selection ? 0U : 1U;
        else if(event->type == InputTypeShort && event->key == InputKeyOk) {
            if(app->selection == 0U) {
                bool restarted = dndadventure_restart_current(app);
                app->screen = restarted ? DndAdventureScreenAdventure : DndAdventureScreenCampaigns;
                dndadventure_set_status(app, restarted ? "Adventure restarted" : "Restart failed");
                if(!restarted) dndadventure_prepare_campaign_rows(app);
            } else {
                app->screen = DndAdventureScreenCampaigns;
                app->selection = app->campaign_count;
                app->scroll = app->selection > 4U ? app->selection - 4U : 0U;
                app->status[0] = '\0';
                dndadventure_prepare_campaign_rows(app);
            }
        }
    }
    dndadventure_refresh(app);
    return true;
}

static bool dndadventure_navigation(void* context) {
    DndAdventureApp* app = context;
    dndadventure_back(app);
    dndadventure_refresh(app);
    return true;
}

static bool dndadventure_load_character(DndAdventureApp* app, const char* args) {
    UNUSED(args);
    if(!app || !app->storage) return false;

    /* Adventure always follows DNDolphins' persisted Active= character.
       Launch arguments are reserved for Adventure flags (for example,
       Journal continuation) and never select a character. */
    if(!dnd_profile_ref_active_exact(app->storage, &app->profile)) {
        app->profile = 0U;
        app->character_loaded = 0U;
        return false;
    }

    app->character_loaded = dnd_profile_projection_load_adventure(
                                app->storage, app->profile, &app->character) ?
                                1U :
                                0U;
    return app->character_loaded != 0U;
}

static DndAdventureApp* dndadventure_app_alloc(const char* args) {
    const bool continue_requested =
        args && strstr(args, POCKET_D20_HANDOFF_ADVENTURE_CONTINUE) != NULL;
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
    view_dispatcher_set_navigation_event_callback(app->dispatcher, dndadventure_navigation);
    view_allocate_model(app->view, ViewModelTypeLockFree, sizeof(DndAdventureApp*));
    DndAdventureApp** model = view_get_model(app->view);
    if(!model) goto fail;
    *model = app;
    view_commit_model(app->view, false);
    view_set_context(app->view, app);
    view_set_draw_callback(app->view, dndadventure_draw);
    view_set_input_callback(app->view, dndadventure_input);

    dndadventure_load_character(app, args);
    if(!dndadventure_campaign_packs_ensure_enabled(app->storage))
        dndadventure_set_status(app, "Pack index rebuild failed");
    app->campaign_count = dndadventure_campaigns_count(app->storage);
    if(!app->campaign_count && !app->status[0]) dndadventure_set_status(app, "No campaign manifests");

    char continue_campaign_id[POCKET_CAMPAIGN_ID_LEN] = {0};
    bool have_continue_campaign =
        continue_requested && app->character_loaded &&
        dndadventure_campaigns_active_load(
            app->storage,
            app->profile,
            continue_campaign_id,
            sizeof(continue_campaign_id)) &&
        continue_campaign_id[0];

    dndadventure_resolve_active(app);
    dndadventure_prepare_campaign_rows(app);

    app->screen = DndAdventureScreenCampaigns;
    if(continue_requested) {
        bool active_matches =
            have_continue_campaign && app->active_campaign_valid &&
            !strcmp(app->active_campaign.id, continue_campaign_id);
        if(active_matches && app->progress.scene[0] && dndadventure_load_scene(app)) {
            app->screen = DndAdventureScreenAdventure;
            app->selection = 0U;
            app->scroll = 0U;
            app->adventure_started = 1U;
            dndadventure_set_status(app, "Continued from Journal");
        } else {
            dndadventure_set_status(app, "No active Adventure to continue");
        }
    }
    view_dispatcher_add_view(app->dispatcher, 0U, app->view);
    view_dispatcher_attach_to_gui(app->dispatcher, app->gui, ViewDispatcherTypeFullscreen);
    return app;

fail:
    dndadventure_scene_free(app);
    dndadventure_campaigns_cache_reset();
    if(app->view) view_free(app->view);
    if(app->dispatcher) view_dispatcher_free(app->dispatcher);
    if(app->storage) furi_record_close(RECORD_STORAGE);
    if(app->gui) furi_record_close(RECORD_GUI);
    free(app);
    return NULL;
}

static void dndadventure_app_free(DndAdventureApp* app) {
    if(!app) return;
    dndadventure_save_progress(app);
    dndadventure_scene_free(app);
    dndadventure_campaigns_cache_reset();
    if(app->dispatcher && app->view) view_dispatcher_remove_view(app->dispatcher, 0U);
    if(app->view) view_free(app->view);
    if(app->dispatcher) view_dispatcher_free(app->dispatcher);
    if(app->storage) furi_record_close(RECORD_STORAGE);
    if(app->gui) furi_record_close(RECORD_GUI);
    free(app);
}

int32_t dndadventure_app(void* context) {
    DndAdventureApp* app = dndadventure_app_alloc(context);
    if(!app) return -1;
    view_dispatcher_switch_to_view(app->dispatcher, 0U);
    view_dispatcher_run(app->dispatcher);
    bool return_to_dnd = app->return_to_dnd;
    dndadventure_app_free(app);
    if(return_to_dnd)
        (void)dnd_handoff_launch_if_present(DNDOLPHINS_FAP_PATH, POCKET_D20_RETURN_FOCUS_ADVENTURE);
    return 0;
}
