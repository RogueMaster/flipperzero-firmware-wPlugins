/*
 * Copyright 2025 Ivan Barsukov
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "view_module_entity.h"

#include "src/engine/game_manager.h"

#include "input_converter.h"
#include "view_i.h" // IWYU pragma: keep

typedef struct
{
    const ViewModuleDescription* description;
    void* view_module;

    InputConverter* input_converter;

    ViewModuleBackCallback back_callback;
    void* back_callback_context;

} ViewModuleContext;

static void
view_module_stop(Entity* self, GameManager* manager, void* _entity_context)
{
    UNUSED(self);
    UNUSED(manager);

    ViewModuleContext* view_module_context = _entity_context;
    view_module_context->description->free(view_module_context->view_module);
    input_converter_free(view_module_context->input_converter);
}

static bool
need_call_back_callback(ViewModuleContext* entity_context, InputEvent* event)
{
    return event->key == InputKeyBack &&
           (event->type == InputTypeShort || event->type == InputTypeLong) &&
           entity_context->back_callback != NULL;
}

static void
view_module_update(Entity* self, GameManager* manager, void* _entity_context)
{
    UNUSED(self);
    ViewModuleContext* entity_context = _entity_context;

    InputState input = game_manager_input_get(manager);
    input_converter_process_state(entity_context->input_converter, &input);

    View* view =
      entity_context->description->get_view(entity_context->view_module);

    InputEvent event;
    while (input_converter_get_event(entity_context->input_converter, &event) ==
           FuriStatusOk) {
        if (need_call_back_callback(entity_context, &event)) {
            bool is_consumed = entity_context->back_callback(
              entity_context->back_callback_context);
            if (is_consumed) {
                continue;
            }
        }

        view->input_callback(&event, view->context);
    }
}

static void
view_module_render(Entity* self,
                   GameManager* manager,
                   Canvas* canvas,
                   void* _entity_context)
{
    UNUSED(self);
    UNUSED(manager);

    ViewModuleContext* entity_context = _entity_context;
    View* view =
      entity_context->description->get_view(entity_context->view_module);
    view->draw_callback(canvas, view_get_model(view));
}

Entity*
view_module_add_to_level(Level* level,
                         GameManager* manager,
                         const ViewModuleDescription* module_description)
{
    UNUSED(manager);
    furi_check(module_description);

    // Alloc entity
    Entity* entity = level_add_entity(level, &view_module_description);

    // Alloc view module
    ViewModuleContext* view_module_context = entity_context_get(entity);
    view_module_context->description = module_description;
    view_module_context->view_module = module_description->alloc();

    // Alloc converter
    view_module_context->input_converter = input_converter_alloc();

    // Callback
    view_module_context->back_callback = NULL;
    view_module_context->back_callback_context = NULL;

    return entity;
}

void*
view_module_get_module(Entity* entity)
{
    ViewModuleContext* view_module_context = entity_context_get(entity);
    return view_module_context->view_module;
}

void
view_module_set_back_callback(Entity* entity,
                              ViewModuleBackCallback back_callback,
                              void* context)
{
    ViewModuleContext* view_module_context = entity_context_get(entity);
    view_module_context->back_callback = back_callback;
    view_module_context->back_callback_context = context;
}

const EntityDescription view_module_description = {
    .start = NULL,
    .stop = view_module_stop,
    .update = view_module_update,
    .render = view_module_render,
    .collision = NULL,
    .event = NULL,
    .context_size = sizeof(ViewModuleContext),
};