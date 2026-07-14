#include <stdlib.h>
// #include <SDL_gfxPrimitives.h>
#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_random.h>
#include <gui/gui.h>
#include <input/input.h>
#include <stdlib.h>
#include <dolphin/dolphin.h>

#include "sdlint.h"
#include "config.h"
#include "enemies.h"
#include "graphics.h"
#include "saves.h"
#include "scenery.h"
#include "shotlist.h"
#include "audio.h"
#include "font.h"
#include "level_objects.h"

#define MENU_SCREEN_MAIN       -1
#define MENU_SCREEN_HIGH_SCORE -2
#define MENU_SCREEN_PAUSE      -3

typedef enum {
    EVENT_KEYDOWN,
    EVENT_KEYUP,
    EVENT_USER,
    EVENT_QUIT,
} EventType;

typedef enum {
    KEY_DOWN,
    KEY_UP,
    KEY_LEFT,
    KEY_RIGHT,
    KEY_BACK,
    KEY_SPACE,
    KEY_SPECIAL,
    KEY_ESCAPE, // Mapped to a long press on back key
} InputEventKey;

typedef struct {
    EventType type;
    InputEventKey key;
} GameEvent;

FuriMutex* mutex;
Uint8 PixelMap
    [84 * 48]; /* Az elõzõ és mostani képkocka, ahol a kettõ nem egyezik, azt kell újrarajzolni */

static void update_timer_callback(void* ctx) {
    furi_assert(ctx);
    FuriMessageQueue* event_queue = ctx;

    GameEvent event = {.type = EVENT_USER};
    furi_message_queue_put(event_queue, &event, 0);
}

static void render_callback(Canvas* const canvas, void* ctx) {
    furi_assert(ctx);
    furi_mutex_acquire(mutex, FuriWaitForever);
    canvas_draw_frame(canvas, 19, 5, 88, 52);
    for(size_t i = 0; i < 84 * 48; i++) {
        if(PixelMap[i] != 0) canvas_draw_dot(canvas, 21 + i % 84, 7 + i / 84);
    }
    furi_mutex_release(mutex);
}

static void input_callback(InputEvent* input_event, void* ctx) {
    furi_assert(ctx);
    FuriMessageQueue* event_queue = ctx;
    GameEvent event;

    switch(input_event->key) {
    case InputKeyUp:
        event.key = KEY_UP;
        break;
    case InputKeyDown:
        event.key = KEY_DOWN;
        break;
    case InputKeyRight:
        event.key = KEY_RIGHT;
        break;
    case InputKeyLeft:
        event.key = KEY_LEFT;
        break;
    case InputKeyOk:
        event.key = KEY_SPACE;
        break;
    case InputKeyBack:
        event.key = KEY_SPECIAL;
        break;
    default:
        return;
    }

    switch(input_event->type) {
    case InputTypePress:
        event.type = EVENT_KEYDOWN;
        break;
    case InputTypeRelease:
        event.type = EVENT_KEYUP;
        break;
    case InputTypeLong:
        if(input_event->key == InputKeyBack) {
            event.type = EVENT_KEYDOWN;
            event.key = KEY_ESCAPE;
        }
        break;
    default:
        return;
    }
    furi_message_queue_put(event_queue, &event, FuriWaitForever);
}

int app_entry(void* p) {
    UNUSED(p);

    mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    if(mutex == NULL) {
        FURI_LOG_E("Space Impact II", "cannot create mutex\r\n");
        return 255;
    }
    ViewPort* view_port = view_port_alloc();
    FuriMessageQueue* event_queue = furi_message_queue_alloc(8, sizeof(GameEvent));
    view_port_draw_callback_set(view_port, render_callback, NULL);
    view_port_input_callback_set(view_port, input_callback, event_queue);
    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);

    /** SDL inicializálás **/
    // TODO: Do audio
    // SDL_AudioSpec audio; /* SDL hangspecifikáció */
    Sint32 AudioFlags = 0; /* Lejátszandó hangok flag-jei */ // TODO: Unused

    /** A játékhoz szükséges változók **/
    Uint8 run = 1; /* Kilépéskor hamisra vált, és a fõciklus kilép */
#ifdef LEGACY_TOP_SCORE
    int j; /* Három iterátorváltozó, késõbb szükség lesz mindre */
#endif /* LEGACY_TOP_SCORE */
    int i; /* Három iterátorváltozó, késõbb szükség lesz mindre */
    // Uint8 OldPixelMap[84 * 48];
    Uint8 IntroPhase = 12; /* A bevezető animációból hátralévő kockák */
    Uint8 FrameHold =
        3; /* Ennyi képkockán át még tartsa a jelenlegit (csak a bevezető animációra érvényes) */
    PlayerObject Player; /* A játékos struktúrája */
    Sint8 Level = -1; /* Szint azonosítója, ha negatív, akkor menü */
    Uint8 MenuItem = 1; /* A menüben kiválasztott lehetõség */
    Uint8 SavedLevel = 0; /* Utoljára félbehagyott pálya */
    Uint8 PlayerUp = 0, PlayerDown = 0, PlayerLeft = 0, PlayerRight = 0, PlayerShooting = 0;
    Uint8 PlayerShootTimer = 0;
    Uint8 LevelCount = 0; /* Hány szintet tartalmaz a játék adatmappája? */
    const Uint8* LastLevel = getLevelData(0);
    Shot* Shots = NULL; /* Egy láncolt lista, az összes lövést tartalmazza */
    EnemyList* Enemies = NULL; /* Ellenségek láncolt listája */
    Uint8 AnimPulse = 0; /* Animációk idõzítõje */
    Scenery* Scene = NULL; /* A táj objektumainak láncolt listája, az elsõ szinten még nincs */
    Uint8 MoveScene; /* A táj mozgatása - a szintek végén mindig megáll */
#ifdef LEGACY_TOP_SCORE
    Uint8 TimeInScores = 0;

#endif /* LEGACY_TOP_SCORE */
    unsigned int TopScores
        [SCORE_COUNT]; /* A 10 legjobb pontszám, azért unsigned int, mert a Uint16-ra a %hu formátumjelző kellene, ami nem ANSI C */
    memset(TopScores, 0, sizeof(TopScores)); /* Alapértelmezetten mind 0 */
    ReadSavedLevel(&SavedLevel); /* Ha lett elmentve utolsó szint, olvassa be */
    ReadTopScore(TopScores); /* Ha vannak mentett rekordok, olvassa be őket */
    while(LastLevel) {
        ++LevelCount; /* Ugrás a következő szintre */
        LastLevel = getLevelData(LevelCount);
    }

    // TODO: Do audio
    /** Hang inicializálása **/
    // audio.freq = SAMPLE_RATE; /* Mintavételezés az alapértelmezett frekvencián */
    // audio.format = AUDIO_S16; /* Előjeles 16 bites egész minták */
    // audio.channels = 1; /* Monó hang */
    // audio.samples = 1024; /* Ennyi mintára hívja meg a hangkezelőt */
    // audio.callback = AudioCallback; /* Hangkezelő függvény */
    // audio.userdata = &AudioFlags; /* A hangkezelő függvénynek folyamatosan átadott paraméter */
    // if (SDL_OpenAudio(&audio, NULL) >= 0) /* Ha sikerül inicializálni a hangot */
    //     SDL_PauseAudio(0); /* Indítsa is el */

    /** Pixeltérképek kibontása **/
    UncompressFont();
    UncompressObjects();

    /** Fõ loop **/
    furi_hal_random_init(); /* Legyen a random bónusz tényleg random */
    FuriTimer* timer = furi_timer_alloc(update_timer_callback, FuriTimerTypePeriodic, event_queue);
    furi_timer_start(timer, furi_kernel_get_tick_frequency() / FRAMERATE);
    GameEvent event;
    while(run) {
        FuriStatus event_status = furi_message_queue_get(event_queue, &event, FuriWaitForever);
        if(event_status != FuriStatusOk) continue;
        furi_mutex_acquire(mutex, FuriWaitForever);

        switch(event.type) {
        /** Billentyûleütések **/
        case EVENT_KEYDOWN:
            if(IntroPhase) { /* Bevezető animáció félbeszakítása, ha megy */
                IntroPhase = 0;
                AudioFlags |=
                    SOUND_MENUBTN; /* Bármilyen gombra adjon ki hangot, még ami nem is csinál semmit */
#ifdef PAUSE
                /** Szünet képernyő **/
            } else if(Level == MENU_SCREEN_PAUSE) {
                if(event.key == KEY_SPACE) {
                    Level = MenuItem == 1 ? SavedLevel : MENU_SCREEN_MAIN;
                } else if(event.key == KEY_UP || event.key == KEY_DOWN)
                    MenuItem = 3 - MenuItem;
                else if(event.key == KEY_ESCAPE || event.key == KEY_SPECIAL)
                    Level = MENU_SCREEN_MAIN;
#endif /* PAUSE */
                /** Rekord- és játék vége képernyõ **/
            } else if(Level == MENU_SCREEN_HIGH_SCORE || Level == LevelCount) {
                if(event.key == KEY_ESCAPE || event.key == KEY_SPECIAL) Level = -1;
                AudioFlags |=
                    SOUND_MENUBTN; /* Bármilyen gombra adjon ki hangot, még ami nem is csinál semmit */
                /** Fõmenü **/
            } else if(Level == MENU_SCREEN_MAIN) {
                AudioFlags |=
                    SOUND_MENUBTN; /* Bármilyen gombra adjon ki hangot, még ami nem is csinál semmit */
                if(event.key == KEY_SPACE) { /* Enter */
                    if(SavedLevel == 0)
                        MenuItem++; /* A menü elemeit a folytatással kezdõdõ menühöz viszonyítja (új játék 1. hely helyett a 2., stb.) */
                    if(MenuItem == 3) {
                        Level = MENU_SCREEN_HIGH_SCORE; /* Ez a rekordképernyõ azonosítõja */
#ifdef LEGACY_TOP_SCORE
                        TimeInScores = 0; /* Kezdje elölről a rekordképernyő animációját */
#endif /* LEGACY_TOP_SCORE */
                        if(SavedLevel ==
                           0) /* Mivel a continue nélküli képernyõnél a menüelem azonosítója nõtt, vissza kell állítani */
                            MenuItem = 2;
                    } else { /* Valamilyen szintet betöltõ opció lett kiválasztva */
                        if(MenuItem == 1) /* Folytatás */
                            Level = SavedLevel; /* Mentett szint betöltése */
                        else /* Új játék */
                            Level = 0; /* 1. szint betöltése */
                        EmptyShotList(
                            &Shots); /* Lövéslista ürítése (hátha maradt az előző játékból) */
                        /* Játékos alaphelyzetbe állítása (még folytatás esetén is, egyedül a pálya van mentve) */
                        Player.Lives = 3; /* 3 élet */
                        Player.Score = 0;
                        Player.Bonus = 3; /* Az új játékok mindig három rakétával indulnak */
                        Player.Weapon = Missile;
                        Player.Pos = NewVec2(3, 20); /* A játékos kezdõpozíciója */
                        Player.Protection = 50; /* Kezdeti védelem */
                        PlayerShootTimer = 0; /* Hideg fegyverrel kezdés */
                        /* Játékos akcióinak nullázása, ha esetleg a játékos nem engedte fel a gombokat az Esc megnyomása elõtt */
                        PlayerUp = PlayerDown = PlayerLeft = PlayerRight = PlayerShooting = 0;
                        LevelSpawner(&Enemies, Level); /* Szint benépesítése */
                        EmptyScenery(
                            &Scene); /* Táj ürítése, hátha egy esetleges elõzõ szintrõl maradt benne valami */
                        MoveScene = 1; /* Tájmozgatás újraindítása */
                    }
                } else if(event.key == KEY_ESCAPE || event.key == KEY_SPECIAL)
                    run = 0;
                else if(event.key == KEY_UP) /* Fel nyíl: elõzõ menüelem, körkörösen */
                    MenuItem = MenuItem == 1 ? (SavedLevel ? 3 : 2) : (MenuItem - 1);
                else if(event.key == KEY_DOWN) /* Le nyíl: következõ menüelem, körkörösen */
                    MenuItem = MenuItem % (SavedLevel ? 3 : 2) + 1;
                /** Játék **/
            } else {
#ifdef GHOSTING
                if(!PlayerUp && !PlayerDown && !PlayerLeft && !PlayerRight && !PlayerShooting)
#endif /* GHOSTING */
                    switch(event.key) {
                    /* Nyílgombok: játékos mozgatása */
                    case KEY_UP:
                        PlayerUp = 1;
                        break;
                    case KEY_DOWN:
                        PlayerDown = 1;
                        break;
                    case KEY_LEFT:
                        PlayerLeft = 1;
                        break;
                    case KEY_RIGHT:
                        PlayerRight = 1;
                        break;
                    case KEY_SPACE:
                        PlayerShooting = 1;
                        break; /* Space: tûz */
                    case KEY_ESCAPE:
                        SavedLevel = Level; /* Szint mentése */
                        SaveLevel(
                            Level); /* Mentse el, hogy erről a szintről lépett ki a játékos */
#ifdef PAUSE
                        Level = MENU_SCREEN_PAUSE; /* Szünet menübe lépés */
#else
                    Level = MENU_SCREEN_MAIN; /* Menübe lépés */
#endif /* PAUSE */
                        MenuItem = 1; /* A menü elsõ elemre állítása */
                        break;
                    default:
                        break;
                    }
            }
            break;
        case EVENT_KEYUP: /* Nyomva tartott billentyûk felengedése esetén a nyomva tartást rögzítõ változók nullázása */
            /* Ilyenek csak játék alatt vannak, de nem kell ezt ellenõrizni, mert máshol nincs hatása az értékeknek */
            /* Egyébként ez switch volt, csak attól 4 bájttal nőne a generált kód */
            if(event.key == KEY_UP) PlayerUp = 0;
            if(event.key == KEY_DOWN) PlayerDown = 0;
            if(event.key == KEY_LEFT) PlayerLeft = 0;
            if(event.key == KEY_RIGHT) PlayerRight = 0;
            if(event.key == KEY_SPACE) PlayerShooting = 0;
            if(event.key == KEY_SPECIAL && Level != MENU_SCREEN_PAUSE &&
               Player.Bonus) { // Moved bonus weapon here, so long press does not launch special weapon
                /* Bónuszlövedék a játékos orrától: fal esetén a pálya tetején kezdődjön, sugár esetén lógjon bele a játékos orrába */
                AddShot(
                    &Shots,
                    NewVec2(Player.Pos.x + 9, Player.Weapon == Wall ? 5 : Player.Pos.y + 2),
                    Player.Weapon == Beam ? 0 : 2,
                    1,
                    Player.Weapon);
                --Player.Bonus; /* Bónusz elhasználása */
                AudioFlags |= SOUND_BONUSWPN; /* Bónuszfegyver hangjának kiadása */
            }
            break;
        /** Képkocka összeállítása **/
        case EVENT_USER:
            memset(PixelMap, 0, sizeof(PixelMap)); /* Kép ürítése */
            /** Bevezető animáció **/
            if(IntroPhase) {
                DrawObject(
                    PixelMap,
                    GetObject(gSpace),
                    NewVec2(8, 12 - IntroPhase)); /* A Space felirat ússzon be felülről */
                DrawObject(
                    PixelMap,
                    GetObject(gImpact),
                    NewVec2(4, 24 + IntroPhase)); /* Az Impact felirat ússzon be alulról */
                DrawOutlinedObject(
                    PixelMap,
                    GetObject(gIntro),
                    NewVec2(56 - IntroPhase * 4, 20)); /* A hajók ússzanak középen */
                if(FrameHold) { /* A képkockát tartsa, vagy lépjen újra, ha letelt az ideje */
                    --FrameHold;
                    if(!FrameHold)
                        FrameHold =
                            --IntroPhase == 1 ?
                                FRAMERATE :
                                2; /* Ha az utolsó képkocka jön, egy másodpercig maradjon */
                }
                /** Játék vége képernyõ **/
            } else if(Level == LevelCount) {
                char ScoreText[6]; /* Maximum ötjegyű lehet + lezáró karakter */
                itoa(Player.Score, ScoreText, 10); /* Pontszám szöveggé alakítása */
                DrawText(PixelMap, "Game over\nYour score:", NewVec2(1, 1), 9);
                DrawText(PixelMap, ScoreText, NewVec2(1, 19), 0);
#ifdef PAUSE
                /** Szünet menü **/
            } else if(Level == MENU_SCREEN_PAUSE) {
                /* A menüelem-jelzõ kezdete, ez Nokia 3310-en 8-2- */
                DrawSmallNumber(
                    PixelMap, 8, 1, NewVec2(57, 0)); /* A számírót meghívni kevesebb bájtba kerül */
                DrawObject(PixelMap, GetObject(gShot), NewVec2(61, 2));
                DrawSmallNumber(PixelMap, 2, 1, NewVec2(65, 0));
                DrawObject(PixelMap, GetObject(gShot), NewVec2(69, 2));
                DrawSmallNumber(PixelMap, 1, 1, NewVec2(73, 0));
                DrawObject(PixelMap, GetObject(gShot), NewVec2(77, 2));
                DrawSmallNumber(
                    PixelMap,
                    gNum0 + MenuItem,
                    1,
                    NewVec2(81, 0)); /* A kiválasztott menüelem száma */
                DrawText(
                    PixelMap,
                    "Continue\nExit",
                    NewVec2(1, 7),
                    11); /* Menüelemek kiírása, az egészet szövegként */
                InvertScreenPart(
                    PixelMap,
                    NewVec2(0, MenuItem * 11 - 5),
                    NewVec2(
                        76,
                        MenuItem * 11 + 5)); /* A kiválasztott menüelem körül invertálja a képet */
                DrawText(PixelMap, "Select", NewVec2(24, 40), 0); /* Select felirat alulra */
                DrawScrollBar(PixelMap, (MenuItem - 1) * 100); /* Görgetõsáv rajzolása */
#endif /* PAUSE */
                /** Rekodrképernyõ **/
            } else if(Level == MENU_SCREEN_HIGH_SCORE) {
#ifdef LEGACY_TOP_SCORE
                const Uint8 OneSign[24] = {
                    0, 0, 1, 0, 0, 1, 1, 0, 1, 1, 1, 0,
                    0, 1, 1, 0, 0, 1, 1, 0, 1, 1, 1, 1}; /* Egy egyes pixeltérképe */
                char ScoreText[6]; /* Maximum ötjegyű lehet + lezáró karakter */
                itoa(TopScores[0], ScoreText, 10); /* Pontszám szöveggé alakítása */
                DrawText(PixelMap, "Top score:", NewVec2(1, 1), 0);
                DrawText(
                    PixelMap,
                    ScoreText,
                    NewVec2(1, 11),
                    0); /* A rögzített legjobb pontszám kiírása */
                for(i = 0; i < 4; ++i) { /* Az egyes oszlopai */
                    for(j = 0; j < 6; ++j) { /* Az egyes sorai */
                        /* Ki gondolta volna, hogy x == 0 || x == 1 helyett x >= 0 && x <= 1 sikeresen lespórol 300 bájtot */
                        Vec2 Pos = NewVec2(64 + i * 5, 1 + j * 4);
                        if(TimeInScores / 3 - (4 - i) - j >= 0 &&
                           TimeInScores / 3 - (4 - i) - j <=
                               1) /* Ez egy lefelé mozgó átlóra képlet, harmad sebességgel */
                            DrawObject(
                                PixelMap,
                                GetObject(TimeInScores / 3 - (4 - i) - j ? gDotFull : gDotEmpty),
                                Pos); /* Egy pixelnyi köz legyen a pontok közt */
                        else if(
                            TimeInScores / 3 - (4 - i) - j > 1 &&
                            OneSign
                                [j * 4 +
                                 i]) /* Ahogy mozognak az átlók lefelé, a nyomvonaluk rajzolja ki az egyest */
                            DrawObject(
                                PixelMap,
                                GetObject(TimeInScores < 45 ? gDotEmpty : gDotFull),
                                Pos); /* 45-nél színváltás */
                    }
                }
                if(++TimeInScores ==
                   54) /* Az animáció 35 és 53 között maradjon, azaz ismételgesse a villogást */
                    TimeInScores = 35;
#else
                /* Helyezásek kiírása */
                Vec2 Pos = {3, 3}; /* Kezdõpozíció */
                for(i = 0; i < SCORE_COUNT; i++) {
                    if(i == 5) /* Az 5. elemnél új oszlopot kell kezdeni felül */
                        Pos = NewVec2(47, 3);
                    DrawSmallNumber(PixelMap, i + 1, i == 9 ? 2 : 1, Pos); /* Helyezés */
                    DrawObject(
                        PixelMap,
                        GetObject(gShot),
                        NewVec2(Pos.x + 4, Pos.y + 2)); /* Elválasztó */
                    DrawSmallNumber(
                        PixelMap, TopScores[i], 5, NewVec2(Pos.x + 24, Pos.y)); /* Pontszám */
                    Pos.y +=
                        9; /* A képernyõ egyenletes elosztásához 4 pixel szükséges a számok közt, egy szám pedig 5 pixel magas, így 9-et kell ugrani */
                }
#endif /* LEGACY_TOP_SCORE*/
                /** Fõmenü **/
            } else if(Level == MENU_SCREEN_MAIN) {
                /* A menüelem-jelzõ kezdete, ez Nokia 3310-en 8-2- */
                DrawSmallNumber(
                    PixelMap, 8, 1, NewVec2(65, 0)); /* A számírót meghívni kevesebb bájtba kerül */
                DrawObject(PixelMap, GetObject(gShot), NewVec2(69, 2));
                DrawSmallNumber(PixelMap, 2, 1, NewVec2(73, 0));
                DrawObject(PixelMap, GetObject(gShot), NewVec2(77, 2));
                DrawSmallNumber(
                    PixelMap,
                    gNum0 + MenuItem,
                    1,
                    NewVec2(81, 0)); /* A kiválasztott menüelem száma */
                DrawText(
                    PixelMap,
                    SavedLevel ? "Continue\nNew game\nTop score" : "New game\nTop score",
                    NewVec2(1, 7),
                    11); /* Menüelemek kiírása, az egészet szövegként */
                InvertScreenPart(
                    PixelMap,
                    NewVec2(0, MenuItem * 11 - 5),
                    NewVec2(
                        76,
                        MenuItem * 11 + 5)); /* A kiválasztott menüelem körül invertálja a képet */
                DrawText(PixelMap, "Select", NewVec2(24, 40), 0); /* Select felirat alulra */
                DrawScrollBar(
                    PixelMap, (MenuItem - 1) * (SavedLevel ? 50 : 100)); /* Görgetõsáv rajzolása */
                /** Játék **/
            } else {
                /* Állapotsáv */
                Uint8 NonInverseLevel = Level < 4 ||
                                        5 < Level; /* Felül van-e a táj és alul az állapotsáv? */
                Sint16 BarTop =
                    NonInverseLevel ?
                        0 :
                        43; /* A felső behúzása az állapotsávnak, néhány pályán alul van */
                Uint8 StartLives =
                    Player.Lives; /* A játékos élete a képkocka feldolgozása előtt */
#ifdef ZEROTH_LIFE /* Az életek számának jelzése nulladik élet módtól függõen */
                for(i = 0; i < Player.Lives - 1; ++i)
#else
                for(i = 0; i < Player.Lives; ++i)
#endif
                    DrawObject(
                        PixelMap, GetObject(gLife), NewVec2(i * 6, BarTop)); /* Szívek rajzolása */
                DrawObject(
                    PixelMap,
                    GetObject((Graphics)(gLife + Player.Weapon)),
                    NewVec2(
                        33,
                        BarTop)); /* Bónuszfegyver ikonjának kirajzolása, ezek a szív után helyezkednek el az enumerációban */
                DrawSmallNumber(
                    PixelMap,
                    Player.Bonus,
                    2,
                    NewVec2(43, BarTop)); /* Hátralévõ bónuszfegyveres lövések kijelzése */
                DrawSmallNumber(
                    PixelMap, Player.Score, 5, NewVec2(71, BarTop)); /* Pontszám kijelzése */
                /* Játéktér */
                ShotListTick(&Shots, PixelMap, &Player); /* Már a pályán lévõ lövedékek kezelése */
                /* Játékos mozgása */
                if(Enemies) { /* Csak akkor mozoghat gombokkal, ha még van ellenség */
                    if(PlayerLeft && Player.Pos.x > (Player.Protection ? 2 : 0)) --Player.Pos.x;
                    if(PlayerRight && Player.Pos.x < 74) ++Player.Pos.x;
                    /* Az alsó és felső 5 pixelre nem mehet a játékos, ha ott a kijelző */
                    if(PlayerUp &&
                       Player.Pos.y > NonInverseLevel * 5 + (Player.Protection ? 2 : 0))
                        --Player.Pos.y;
                    if(PlayerDown &&
                       Player.Pos.y < 36 + NonInverseLevel * 5 - (Player.Protection ? 2 : 0))
                        ++Player.Pos.y;
                } else { /* Különben pálya vége animáció */
                    EmptyShotList(&Shots); /* Lövéslista ürítése */
                    if(Player.Pos.x >
                       84) { /* Ha az animációnak vége, a játékos kiment a pályáról, töltse be a következõ szintet */
                        Player.Pos = NewVec2(3, 20); /* Pozíció vissza alapra */
                        PlayerShootTimer = 0; /* Fegyver lehûtése */
                        PlayerUp = PlayerDown = PlayerLeft = PlayerRight = PlayerShooting =
                            0; /* Játékos akcióinak nullázása, ha a játékos nem engedte fel a gombokat az animáció alatt sem */
                        LevelSpawner(&Enemies, ++Level); /* Következõ szint benépesítése */
                        if(Level == LevelCount) { /* Ha a következő szint a játék vége képernyő */
                            PlaceTopScore(
                                TopScores,
                                Player.Score); /* A végső pontszámot helyezze el a legjobbak közt */
                            SavedLevel = 0; /* Nincs honnan folytatni egy befejezett játékot */
                            SaveLevel(SavedLevel); /* Mentés törlése */
                        } else /* Ha van még pálya, ürítse a tájat, hogy legyen hova rajzolni neki */
                            EmptyScenery(&Scene);
                        MoveScene = 1; /* Tájmozgatás újraindítása */
                    } else { /* Kiúszás a képernyõrõl */
                        Sint16 OutPosition =
                            NonInverseLevel ?
                                10 :
                                31; /* A kiúszási sor attól függ, hogy felül vagy alul van-e a táj */
                        if(Player.Pos.y < OutPosition) /* Először menjen a kiúszási sorba */
                            ++Player.Pos.y; /* Ha alatta van, ússzon fel */
                        else if(Player.Pos.y > OutPosition)
                            --Player.Pos.y; /* Ha felette van, ússzon le */
                        else /* Ha már a kiúszási sorban van, ússzon ki jobbra */
                            Player.Pos.x += 3;
                    }
                }
                /* Ha van kezdeti védelem, minden páros képkockában váltson a két animációs fázisa között */
                DrawObject(
                    PixelMap,
                    GetObject(
                        Player.Protection ? G_PROTECTION_A1 + (Player.Protection / 2) % 2 :
                                            G_PLAYER),
                    Player.Protection ? NewVec2(Player.Pos.x - 2, Player.Pos.y - 2) : Player.Pos);
                if(PlayerShootTimer) /* Ha a fegyver nem hûlt ki... */
                    PlayerShootTimer--; /* ...akkor hûljön ki */
                if(PlayerShooting && PlayerShootTimer == 0) { /* Csak 0-s idõzítõvel lehet lõni */
                    AddShot(
                        &Shots,
                        NewVec2(Player.Pos.x + 9, Player.Pos.y + 3),
                        2,
                        1,
                        Standard); /* Lövés elhelyezése a pályán */
                    PlayerShootTimer =
                        4; /* Felmelegszik a fegyver, a következõ 5 képkockában nem lehet vele lõni */
                    AudioFlags |= SOUND_SHOT; /* Lövéshang kérése a következő hangkiadáskor */
                }
                AnimPulse =
                    1 -
                    AnimPulse; /* 1 és 0 között ugrál, hogy felezze az animációk sebességét, különben túl gyors */
                EnemyListTick(
                    &Enemies,
                    &Player,
                    PixelMap,
                    &Shots,
                    AnimPulse,
                    &MoveScene); /* Ellenségek kezelése */
                HandleScenery(&Scene, PixelMap, MoveScene, &Player, Level); /* Táj mutatása */
                if(Level == 0 || !NonInverseLevel) /* Néhány szinten invertált a kép */
                    InvertScreen(PixelMap);
                if(Player.Protection) /* Ha a játékosnak van védelme */
                    Player.Protection--; /* Fogyjon el */
                if(Player.Lives == 0) { /* A játékos halála esetén */
                    PlaceTopScore(
                        TopScores,
                        Player.Score); /* Pontszám elhelyetése a legjobbak közt és mentés */
                    Level =
                        LevelCount; /* Ugrás a játék vége képernyőre, ami az utolsó pálya helyett van */
                    SavedLevel = 0; /* Nincs honnan folytatni egy befejezett játékot */
                    SaveLevel(SavedLevel); /* Mentés törlése */
                    AudioFlags |= SOUND_DEATH; /* Halálhang kiadása */
                } else if(Player.Lives != StartLives) { /* Ha a játékos a kör alatt életet veszített */
                    Player.Pos = NewVec2(3, 20); /* Kerüljön a kezdőpozícióba */
                    Player.Protection = 50; /* Kapjon védelmet */
                    AudioFlags |= SOUND_DEATH; /* Halálhang kiadása */
                }
            }
            view_port_update(view_port);
            break;
        case EVENT_QUIT: /* Az operációs rendszer kilépés parancsára álljon le a futás, függetlenül a játék fázisától */
            run = 0;
            break;
        }
        furi_mutex_release(mutex);
    }

    furi_timer_stop(timer);
    view_port_enabled_set(view_port, false);
    gui_remove_view_port(gui, view_port);
    furi_record_close(RECORD_GUI);
    view_port_free(view_port);
    furi_message_queue_free(event_queue);
    /** Kilépés **/
    EmptyEnemyList(&Enemies); /* Megmaradt ellenségek felszabadítása */
    EmptyScenery(&Scene); /* Megmaradt pályaelemek felszabadítása */
    EmptyShotList(&Shots); /* Megmaradt lövések felszabadítása */
    FreeDynamicGraphics(); /* Dinamikus grafikai objektumok felszabadítása */
    FreeDynamicEnemies(); /* Dinamikus ellenségek felszabadítása */
    // TODO: do audio
    // SDL_PauseAudio(1); /* Hang megállítása */
    return 0;
}
