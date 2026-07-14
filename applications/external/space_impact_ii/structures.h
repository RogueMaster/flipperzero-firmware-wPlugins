#ifndef STRUCTURES_H
#define STRUCTURES_H

#include <stdlib.h>
// #include <SDL.h>
#include "sdlint.h"

/** Enumerációk **/

/** WeaponKind: Lövedékek típusa **/
typedef enum {
    Standard = 0, /* Sima lövés */
    Missile = 1, /* Rakéta */
    Beam = 2, /* Sugár */
    Wall = 3 /* Fal */
} WeaponKind;

/** Graphics: Objektumok azonosítói **/
typedef enum {
    /* Számok, 3*5-ös méretben */
    gNum0 = 0,
    gNum1,
    gNum2,
    gNum3,
    gNum4,
    gNum5,
    gNum6,
    gNum7,
    gNum8,
    gNum9,
    /* Menüelemek */
    gSpace,
    gIntro,
    gImpact,
    gScrollMark,
    gDotEmpty,
    gDotFull,
    /* Játékosssal kapcsolatos modellek és ikonok */
    gLife, /* Életjel */
    gMissileIcon, /* Rakéta ikonja */
    gBeamIcon, /* Sugár ikonja */
    gWallIcon, /* Fal ikonja */
    gShot, /* Lövés */
    gExplosionA1,
    gExplosionA2 /* Robbanás animáció 2 lépése */
} Graphics;

/* Fájlba áthelyezett objektumok */
#define G_PROTECTION_A1 256 + 250
#define G_PROTECTION_A2 256 + 251
#define G_MISSILE       256 + 252
#define G_BEAM          256 + 253
#define G_WALL          256 + 254
#define G_PLAYER        256 + 255

/** Struktúrák **/

/** Vec2: Kétdimenziós vektor, hosszúságot és szélességet tárol **/
typedef struct Vec2 {
    Sint16 x,
        y; /* Azért elõjeles, mert lehet a képernyõrõl balra kiment lövedék, vagy felülrõl beúszó ellenség is */
} Vec2;

/** Object: Objektumokat tárol, a képméretet, valamint egy kétdimenziós tömböt a pixelekkel (0 - inaktív, 1 - aktív) **/
typedef struct Object {
    Vec2 Size; /* Az objektum mérete */
    Uint8* Samples; /* Size.x * Size.y méretû tömb az objektum képpontjaihoz */
} Object;

/** PlayerObject: A játékos objektuma **/
typedef struct PlayerObject {
    Vec2 Pos; /* Bal felső sarkának pozíciója a képernyőn */
    Uint8 Lives; /* Életek, egy élet = egy találat */
    Uint16
        Score; /* Pontszám: bár a kijelző 99999-ig tud mutatni eredményt, az Uint16 maximumának elérése lehetetlen */
    Uint8 Bonus; /* Hátralévõ bónuszfegyveres támadások */
    WeaponKind Weapon; /* Aktuális bónuszfegyver */
    Uint8 Protection; /* Hány képkocka sebzésvédelem van hátra */
} PlayerObject;

/** Enemy: Ellenség struktúra **/
typedef struct Enemy {
    Uint16 Model; /* Az ellenséghez tartozó grafikai objektum azonosítója */
    Vec2 Size; /* Az ellenség mérete */
    Uint8 AnimCount; /* Animációs lépések száma */
    Sint8 Lives; /* Ennyi találatot él túl, a lövedékek sebzésének megmaradása miatt előjeles */
    Uint8 Floats; /* Ha beért a pályára, nem megy tovább */
    Uint8 ShotTime; /* Képkockák két lövés közt, 0, ha nem lő */
    /* Az alábbi két opciót kombinálva fel-le mozog */
    Uint8 MoveUp; /* Ha beért a pályára, felfelé mozog */
    Uint8 MoveDown; /* Ha beért a pályára, lefelé mozog */
    Uint8 MoveAnyway; /* Ne várjon a képernyőre beérkezéssel, azonnal kezdje a mozgást */
    Vec2 MovesBetween; /* A két y koordináta, amik közt fel-le mozog */
} Enemy;

/** Ellenségek láncolt listája **/
typedef struct EnemyList {
    Vec2 Pos; /* Pozíció */
    Enemy Type; /* Ellenség típusa */
    Uint8 AnimState; /* Animáció állapota */
    Sint8 Lives; /* Életpontok, ennyi lövést él még túl */
    Sint8 MoveDir; /* Indulási mozgásirány, 1: fel, -1: le */
    Uint8 Cooldown; /* Fegyver kihűlése */
    struct EnemyList* Next; /* Következõ elem */
} EnemyList, *EnemyListStart;

/** Lövések láncolt listája **/
typedef struct Shot {
    Vec2 Pos; /* Pozíció */
    Sint8 v; /* Sebesség, mint egydimenziós irányvektor */
    Uint8 FromPlayer; /* A játékos lõtte-e ? 1 : 0 */
    WeaponKind Kind; /* A lövés típusa */
    Uint8 Damage; /* Ha bónuszfegyver, akkor van egy sebzése, amit ellenségek közt el tud osztani */
    Vec2 Size; /* A kilõtt objektum mérete */
    struct Shot* Next; /* Következõ elem */
} Shot, *ShotList;

/** A táj láncolt listája **/
typedef struct Scenery {
    Uint16 Model; /* Grafikai objektum azonosítója */
    Vec2 Pos; /* Hely a szinten */
    struct Scenery* Next; /* Következõ elem */
} Scenery, *SceneryList;

#endif /* STRUCTURES_H */
