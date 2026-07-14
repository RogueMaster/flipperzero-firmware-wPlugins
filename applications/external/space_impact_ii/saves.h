#ifndef SAVES_H
#define SAVES_H
#include <stddef.h>
#include "sdlint.h"

#define SCORE_COUNT 10

/* A ment�si funkci�k le�r�sa a saves.c-ben tal�lhat� */
void ReadSavedLevel(Uint8*);
void ReadTopScore(unsigned int*);
void SaveLevel(Uint8);
void PlaceTopScore(unsigned int*, Uint16);

#endif /* SAVES_H */
