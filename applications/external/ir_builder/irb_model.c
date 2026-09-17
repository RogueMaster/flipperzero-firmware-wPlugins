#include "irb_model.h"

#include <stdio.h>
#include <string.h>

const char* const irb_group_names[IRB_GROUPS] = {"Power", "Vol_up", "Vol_dn", "Ch_next", "Ch_prev",
                                                 "Mute",  "OK",     "Up",     "Down",    "Left",
                                                 "Right", "Menu",   "Back",   "Source"};
const char* const irb_slot_titles[IRB_SLOTS] = {"Power on",    "Power off",  "Volume up",
                                                "Volume down", "Channel up", "Channel down",
                                                "Mute",        "Unmute"};
const uint8_t irb_slot_group[IRB_SLOTS] = {0, 0, 1, 2, 3, 4, 5, 5};
const char* const irb_nav_labels[IRB_NAV_KEYS] = {"Up",   "Left", "OK",   "Right",
                                                  "Down", "Menu", "Back", "Input"};
const uint8_t irb_nav_group[IRB_NAV_KEYS] = {IrbGroupUp,    IrbGroupLeft,  IrbGroupOK,
                                             IrbGroupRight, IrbGroupDown,  IrbGroupMenu,
                                             IrbGroupBack,  IrbGroupSource};

uint32_t irb_base_count(const IrbProject* project) {
    uint32_t count = 0;
    for(unsigned i = 0; i < IRB_SLOTS; ++i)
        count += irb_project_active(project, i);
    return count;
}
uint32_t irb_nav_count(const IrbProject* project) {
    uint32_t count = 0;
    for(unsigned i = 0; i < IRB_NAV_KEYS; ++i)
        count += irb_project_active(project, IRB_NAV_SLOT_BASE + i);
    return count;
}
bool irb_slot_is_nav(uint32_t slot) {
    return slot >= IRB_NAV_SLOT_BASE && slot < IRB_NAV_SLOT_BASE + IRB_NAV_KEYS;
}
unsigned irb_nav_key_from_slot(uint32_t slot) {
    return irb_slot_is_nav(slot) ? slot - IRB_NAV_SLOT_BASE : IRB_NAV_KEYS;
}
int irb_slot_group_index(uint32_t slot) {
    if(slot < IRB_SLOTS) return irb_slot_group[slot];
    if(irb_slot_is_nav(slot)) return irb_nav_group[irb_nav_key_from_slot(slot)];
    return -1;
}
uint32_t irb_project_position(const IrbProject* project, uint32_t slot) {
    if(slot < IRB_SLOTS) return project->positions[slot];
    if(irb_slot_is_nav(slot)) return project->nav_positions[irb_nav_key_from_slot(slot)];
    return 0;
}
static void compact_imports(IrbProject* project) {
    for(int source = (int)project->import_count - 1; source >= 0; --source) {
        bool used = false;
        for(unsigned i = 0; i < project->extra_count; ++i)
            used |= project->extras[i].source == (uint32_t)source;
        for(unsigned i = 0; i < IRB_POSITION_SLOTS; ++i)
            used |= project->mapped[i].offset && project->mapped[i].source == (uint32_t)source;
        if(used) continue;
        memmove(&project->imports[source], &project->imports[source + 1],
                (project->import_count - source - 1) * sizeof(IrbImportSource));
        memset(&project->imports[--project->import_count], 0, sizeof(IrbImportSource));
        for(unsigned i = 0; i < project->extra_count; ++i)
            if(project->extras[i].source > (uint32_t)source) --project->extras[i].source;
        for(unsigned i = 0; i < IRB_POSITION_SLOTS; ++i)
            if(project->mapped[i].offset && project->mapped[i].source > (uint32_t)source)
                --project->mapped[i].source;
    }
}
void irb_project_set_position(IrbProject* project, uint32_t slot, uint32_t position) {
    unsigned index = slot < IRB_SLOTS        ? slot
                     : irb_slot_is_nav(slot) ? IRB_SLOTS + irb_nav_key_from_slot(slot)
                                             : IRB_POSITION_SLOTS;
    if(index < IRB_POSITION_SLOTS) memset(&project->mapped[index], 0, sizeof(IrbSignalRef));
    if(slot < IRB_SLOTS)
        project->positions[slot] = position;
    else if(irb_slot_is_nav(slot))
        project->nav_positions[irb_nav_key_from_slot(slot)] = position;
    compact_imports(project);
}
const IrbSignalRef* irb_project_imported(const IrbProject* project, uint32_t slot) {
    unsigned index = slot < IRB_SLOTS        ? slot
                     : irb_slot_is_nav(slot) ? IRB_SLOTS + irb_nav_key_from_slot(slot)
                                             : IRB_POSITION_SLOTS;
    return index < IRB_POSITION_SLOTS && project->mapped[index].offset ? &project->mapped[index]
                                                                       : NULL;
}
bool irb_project_set_imported(IrbProject* project, uint32_t slot, uint32_t source,
                              uint32_t offset) {
    unsigned index = slot < IRB_SLOTS        ? slot
                     : irb_slot_is_nav(slot) ? IRB_SLOTS + irb_nav_key_from_slot(slot)
                                             : IRB_POSITION_SLOTS;
    if(index >= IRB_POSITION_SLOTS || !offset) return false;
    if(slot < IRB_SLOTS)
        project->positions[slot] = 0;
    else
        project->nav_positions[irb_nav_key_from_slot(slot)] = 0;
    project->mapped[index] = (IrbSignalRef){.source = source, .offset = offset};
    return true;
}
static bool label_matches(const char* label, const char* expected) {
    while(*label && *expected) {
        char c = *label++;
        if(c == '_' || c == '-' || c == ' ') continue;
        if(c >= 'A' && c <= 'Z') c += 'a' - 'A';
        if(c != *expected++) return false;
    }
    return !*label && !*expected;
}
int irb_nav_slot(const IrbProject* project, unsigned key) {
    static const char* const aliases[IRB_NAV_KEYS][3] = {
        {"up", "arrowup", "dpadup"},       {"left", "arrowleft", "dpadleft"},
        {"ok", "enter", "select"},         {"right", "arrowright", "dpadright"},
        {"down", "arrowdown", "dpaddown"}, {"menu", NULL, NULL},
        {"back", "return", NULL},          {"source", "input", "av"}};
    if(key >= IRB_NAV_KEYS) return -1;
    if(irb_project_active(project, IRB_NAV_SLOT_BASE + key)) return IRB_NAV_SLOT_BASE + key;
    for(unsigned i = 0; i < project->extra_count; ++i)
        for(unsigned j = 0; j < 3 && aliases[key][j]; ++j)
            if(label_matches(project->extras[i].label, aliases[key][j])) return IRB_SLOTS + i;
    return -1;
}
bool irb_has_navigation(const IrbProject* project) {
    for(unsigned i = 0; i < IRB_NAV_KEYS; ++i)
        if(irb_nav_slot(project, i) >= 0) return true;
    return false;
}
unsigned irb_nav_move(unsigned key, unsigned direction) {
    // Directions follow the firmware InputKey values: Up, Down, Right, Left.
    static const uint8_t next[IRB_NAV_KEYS][4] = {{6, 2, 3, 1}, {0, 5, 2, 3}, {0, 4, 3, 1},
                                                  {0, 7, 1, 2}, {2, 6, 7, 5}, {1, 0, 6, 7},
                                                  {4, 0, 7, 5}, {3, 0, 5, 6}};
    return key < IRB_NAV_KEYS && direction < 4 ? next[key][direction] : key;
}

void irb_project_init(IrbProject* project) {
    static const char* const labels[IRB_SLOTS] = {"on",    "off",     "vol_up", "vol_down",
                                                  "ch_up", "ch_down", "mute",   "unmute"};
    memset(project, 0, sizeof(*project));
    snprintf(project->library, sizeof(project->library), "%s", IRB_DEFAULT_LIBRARY);
    snprintf(project->name, sizeof(project->name), "IR_Remote");
    for(size_t i = 0; i < IRB_SLOTS; ++i) {
        snprintf(project->labels[i], IRB_NAME_SIZE, "%s", labels[i]);
    }
    for(size_t i = 0; i < IRB_MAX_BUTTONS; ++i)
        project->order[i] = i;
}

uint32_t irb_project_count(const IrbProject* project) {
    uint32_t count = 0;
    for(size_t i = 0; i < IRB_SLOTS; ++i)
        count += irb_project_active(project, i);
    return count + irb_nav_count(project) + project->extra_count;
}

bool irb_project_active(const IrbProject* project, uint32_t slot) {
    if(slot < IRB_SLOTS)
        return project->positions[slot] != 0 || irb_project_imported(project, slot);
    if(irb_slot_is_nav(slot))
        return project->nav_positions[irb_nav_key_from_slot(slot)] != 0 ||
               irb_project_imported(project, slot);
    return slot < IRB_SLOTS + project->extra_count;
}

const char* irb_project_label(const IrbProject* project, uint32_t slot) {
    if(slot < IRB_SLOTS) return project->labels[slot];
    if(irb_slot_is_nav(slot)) return irb_nav_labels[irb_nav_key_from_slot(slot)];
    if(slot < IRB_SLOTS + project->extra_count) return project->extras[slot - IRB_SLOTS].label;
    return "";
}

bool irb_project_label_available(const IrbProject* project, const char* label, uint32_t except) {
    for(uint32_t i = 0; i < IRB_SLOTS + project->extra_count; ++i)
        if(i != except && !strcmp(label, irb_project_label(project, i))) return false;
    return true;
}

uint32_t irb_project_slots(const IrbProject* project, uint8_t* slots, bool active_only) {
    uint32_t count = 0;
    for(unsigned i = 0; i < IRB_MAX_BUTTONS; ++i) {
        uint32_t slot = project->order[i];
        if(slot < IRB_SLOTS + project->extra_count &&
           (!active_only || irb_project_active(project, slot)))
            slots[count++] = slot;
    }
    return count;
}

void irb_project_move(IrbProject* project, uint32_t slot, bool down) {
    for(int i = 0; i < IRB_MAX_BUTTONS; ++i) {
        if(project->order[i] != slot) continue;
        for(int next = i + (down ? 1 : -1); next >= 0 && next < IRB_MAX_BUTTONS;
            next += down ? 1 : -1) {
            if(project->order[next] >= IRB_SLOTS + project->extra_count) continue;
            project->order[i] = project->order[next];
            project->order[next] = slot;
            return;
        }
    }
}

void irb_project_remove(IrbProject* project, uint32_t slot) {
    if(slot < IRB_SLOTS) {
        project->positions[slot] = 0;
        memset(&project->mapped[slot], 0, sizeof(IrbSignalRef));
    } else if(irb_slot_is_nav(slot)) {
        project->nav_positions[irb_nav_key_from_slot(slot)] = 0;
        memset(&project->mapped[IRB_SLOTS + irb_nav_key_from_slot(slot)], 0, sizeof(IrbSignalRef));
    } else {
        uint32_t extra = slot - IRB_SLOTS;
        if(extra >= project->extra_count) return;
        memmove(&project->extras[extra], &project->extras[extra + 1],
                (project->extra_count - extra - 1) * sizeof(IrbExtraButton));
        memset(&project->extras[--project->extra_count], 0, sizeof(IrbExtraButton));
        for(unsigned i = 0; i < IRB_MAX_BUTTONS; ++i) {
            if(project->order[i] == slot)
                project->order[i] = IRB_MAX_BUTTONS - 1;
            else if(project->order[i] > slot)
                --project->order[i];
        }
    }
    compact_imports(project);
}

bool irb_name_valid(const char* name, bool filename) {
    size_t length = 0;
    while(length < IRB_NAME_SIZE && name[length])
        ++length;
    if(!length || length >= IRB_NAME_SIZE || name[0] == ' ' || name[length - 1] == ' ')
        return false;
    for(size_t i = 0; i < length; ++i) {
        unsigned char c = (unsigned char)name[i];
        if(c < 32 || c > 126) return false;
        if(filename && !((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                         (c >= '0' && c <= '9') || c == '_' || c == '-' || c == ' '))
            return false;
    }
    return true;
}

bool irb_library_path_valid(const char* path) {
    size_t length = 0;
    while(length < IRB_PATH_SIZE && path[length])
        ++length;
    if(length < 9 || length >= IRB_PATH_SIZE || strncmp(path, "/ext/", 5)) return false;
    if(path[length - 3] != '.' || (path[length - 2] != 'i' && path[length - 2] != 'I') ||
       (path[length - 1] != 'r' && path[length - 1] != 'R'))
        return false;
    // Keep file identity unambiguous for the source-overwrite guard. Requiring
    // .ir also excludes our draft, backup and staging files as source libraries.
    const char* start = path + 5;
    for(const char* p = start;; ++p) {
        unsigned char c = (unsigned char)*p;
        if(c == '/' || c == 0) {
            if(p == start || start[0] == ' ' || p[-1] == ' ' || p[-1] == '.') return false;
            if(c == 0) break;
            start = p + 1;
        } else if(c < 32 || c == 127 || strchr("\\:*?\"<>|", c)) {
            return false;
        }
    }
    return true;
}

bool irb_project_valid(const IrbProject* project) {
    bool seen[IRB_MAX_BUTTONS] = {0};
    if(!irb_name_valid(project->name, true) || !irb_library_path_valid(project->library))
        return false;
    if(project->extra_count > IRB_MAX_EXTRAS || project->import_count > IRB_MAX_IMPORTS)
        return false;
    for(size_t i = 0; i < IRB_MAX_BUTTONS; ++i) {
        if(project->order[i] >= IRB_MAX_BUTTONS || seen[project->order[i]]) return false;
        seen[project->order[i]] = true;
    }
    for(size_t i = 0; i < IRB_SLOTS + project->extra_count; ++i)
        if(!irb_name_valid(irb_project_label(project, i), false) ||
           !irb_project_label_available(project, irb_project_label(project, i), i))
            return false;
    for(size_t i = 0; i < project->import_count; ++i)
        if(!irb_library_path_valid(project->imports[i].path) || !project->imports[i].size)
            return false;
    for(size_t i = 0; i < project->extra_count; ++i)
        if(project->extras[i].source >= project->import_count ||
           project->extras[i].offset >= project->imports[project->extras[i].source].size)
            return false;
    for(size_t i = 0; i < IRB_POSITION_SLOTS; ++i) {
        uint32_t position =
            i < IRB_SLOTS ? project->positions[i] : project->nav_positions[i - IRB_SLOTS];
        if(project->mapped[i].offset && position) return false;
        if(project->mapped[i].offset &&
           (project->mapped[i].source >= project->import_count ||
            project->mapped[i].offset >= project->imports[project->mapped[i].source].size))
            return false;
    }
    return true;
}

int irb_group_from_name(const char* name) {
    for(size_t i = 0; i < IRB_GROUPS; ++i) {
        if(!strcmp(name, irb_group_names[i])) return (int)i;
    }
    return -1;
}

uint32_t irb_position_step(uint32_t value, uint32_t limit, bool increase, uint32_t repeats) {
    if(!limit) return 0;
    if(value < 1) value = 1;
    if(value > limit) value = limit;
    uint32_t step = repeats >= 20 ? 10 : repeats >= 8 ? 5 : 1;
    if(increase) return limit - value < step ? limit : value + step;
    return value <= step ? 1 : value - step;
}

uint32_t irb_hash_update(uint32_t hash, const void* data, size_t length) {
    const uint8_t* bytes = data;
    for(size_t i = 0; i < length; ++i) {
        hash ^= bytes[i];
        hash *= UINT32_C(16777619);
    }
    return hash;
}
