#include "pocket_d20_campaigns.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CAMPAIGN_BUNDLED_INDEX APP_ASSETS_PATH("campaigns/index.txt")
#define CAMPAIGN_USER_INDEX APP_ASSETS_PATH("campaigns/custom_index.txt")
#define CAMPAIGN_BUNDLED_SCENES APP_ASSETS_PATH("campaigns/%s/%s")
#define CAMPAIGN_USER_SCENES APP_ASSETS_PATH("campaigns/custom_%s/%s")
#define CAMPAIGN_LINE_LEN 512U
#define CAMPAIGN_MAX_SCENES 64U

static void campaign_copy(char* out, size_t size, const char* value) {
    if(!size) return;
    strncpy(out, value ? value : "", size - 1U);
    out[size - 1U] = '\0';
}

static bool campaign_read_line(File* file, char* line, size_t size) {
    size_t position = 0U;
    char value;
    while(position + 1U < size && storage_file_read(file, &value, 1U) == 1U) {
        if(value == '\r') continue;
        if(value == '\n') break;
        line[position++] = value;
    }
    line[position] = '\0';
    return position > 0U;
}

static uint8_t campaign_split(char* line, char** fields, uint8_t capacity) {
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

static bool campaign_parse(char* line, bool bundled, PocketCampaignSummary* output) {
    if(!line[0] || line[0] == '#') return false;
    char* fields[7];
    if(campaign_split(line, fields, 7U) != 7U) return false;
    memset(output, 0, sizeof(*output));
    campaign_copy(output->id, sizeof(output->id), fields[0]);
    campaign_copy(output->name, sizeof(output->name), fields[1]);
    output->pack_version = (uint8_t)strtoul(fields[2], NULL, 10);
    output->minimum_app = (uint16_t)strtoul(fields[3], NULL, 10);
    output->maximum_app = (uint16_t)strtoul(fields[4], NULL, 10);
    campaign_copy(output->entry_scene, sizeof(output->entry_scene), fields[5]);
    campaign_copy(output->scenes_file, sizeof(output->scenes_file), fields[6]);
    output->bundled = bundled ? 1U : 0U;
    return output->id[0] && output->name[0] && output->entry_scene[0] &&
           output->scenes_file[0];
}

static uint16_t campaign_count_path(Storage* storage, const char* path, bool bundled) {
    File* file = storage_file_alloc(storage);
    uint16_t count = 0U;
    if(storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        char line[CAMPAIGN_LINE_LEN];
        PocketCampaignSummary campaign;
        while(campaign_read_line(file, line, sizeof(line)))
            if(campaign_parse(line, bundled, &campaign) && count < UINT16_MAX) ++count;
    }
    storage_file_close(file);
    storage_file_free(file);
    return count;
}

static bool campaign_at_path(
    Storage* storage,
    const char* path,
    bool bundled,
    uint16_t wanted,
    PocketCampaignSummary* output) {
    File* file = storage_file_alloc(storage);
    bool found = false;
    if(storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        char line[CAMPAIGN_LINE_LEN];
        uint16_t index = 0U;
        while(campaign_read_line(file, line, sizeof(line))) {
            PocketCampaignSummary campaign;
            if(!campaign_parse(line, bundled, &campaign)) continue;
            if(index++ == wanted) {
                *output = campaign;
                found = true;
                break;
            }
        }
    }
    storage_file_close(file);
    storage_file_free(file);
    return found;
}

uint16_t pocket_campaign_count(Storage* storage) {
    uint32_t count = campaign_count_path(storage, CAMPAIGN_BUNDLED_INDEX, true) +
                     campaign_count_path(storage, CAMPAIGN_USER_INDEX, false);
    return count > UINT16_MAX ? UINT16_MAX : (uint16_t)count;
}

bool pocket_campaign_at(Storage* storage, uint16_t index, PocketCampaignSummary* output) {
    uint16_t bundled = campaign_count_path(storage, CAMPAIGN_BUNDLED_INDEX, true);
    return index < bundled ?
        campaign_at_path(storage, CAMPAIGN_BUNDLED_INDEX, true, index, output) :
        campaign_at_path(storage, CAMPAIGN_USER_INDEX, false, index - bundled, output);
}

bool pocket_campaign_find(Storage* storage, const char* id, PocketCampaignSummary* output) {
    uint16_t total = pocket_campaign_count(storage);
    for(uint16_t i = 0U; i < total; ++i)
        if(pocket_campaign_at(storage, i, output) && !strcmp(output->id, id)) return true;
    return false;
}

bool pocket_campaign_scene_path(
    Storage* storage,
    const PocketCampaignSummary* campaign,
    char* output,
    size_t size) {
    snprintf(output, size, CAMPAIGN_USER_SCENES, campaign->id, campaign->scenes_file);
    if(storage_file_exists(storage, output)) return true;
    if(campaign->bundled) {
        snprintf(output, size, CAMPAIGN_BUNDLED_SCENES, campaign->id, campaign->scenes_file);
        return storage_file_exists(storage, output);
    }
    return false;
}

static void campaign_progress_path(
    char* output,
    size_t size,
    uint32_t profile_id,
    const char* campaign_id,
    const char* suffix) {
    snprintf(output, size, APP_ASSETS_PATH("campaigns/custom_progress_%08lx_%s.%s"),
             (unsigned long)profile_id, campaign_id, suffix);
}

bool pocket_campaign_progress_save(
    Storage* storage,
    uint32_t profile_id,
    const PocketCampaignSummary* campaign,
    const PocketCharacter* character) {
    storage_common_mkdir(storage, APP_ASSETS_PATH("campaigns"));
    char path[192], temp[192], backup[192], line[160];
    campaign_progress_path(path, sizeof(path), profile_id, campaign->id, "txt");
    campaign_progress_path(temp, sizeof(temp), profile_id, campaign->id, "tmp");
    campaign_progress_path(backup, sizeof(backup), profile_id, campaign->id, "bak");
    File* file = storage_file_alloc(storage);
    bool ok = storage_file_open(file, temp, FSAM_WRITE, FSOM_CREATE_ALWAYS);
#define CAMPAIGN_WRITE(...) do { \
    int length = snprintf(line, sizeof(line), __VA_ARGS__); \
    if(length <= 0 || (size_t)length >= sizeof(line) || \
       storage_file_write(file, line, (size_t)length) != (size_t)length) ok = false; \
} while(false)
    if(ok) CAMPAIGN_WRITE("CampaignProgress=1\n");
    if(ok) CAMPAIGN_WRITE("Campaign=%s\n", campaign->id);
    if(ok) CAMPAIGN_WRITE("Scene=%s\n", character->adventure_scene);
    if(ok) CAMPAIGN_WRITE("Checkpoint=%s\n", character->adventure_checkpoint);
    if(ok) CAMPAIGN_WRITE("Flags=%lu,%lu\n", (unsigned long)character->adventure_quest_flags,
                           (unsigned long)character->adventure_achievements);
#undef CAMPAIGN_WRITE
    storage_file_close(file);
    storage_file_free(file);
    if(!ok) {
        storage_common_remove(storage, temp);
        return false;
    }
    storage_common_remove(storage, backup);
    bool had_old = storage_common_rename(storage, path, backup) == FSE_OK;
    if(storage_common_rename(storage, temp, path) != FSE_OK) {
        if(had_old) storage_common_rename(storage, backup, path);
        return false;
    }
    storage_common_remove(storage, backup);
    return true;
}

bool pocket_campaign_progress_load(
    Storage* storage,
    uint32_t profile_id,
    const PocketCampaignSummary* campaign,
    PocketCharacter* character) {
    campaign_copy(character->adventure_campaign, sizeof(character->adventure_campaign), campaign->id);
    campaign_copy(character->adventure_scene, sizeof(character->adventure_scene), campaign->entry_scene);
    campaign_copy(character->adventure_checkpoint, sizeof(character->adventure_checkpoint), campaign->entry_scene);
    character->adventure_quest_flags = 0U;
    character->adventure_achievements = 0U;
    char path[192];
    campaign_progress_path(path, sizeof(path), profile_id, campaign->id, "txt");
    File* file = storage_file_alloc(storage);
    if(!storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_free(file);
        return true;
    }
    char line[CAMPAIGN_LINE_LEN];
    while(campaign_read_line(file, line, sizeof(line))) {
        char* value = strchr(line, '=');
        if(!value) continue;
        *value++ = '\0';
        if(!strcmp(line, "Scene")) campaign_copy(character->adventure_scene,
            sizeof(character->adventure_scene), value);
        else if(!strcmp(line, "Checkpoint")) campaign_copy(character->adventure_checkpoint,
            sizeof(character->adventure_checkpoint), value);
        else if(!strcmp(line, "Flags")) {
            unsigned long quests = 0U, achievements = 0U;
            if(sscanf(value, "%lu,%lu", &quests, &achievements) == 2) {
                character->adventure_quest_flags = (uint32_t)quests;
                character->adventure_achievements = (uint32_t)achievements;
            }
        }
    }
    storage_file_close(file);
    storage_file_free(file);
    return true;
}

static bool campaign_scene_present(char (*ids)[POCKET_D20_SHORT_LEN], uint8_t count, const char* id) {
    if(!id[0] || !strcmp(id, "-")) return true;
    for(uint8_t i = 0U; i < count; ++i) if(!strcmp(ids[i], id)) return true;
    return false;
}

static void campaign_note_problem(
    PocketCampaignDiagnostics* output,
    const char* id,
    const char* problem) {
    if(output->problem_id[0]) return;
    campaign_copy(output->problem_id, sizeof(output->problem_id), id);
    campaign_copy(output->problem, sizeof(output->problem), problem);
}

static void campaign_validate_scenes(
    Storage* storage,
    const PocketCampaignSummary* campaign,
    PocketCampaignDiagnostics* output) {
    char path[192];
    if(!pocket_campaign_scene_path(storage, campaign, path, sizeof(path))) {
        ++output->missing_scene_files;
        campaign_note_problem(output, campaign->id, "Scene file missing");
        return;
    }
    char(*scene_ids)[POCKET_D20_SHORT_LEN] = calloc(CAMPAIGN_MAX_SCENES, POCKET_D20_SHORT_LEN);
    if(!scene_ids) {
        campaign_note_problem(output, campaign->id, "Diagnostics memory low");
        return;
    }
    File* file = storage_file_alloc(storage);
    uint8_t scene_count = 0U;
    if(storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        char line[CAMPAIGN_LINE_LEN];
        while(campaign_read_line(file, line, sizeof(line))) {
            char* fields[11];
            uint8_t count = campaign_split(line, fields, 11U);
            if(count != 5U || strcmp(fields[0], "S")) continue;
            if(campaign_scene_present(scene_ids, scene_count, fields[1])) {
                ++output->duplicate_scene_ids;
                campaign_note_problem(output, campaign->id, "Duplicate scene ID");
            } else if(scene_count < CAMPAIGN_MAX_SCENES) {
                campaign_copy(scene_ids[scene_count++], POCKET_D20_SHORT_LEN, fields[1]);
            }
        }
    }
    storage_file_close(file);
    if(!campaign_scene_present(scene_ids, scene_count, campaign->entry_scene)) {
        ++output->missing_entry_scenes;
        campaign_note_problem(output, campaign->id, "Entry scene missing");
    }
    if(storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        char line[CAMPAIGN_LINE_LEN];
        while(campaign_read_line(file, line, sizeof(line))) {
            char* fields[11];
            uint8_t count = campaign_split(line, fields, 11U);
            if(count != 11U || strcmp(fields[0], "C")) continue;
            if(!campaign_scene_present(scene_ids, scene_count, fields[5]) ||
               !campaign_scene_present(scene_ids, scene_count, fields[6])) {
                ++output->broken_links;
                campaign_note_problem(output, campaign->id, "Broken scene link");
            }
        }
    }
    storage_file_close(file);
    storage_file_free(file);
    free(scene_ids);
}

void pocket_campaign_diagnose(Storage* storage, PocketCampaignDiagnostics* output) {
    memset(output, 0, sizeof(*output));
    output->records = pocket_campaign_count(storage);
    for(uint16_t i = 0U; i < output->records; ++i) {
        PocketCampaignSummary campaign;
        if(!pocket_campaign_at(storage, i, &campaign)) continue;
        if(campaign.pack_version != POCKET_CAMPAIGN_PACK_VERSION ||
           campaign.minimum_app > POCKET_CAMPAIGN_APP_VERSION ||
           (campaign.maximum_app && campaign.maximum_app < POCKET_CAMPAIGN_APP_VERSION)) {
            ++output->incompatible;
            campaign_note_problem(output, campaign.id, "Incompatible manifest");
        }
        for(uint16_t prior = 0U; prior < i; ++prior) {
            PocketCampaignSummary previous;
            if(pocket_campaign_at(storage, prior, &previous) && !strcmp(previous.id, campaign.id)) {
                ++output->duplicate_campaign_ids;
                campaign_note_problem(output, campaign.id, "Duplicate campaign ID");
                break;
            }
        }
        campaign_validate_scenes(storage, &campaign, output);
    }
}
