#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define IRB_SLOTS 8
#define IRB_VERSION "3.2"
#define IRB_MAX_EXTRAS 48
#define IRB_MAX_BUTTONS (IRB_SLOTS + IRB_MAX_EXTRAS)
#define IRB_MAX_IMPORTS 4
#define IRB_GROUPS 14
#define IRB_NAME_SIZE 32
#define IRB_PATH_SIZE 256
#define IRB_DEFAULT_LIBRARY "/ext/apps_data/ir_builder/tv_builder.ir"
#define IRB_LEGACY_DEFAULT_LIBRARY "/ext/infrared/assets/tv.ir"
#define IRB_BUNDLED_LIBRARY APP_ASSETS_PATH("tv_builder.ir")
#define IRB_NAV_KEYS 8
#define IRB_NAV_SLOT_BASE IRB_MAX_BUTTONS
#define IRB_POSITION_SLOTS (IRB_SLOTS + IRB_NAV_KEYS)

typedef enum {
    IrbGroupPower,
    IrbGroupVolUp,
    IrbGroupVolDown,
    IrbGroupChannelUp,
    IrbGroupChannelDown,
    IrbGroupMute,
    IrbGroupOK,
    IrbGroupUp,
    IrbGroupDown,
    IrbGroupLeft,
    IrbGroupRight,
    IrbGroupMenu,
    IrbGroupBack,
    IrbGroupSource,
} IrbGroup;

typedef struct {
    char path[IRB_PATH_SIZE];
    uint32_t hash;
    uint32_t size;
} IrbImportSource;

typedef struct {
    char label[IRB_NAME_SIZE];
    uint32_t source;
    uint32_t offset;
} IrbExtraButton;

typedef struct {
    uint32_t source;
    uint32_t offset;
} IrbSignalRef;

typedef struct {
    char library[IRB_PATH_SIZE];
    char name[IRB_NAME_SIZE];
    char labels[IRB_SLOTS][IRB_NAME_SIZE];
    uint32_t positions[IRB_SLOTS];
    uint32_t nav_positions[IRB_NAV_KEYS];
    IrbSignalRef mapped[IRB_POSITION_SLOTS];
    uint32_t order[IRB_MAX_BUTTONS];
    uint32_t source_hash;
    uint32_t source_size;
    uint32_t extra_count;
    uint32_t import_count;
    IrbImportSource imports[IRB_MAX_IMPORTS];
    IrbExtraButton extras[IRB_MAX_EXTRAS];
} IrbProject;

extern const char* const irb_group_names[IRB_GROUPS];
extern const char* const irb_slot_titles[IRB_SLOTS];
extern const uint8_t irb_slot_group[IRB_SLOTS];
extern const uint8_t irb_nav_group[IRB_NAV_KEYS];

void irb_project_init(IrbProject* project);
uint32_t irb_project_count(const IrbProject* project);
uint32_t irb_base_count(const IrbProject* project);
uint32_t irb_nav_count(const IrbProject* project);
bool irb_slot_is_nav(uint32_t slot);
unsigned irb_nav_key_from_slot(uint32_t slot);
int irb_slot_group_index(uint32_t slot);
uint32_t irb_project_position(const IrbProject* project, uint32_t slot);
void irb_project_set_position(IrbProject* project, uint32_t slot, uint32_t position);
bool irb_project_set_imported(IrbProject* project, uint32_t slot, uint32_t source, uint32_t offset);
const IrbSignalRef* irb_project_imported(const IrbProject* project, uint32_t slot);
int irb_nav_slot(const IrbProject* project, unsigned key);
bool irb_has_navigation(const IrbProject* project);
unsigned irb_nav_move(unsigned key, unsigned direction);
extern const char* const irb_nav_labels[IRB_NAV_KEYS];
bool irb_project_active(const IrbProject* project, uint32_t slot);
const char* irb_project_label(const IrbProject* project, uint32_t slot);
bool irb_project_label_available(const IrbProject* project, const char* label, uint32_t except);
uint32_t irb_project_slots(const IrbProject* project, uint8_t* slots, bool active_only);
void irb_project_remove(IrbProject* project, uint32_t slot);
void irb_project_move(IrbProject* project, uint32_t slot, bool down);
bool irb_project_valid(const IrbProject* project);
bool irb_name_valid(const char* name, bool filename);
bool irb_library_path_valid(const char* path);
int irb_group_from_name(const char* name);
uint32_t irb_position_step(uint32_t value, uint32_t limit, bool increase, uint32_t repeats);
uint32_t irb_hash_update(uint32_t hash, const void* data, size_t length);
