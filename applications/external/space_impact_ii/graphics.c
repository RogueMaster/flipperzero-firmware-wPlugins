#include <string.h>
#include "graphics.h"
#include "font.h"
#include "saves.h"
#include "game_objects.h"

/** Kirajzol egy objektumot, bemenetei: pixeltérkép, objektum, pozíció **/
void DrawObject(Uint8* PixelMap, Object obj, Vec2 pos) {
    int x = pos.x, ex = pos.x + obj.Size.x, y = pos.y,
        ey = pos.y + obj.Size.y; /* Kezdeti- és végkoordináták */
    int px, py; /* Pásztázáshoz használt változók */
    for(px = x; px < ex;
        px++) /* Képernyõ bejárása azon a téglalapon, ahová az objektum rajzolva lesz */
        for(py = y; py < ey; py++)
            if(obj.Samples
                   [(py - y) * obj.Size.x + px -
                    x]) /* Csak akkor kell rajzolni, ha aktív az objektum ide rajzolandó pixele, két okból: */
                /* 1. úgyis ki lesz ürítve a kép, 2. ne írjon felül inaktív pixel frissen aktívat */
                if(px < 84 && py < 48 && px >= 0 && py >= 0) /* Ne írjon a képernyõn túlra */
                    PixelMap[py * 84 + px] = 1;
}

/** Kirajzol egy objektumot körvonalazva, bemenetei: pixeltérkép, objektum, pozíció **/
void DrawOutlinedObject(Uint8* PixelMap, Object obj, Vec2 pos) {
    int x = pos.x, ex = pos.x + obj.Size.x, y = pos.y,
        ey = pos.y + obj.Size.y; /* Kezdeti- és végkoordináták */
    int px, py; /* Pásztázáshoz használt változók */
    for(px = x; px < ex;
        px++) { /* Első lépés: körberajzolás háttérszínnel, hogy eltűnjön, ami véletlenül már ott van */
        for(py = y; py < ey;
            py++) { /* Képernyõ bejárása azon a téglalapon, ahová az objektum rajzolva lesz */
            if(obj.Samples
                   [(py - y) * obj.Size.x + px - x]) { /* Csak létező pixeleket kell körberajzolni */
                if(px < 84 && py < 48 && px >= 0 && py >= 0) { /* Ne írjon a képernyõn túlra */
                    Uint16 PixelPos =
                        py * 84 + px; /* A körberajzolandó pixel helye a pixeltérképen */
                    if(py) { /* Felső sor */
                        if(px)
                            PixelMap[PixelPos - 85] =
                                0; /* Balra fent álló pixel átrajzolása, ha az létezik */
                        PixelMap[PixelPos - 84] =
                            0; /* Fent álló pixel átrajzolása, ha az létezik */
                        if(px != 83)
                            PixelMap[PixelPos - 83] =
                                0; /* Jobbra fent álló pixel átrajzolása, ha az létezik */
                    }
                    if(px)
                        PixelMap[PixelPos - 1] =
                            0; /* Balra álló pixel átrajzolása, ha az létezik */
                    if(px != 83)
                        PixelMap[PixelPos + 1] =
                            0; /* Jobbra álló pixel átrajzolása, ha az létezik */
                    if(py != 47) { /* Alsó sor */
                        if(px)
                            PixelMap[PixelPos + 85] =
                                0; /* Balra lent álló pixel átrajzolása, ha az létezik */
                        PixelMap[PixelPos + 84] =
                            0; /* Lent álló pixel átrajzolása, ha az létezik */
                        if(px != 83)
                            PixelMap[PixelPos + 83] =
                                0; /* Jobbra lent álló pixel átrajzolása, ha az létezik */
                    }
                }
            }
        }
    }
    DrawObject(PixelMap, obj, pos); /* Második lépés: valódi rajzolás */
}

/** Kirajzol egy kis számot, bemenetei: pixeltérkép, szám, számjegyek száma (a maradékot vezérnullákkal tölti ki), az utolsó számjegy pozíciója **/
void DrawSmallNumber(Uint8* PixelMap, Uint16 Num, Uint8 Digits, Vec2 LastDigit) {
    while(
        Digits--) { /* A számjegyek száma mindig a bemenetbõl jön, nincs dinamikus rajzolás az eredetiben sem */
        DrawObject(
            PixelMap,
            GetObject((Graphics)(Num % 10)),
            NewVec2(
                LastDigit.x,
                LastDigit
                    .y)); /* Utolsó számjegy kirajzolása, az enumerációban minden szám a neki megfelelõ helyen van */
        Num /= 10; /* Az utolsó számjegy elhagyása a következõ körhöz */
        LastDigit.x -= 4; /* Az utolsó számjegytõl kezdve visszafelé rajzol a funkció */
    }
}

/** Kiír egy szöveget, bemenetei: pixeltérkép, szöveg, bal felső pixel pozíciója, sorok magassága **/
void DrawText(Uint8* PixelMap, const char* Text, Vec2 Pos, int LineHeight) {
    Sint16 PosX =
        Pos.x; /* A bal behúzás tárolása sortörés esetére, mivel a Pos-t fogja módosítani a mozgáshoz */
    while(*Text) { /* Amíg a vizsgált karakter nem lezáró nulla */
        if(*Text == '\n') { /* Sortörés esetén mozgassa a kurzort a következő sor elejére */
            Pos.x = PosX;
            Pos.y += LineHeight;
        } else {
            DrawObject(
                PixelMap,
                NewObject(NewVec2(5, 8), SpaceFont[(unsigned char)*Text]),
                Pos); /* Jelenlegi karakter kirajzolása (a cast egy irreleváns figyelmeztetést szüntet meg) */
            Pos.x += 6; /* Ugrás a következő helyre a sorban */
        }
        ++Text; /* Ugrás a következő karakterre */
    }
}

/** Kirajzol egy görgetõsávot, elsõ bemenete a pixeltérkép, második bemenete százalékosan a görgetés pozíciója **/
void DrawScrollBar(Uint8* PixelMap, Uint8 Percent) {
    Uint16 i;
    Uint8 Row = Percent / 4 + 6; /* Az oszlop, amitõl a scrolljelzõ rajzolódik */
    for(i = 6 * 84 + 81; i < 39 * 84 + 81;
        i += 84) /* 6. és 38. sor közt a 81. oszlopban soronként */
        PixelMap[i] = 1; /* Elsõször a 3*32-es méretû sáv teljes baloldalát kiszínezi */
    InvertScreenPart(
        PixelMap,
        NewVec2(81, Row + 1),
        NewVec2(
            81,
            Row +
                5)); /* Invertálással eltünteti a jelzõ helyérõl a berajzolt függõleges vonalat */
    DrawObject(
        PixelMap, GetObject(gScrollMark), NewVec2(81, Percent / 4 + 6)); /* Kirajzolja a jelzõt */
}

/** A teljes képernyõ invertálása, bemenete a pixeltérkép **/
void InvertScreen(Uint8* PixelMap) {
    int i;
    for(i = 0; i < 4032; ++i) /* Mind a 84x48 pixel bejárása és invertálása */
        PixelMap[i] = 1 - PixelMap[i];
}

/** A képernyõ részét invertálja, elsõ bemenete a pixeltérkép, ezt követik a kezdeti-, majd a végkoordináták **/
void InvertScreenPart(Uint8* PixelMap, Vec2 From, Vec2 To) {
    Sint8 yFrom =
        From.y; /* A belsõ ciklust mindig elölrõl kell kezdeni, ezért el kell tárolni a kezdeti értékét */
    for(; From.x <= To.x; ++From.x) /* Bejárás a kezdet és cél között mindkét tengelyen */
        for(From.y = yFrom; From.y <= To.y; From.y++) {
            Uint8* Pixel = PixelMap + From.y * 84 + From.x; /* Az adott pixel memóriacíme */
            *Pixel = 1 - *Pixel;
        }
}

/** Pixeltérképek: néhány objektum pixeltérképe a játékba van fordítva, ilyeneket a szerkesztőből lehet generálni */
Uint8 /* Hardcode-olt pixeltérképek */
    /* 3x5-ös számok */
    pmNum[10][15] =
        {{1, 1, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 1, 1},
         {0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1},
         {1, 1, 1, 0, 0, 1, 1, 1, 1, 1, 0, 0, 1, 1, 1},
         {1, 1, 1, 0, 0, 1, 1, 1, 1, 0, 0, 1, 1, 1, 1},
         {1, 0, 1, 1, 0, 1, 1, 1, 1, 0, 0, 1, 0, 0, 1},
         {1, 1, 1, 1, 0, 0, 1, 1, 1, 0, 0, 1, 1, 1, 1},
         {1, 1, 1, 1, 0, 0, 1, 1, 1, 1, 0, 1, 1, 1, 1},
         {1, 1, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1},
         {1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1},
         {1, 1, 1, 1, 0, 1, 1, 1, 1, 0, 0, 1, 1, 1, 1}},
    pmSpace[804], pmIntro[531],
    pmImpact[912], /* Ezek a pixeltérképek tömörítve lettek. Csak 256-nál nagyobb pixeltérképek tömörítésével csökken az alkalmazásméret. */
    cmSpace[101] = {15,  255, 63,  248, 127, 131, 252, 127, 227, 255, 199, 255, 159, 249, 255,
                    143, 252, 120, 0,   224, 231, 15,  60,  3,   192, 30,  0,   56,  28,  225,
                    207, 0,   120, 3,   192, 7,   3,   28,  57,  224, 15,  0,   255, 240, 255,
                    231, 255, 60,  1,   255, 143, 255, 31,  248, 255, 231, 0,   63,  224, 3,
                    231, 192, 28,  121, 224, 15,  128, 0,   124, 248, 7,   143, 60,  1,   240,
                    0,   15,  31,  0,   241, 231, 128, 62,  1,   255, 227, 224, 28,  56,  255,
                    231, 255, 127, 240, 248, 7,   143, 7,   249, 255, 12},
    cmIntro[65] = {0,  0,   0,   0,   0,  0,   45,  193, 128, 0,   0,   14,  0,
                   2,  244, 27,  0,   0,  2,   120, 0,   191, 168, 240, 0,   0,
                   62, 156, 11,  219, 41, 128, 0,   9,   228, 242, 220, 163, 192,
                   0,  0,   224, 125, 0,  1,   176, 0,   0,   32,  19,  192, 0,
                   96, 0,   0,   0,   1,  192, 0,   0,   0,   0,   0,   0,   2},
    cmImpact[114] = {31,  31,  135, 207, 252, 63,  193, 254, 127, 241, 225, 252, 252, 255, 231,
                     254, 127, 231, 255, 30,  31,  255, 204, 30,  225, 231, 128, 7,   131, 225,
                     255, 249, 193, 206, 28,  240, 0,   240, 62,  63,  255, 156, 24,  225, 207,
                     0,   15,  3,   195, 206, 241, 255, 159, 252, 240, 0,   240, 60,  60,  207,
                     31,  241, 255, 206, 0,   15,  3,   195, 192, 227, 192, 28,  121, 224, 1,
                     224, 60,  120, 14,  60,  3,   199, 158, 0,   30,  7,   135, 129, 227, 192,
                     60,  121, 224, 1,   224, 120, 120, 30,  60,  3,   199, 31,  252, 30,  15,
                     143, 1,   231, 128, 120, 240, 127, 131, 192},
    pmScrollMark[21] = {1, 1, 0, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 1, 1},
    pmDotEmpty[12] = {0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1}, /* Üres pont a rekordképernyőre */
    pmDotFull[12] = {0, 1, 1, 0, 1, 1, 1, 1, 0, 1, 1}, /* Teli pont a rekordképernyőre */
    pmLife[25] = {1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 0, 0, 0, 1}, /* Élet */
    pmMissileIcon[25] = {0, 0, 0, 0, 0, 1, 0, 1, 1, 0, 1, 1, 1, 1, 1, 1, 0, 1, 1}, /* Rakéta ikon */
    pmBeamIcon[25] = {0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1}, /* Sugár ikon */
    pmWallIcon[25] = {0, 1, 1, 1, 0, 1, 1, 0, 1, 1, 1, 1,
                      0, 1, 1, 1, 1, 0, 1, 1, 0, 1, 1, 1}, /* Fal ikon */
    pmShot[3] = {1, 1, 1},
    pmExplosion[2][25] = {
        {0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1},
        {0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 1, 0, 0, 0, 1}};

Object StaticObject[] = {
    /* Statikus objektumok, az elemekre a Graphics enumeráció tárol neveket */
    {{3, 5}, pmNum[0]}, /* gNum0 */
    {{3, 5}, pmNum[1]}, /* gNum1 */
    {{3, 5}, pmNum[2]}, /* gNum2 */
    {{3, 5}, pmNum[3]}, /* gNum3 */
    {{3, 5}, pmNum[4]}, /* gNum4 */
    {{3, 5}, pmNum[5]}, /* gNum5 */
    {{3, 5}, pmNum[6]}, /* gNum6 */
    {{3, 5}, pmNum[7]}, /* gNum7 */
    {{3, 5}, pmNum[8]}, /* gNum8 */
    {{3, 5}, pmNum[9]}, /* gNum9 */
    {{67, 12}, pmSpace}, /* gSpace */
    {{59, 9}, pmIntro}, /* gIntro */
    {{76, 12}, pmImpact}, /* gImpact */
    {{3, 7}, pmScrollMark}, /* gScrollMark */
    {{4, 3}, pmDotEmpty}, /* gDotEmpty */
    {{4, 3}, pmDotFull}, /* gDotFull */
    {{5, 5}, pmLife}, /* gLife */
    {{5, 5}, pmMissileIcon}, /* gMissileIcon */
    {{5, 5}, pmBeamIcon}, /* gBeamIcon */
    {{5, 5}, pmWallIcon}, /* gWallIcon */
    {{3, 1}, pmShot}, /* gShot */
    {{5, 5}, pmExplosion[0]}, /* gExplosionA1 */
    {{5, 5}, pmExplosion[1]}, /* gExplosionA2 */
};

Object* DynamicObject[256] = {
    NULL}; /* 256 dinamikus objektum helye, amit fájlból tölt be a játék, ha hivatkozik rájuk bármi */

/** Tömörített pixeltérkép helyben kibontása, bemenetei: pixeltérkép, pixelek száma, tömörített bájtok száma **/
void UncompressPixelMap(Uint8* PixelMap, Uint16 Pixels, Uint16 Bytes) {
    Uint8 Bits =
        Pixels %
        8; /* Visszafelé fog haladni, az utolsó bájt viszont nem 8 pixelnyi információt tartalmaz, hanem a maradékot */
    if(Bits == 0) /* Kivéve, ha 8-cal osztható az objektum mérete */
        Bits = 8; /* Mert akkor tényeg 8 pixelnyi adatunk van */
    while(Bytes--) { /* Amíg van kibontatlan beolvasott bájt */
        while(Bits--) { /* Amíg a vizsgált bájt tartalmaz információt */
            PixelMap[Bytes * 8 + Bits] =
                PixelMap[Bytes] % 2; /* A bit elhelyezése a pixeltérképben */
            if(Bytes != 0 ||
               Bits !=
                   0) /* Az utolsó pixelre ez ne fusson le, különben a bal felső sarkat nullázná */
                PixelMap[Bytes] >>= 1; /* A feldolgozott bit levágása */
        }
        Bits = 8; /* Visszafelé haladva már biztosan 8 pixelnyi adat van minden bájtban */
    }
}

/** Egy tömörített objektum kibontása, bemenetei: tömörített tömb és mérete, cél pixeltérkép és mérete **/
void UncompressObject(Uint8* Compressed, int CompressedSize, Uint8* Container, int ContainterSize) {
    memcpy(Container, Compressed, CompressedSize); /* Tömörített adat átmásolása */
    UncompressPixelMap(Container, ContainterSize, CompressedSize); /* És helyben kibontása */
}

/** Tömörített objektumok kibontása **/
void UncompressObjects() {
    UncompressObject(cmSpace, sizeof(cmSpace), pmSpace, sizeof(pmSpace));
    UncompressObject(cmIntro, sizeof(cmIntro), pmIntro, sizeof(pmIntro));
    UncompressObject(cmImpact, sizeof(cmImpact), pmImpact, sizeof(pmImpact));
}

/** Egy objektum grafikai struktúráját adja vissza, ami a méreteit és a pixeltérkép memóriacímét tartalmazza, bemenete az objektum azonosítója **/
Object GetObject(Uint16 ObjectID) {
    if(ObjectID < 256) /* Ha hardcode-olt az objektum */
        return StaticObject
            [ObjectID]; /* Egyszerűen a statikus objektumok ID-edik helyét küldje vissza */
    /** Dinamikus objektumok beolvasása **/
    else { /* 256-tól 511-ig a dinamikusak jönnek */
        ObjectID %=
            256; /* Innentől a dinamikus objektum azonosítója kell, a modulo egy esetleges túlindexelést véd */
        if(!DynamicObject[ObjectID]) { /* Ha még nincs betöltve */
            Uint8 Size[2];
            Uint8* NewPixelMap;
            Uint16 Pixels, Bytes;
            /** Fájl megnyitása **/
            const Uint8* ObjectData = getObjectData(ObjectID);
            if(!ObjectData) /* Ha nem talált fájlt, vagy nem fért hozzá */
                return NewObject(NewVec2(0, 0), NULL); /* Adjon vissza üres objektumot */
            /** Beolvasás **/
            memcpy(
                Size,
                ObjectData,
                2 * sizeof(
                        Uint8)); /* Olvassa be az objektum méreteit, mintha egy Uint8-akból álló Vec2 lenne */

            Pixels = (Uint16)Size[0] * (Uint16)Size[1]; /* Az objektum pixelszáma, X * Y */
            Bytes = Pixels / 8 + (Pixels % 8 != 0); /* A kibontatlan objektum bájtjai */
            NewPixelMap = (Uint8*)malloc(Pixels); /* Pixeltérkép lefoglalása */
            memcpy(NewPixelMap, &ObjectData[2], Bytes * sizeof(Uint8));
            UncompressPixelMap(NewPixelMap, Pixels, Bytes); /* Kibontás helyben */
            /** Tárolás **/
            DynamicObject[ObjectID] =
                (Object*)malloc(sizeof(Object)); /* Memória foglalása új objektumnak */
            DynamicObject[ObjectID]->Size.x = Size[0]; /* Szélesség eltárolása */
            DynamicObject[ObjectID]->Size.y = Size[1]; /* Magasság eltárolása */
            DynamicObject[ObjectID]->Samples = NewPixelMap; /* Minták eltárolása */
        }
        return *DynamicObject[ObjectID];
    }
    return NewObject(NewVec2(0, 0), NULL); /* Ha dinamikus sem volt, akkor semmi */
}

/** Az összes dinamikusan foglalt objektum felszabadítása **/
void FreeDynamicGraphics() {
    int i;
    for(i = 0; i < 256; ++i) { /* A tömbön végighaladva, */
        if(DynamicObject[i]) { /* Ha talál betöltött dinamikus objektumot, */
            free(DynamicObject[i]->Samples); /* Szabadítsa fel előbb a pixeltérképét, */
            free(DynamicObject[i]); /* Majd őt magát. */
        }
    }
}

/** Visszaad egy új vektort, hogy el lehessen kerülni a pedantic "ISO C90 forbids compound literals" figyelmeztetését **/
Vec2 NewVec2(Sint16 x, Sint16 y) {
    Vec2 ret;
    ret.x = x;
    ret.y = y;
    return ret;
}

/** Visszaad egy új objektumot, hogy el lehessen kerülni a pedantic "ISO C90 forbids compound literals" figyelmeztetését **/
Object NewObject(Vec2 Size, Uint8* Samples) {
    Object ret;
    ret.Size = Size;
    ret.Samples = Samples;
    return ret;
}
