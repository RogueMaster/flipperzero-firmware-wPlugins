#ifndef TEXT_INPUT_H
#define TEXT_INPUT_H

#include <gui/elements.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>

#define HEADER_H 12U

void scene_on_enter_text_input(void* context);
bool scene_on_event_text_input(void* context, SceneManagerEvent event);
void scene_on_exit_text_input(void* context);

#endif // TEXT_INPUT_H
