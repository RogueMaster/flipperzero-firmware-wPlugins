#pragma once

#include <furi.h>
#include <gui/view.h>

typedef struct SmallSubmenu SmallSubmenu;
typedef void (*SmallSubmenuItemCallback)(void* context, uint32_t index);

SmallSubmenu* small_submenu_alloc(void);
void small_submenu_free(SmallSubmenu* submenu);
View* small_submenu_get_view(SmallSubmenu* submenu);

void small_submenu_add_item(
    SmallSubmenu* submenu,
    const char* label,
    uint32_t index,
    SmallSubmenuItemCallback callback,
    void* callback_context);

void small_submenu_reset(SmallSubmenu* submenu);
void small_submenu_set_header(SmallSubmenu* submenu, const char* header);
