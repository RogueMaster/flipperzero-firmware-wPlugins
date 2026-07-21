#pragma once
#include "Scenes.hpp"

namespace FTasks::Input {
template <Scenes::Scenes T>
void callback(void* context) noexcept {
    const auto* app = static_cast<UFZ::Application*>(context);
    auto* ctx = CTX(app->getUserPointer());
    // TextInput fills the 128-byte buffer allocated in enter() with a NUL-terminated string, leaving the
    // std::string at its full 128-byte size. Trim it back to the text the user actually typed: the padding
    // is invisible through c_str() but writeContainerString writes size() bytes, so it would end up on disk
    ctx->inputTextBuffer.resize(std::strlen(ctx->inputTextBuffer.c_str()));

    if constexpr(T == Scenes::DESCRIPTION_TEXT_EDIT)
        CURRENT_NOTE(ctx).second = ctx->inputTextBuffer;
    else
        CURRENT_NOTE(ctx).first = ctx->inputTextBuffer;

    // Handle navigation when editing and creating new notes
    auto id = Scenes::EDIT_MENU;
    if(ctx->bMakingNewNote) {
        if constexpr(T == Scenes::DESCRIPTION_TEXT_EDIT) {
            id = Scenes::MAIN_MENU;

            ctx->bMakingNewNote = false; // Note successfully created
        } else
            id = Scenes::DESCRIPTION_TEXT_EDIT;
    }

    // Persist every completed edit right away so a crash or battery pull can't lose it. During creation
    // bMakingNewNote is still set after the name step, which skips saving the half-made note; the description
    // step clears it above and saves the finished note
    if(!ctx->bMakingNewNote) Data::save(*app);

    SEND_CUSTOM_EVENT(app, id);
}

template <Scenes::Scenes T>
void enter(void* context) noexcept {
    auto* popup = GET_WIDGET_P(context, UFZ::TextInput, T);
    auto* ctx = CTX(popup->application->getUserPointer());

    if constexpr(T == Scenes::DESCRIPTION_TEXT_EDIT) {
        popup->setHeaderText("New description");
        ctx->inputTextBuffer = CURRENT_NOTE(ctx).second;
    } else {
        popup->setHeaderText("New name");
        ctx->inputTextBuffer = CURRENT_NOTE(ctx).first;
    }
    // The 128-byte buffer (127 characters + NUL) is deliberate: a max-length description fits the
    // Description popup screen exactly, so no wrapping or scrolling widget is needed
    ctx->inputTextBuffer.resize(128);
    ctx->inputTextBuffer.back() =
        '\0'; // Keep the terminator inside the 128 bytes handed to TextInput below
    popup->setResultCallback(
        callback<T>,
        popup->application,
        ctx->inputTextBuffer.data(),
        ctx->inputTextBuffer.size(),
        false);

    RENDER_VIEW(popup->application, T);
}

bool event(void* context, SceneManagerEvent event) noexcept;

template <Scenes::Scenes T>
void exit(void* context) noexcept {
    GET_WIDGET_P(context, UFZ::TextInput, T)->reset();
}
}
