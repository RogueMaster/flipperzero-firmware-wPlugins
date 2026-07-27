#include "nfc_magic_scene_write_check_common.h"

#include "../nfc_magic_app_i.h"

void nfc_magic_write_check_handle_event(
    NfcMagicApp* instance,
    WriteProblemsEvent event,
    uint32_t exit_menu_scene) {
    NfcMagicAppWriteProblemsContext* problems_context = &instance->write_problems_context;

    if(event == WriteProblemsEventCenterPressed) {
        if(problems_context->problem_index == problems_context->problems_total - 1) {
            // Last warning acknowledged -> run the operation.
            if(instance->gen2_poller_is_wipe_mode) {
                scene_manager_next_scene(instance->scene_manager, NfcMagicSceneWipe);
            } else {
                scene_manager_next_scene(instance->scene_manager, NfcMagicSceneWrite);
            }
        } else {
            // Advance to the next set problem bit.
            problems_context->problem_index++;
            problems_context->problem_index_abs++;
            write_problems_set_problem_index(
                instance->write_problems, problems_context->problem_index);

            for(uint8_t i = problems_context->problem_index_abs;
                i < GEN2_POLLER_WRITE_PROBLEMS_LEN;
                i++) {
                if(problems_context->problems.all_problems & (1 << i)) {
                    write_problems_set_content(instance->write_problems, gen2_problem_strings[i]);
                    problems_context->problem_index_abs = i;
                    break;
                }
            }
        }
    } else if(event == WriteProblemsEventLeftPressed) {
        if(problems_context->problem_index == 0) {
            // Back out to the caller's menu. The exit menu is the ONLY thing that differs between
            // the two write-check scenes; the caller passes the one on its navigation stack
            // (Gen2Menu for Gen2, MfClassicMenu for Classic) so this never silently no-ops.
            scene_manager_search_and_switch_to_previous_scene(
                instance->scene_manager, exit_menu_scene);
        } else {
            // Page back to the previous set problem bit.
            problems_context->problem_index--;
            problems_context->problem_index_abs--;
            write_problems_set_problem_index(
                instance->write_problems, problems_context->problem_index);

            for(int i = (int)problems_context->problem_index_abs; i >= 0; i--) {
                if(problems_context->problems.all_problems & (1 << i)) {
                    write_problems_set_content(instance->write_problems, gen2_problem_strings[i]);
                    problems_context->problem_index_abs = i;
                    break;
                }
            }
        }
    }
}
