#include <string.h>
#include "sdlint.h"
#include "config.h"
#include "enemies.h"
#include "graphics.h"
#include "saves.h"
#include "shotlist.h"
#include "enemy_objects.h"
#include "level_objects.h"

/** Ellenség hozzáadása a paraméterben kapott láncolt listához, pozícióban, azonosítóval, mozgásiránnyal **/
void AddEnemy(EnemyListStart* Enemies, Vec2 Pos, Uint8 EnemyID, Sint8 MoveDir) {
    EnemyList* CreateAt =
        *Enemies; /* A létrehozási memóriahely, ahova íródik a lista új utolsó eleme */
    if(*Enemies) { /* Ha van már lista, meg kell keresni a végét, és ott létrehozni az új elemet */
        while(CreateAt->Next) /* Az új cím a listán végiglépked az utolsó elemig */
            CreateAt = CreateAt->Next;
        CreateAt->Next = (EnemyList*)malloc(sizeof(
            EnemyList)); /* Az utolsó elem a semmi helyett az újonnan lefoglalt helyre mutat */
        CreateAt = CreateAt->Next; /* Az íráshoz használt cím kerüljön a változóba */
    } else /* Ha nincs még lista, a pointerre kell lefoglalni a címet */
        CreateAt = *Enemies = (EnemyList*)malloc(sizeof(EnemyList));
    /* Bemenetként kapott adatok átadása az új elemnek */
    CreateAt->Pos = Pos;
    CreateAt->Type = GetEnemy(EnemyID);
    CreateAt->AnimState = 0; /* Az animációk indulási pontja mindig az 1. fázis */
    CreateAt->Lives = CreateAt->Type.Lives;
    CreateAt->MoveDir = MoveDir;
    CreateAt->Cooldown =
        CreateAt->Type.ShotTime; /* Nem úgy élednek, hogy eleve tudnának lőni, ki kell hűlniük */
    CreateAt->Next = NULL; /* Nincs következõ elem, lista vége jel */
}

/** A bemenetben kapott listáról a bemenetben kapott címen található elemet eltávolítja **/
void RemoveEnemy(EnemyListStart* List, EnemyList* Address) {
    EnemyList *Checked = *List,
              *Last = *List; /* Az elsõ változó a jelenleg vizsgált cím, a második az azelõtti */
    while(Checked) { /* Ameddig van mit nézni */
        if(Checked == Address) { /* Ha megvan az egyezés */
            EnemyList* Current = Checked; /* A jelenlegi memóriacím */
            if(Checked ==
               *List) /* Ha a lista elsõ elemét kell törölni, akkor a lista pointerjét át kell helyezni a második elemre */
                *List = (*List)->Next;
            Last->Next =
                Current
                    ->Next; /* Az elõzõleg vizsgált elem következõre mutatóját kell a jelenleg vizsgált elemet követõre mutatni */
            free(Current); /* A törölt elem felszabadítása */
            return; /* Innen a funkciónak már nincs dolga, félbe lehet szakítani */
        }
        Last = Checked; /* Az utolsó elem a jelenleg vizsgált */
        Checked = Checked->Next; /* A következõ elemet vizsgálja a következõ körben */
    }
}

/** A paraméterben kapott ellenséglista ürítése **/
void EmptyEnemyList(EnemyListStart* Enemies) {
    EnemyList* Last = *Enemies; /* Utolsó elem címének tárolása a törléshez */
    while(*Enemies) { /* Ameddig van még elem */
        *Enemies = (*Enemies)->Next; /* A pointer lépjen a következõ elemre */
        free(Last); /* Az elõzõleg vizsgált elem felszabadítása */
        Last = *Enemies; /* Most már ez az utoljára vizsgált elem */
    }
    *Enemies = NULL; /* Ne mutasson sehova, mert nincs semmi lefoglalva */
}

/** Az ellenségeket kezelõ funkció, bemenetként kéri az ellenségek listáját, a játékost,
    a pixeltérképet (ez rajzolja ki õket, ha közben nem halnak meg), a lövéslistát (a találatokat ez törli),
    egy igaz-hamis értéket, hogy animációs fáizisban van-e a játék, és a táj mozgatásának igaz-hamis értékét **/
void EnemyListTick(
    EnemyListStart* Enemies,
    PlayerObject* Player,
    Uint8* PixelMap,
    ShotList* Shots,
    Uint8 AnimPulse,
    Uint8* MoveScene) {
    Shot* CheckedShot; /* Egy pointer lefoglalása, amivel a lövéslistán iterál majd végig többször is */
    EnemyList* Current = *Enemies; /* A jelenleg vizsgált ellenség, ez járja be a listát */
    while(Current) { /* Amíg vannak a láncolt listán ellenségek */
        Uint8 Alive = Current->Lives > 0; /* Él-e még az ellenség */
        Uint8 InScreen =
            Current->Pos.x <=
            60; /* Azért nem egyenlõség, mert néhány fõellenség elõre-hátra is mozog */
        if(!Current->Next &&
           InScreen) /* Ha az utolsó ellenség, ami a fõellenség, beért a képernyõre... */
            *MoveScene = 0; /* ...álljon le a táj mozgása */
        if((Current->Type.MoveUp || Current->Type.MoveDown) &&
           (InScreen ||
            Current->Type
                .MoveAnyway)) { /* Ha bármiféle mozgás engedélyezve van, és az ellenség már látszik */
            if(Current->MoveDir == 1 &&
               Current->Pos.y == Current->Type.MovesBetween.y) /* Alsó határ esetén */
                Current->MoveDir =
                    -Current->Type
                         .MoveUp; /* Attól függõen, hogy mozoghat-e fel, mozogjon fel, vagy álljon meg */
            else if(
                Current->MoveDir == -1 &&
                Current->Pos.y == Current->Type.MovesBetween.x) /* Felsõ határ esetén */
                Current->MoveDir =
                    Current->Type
                        .MoveDown; /* Attól függõen, hogy mozoghat-e le, mozogjon le, vagy álljon meg */
            Current->Pos.y += Current->MoveDir; /* Mozgatás */
        }
        if(!Current->Type.Floats ||
           !InScreen) /* Mozgatás balra, kivéve, ha lebegõ ellenségrõl van szó, az álljon meg a 60. oszlopnál */
            Current->Pos.x--;
        if(AnimPulse) /* Animációs fázisokban járjon körkörösen az animáció minden lépése */
            Current->AnimState = (Current->AnimState + 1) % Current->Type.AnimCount;
        if(Alive) { /* Csak akkor ellenõrizze lövésekre, ha él */
            Uint8 GotHit = 0; /* Eltalálta-e a játékos az ellenséget */
            CheckedShot = *Shots; /* A láncolt lista bejárásához szükséges memóriacím */
            while(CheckedShot &&
                  !GotHit) { /* Végighaladás a lövéslistán, ha még nem volt találat */
                if(CheckedShot
                       ->FromPlayer) { /* Az ellenségek ne lõjék le egymást, csak a játékos lövéseit kell figyelembe venni */
                    if(Intersect(
                           CheckedShot->Pos,
                           CheckedShot->Size,
                           Current->Pos,
                           Current->Type
                               .Size)) { /* Az ellenség és a lövedék köré rajzolt négyzet illeszkedik-e */
                        GotHit = 1; /* Találat észlelve */
#ifdef BONUS_COLLIDER
                        if(Current->Type.Lives == 127) {
                            Player->Score +=
                                CheckedShot->Damage *
                                5; /* A lövedék teljes hátralévő sebzése legyen pont */
                            RemoveShot(Shots, CheckedShot); /* A lövedék eltávolítása mindenképp */
                        } else {
#else
                        if(Current->Type.Lives !=
                           127) { /* Ha nem bónuszobjektum, vegye is találatnak */
#endif
                            Current->Lives -=
                                CheckedShot
                                    ->Damage; /* Ellenség életének csökkentése a lövedék sebzésével */
                            Player->Score += 5; /* Pontjutalom a találatért */
                            if(Current->Lives <
                               0) { /* Ha a lövedék nem használta el minden sebzését */
                                CheckedShot->Damage =
                                    -Current
                                         ->Lives; /* Az ellenség életének ellentettje pont a megmaradt sebzés lesz */
                                Current->Lives =
                                    0; /* Az ellenség nempozitív életei a robbanás animáció állapotai */
                            } else
                                RemoveShot(
                                    Shots,
                                    CheckedShot); /* A lövedék eltávolítása, ha elfogyott a sebzése */
                            if(Current->Lives == 0) { /* Halál esetén */
                                Player->Score += 5; /* A játékos kapjon 5 további pontot */
                                Alive =
                                    0; /* Már nem él az ellenség, így járjon el a későbbi elágazásokban */
                                Current->Type.MoveUp = Current->Type.MoveDown =
                                    0; /* Mozgások kikapcsolása, hogy a robbanás animáció ne mozogjon */
                            }
                        }
                    }
                }
                if(!GotHit) /* Csak akkor folytatódjon a vizsgálat, ha nem észlelt találatot */
                    CheckedShot =
                        CheckedShot->Next; /* Folytatás a láncolt lista következõ elemével */
            }
        }
        if(Alive && Intersect(
                        Player->Pos,
                        NewVec2(10, 7),
                        Current->Pos,
                        Current->Type.Size)) { /* Ha még mindig él, de a játékos nekiment */
            Alive = 0; /* Ütközésbe mindenképp belehal */
            if(Current->Type.Lives == 127) { /* Bónuszhordozóba ütközés esetén adjon bónuszt */
                WeaponKind NewKind = (WeaponKind)(rand() % 3 + 1); /* Új véletlen bónuszfegyver */
                if(NewKind !=
                   Player->Weapon) { /* Ha nem ilyenje volt a játékosnak, nullázza a bónuszok számát, és adjon neki ilyet */
                    Player->Bonus = 0;
                    Player->Weapon = NewKind;
                }
                Player->Bonus +=
                    4 - (int)NewKind; /* Rakétából 3-at, sugárból 2-t, falból 1-et ad */
                Current->Lives = -2; /* Robbanás animációk átugrása */
            } else if(
                Player->Lives &&
                !Player->Protection) /* Ha a játékos még él, és nincs védelme, ugyanis a main-ben elõbb hívódik meg a lövések kezelése, amiben meghalhat még */
                Player->Lives--; /* A játékos bárminemû sebzéstõl 1 életet veszít */
        }
        if(Current->Pos.x < -Current->Type.Size.x ||
           !Alive) { /* Ha kiúszott balra, vagy meghalt, robbanás animáció, majd törlés */
            Current->Lives--; /* Animációs fázis (élet abszolútértéke) növelése */
            if(Current->Lives == -3) { /* A végső fázis: törlés */
                EnemyList* Remove = Current; /* A törlendő elem */
                Current =
                    Current
                        ->Next; /* Következő körben folytatás a láncolt lista következõ elemével, mielőtt törlődik a listaelem */
                RemoveEnemy(Enemies, Remove); /* Ellenség törlése */
                continue; /* Ne fusson tovább ez az iteráció, ne érje el újra a Current = Current->Next; sort */
            } else { /* Robbanás animáció */
                Vec2 AnimCenter = NewVec2(
                    Current->Pos.x + Current->Type.Size.x / 2,
                    Current->Pos.y +
                        Current->Type.Size.y / 2); /* Animációs objektum középpontja */
                /* A rajzolási pozíciókba a balra mozgás is bele van számítva */
                /* Ez, és az animációs fázis egyszerűen számítható, ha tudjuk, hogy az élet -1 és -2 lehet a fázistól függően, valamint a két objektum azonosítója egymás után jön */
                DrawObject(
                    PixelMap,
                    GetObject(gExplosionA1 - Current->Lives - 1),
                    NewVec2(AnimCenter.x - 3 - Current->Lives, AnimCenter.y - 2));
            }
        } else {
            if(Current->Pos.x < 84) { /* Ha él még, és már beúszott a képernyõre */
                DrawObject(
                    PixelMap,
                    GetObject(Current->Type.Model + Current->AnimState),
                    Current->Pos); /* Kirajzolás */
                if(Current->Type.ShotTime) { /* Ha tud lőni */
                    Current->Cooldown--; /* Hűljön a fegyver */
                    if(Current->Cooldown == 0) { /* Ha már hideg a fegyver */
                        AddShot(
                            Shots,
                            NewVec2(
                                Current->Pos.x - 1, Current->Pos.y + (Current->Type.Size.y / 2)),
                            -2,
                            0,
                            Standard); /* Lövés */
                        Current->Cooldown = Current->Type.ShotTime; /* Kihűlés visszaállítása */
                    }
                }
            }
        }
        Current = Current->Next; /* Folytatás a láncolt lista következõ elemével */
    }
    /** Bónuszfegyverek kezelése **/
    CheckedShot = *Shots; /* A láncolt lista bejárásához szükséges memóriacím */
    while(CheckedShot) { /* Ameddig van következõ elem */
        Shot* LastShot = CheckedShot;
        if(CheckedShot->Kind == Missile) { /* Rakéták esete */
            Uint8 TargetHeight =
                CheckedShot->Pos.y; /* Célmagasság, ha nincs közeli ellenség, maradjon itt */
            EnemyList* Current =
                *Enemies; /* Az ellenségek listáját végig kell nézni a legközelebbi meghatározásához */
            while(
                Current) { /* A következő sorok arra épülnek, hogy a listán X pozíció alapján növekvő sorrendben vannak az elemek */
                if(Current->Pos.x >
                   84) /* A képernyõn nem látszódó ellenségeket már ne kövesse le */
                    Current =
                        NULL; /* Nyugodtan abba lehet itt hagyni, mert egymás után vannak hozzáadva */
                else if(
                    Current->Pos.x >
                    CheckedShot->Pos
                        .x) { /* Ha megvan az elsõ találat a rakétától jobbra található ellenségre */
                    TargetHeight = Current->Pos.y; /* A cél legyen a találat magassága */
                    Current = NULL; /* Abba lehet hagyni a keresést, megvolt a legközelebbi */
                } else /* Ha nem történt ok a leállásra, menjen tovább a lista */
                    Current = Current->Next;
            }
            if(CheckedShot->Pos.y !=
               TargetHeight) { /* Ha nincs a célmagasságon, haladjon affelé */
                if(CheckedShot->Pos.y < TargetHeight) /* Ha fölötte van, ereszkedjen */
                    CheckedShot->Pos.y++;
                else /* Különben emelkedjen */
                    CheckedShot->Pos.y--;
            }
        }
        /* Ha sugár típusú lõszer van a pályán, azt törölni kell, mert az élettartama 1 képkocka */
        CheckedShot = CheckedShot->Next;
        if(LastShot->Kind == Beam) RemoveShot(Shots, LastShot);
    }
}

/** Ellenségeket helyez el a szinten fix helyekre, bemenete a láncolt lista kezdete, és a szint száma **/
void LevelSpawner(EnemyListStart* Enemies, Uint8 Level) {
    Uint8 EnemyCount;
    const Uint8* LevelData = getLevelData(Level);
    if(!LevelData) /* Ha nem talált fájlt, vagy nem fért hozzá */
        return; /* Töltsön be üres pályát, majd a játékmechanika továbblép a következőre */
    EmptyEnemyList(Enemies); /* Kilépés esetén megmaradt ellenségek ürítése */
    EnemyCount = LevelData[0];
    const Uint8* LevelDataEnemies = &(LevelData[1]);
    while(EnemyCount--) {
        Uint8 EnemyData[5]; /* Ellenségadatok tömbje */
        memcpy(EnemyData, LevelDataEnemies, 5 * sizeof(Uint8));
        AddEnemy(
            Enemies,
            NewVec2(EnemyData[0] * 256 + EnemyData[1], EnemyData[2]),
            EnemyData[3],
            (Sint8)EnemyData[4] - 1); /* És így könnyebben hozzáadható a listához */
        LevelDataEnemies += 5 * sizeof(Uint8);
    }
}

Enemy* Enemies[256] = {
    NULL}; /* 256 dinamikus ellenség helye, amit fájlból tölt be a játék, ha hivatkozik rájuk bármi */

/** Ellenségek adatbázisa, a bemenetben adott azonosító alapján küld vissza egy ellenségstruktúrát **/
Enemy GetEnemy(Uint8 ID) {
    if(!Enemies[ID]) { /* Ha még nincs betöltve */
        Uint8 i, ModelID;
        Uint8 MovesBetween[2]; /* A két koordináta, ami közt mozoghat fel-le */
        Enemies[ID] = (Enemy*)malloc(sizeof(Enemy)); /* Memória foglalása új ellenségnek */
        const Uint8* enemyData = getEnemyData(ID);
        if(!enemyData) {
            Enemy Empty;
            memset(&Empty, 0, sizeof(Enemy)); /* Valóban semmi legyen az a semmi */
            return Empty;
        }
        ModelID = enemyData[0];
        Enemies[ID]->Model =
            ModelID +
            256; /* A modellre hivatkozás dinamikus legyen (lásd GetObject (graphics.c)) */
        Enemies[ID]->Size.x = Enemies[ID]->Size.y =
            0; /* A méret beolvasás után lesz meghatározva */
        Enemies[ID]->AnimCount = enemyData
            [1]; // fread(&Enemies[ID]->AnimCount, sizeof(Uint8), 7, ObjectData); /* Ameddig Uint8-ak vannak egymás után a struktúrában, egy művelettel olvasható mind */
        Enemies[ID]->Lives = enemyData[2];
        Enemies[ID]->Floats = enemyData[3];
        Enemies[ID]->ShotTime = enemyData[4];
        Enemies[ID]->MoveUp = enemyData[5];
        Enemies[ID]->MoveDown = enemyData[6];
        Enemies[ID]->MoveAnyway = enemyData[7];
        memcpy(
            MovesBetween,
            &enemyData[8],
            2 * sizeof(
                    Uint8)); // fread(MovesBetween, sizeof(Uint8), 2, ObjectData); /* A két értéket egyszerre olvassa ki az előző olvasás mintájára */
        Enemies[ID]->MovesBetween.x = MovesBetween
            [0]; /* Uint8-ak fájlából Sint16-okat csak így lehet mindennel kompatibilisen beállítani */
        Enemies[ID]->MovesBetween.y = MovesBetween[1];
        /* Méret meghatározása */
        for(i = ModelID; i < ModelID + Enemies[ID]->AnimCount;
            ++i) { /* Mindkét dimenzióból a legnagyobbat keresi az összes animációs fázisból */
            Object AnimPhase = GetObject(
                Enemies[ID]->Model); /* Ha a modellt egyszer beolvasta, többször nem fogja */
            if(Enemies[ID]->Size.x < AnimPhase.Size.x) Enemies[ID]->Size.x = AnimPhase.Size.x;
            if(Enemies[ID]->Size.y < AnimPhase.Size.y) Enemies[ID]->Size.y = AnimPhase.Size.y;
        }
    }
    return *Enemies[ID]; /* Beolvasott ellenség visszaadása */
}

/** Az összes dinamikusan foglalt ellenség felszabadítása **/
void FreeDynamicEnemies() {
    int i;
    for(i = 0; i < 256; i++) /* A tömbön végighaladva, */
        if(Enemies[i]) /* Ha talál betöltött dinamikus ellenséget, */
            free(Enemies[i]); /* Szabadítsa fel. */
}
