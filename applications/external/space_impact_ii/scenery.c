#include <furi_hal_random.h>
#include "enemies.h"
#include "graphics.h"
#include "scenery.h"
#include "shotlist.h"

/** Tájadat struktúra **/
typedef struct SceneryData {
    Uint16 FirstObject; /* Az első objektum grafikai azonosítója */
    Uint8 Objects; /* Az objektumok száma, ennyi tartozik a szinthez az előzőleg megadott azonosítótól kezdve */
    Uint8 Upper; /* Fent jelenik meg */
} SceneryData;

/** A szintekhez tartozó tájadatok **/
SceneryData ScData[6] = {
    {0, 0, 0}, /* Az 1. szinten nincs táj */
    {256 + 0, 2, 0}, /* 2. szint, 0. dinamikus helytől 2 elemű, 700 pixel széles táj */
    {256 + 2, 6, 0}, /* 3. szint, 2. dinamikus helytől 6 elemű, 750 pixel széles táj */
    {256 + 8, 6, 0}, /* 4. szint, 8. dinamikus helytől 6 elemű, 1000 pixel széles táj */
    {256 + 14, 4, 1}, /* 5. szint, 14. dinamikus helytől 4 elemű, 1250 pixel széles felső táj */
    {256 + 14, 4, 1}, /* 6. szint, az 5. szint elemeiből, 1600 pixel szélesen */
};

/** Tájelem hozzáadása a paraméterben kapott listához, grafikai azonosítóval, pozícióban **/
void AddScenery(SceneryList* List, Graphics Model, Vec2 Pos) {
    Scenery* CreateAt =
        *List; /* A létrehozási memóriahely, ahova íródik a lista új utolsó eleme */
    if(*List) { /* Ha van már lista, meg kell keresni a végét, és ott létrehozni az új elemet */
        while(CreateAt->Next) /* Az új cím a listán végiglépked az utolsó elemig */
            CreateAt = CreateAt->Next;
        CreateAt->Next = (Scenery*)malloc(sizeof(
            Scenery)); /* Az utolsó elem a semmi helyett az újonnan lefoglalt helyre mutat */
        CreateAt = CreateAt->Next; /* Az íráshoz használt cím kerüljön a változóba */
    } else /* Ha nincs még lista, a pointerre kell lefoglalni a címet */
        CreateAt = *List = (Scenery*)malloc(sizeof(Scenery));
    /* Bemenetként kapott adatok átadása az új elemnek */
    CreateAt->Model = Model;
    CreateAt->Pos = Pos;
    CreateAt->Next = NULL; /* Nincs következő elem, lista vége jel */
}

/** A paraméterben kapott táj törlése **/
void EmptyScenery(SceneryList* List) {
    Scenery* Last = *List; /* Utolsó elem címének tárolása a törléshez */
    while(*List) { /* Ameddig van még elem */
        *List = (*List)->Next; /* A pointer lépjen a következő elemre */
        free(Last); /* Az előzőleg vizsgált elem felszabadítása */
        Last = *List; /* Most már ez az utoljára vizsgált elem */
    }
    *List = NULL; /* Ne mutasson sehova, mert nincs semmi lefoglalva */
}

/** A tájat kirajzoló és a listáját kezelõ funkció, bemenete a táj láncolt listája, a pixeltérkép, ahova rajzolni fog,
    valamint egy igaz-hamis érték, ami azt jelzi, hogy a táj mozoghat-e tovább, vagy sem */
void HandleScenery(
    SceneryList* List,
    Uint8* PixelMap,
    Uint8 Move,
    PlayerObject* Player,
    Sint8 Level) {
    Scenery* First =
        *List; /* Az elsõ elem tárolása, mert a pointer módosul a lista bejárásával, ezért vissza kell majd állítani */
    Sint16 LastX = 0; /* Az utolsó rajzolt képpont */
    Object Model; /* Az aktuálisan kirajzolt modell tárolója */
    while(*List) { /* Ameddig van elem a listán */
        if(Move) /* Ha fõellenség került a játékos elé, a táj megáll mozogni */
            (*List)->Pos.x--; /* Egyébként a táj balra úszik folyamatosan */
        Model = GetObject(
            (*List)->Model); /* Előre szükség van az objektum lekérésére, a mérete miatt */
        if(Level != 1 &&
           Intersect(
               (*List)->Pos,
               Model.Size,
               Player->Pos,
               NewVec2(
                   10,
                   7))) /* Ütközéskor sebződjön a játékos, védelmen át is, de a felhős szinten ne */
            Player->Lives--;
        if((*List)->Pos.x < -Model.Size.x) { /* Ha balra kiúszott az egész objektum */
            *List =
                (*List)
                    ->Next; /* Elõre ugorjon a következõ elemre, mielõtt felszadítaná a memóriahelyét */
            free(
                First); /* Nyugodtan lehet a kiindulási helyet felszabadítani, mert a tájat balról jobbra tölti fel a játék, így ha kiesik valamelyik, az biztos az elsõ */
            First = *List; /* Az új elsõ elem a lista következõ eleme legyen */
        } else { /* Ha a képernyõn van a listaelem */
            LastX =
                (*List)->Pos.x +
                Model.Size
                    .x; /* Utolsó pozíció tárolása, ha esetleg hozzá kellene fűzni egy tájelemet a végéhez */
            DrawObject(PixelMap, Model, (*List)->Pos); /* Aktuális tájelem kirajzolása */
            *List = (*List)->Next; /* Folytatás a következõ elemmel */
        }
    }
    if(Level != 0) {
        while(LastX < 84) { /* Amíg nincs teljesen végigrajzolva a képernyő */
            Object Model;
            Scenery* NewScenery = (Scenery*)malloc(sizeof(Scenery)); /* Adjon hozzá újat */
            NewScenery->Model =
                ScData[Level].FirstObject + furi_hal_random_get() % ScData[Level].Objects;
            /* Véletlenszerű grafikai azonosító választása a szint objektumai közül */
            Model = GetObject(NewScenery->Model); /* Az objektumra szükség lesz, a méretei miatt */
            NewScenery->Pos = NewVec2(
                LastX,
                ScData[Level].Upper ?
                    0 :
                    48 -
                        Model.Size
                            .y); /* Elhelyezés a jelenlegi elemek után, pályától függően fel vagy le */
            NewScenery->Next = NULL; /* Nincs következő elem, lista vége jel */
            LastX = NewScenery->Pos.x + Model.Size.x; /* Utolsó pozíció frissítése az új elemre */
            if(First == NULL) /* Ha nincs semmi a listán */
                First = NewScenery; /* Fűzze hozzá ezt a függvény utolsó sora segítségével */
            else {
                for(*List = First; (*List)->Next; *List = (*List)->Next)
                    ; /* Keresse meg a lista végét */
                (*List)->Next = NewScenery; /* És fűzze hozzá */
            }
        }
    }
    *List = First; /* Az új elsõ elem beállítása */
}
