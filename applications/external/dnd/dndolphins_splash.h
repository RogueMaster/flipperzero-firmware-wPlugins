#pragma once
#include <stdbool.h>
typedef struct DndSplash DndSplash;
DndSplash* dndolphins_splash_begin(bool introduction);
void dndolphins_splash_wait(DndSplash* splash);
void dndolphins_splash_end(DndSplash* splash);
