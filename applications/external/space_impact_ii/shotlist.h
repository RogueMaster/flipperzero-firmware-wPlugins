#ifndef SHOTLIST_H
#define SHOTLIST_H

#include "structures.h"
#include "sdlint.h"

/** A l�v�slista funkci�inak le�r�sa a shotlist.c-ben tal�lhat� **/
void AddShot(ShotList*, Vec2, Sint8, Uint8, WeaponKind);
void EmptyShotList(ShotList*);
void RemoveShot(ShotList*, Shot*);
Uint8 Intersect(Vec2, Vec2, Vec2, Vec2);
void ShotListTick(ShotList*, Uint8*, PlayerObject*);

#endif /* SHOTLIST_H */
