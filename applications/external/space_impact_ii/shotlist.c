#include "graphics.h"
#include "shotlist.h"

/** Lövéstípusok adatai **/
/* Azért nincs struktúrában, mert annak a megvalósítása növeli a fájlméretet */
Uint8 ShotDamages[4] = {1, 3, 10, 25}; /* Sebzés */
Vec2 ShotSizes[4] = {{3, 1}, {5, 3}, {84, 3}, {1, 43}}; /* Méret */

/** Lövés hozzáadása a paraméterben kapott láncolt listához, pozícióban, sebességgel, egy igaz-hamis értékkel, hogy a játékostól van-e, valamint típussal **/
void AddShot(ShotList* Shots, Vec2 Pos, Sint8 v, Uint8 FromPlayer, WeaponKind Kind) {
    Shot* CreateAt = *Shots; /* A létrehozási memóriahely, ahova íródik a lista új utolsó eleme */
    if(*Shots) { /* Ha van már lista, meg kell keresni a végét, és ott létrehozni az új elemet */
        while(CreateAt->Next) /* Az új cím a listán végiglépked az utolsó elemig */
            CreateAt = CreateAt->Next;
        CreateAt->Next = (Shot*)malloc(
            sizeof(Shot)); /* Az utolsó elem a semmi helyett az újonnan lefoglalt helyre mutat */
        CreateAt = CreateAt->Next; /* Az íráshoz használt cím kerüljön a változóba */
    } else /* Ha nincs még lista, a pointerre kell lefoglalni a címet */
        CreateAt = *Shots = (Shot*)malloc(sizeof(Shot));
    /* Bemenetként kapott adatok átadása az új elemnek */
    CreateAt->Pos = Pos;
    CreateAt->v = v;
    CreateAt->FromPlayer = FromPlayer;
    CreateAt->Kind = Kind;
    /* A típusra jellemzõ tulajdonságok beállítása típustól függõen */
    CreateAt->Damage = ShotDamages[Kind];
    CreateAt->Size = ShotSizes[Kind];
    CreateAt->Next = NULL; /* Nincs következõ elem, lista vége jel */
}

/** A paraméterben kapott lövéslista ürítése **/
void EmptyShotList(ShotList* Shots) {
    Shot* Last = *Shots; /* Utolsó elem címének tárolása a törléshez */
    while(*Shots) { /* Ameddig van még elem */
        *Shots = (*Shots)->Next; /* A pointer lépjen a következõ elemre */
        free(Last); /* Az elõzõleg vizsgált elem felszabadítása */
        Last = *Shots; /* Most már ez az utoljára vizsgált elem */
    }
    *Shots = NULL; /* Ne mutasson sehova, mert nincs semmi lefoglalva */
}

/** A bemenetben kapott listáról a bemenetben kapott címen található elemet eltávolítja **/
void RemoveShot(ShotList* Shots, Shot* Address) {
    Shot *Checked = *Shots,
         *Last = *Shots; /* Az elsõ változó a jelenleg vizsgált cím, a második az azelõtti */
    while(Checked) { /* Ameddig van mit nézni */
        if(Checked == Address) { /* Ha megvan az egyezés */
            Shot* Current = Checked; /* A jelenlegi memóriacím */
            if(Checked ==
               *Shots) /* Ha a lista elsõ elemét kell törölni, akkor a lista pointerjét át kell helyezni a második elemre */
                *Shots = (*Shots)->Next;
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

/** Két téglalap adatait kéri be kezdõpozíció és méret formájában, majd igaz-hamis értéket ad, hogy van-e közös pontjuk **/
Uint8 Intersect(Vec2 Start1, Vec2 Size1, Vec2 Start2, Vec2 Size2) {
    /* Akkor fedi egymást a két téglalap, ha nem fordul elõ, hogy 1-es a 2-estõl teljesen balra/jobbra/fel/le van */
    return !(
        Start1.x > Start2.x + Size2.x - 1 || Start1.y > Start2.y + Size2.y - 1 ||
        Start1.x + Size1.x - 1 < Start2.x || Start1.y + Size1.y - 1 < Start2.y);
}

/** A bemenetben kapott lövéslista kezelése, a második bemenetben kapott pixeltérképre rajzolja ki õket **/
void ShotListTick(ShotList* Shots, Uint8* PixelMap, PlayerObject* Player) {
    Shot* Current = *Shots; /* A jelenleg vizsgált lövés, ez járja be a listát */
    while(Current) { /* Amíg vannak a láncolt listán lövések */
        Uint8 HitPlayer = !Current->FromPlayer &&
                          Intersect(
                              Current->Pos,
                              Current->Size,
                              Player->Pos,
                              NewVec2(10, 7)); /* Eltalálta-e a játékost ellenséges lövés */
        Uint8 HitOther = 0; /* Eltalált-e egy másik lövedéket */
        Shot* CheckForHit =
            *Shots; /* Ezzel megy végig a láncolt listán, újra az összes lövedéken */
        Current->Pos.x +=
            Current->v; /* A lövés mozogjon a megadott irányba, a megadott sebességgel */
        if(HitPlayer &&
           !Player->Protection) /* Találatnál fogy a játékos élete, ha nincs védelme */
            Player->Lives--;
        while(
            CheckForHit &&
            !HitOther) { /* Ameddig nem talált el semmit, nézze tovább a listát (biztos, hogy egy lövedék csak egy másikat találhat el a gyári pájadizájnnal) */
            HitOther =
                CheckForHit != Current &&
                !(CheckForHit->FromPlayer &&
                  Current
                      ->FromPlayer) && /* Saját magát nem találhatja el, és a játékos két lövedéke sem ütheti egymást */
                Intersect(
                    Current->Pos,
                    Current->Size,
                    CheckForHit->Pos,
                    CheckForHit->Size); /* Ütközés detektálása */
            if(HitOther) { /* Ha találat van */
                Current->Damage--; /* A jelenlegi lövedék hátralévő sebzése nő */
                CheckForHit->Damage--; /* Az eltalált lövedéké is */
                if(CheckForHit->Damage ==
                   0) /* Az eltalált lövedék itt törlődik, ha elfogyott a sebzése, a jelenleginek külön blokkja van */
                    RemoveShot(Shots, CheckForHit);
            }
            CheckForHit = CheckForHit->Next; /* A lista továbbvizsgálata */
        }
        if(Current->Pos.x < -2 || Current->Pos.x > 83 || HitPlayer ||
           Current->Damage ==
               0) { /* Ha kiment a képernyõrõl, eltalálta a játékost, vagy egy másik lövedék lesebezte, törölni kell */
            Shot* Remove = Current; /* A törlendő elem */
            Current =
                Current
                    ->Next; /* Következő körben folytatás a láncolt lista következõ elemével, mielőtt törlődik a listaelem */
            RemoveShot(Shots, Remove); /* Lövés törlése */
        } else {
            /* A második paraméter egy felszámolt switch, ami a négyféle lövedékobjektum közül választja ki a megfelelőt */
            DrawObject(
                PixelMap,
                GetObject(Current->Kind == Standard ? gShot : G_MISSILE + Current->Kind - Missile),
                Current->Pos);
            Current = Current->Next; /* A következõ elemet vizsgálja a következõ körben */
        }
    }
}
