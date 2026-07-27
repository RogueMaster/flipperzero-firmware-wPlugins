#include "../nfc_magic_app_i.h"
#include "nfc_magic_scene_write_check_common.h"

void nfc_magic_scene_gen2_write_check_view_callback(WriteProblemsEvent event, void* context) {
    // Gen2 path: Left/"Retry" returns to Gen2Menu (the menu on this flow's stack). Shared body in
    // nfc_magic_write_check_handle_event so the two write-check scenes can't diverge again.
    nfc_magic_write_check_handle_event(context, event, NfcMagicSceneGen2Menu);
}

void nfc_magic_scene_gen2_write_check_on_enter(void* context) {
    NfcMagicApp* instance = context;

    Gen2PollerWriteProblems problems = gen2_poller_check_target_problems(instance->target_dev);
    if(!instance->gen2_poller_is_wipe_mode) {
        problems.all_problems |=
            gen2_poller_check_source_problems(instance->source_dev).all_problems;
    }

    WriteProblems* write_problems = instance->write_problems;
    uint8_t problems_count = 0;
    uint8_t current_problem = 0;
    furi_assert(!problems.no_data, "No MFC data in nfc device");

    if(problems.all_problems == 0) {
        if(instance->gen2_poller_is_wipe_mode) {
            scene_manager_next_scene(instance->scene_manager, NfcMagicSceneWipe);
            return;
        } else {
            scene_manager_next_scene(instance->scene_manager, NfcMagicSceneWrite);
            return;
        }
    }

    // Count the total number of problems
    for(uint8_t i = 0; i < GEN2_POLLER_WRITE_PROBLEMS_LEN; i++) {
        if(problems.all_problems & (1 << i)) {
            problems_count++;
        }
    }

    // Init the view
    write_problems_set_callback(
        write_problems, nfc_magic_scene_gen2_write_check_view_callback, instance);
    write_problems_set_problems_total(write_problems, problems_count);
    write_problems_set_problem_index(write_problems, current_problem);

    // Set the initial content to the first problem
    for(uint8_t i = 0; i < GEN2_POLLER_WRITE_PROBLEMS_LEN; i++) {
        if(problems.all_problems & (1 << i)) {
            write_problems_set_content(write_problems, gen2_problem_strings[i]);
            current_problem = i;
            break;
        }
    }

    // Save the context. problem_index is the 0-based DISPLAY position (compared against
    // problems_total-1 to decide "advance vs proceed"); problem_index_abs is the bit position of
    // the current problem in the bitmap (the loop above left current_problem at the first set bit).
    // Seeding problem_index with the bit index made "Next" never reach problems_total-1 when the
    // first problem wasn't bit 0 (e.g. "Can't find keys" = bit 3) -> the button did nothing.
    instance->write_problems_context.problem_index = 0;
    instance->write_problems_context.problem_index_abs = current_problem;
    instance->write_problems_context.problems_total = problems_count;
    instance->write_problems_context.problems = problems;

    // Setup and start worker
    view_dispatcher_switch_to_view(instance->view_dispatcher, NfcMagicAppViewWriteProblems);
}

bool nfc_magic_scene_gen2_write_check_on_event(void* context, SceneManagerEvent event) {
    NfcMagicApp* instance = context;
    UNUSED(event);
    UNUSED(context);
    UNUSED(instance);
    bool consumed = false;

    return consumed;
}

void nfc_magic_scene_gen2_write_check_on_exit(void* context) {
    NfcMagicApp* instance = context;

    instance->write_problems_context.problem_index = 0;
    instance->write_problems_context.problem_index_abs = 0;
    instance->write_problems_context.problems_total = 0;
    instance->write_problems_context.problems.all_problems = 0;

    // Backing out to the menu pops the dict-attack scene WITHOUT running its on_exit (the scene
    // manager only exits the current scene), so its KeysDict would leak. Free it here if still
    // owned; dict-attack on_exit NULLs it on the forward path, so this won't double-free.
    if(instance->nfc_dict_context.dict) {
        keys_dict_free(instance->nfc_dict_context.dict);
        instance->nfc_dict_context.dict = NULL;
    }

    write_problems_reset(instance->write_problems);
}
