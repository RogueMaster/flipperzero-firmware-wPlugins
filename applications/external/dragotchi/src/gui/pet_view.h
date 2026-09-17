#ifndef __pet_view_h__
#define __pet_view_h__
#include <gui/view.h>
#include "../game_structs.h"
#define PET_EVT_MENU 100u   /* OK on the main screen opens the menu */
View *pet_view_alloc(void *context);
void pet_view_free(View *);
void pet_view_update(View *, const struct GameState *, bool night, uint32_t remaining_sec);
#endif
