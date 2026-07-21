#pragma once
#include "Scenes.hpp"

namespace FTasks::List {
void callback(void* context, uint32_t index) noexcept;
void viewInputEvent(UFZ::Application& application, const UFZ::View& view) noexcept;
bool viewInputEventCallback(InputEvent* event, void* context) noexcept;

template <Scenes::Scenes T>
void enter(void* context) noexcept {
    auto* menu = GET_WIDGET_P(context, UFZ::Submenu, T);
    auto* ctx = CTX(menu->application->getUserPointer());

    menu->reset();

    if constexpr(T == Scenes::EDIT_MENU) {
        ctx->tmpBuffer = "Editing: " + CURRENT_NOTE(ctx).first;
        UNUSED(menu->setHeader(ctx->tmpBuffer.c_str())
                   .addItem("Description", Dialogs::DESCRIPTION, callback, menu->application)
                   .addItem(
                       (ctx->currentContainer == &ctx->containers.todo ? "Mark as done" :
                                                                         "Mark as not done"),
                       Dialogs::DONE,
                       callback,
                       menu->application)
                   .addItem("Rename", Dialogs::RENAME, callback, menu->application)
                   .addItem(
                       "Edit description", Dialogs::EDIT_DESCRIPTION, callback, menu->application)
                   .addItem("Delete", Dialogs::DELETE, callback, menu->application));
    } else {
        UNUSED(menu->setHeader(
            ctx->currentContainer == &ctx->containers.todo ? "Tasks - TODO" : "Tasks - Done"));
        for(size_t i = 0; i < ctx->currentContainer->size(); i++)
            UNUSED(menu->addItem(
                (*ctx->currentContainer)[i].first.c_str(),
                noteEventOffset + i,
                callback,
                menu->application));

        // Return the user to the task they were working on, but only after edits that kept the index valid
        if(ctx->bPreserveSelection) {
            if(ctx->currentNoteIndex < ctx->currentContainer->size())
                UNUSED(menu->setSelectedItem(
                    static_cast<uint32_t>(noteEventOffset + ctx->currentNoteIndex)));
            ctx->bPreserveSelection = false;
        }
    }
    RENDER_VIEW(menu->application, T);
}

template <Scenes::Scenes T>
bool event(void* context, const SceneManagerEvent event) noexcept {
    const auto* app = static_cast<UFZ::Application*>(context);
    auto* ctx = CTX(app->getUserPointer());

    bool consumed = false;
    if(event.type == SceneManagerEventTypeCustom) {
        consumed = true;

        if constexpr(T == Scenes::MAIN_MENU) {
            if(event.event == Scenes::MAIN_MENU) // Switching containers
            {
                ctx->currentContainer = ctx->currentContainer == &ctx->containers.todo ?
                                            &ctx->containers.done :
                                            &ctx->containers.todo;
                ctx->currentNoteIndex = 0;
                ctx->bPreserveSelection = false;

                FORCE_NEXT_SCENE(app, Scenes::MAIN_MENU);
                return consumed;
            }

            // Every real note event carries noteEventOffset + i. Anything below that is a scene id delivered
            // here in error; subtracting the offset would underflow currentNoteIndex (a size_t) into a wild
            // index that the next EDIT_MENU enter() dereferences unchecked. The container-switch id (0) is
            // handled above; this guards the rest of the range should another scene id ever reach us
            if(event.event < noteEventOffset) return consumed;

            ctx->currentNoteIndex = (event.event - noteEventOffset);
            ctx->bPreserveSelection = true;

            if(ctx->currentContainer == &ctx->containers.todo &&
               ctx->currentNoteIndex == 0) // We're making a new note
            {
                ctx->containers.todo.emplace_back("Task name", "Description");
                ctx->bMakingNewNote = true;
                ctx->currentNoteIndex = ctx->containers.todo.size() - 1;

                FORCE_NEXT_SCENE(app, Scenes::NAME_TEXT_EDIT);
            } else
                FORCE_NEXT_SCENE(app, Scenes::EDIT_MENU);
        } else {
            Scenes::Scenes scene;

            switch(event.event) {
            case Dialogs::DESCRIPTION:
                scene = Scenes::DESCRIPTION;
                break;
            case Dialogs::DONE: {
                // Nothing to mark as done while the TODO list only holds the "+ New task" sentinel. Staying put
                // avoids re-entering this scene, which would cost the user an extra press of the back button
                if(ctx->currentContainer == &ctx->containers.todo &&
                   ctx->containers.todo.size() == 1)
                    return consumed;

                NoteContainer* otherContainer = ctx->currentContainer == &ctx->containers.todo ?
                                                    &ctx->containers.done :
                                                    &ctx->containers.todo;

                ctx->bPreserveSelection =
                    false; // The task moves containers, so the index no longer refers to it
                otherContainer->push_back(CURRENT_NOTE(ctx));
                ctx->currentContainer->erase(
                    ctx->currentContainer->begin() +
                    static_cast<NoteContainer::difference_type>(ctx->currentNoteIndex));

                Data::save(
                    *app); // Persist the move right away so a crash or battery pull can't lose it

                FORCE_NEXT_SCENE(app, Scenes::MAIN_MENU);
                return consumed;
            }
            case Dialogs::RENAME:
                scene = Scenes::NAME_TEXT_EDIT;
                break;
            case Dialogs::EDIT_DESCRIPTION:
                scene = Scenes::DESCRIPTION_TEXT_EDIT;
                break;
            case Dialogs::DELETE:
                scene = Scenes::DELETE;
                break;
            // A stray event id stays put instead of navigating, mirroring the noteEventOffset guard in the
            // MAIN_MENU branch above. Every Dialogs value needs a case: -Wswitch cannot flag a missing one
            // because event.event is a raw uint32_t, not a Dialogs enum
            default:
                return consumed;
            }
            NEXT_SCENE(app, scene);
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        if constexpr(T == Scenes::EDIT_MENU)
            FORCE_NEXT_SCENE(app, Scenes::MAIN_MENU);
        else
            EXIT_APPLICATION(app);
        consumed = true;
    }
    return consumed;
}

template <Scenes::Scenes T>
void exit(void* context) noexcept {
    auto* widget = GET_WIDGET_P(context, UFZ::Submenu, T);
    widget->reset();
}
}
