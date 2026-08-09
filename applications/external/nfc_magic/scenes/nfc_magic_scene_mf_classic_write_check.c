#include "../nfc_magic_app_i.h"
#include "nfc_magic_scene_write_check_common.h"

void nfc_magic_scene_mf_classic_write_check_view_callback(WriteProblemsEvent event, void* context) {
    // Classic path: Left/"Retry" returns to MfClassicMenu. Shared body in
    // nfc_magic_write_check_handle_event so the two write-check scenes can't diverge again.
    nfc_magic_write_check_handle_event(context, event, NfcMagicSceneMfClassicMenu);
}

void nfc_magic_scene_mf_classic_write_check_on_enter(void* context) {
    NfcMagicApp* instance = context;

    Gen2PollerWriteProblems problems = gen2_poller_check_target_problems(instance->target_dev);
    if(!instance->gen2_poller_is_wipe_mode) {
        problems.all_problems |=
            gen2_poller_check_source_problems(instance->source_dev).all_problems;
    }
    FURI_LOG_D("GEN2", "Problems: %d", problems.all_problems);

    WriteProblems* write_problems = instance->write_problems;
    uint8_t problems_count = 0;
    uint8_t current_problem = 0;
    furi_assert(!problems.no_data, "No MFC data in nfc device");

    // Set the uid_locked problem to true as we have a Mifare Classic card
    problems.uid_locked = true;

    // Count the number of problems
    for(uint8_t i = 0; i < GEN2_POLLER_WRITE_PROBLEMS_LEN; i++) {
        if(problems.all_problems & (1 << i)) {
            problems_count++;
        }
    }

    // Init the view
    write_problems_set_callback(
        write_problems, nfc_magic_scene_mf_classic_write_check_view_callback, instance);
    write_problems_set_problems_total(write_problems, problems_count);
    write_problems_set_problem_index(write_problems, current_problem);

    // Set the first problem
    for(uint8_t i = current_problem; i < GEN2_POLLER_WRITE_PROBLEMS_LEN; i++) {
        if(problems.all_problems & (1 << i)) {
            write_problems_set_content(instance->write_problems, gen2_problem_strings[i]);
            instance->write_problems_context.problem_index_abs = i;
            break;
        }
    }

    // Save the problems context
    instance->write_problems_context.problem_index = current_problem;
    instance->write_problems_context.problems_total = problems_count;
    instance->write_problems_context.problems = problems;

    // Setup and start worker
    view_dispatcher_switch_to_view(instance->view_dispatcher, NfcMagicAppViewWriteProblems);
}

bool nfc_magic_scene_mf_classic_write_check_on_event(void* context, SceneManagerEvent event) {
    NfcMagicApp* instance = context;
    UNUSED(event);
    UNUSED(context);
    UNUSED(instance);
    bool consumed = false;

    return consumed;
}

void nfc_magic_scene_mf_classic_write_check_on_exit(void* context) {
    NfcMagicApp* instance = context;

    instance->write_problems_context.problem_index = 0;
    instance->write_problems_context.problem_index_abs = 0;
    instance->write_problems_context.problems_total = 0;
    instance->write_problems_context.problems.all_problems = 0;

    // Backing out to the menu pops the dict-attack scene without running its on_exit, leaking its
    // KeysDict; free it here if still owned (dict-attack on_exit NULLs it on the forward path).
    if(instance->nfc_dict_context.dict) {
        keys_dict_free(instance->nfc_dict_context.dict);
        instance->nfc_dict_context.dict = NULL;
    }

    write_problems_reset(instance->write_problems);
}
