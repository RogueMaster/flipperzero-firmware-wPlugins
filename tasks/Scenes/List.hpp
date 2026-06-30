#pragma once
#include "Scenes.hpp"

namespace FTasks::List
{
    void callback(void* context, uint32_t index) noexcept;
    void viewInputEvent(UFZ::Application& application, const UFZ::View& view) noexcept;
    bool viewInputEventCallback(InputEvent* event, void* context) noexcept;

    template<Scenes::Scenes T>
    void enter(void* context) noexcept
    {
        auto* menu = GET_WIDGET_P(context, UFZ::Submenu, T);
        auto* ctx = CTX(menu->application->getUserPointer());

        menu->reset();

        if constexpr (T == Scenes::EDIT_MENU)
        {
            ctx->tmpBuffer = "Editing: " +  (*ctx->currentContainer)[ctx->currentNoteIndex].first;
            UNUSED(menu->setHeader(ctx->tmpBuffer.c_str())
                           .addItem("Description",         Dialogs::DESCRIPTION,        callback, menu->application)
                           .addItem((ctx->currentContainer == &ctx->containers.todo ? "Mark as done" : "Mark as not done"), Dialogs::DONE, callback, menu->application)
                           .addItem("Rename",              Dialogs::RENAME,             callback, menu->application)
                           .addItem("Edit description",    Dialogs::EDIT_DESCRIPTION,   callback, menu->application)
                           .addItem("Delete",              Dialogs::DELETE,             callback, menu->application));
        }
        else
        {
            UNUSED(menu->setHeader(ctx->currentContainer == &ctx->containers.todo ? "Tasks - TODO" : "Tasks - Done"));
            for (size_t i = 0; i < ctx->currentContainer->size(); i++)
                UNUSED(menu->addItem((*ctx->currentContainer)[i].first.c_str(), Scenes::POPUP + i, callback, menu->application));
        }
        RENDER_VIEW(menu->application, T);
    }

    template<Scenes::Scenes T>
    bool event(void* context, const SceneManagerEvent event) noexcept
    {
        const auto* app = static_cast<UFZ::Application*>(context);
        auto* ctx = CTX(app->getUserPointer());

        bool consumed = false;
        if (event.type == SceneManagerEventTypeCustom)
        {
            consumed = true;

            if constexpr (T == Scenes::MAIN_MENU)
            {
                if (event.event == Scenes::MAIN_MENU) // Switching containers
                {
                    ctx->currentContainer = ctx->currentContainer == &ctx->containers.todo ? &ctx->containers.done : &ctx->containers.todo;
                    ctx->currentNoteIndex = 0;

                    FORCE_NEXT_SCENE(app, Scenes::MAIN_MENU);
                    return consumed;
                }
                ctx->currentNoteIndex = (event.event - Scenes::POPUP);

                if (ctx->currentContainer == &ctx->containers.todo && ctx->currentNoteIndex == 0) // We're making a new note
                {
                    ctx->containers.todo.emplace_back("Task name", "Description");
                    ctx->bMakingNewNote = true;
                    ctx->currentNoteIndex = ctx->containers.todo.size() - 1;

                    FORCE_NEXT_SCENE(app, Scenes::NAME_TEXT_EDIT);
                }
                else
                    FORCE_NEXT_SCENE(app, Scenes::EDIT_MENU);
            }
            else
            {
                Scenes::Scenes scene;
                NoteContainer* otherContainer;

                switch (event.event)
                {
                    case Dialogs::DESCRIPTION:
                        scene = Scenes::DESCRIPTION;
                        break;
                    case Dialogs::DONE:
                        if (ctx->currentContainer == &ctx->containers.todo)
                        {
                            if (ctx->containers.todo.size() == 1)
                            {
                                scene = Scenes::EDIT_MENU;
                                break;
                            }
                            otherContainer = &ctx->containers.done;
                        }
                        else
                            otherContainer = &ctx->containers.todo;

                        otherContainer->push_back((*ctx->currentContainer)[ctx->currentNoteIndex]);
                        ctx->currentContainer->erase(ctx->currentContainer->begin() + static_cast<NoteContainer::difference_type>(ctx->currentNoteIndex));

                        FORCE_NEXT_SCENE(app, Scenes::MAIN_MENU);
                        scene = Scenes::EDIT_MENU;
                        break;
                    case Dialogs::RENAME:
                        scene = Scenes::NAME_TEXT_EDIT;
                        break;
                    case Dialogs::EDIT_DESCRIPTION:
                        scene = Scenes::DESCRIPTION_TEXT_EDIT;
                        break;
                    case Dialogs::DELETE:
                        scene = Scenes::DELETE;
                        break;
                    default:
                        scene = Scenes::MAIN_MENU;
                        break;
                }
                // In case the scene we're going to is the same as the current scene don't do any navigation as this will
                // add the current scene to the last and current scenes, requiring an additional press of the back button
                // to go back
                if (scene != T)
                    NEXT_SCENE(app, scene);
            }
        }
        else if (event.type == SceneManagerEventTypeBack)
        {
            if constexpr (T == Scenes::EDIT_MENU)
                FORCE_NEXT_SCENE(app, Scenes::MAIN_MENU);
            else
                EXIT_APPLICATION(app);
            consumed = true;
        }
        return consumed;
    }

    template<Scenes::Scenes T>
    void exit(void* context) noexcept
    {
        auto* widget = GET_WIDGET_P(context, UFZ::Submenu, T);
        widget->reset();
    }
}