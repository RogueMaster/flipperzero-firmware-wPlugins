#pragma once

/* One ADD_SCENE per scene */
#define MFP_SCENE_LIST(ADD_SCENE)       \
    ADD_SCENE(start,       Start)       \
    ADD_SCENE(read,        Read)        \
    ADD_SCENE(saved,       Saved)       \
    ADD_SCENE(dict_select, DictSelect)  \
    ADD_SCENE(read_all,    ReadAll)     \
    ADD_SCENE(read_all_result, ReadAllResult) \
    ADD_SCENE(actions,         Actions)       \
    ADD_SCENE(save_name,       SaveName)      \
    ADD_SCENE(save_success,    SaveSuccess)   \
    ADD_SCENE(delete_confirm,  DeleteConfirm) \
    ADD_SCENE(delete_success,  DeleteSuccess) \
    ADD_SCENE(dump_view,       DumpView)      \
    ADD_SCENE(emulate_setup,   EmulateSetup)  \
    ADD_SCENE(emulate,         Emulate)       \
    ADD_SCENE(card_info,       CardInfo)

typedef enum {
#define ADD_SCENE(id, Name) MfpScene##Name,
    MFP_SCENE_LIST(ADD_SCENE)
#undef ADD_SCENE
    MfpSceneNum,
} MfpScene;
