#ifndef GRAPHICS_H
#define GRAPHICS_H

#include "sdlint.h"
#include "structures.h"

/** A grafikus funkciók leírása a graphics.c-ben található **/
void DrawObject(Uint8*, Object, Vec2);
void DrawOutlinedObject(Uint8*, Object, Vec2);
void DrawSmallNumber(Uint8*, Uint16, Uint8, Vec2);
void DrawText(Uint8*, const char*, Vec2, int);
void DrawScrollBar(Uint8*, Uint8);
void InvertScreen(Uint8*);
void InvertScreenPart(Uint8*, Vec2, Vec2);
void UncompressPixelMap(Uint8*, Uint16, Uint16);
void UncompressObjects();
Object GetObject(Uint16);
void FreeDynamicGraphics();
Vec2 NewVec2(Sint16, Sint16);
Object NewObject(Vec2, Uint8*);

#endif /* GRAPHICS_H */
