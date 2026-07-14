#ifndef SCENERY_H
#define SCENERY_H

#include "structures.h"
#include "sdlint.h"

/** A t�j funkci�inak le�r�sa a scenery.c-ben tal�lhat� **/
void AddScenery(SceneryList*, Graphics, Vec2);
void EmptyScenery(SceneryList*);
void HandleScenery(SceneryList*, Uint8*, Uint8, PlayerObject*, Sint8);

#endif /* SCENERY_H */
