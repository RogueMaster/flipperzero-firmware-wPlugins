#include "Input.hpp"

bool FTasks::Input::event(void* context, const SceneManagerEvent event) noexcept {
    const auto* app = static_cast<UFZ::Application*>(context);

    if(event.type == SceneManagerEventTypeCustom) {
        FORCE_NEXT_SCENE(app, event.event);
        return true;
    }

    // Backing out of either half of the two-step creation flow cancels it. List::event appends the placeholder to
    // todo before the name is ever entered, so without this the aborted note stays in the list as "Task name" and
    // bMakingNewNote stays set, sending the next rename through the description step as well
    if(event.type == SceneManagerEventTypeBack) {
        auto* ctx = CTX(app->getUserPointer());
        if(ctx->bMakingNewNote) {
            // Creation only ever appends to todo, and nothing between that append and here removes an entry,
            // so the placeholder is still the last element
            ctx->containers.todo.pop_back();

            ctx->bMakingNewNote = false;
            ctx->bPreserveSelection = false; // The index refers to the note that just went away
            ctx->currentNoteIndex = 0;

            FORCE_NEXT_SCENE(app, Scenes::MAIN_MENU);
            return true;
        }
    }
    return false;
}
