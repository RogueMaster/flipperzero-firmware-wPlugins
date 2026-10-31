// Hazke for Flipper Zero: the Cardputer space trader cut down to six
// buttons and a 128x64 black-and-white screen.

#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <input/input.h>
#include <notification/notification_messages.h>

#include "Platform.h"
#include "Config.h"
#include "Audio.h"
#include "Galaxy.h"
#include "SolarSystem.h"
#include "Market.h"
#include "GameState.h"
#include "NPCShip.h"
#include "Faction.h"
#include "Rank.h"
#include "Combat.h"
#include "Missile.h"
#include "Quest.h"
#include "Hyperspace.h"
#include "Flight.h"
#include "Render.h"
#include "World.h"
#include "Save.h"
#include "Capture.h"

namespace {

// SCREEN setting: dark (lit lines on black) or light (black lines on the
// lit screen). The frame buffer always marks drawn pixels; blit inverts
// it for the dark screen.
bool lightScreen = false;
Color Fg() {
    return lightScreen ? ColorBlack : ColorWhite;
}
Color Bg() {
    return lightScreen ? ColorWhite : ColorBlack;
}

// ---------- App plumbing ----------

struct App {
    FuriMutex* mutex;
    FuriMessageQueue* queue;
    ViewPort* vp;
    Gui* gui;
    bool running;
    volatile uint32_t dropped; // input events lost to a full queue
};
App app;

enum class Mode : uint8_t {
    Title,
    Help,
    Flight,
    Pause,
    Map,
    Chart,
    Hyperspace,
    Landed,
    Market,
    Equip,
    Status,
    GameOver,
    Jobs,
};

GameState game;
Mode mode = Mode::Title;
float modeT = 0.0f;
int currentSystem = 0;
int landedPOI = -1;
bool hasSave = false;

struct Menu {
    int sel;
    int scroll;
};
Menu titleM, pauseM, landedM, marketM, equipM, statusM, helpM, jobsM;
int mapSel = 0;
int chartSel = 0;
bool chartFromGate = false;
int jumpTarget = 0;
Mode statusReturn = Mode::Landed;

// Short message on menu screens.
char notice[28] = "";
float noticeT = 0.0f;
void setNotice(const char* msg) {
    strncpy(notice, msg, sizeof(notice) - 1);
    notice[sizeof(notice) - 1] = '\0';
    noticeT = 1.6f;
}

// What the planet you're on still has for sale.
uint8_t localQty[Market::N];
int localSys = -1, localPOI = -1;

void marketEnter(int sys, int poi) {
    bool fresh = (sys != localSys || poi != localPOI);
    if(fresh) {
        for(int i = 0; i < (int)Market::N; i++)
            localQty[i] = Market::qtyAtPlanet(sys, poi, i);
        localSys = sys;
        localPOI = poi;
    }
    // A job's goods are never sold at the planet that posted it, and any
    // other planet in the system has enough of them.
    const Quest::Slot& q = Quest::active;
    if(Quest::isActive() && Quest::status == Quest::Status::InProgress && (int)q.fromSys == sys &&
       q.commodity < (uint8_t)Market::N) {
        if((int)q.fromPOI == poi)
            localQty[q.commodity] = 0;
        else if(fresh && localQty[q.commodity] < q.qty)
            localQty[q.commodity] = q.qty;
    }
}

// The planet the active job wants you at next in this system, or -1.
int jobTargetPOI() {
    if(!Quest::isActive() || (int)Quest::active.fromSys != currentSystem) return -1;
    const Quest::Slot& q = Quest::active;
    if(Quest::status == Quest::Status::ReadyToTurnIn) return q.fromPOI;
    switch(q.type) {
    case Quest::Type::Delivery:
    case Quest::Type::VisitPlanet:
    case Quest::Type::Courier:
        return q.toPOI != 0xFF ? q.toPOI : -1;
    default:
        return -1;
    }
}

// ---------- Buttons ----------

struct Keys {
    bool held[InputKeyMAX];
    bool press[InputKeyMAX];
    bool rep[InputKeyMAX];
    bool release[InputKeyMAX];
    uint32_t downAt[InputKeyMAX];
    uint32_t lastSign[InputKeyMAX]; // last Press / Long / Repeat: proof it's still held
    bool backArmed; // BACK went down in flight: a tap steps speed, a hold opens the menu
    bool backLong; // the input service reported a long BACK press
    bool fireArmed; // OK went down in flight
    uint32_t okLast; // when OK last went down in flight, for the double click
    bool okDouble; // second OK press inside DoubleClickMs: fire a missile
};
Keys keys;

void setMode(Mode m) {
    mode = m;
    modeT = 0.0f;
    keys.backArmed = false;
    keys.fireArmed = false;
}

void handleEvent(const InputEvent& e) {
    if(e.key >= InputKeyMAX) return;
    if(e.type == InputTypePress || e.type == InputTypeLong || e.type == InputTypeRepeat) {
        keys.lastSign[e.key] = furi_get_tick();
    }
    switch(e.type) {
    case InputTypePress:
        keys.held[e.key] = true;
        keys.press[e.key] = true;
        keys.downAt[e.key] = furi_get_tick();
        if(mode == Mode::Flight) {
            if(e.key == InputKeyBack) keys.backArmed = true;
            if(e.key == InputKeyOk) {
                keys.fireArmed = true;
                uint32_t now = furi_get_tick();
                if(keys.okLast && now - keys.okLast <= Config::DoubleClickMs) {
                    keys.okDouble = true;
                    keys.okLast = 0; // a third press starts a new pair
                } else {
                    keys.okLast = now;
                }
            }
        }
        break;
    case InputTypeRelease:
        keys.held[e.key] = false;
        keys.release[e.key] = true;
        if(e.key == InputKeyOk) keys.fireArmed = false;
        break;
    case InputTypeRepeat:
        keys.rep[e.key] = true;
        break;
    case InputTypeLong:
        if(e.key == InputKeyBack) keys.backLong = true;
        break;
    default:
        break;
    }
}

void releaseKey(int k) {
    keys.held[k] = false;
    keys.release[k] = true;
    if(k == InputKeyOk) keys.fireArmed = false;
}

// The Flipper keeps sending Long / Repeat events while a button is down.
// A key that has gone quiet for this long has been let go, even if its
// Release never arrived.
constexpr uint32_t StaleKeyMs = 1500;

void releaseStaleKeys() {
    uint32_t now = furi_get_tick();
    for(int k = 0; k < InputKeyMAX; k++) {
        if(keys.held[k] && now - keys.lastSign[k] > StaleKeyMs) releaseKey(k);
    }
}

void clearEdges() {
    memset(keys.press, 0, sizeof(keys.press));
    memset(keys.rep, 0, sizeof(keys.rep));
    memset(keys.release, 0, sizeof(keys.release));
    keys.backLong = false;
    keys.okDouble = false;
}

bool pressed(InputKey k) {
    return keys.press[k];
}
bool nav(InputKey k) {
    return keys.press[k] || keys.rep[k];
}

// Up/down through a list, wrapping, keeping the cursor on screen.
bool listNav(Menu& m, int count, int visible) {
    bool moved = false;
    if(nav(InputKeyUp)) {
        m.sel = (m.sel - 1 + count) % count;
        moved = true;
    }
    if(nav(InputKeyDown)) {
        m.sel = (m.sel + 1) % count;
        moved = true;
    }
    if(m.sel >= count) m.sel = count - 1;
    if(m.sel < m.scroll) m.scroll = m.sel;
    if(m.sel >= m.scroll + visible) m.scroll = m.sel - visible + 1;
    if(m.scroll < 0) m.scroll = 0;
    if(moved) Audio::uiMove();
    return moved;
}

void fmtCredits(char* out, size_t cap, int tenths) {
    snprintf(out, cap, "%d.%d", tenths / 10, (tenths < 0 ? -tenths : tenths) % 10);
}

// ---------- Canvas helpers ----------

constexpr int RowH = 9;
constexpr int RowY0 = 13;

void tiny(Canvas* c, int x, int y, const char* s) {
    for(; *s; s++, x += 4) {
        char ch = *s;
        if(ch >= 'a' && ch <= 'z') ch = (char)(ch - 32);
        if(ch < 32 || ch > 95) ch = '?';
        uint16_t g = Fb::Font3x5[ch - 32];
        for(int row = 0; row < 5; row++) {
            int bits = (g >> (3 * (4 - row))) & 7;
            if(bits & 4) canvas_draw_dot(c, x, y + row);
            if(bits & 2) canvas_draw_dot(c, x + 1, y + row);
            if(bits & 1) canvas_draw_dot(c, x + 2, y + row);
        }
    }
}

void tinyRight(Canvas* c, int x1, int y, const char* s) {
    tiny(c, x1 - Fb::textWidth(s) + 1, y, s);
}

void tinyCenter(Canvas* c, int cx, int y, const char* s) {
    tiny(c, cx - Fb::textWidth(s) / 2, y, s);
}

void header(Canvas* c, const char* title, const char* right) {
    canvas_set_font(c, FontPrimary);
    canvas_draw_str(c, 1, 9, title);
    if(right) {
        canvas_set_font(c, FontSecondary);
        canvas_draw_str_aligned(c, 127, 9, AlignRight, AlignBottom, right);
    }
    canvas_draw_line(c, 0, 11, 127, 11);
}

void row(Canvas* c, int y0, bool sel, const char* left, const char* right) {
    canvas_set_font(c, FontSecondary);
    if(sel) {
        canvas_draw_box(c, 0, y0, 128, RowH);
        canvas_set_color(c, Bg());
    }
    canvas_draw_str(c, 3, y0 + 8, left);
    if(right) canvas_draw_str_aligned(c, 125, y0 + 8, AlignRight, AlignBottom, right);
    canvas_set_color(c, Fg());
}

void scrollMarks(Canvas* c, const Menu& m, int visible, int total, int y0) {
    if(m.scroll > 0) canvas_draw_dot(c, 127, y0);
    if(m.scroll + visible < total) canvas_draw_dot(c, 127, y0 + visible * RowH - 1);
}

void footer(Canvas* c, const char* hint) {
    if(noticeT > 0.0f) {
        canvas_draw_box(c, 0, 57, 128, 7);
        canvas_set_color(c, Bg());
        tinyCenter(c, 64, 58, notice);
        canvas_set_color(c, Fg());
    } else if(hint) {
        tinyCenter(c, 64, 58, hint);
    }
}

// The frame buffer already has the canvas's layout: copy it straight in.
void blit(Canvas* c) {
    uint8_t* dst = canvas_get_buffer(c);
    if(!dst || canvas_get_buffer_size(c) != sizeof(Fb::buf)) return;
    if(lightScreen) {
        memcpy(dst, Fb::buf, sizeof(Fb::buf));
    } else {
        const uint32_t* src = (const uint32_t*)Fb::buf;
        uint32_t* out = (uint32_t*)dst;
        for(size_t i = 0; i < sizeof(Fb::buf) / 4; i++)
            out[i] = ~src[i];
    }
}

// ---------- Title ----------

enum TitleItem {
    TContinue,
    TNew,
    THelp,
    TFx,
    TScreen
};
TitleItem titleItems[5];
int titleCount = 0;

void buildTitleItems() {
    titleCount = 0;
    if(hasSave) titleItems[titleCount++] = TContinue;
    titleItems[titleCount++] = TNew;
    titleItems[titleCount++] = THelp;
    titleItems[titleCount++] = TFx;
    titleItems[titleCount++] = TScreen;
    if(titleM.sel >= titleCount) titleM.sel = 0;
}

void goTitle() {
    hasSave = Save::exists();
    buildTitleItems();
    titleM.sel = 0;
    titleM.scroll = 0;
    setMode(Mode::Title);
}

void cycleFx(int dir) {
    Audio::mode = (uint8_t)((Audio::mode + Audio::ModeCount + dir) % Audio::ModeCount);
    Save::writeSettings(Audio::mode, lightScreen);
    Audio::uiSelect();
}

void toggleScreen() {
    lightScreen = !lightScreen;
    Save::writeSettings(Audio::mode, lightScreen);
    Audio::uiSelect();
}

void newGame() {
    game.reset();
    Quest::resetAll();
    Galaxy::marketEpoch = 0;
    currentSystem = 0;
    localSys = -1;
    SystemFlight::landNormalValid = false;
    SystemFlight::state.loadedSys = -1;
    SystemFlight::enter(currentSystem);
    SystemFlight::toast("TAP BACK=SPEED HOLD=MENU", 5.0f);
    World::scatterDust();
    setMode(Mode::Flight);
}

bool continueGame() {
    Save::Data d;
    if(!Save::read(d)) return false;
    game.reset();
    Save::apply(d, game);
    currentSystem = d.system;
    SystemFlight::loadLayout(currentSystem);
    NPCShip::spawnFor(currentSystem, SystemFlight::layout);
    landedPOI = d.planetPOI;
    if(landedPOI >= SystemFlight::layout.numPOIs) landedPOI = SystemFlight::firstPlanet();
    SystemFlight::landNormalValid = false;
    memcpy(localQty, d.localQty, sizeof(localQty));
    localSys = currentSystem;
    localPOI = landedPOI;
    landedM = {0, 0};
    setMode(Mode::Landed);
    return true;
}

void updateTitle() {
    World::drawTitleShip(modeT);
    listNav(titleM, titleCount, 5);
    TitleItem it = titleItems[titleM.sel];
    if(it == TFx && (nav(InputKeyLeft) || nav(InputKeyRight))) {
        cycleFx(nav(InputKeyLeft) ? -1 : 1);
    }
    if(it == TScreen && (nav(InputKeyLeft) || nav(InputKeyRight))) toggleScreen();
    if(pressed(InputKeyOk)) {
        switch(it) {
        case TContinue:
            Audio::uiSelect();
            if(!continueGame()) {
                hasSave = false;
                buildTitleItems();
            }
            break;
        case TNew:
            Audio::uiSelect();
            newGame();
            break;
        case THelp:
            Audio::uiSelect();
            helpM = {0, 0};
            setMode(Mode::Help);
            break;
        case TFx:
            cycleFx(1);
            break;
        case TScreen:
            toggleScreen();
            break;
        }
    }
    if(pressed(InputKeyBack)) app.running = false;
}

void drawTitle(Canvas* c) {
    blit(c);
    canvas_set_font(c, FontPrimary);
    canvas_draw_str(c, 2, 11, "HAZKE");
    tiny(c, 38, 5, Config::VersionTag);
    canvas_set_font(c, FontSecondary);
    for(int i = 0; i < titleCount; i++) {
        char buf[20];
        const char* s = "";
        switch(titleItems[i]) {
        case TContinue:
            s = "CONTINUE";
            break;
        case TNew:
            s = "NEW GAME";
            break;
        case THelp:
            s = "CONTROLS";
            break;
        case TFx:
            snprintf(buf, sizeof(buf), "FX: %s", Audio::modeName());
            s = buf;
            break;
        case TScreen:
            s = lightScreen ? "SCREEN: LIGHT" : "SCREEN: DARK";
            break;
        }
        int y0 = 15 + i * 10;
        if(i == titleM.sel) {
            canvas_draw_box(c, 0, y0, 66, 10);
            canvas_set_color(c, Bg());
        }
        canvas_draw_str(c, 3, y0 + 8, s);
        canvas_set_color(c, Fg());
    }
}

// ---------- Controls help ----------

struct HelpLine {
    const char* a;
    const char* b;
};
const HelpLine helpLines[] = {
    {"UP/DOWN", "pitch"},
    {"LEFT/RIGHT", "roll"},
    {"OK hold", "laser"},
    {"OK x2", "missile"},
    {"BACK tap", "speed"},
    {"BACK hold", "menu"},
    {"Any arrow", "manual"},
    {"Planet", "fly in: land"},
    {"Gate", "fly in: jump"},
    {"AUTODOCK", "menu: land"},
    {"ATTACK", "menu: lock"},
    {"ECM", "automatic"},
    {"Saving", "on landing"},
};
constexpr int HelpCount = sizeof(helpLines) / sizeof(helpLines[0]);

void updateHelp() {
    if(nav(InputKeyUp) && helpM.scroll > 0) helpM.scroll--;
    if(nav(InputKeyDown) && helpM.scroll < HelpCount - 5) helpM.scroll++;
    if(pressed(InputKeyBack) || pressed(InputKeyOk)) {
        Audio::uiBack();
        goTitle();
    }
}

void drawHelp(Canvas* c) {
    header(c, "CONTROLS", nullptr);
    canvas_set_font(c, FontSecondary);
    for(int i = 0; i < 5 && helpM.scroll + i < HelpCount; i++) {
        const HelpLine& h = helpLines[helpM.scroll + i];
        int y = RowY0 + i * RowH + 8;
        canvas_draw_str(c, 2, y, h.a);
        canvas_draw_str_aligned(c, 126, y, AlignRight, AlignBottom, h.b);
    }
    Menu m = {0, helpM.scroll};
    scrollMarks(c, m, 5, HelpCount, RowY0);
}

// ---------- Flight ----------

void openPause() {
    Audio::uiBack();
    pauseM.sel = 0;
    pauseM.scroll = 0;
    setMode(Mode::Pause);
}

void throttleStep() {
    if(SystemFlight::state.warping || SystemFlight::autoDock) {
        SystemFlight::toast(SystemFlight::state.warping ? "WARP OFF" : "AUTODOCK OFF");
        SystemFlight::disengage();
        Audio::uiBack();
        return;
    }
    constexpr int Top = SystemFlight::ThrottleSteps - 1;
    int cur = (int)(game.speed * (float)Top + 0.5f);
    int next = (cur + 1) % SystemFlight::ThrottleSteps;
    game.speed = (float)next / (float)Top;
    Audio::throttleStep();
}

void land(int poi) {
    Audio::dockChime();
    {
        // Remember the side we came down on; launch goes back out there.
        const auto& p = SystemFlight::layout.poi[poi];
        const auto& st = SystemFlight::state;
        float n[3] = {st.px - (float)p.x, st.py - (float)p.y, st.pz - (float)p.z};
        float l = sqrtf(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
        SystemFlight::landNormalValid = l > 1.0f;
        if(SystemFlight::landNormalValid) {
            for(int k = 0; k < 3; k++)
                SystemFlight::landNormal[k] = n[k] / l;
        }
    }
    landedPOI = poi;
    marketEnter(currentSystem, poi);
    Quest::onDock(game, currentSystem, poi);
    hasSave = Save::write(game, currentSystem, poi, localQty);
    landedM = {0, 0};
    setMode(Mode::Landed);
    if(hasSave) setNotice("GAME SAVED");
}

void updateFlight(float dt) {
    InputState in = {};
    // A tap shorter than a frame still counts for that frame.
    in.pitchUp = keys.held[InputKeyUp] || keys.press[InputKeyUp];
    in.pitchDown = keys.held[InputKeyDown] || keys.press[InputKeyDown];
    in.rollLeft = keys.held[InputKeyLeft] || keys.press[InputKeyLeft];
    in.rollRight = keys.held[InputKeyRight] || keys.press[InputKeyRight];
    in.fire = keys.held[InputKeyOk] && keys.fireArmed;

    if(!SystemFlight::state.dying && keys.backArmed) {
        if(keys.backLong || (keys.held[InputKeyBack] &&
                             furi_get_tick() - keys.downAt[InputKeyBack] >= Config::BackHoldMs)) {
            openPause();
            return;
        }
        if(keys.release[InputKeyBack]) {
            keys.backArmed = false;
            throttleStep();
        }
    }

    if(keys.okDouble && !SystemFlight::state.dying && !SystemFlight::state.warping) {
        SystemFlight::fireMissile(game);
    }

    World::animT += dt;
    auto ev = SystemFlight::step(game, in, dt);
    switch(ev) {
    case SystemFlight::Event::Landed:
        land(SystemFlight::planetInLandingRange());
        return;
    case SystemFlight::Event::Gate: {
        Audio::uiSelect();
        SystemFlight::disengage();
        chartFromGate = true;
        chartSel = Galaxy::gateCount[currentSystem] > 0 ? Galaxy::gates[currentSystem][0] :
                                                          currentSystem;
        setMode(Mode::Chart);
        return;
    }
    case SystemFlight::Event::Dead:
        setMode(Mode::GameOver);
        return;
    default:
        break;
    }
    World::drawFlight(game);
}

// ---------- Flight menu ----------

enum PauseItem {
    PResume,
    PAttack,
    PTarget,
    PEngage,
    PMissile,
    PMap,
    PChart,
    PStatus,
    PFx,
    PScreen,
    PQuit,
    PCount
};
constexpr int PauseVisible = 5;

void resume() {
    setMode(Mode::Flight);
}

void updatePause() {
    listNav(pauseM, PCount, PauseVisible);
    int it = pauseM.sel;
    if(it == PTarget && (nav(InputKeyLeft) || nav(InputKeyRight) || pressed(InputKeyOk))) {
        SystemFlight::cycleTarget(nav(InputKeyLeft) ? -1 : 1);
        Audio::uiMove();
    } else if(it == PFx && (nav(InputKeyLeft) || nav(InputKeyRight) || pressed(InputKeyOk))) {
        cycleFx(nav(InputKeyLeft) ? -1 : 1);
    } else if(it == PScreen && (nav(InputKeyLeft) || nav(InputKeyRight) || pressed(InputKeyOk))) {
        toggleScreen();
    } else if(pressed(InputKeyOk)) {
        switch(it) {
        case PResume:
            Audio::uiSelect();
            resume();
            break;
        case PAttack:
            if(SystemFlight::attackNearest()) {
                resume();
            } else {
                Audio::deny();
                setNotice("NO HOSTILES NEAR");
            }
            break;
        case PEngage:
            if(SystemFlight::engaged()) {
                SystemFlight::disengage();
                Audio::uiBack();
                resume();
            } else if(SystemFlight::engage()) {
                resume();
            } else {
                setNotice(
                    SystemFlight::state.outOfZone ? "OUTSIDE THE SYSTEM" : "PICK A TARGET FIRST");
            }
            break;
        case PMissile:
            if(SystemFlight::fireMissile(game))
                resume();
            else
                setNotice(SystemFlight::toastMsg);
            break;
        case PMap: {
            Audio::uiSelect();
            const auto& st = SystemFlight::state;
            mapSel = st.lockedNPC >= 0 ? SystemFlight::layout.numPOIs + st.lockedNPC :
                     st.targetIdx >= 0 ? st.targetIdx :
                                         0;
            setMode(Mode::Map);
            break;
        }
        case PChart:
            Audio::uiSelect();
            chartFromGate = false;
            chartSel = currentSystem;
            setMode(Mode::Chart);
            break;
        case PStatus:
            Audio::uiSelect();
            statusReturn = Mode::Pause;
            statusM = {0, 0};
            setMode(Mode::Status);
            break;
        case PQuit:
            Audio::uiBack();
            goTitle();
            break;
        default:
            break;
        }
    }
    if(pressed(InputKeyBack)) {
        Audio::uiBack();
        resume();
    }
}

void drawPause(Canvas* c) {
    header(c, "FLIGHT", Galaxy::systems[currentSystem].name);
    for(int i = 0; i < PauseVisible; i++) {
        int it = pauseM.scroll + i;
        if(it >= PCount) break;
        char right[24] = "";
        const char* left = "";
        switch(it) {
        case PResume:
            left = "RESUME";
            break;
        case PAttack: {
            left = "ATTACK NEAREST";
            int h = SystemFlight::nearestHostile();
            if(h >= 0) {
                const auto& sh = NPCShip::ships[h];
                char ds[12];
                World::formatDist(ds, sizeof(ds), SystemFlight::distTo(sh.wx, sh.wy, sh.wz));
                snprintf(right, sizeof(right), "%s", ds);
            } else {
                snprintf(right, sizeof(right), "NONE");
            }
            break;
        }
        case PTarget: {
            left = "TARGET";
            char nm[18];
            SystemFlight::targetName(nm, sizeof(nm));
            snprintf(right, sizeof(right), "< %s >", nm);
            break;
        }
        case PEngage:
            if(SystemFlight::state.warping) {
                left = "STOP WARP";
            } else if(SystemFlight::state.autoLock) {
                left = SystemFlight::autoDock ? "STOP AUTODOCK" : "AUTOLOCK OFF";
            } else {
                switch(SystemFlight::engageKind()) {
                case SystemFlight::Engage::Warp:
                case SystemFlight::Engage::Dock:
                    left = SystemFlight::targetIsGate() ? "AUTO GATE" : "AUTODOCK";
                    break;
                case SystemFlight::Engage::Autolock:
                    left = "LOCK ON";
                    break;
                default:
                    left = "ENGAGE";
                    break;
                }
            }
            {
                float d = SystemFlight::targetDistance();
                if(d >= 0.0f) World::formatDist(right, sizeof(right), d);
            }
            break;
        case PMissile:
            left = "FIRE MISSILE";
            snprintf(right, sizeof(right), "x%d", game.missiles);
            break;
        case PMap:
            left = "SYSTEM MAP";
            break;
        case PChart:
            left = "GALAXY CHART";
            break;
        case PStatus:
            left = "STATUS";
            break;
        case PFx:
            left = "FX";
            snprintf(right, sizeof(right), "< %s >", Audio::modeName());
            break;
        case PScreen:
            left = "SCREEN";
            snprintf(right, sizeof(right), "< %s >", lightScreen ? "LIGHT" : "DARK");
            break;
        case PQuit:
            left = "QUIT TO TITLE";
            break;
        }
        row(c, RowY0 + i * RowH, it == pauseM.sel, left, right[0] ? right : nullptr);
    }
    scrollMarks(c, pauseM, PauseVisible, PCount, RowY0);
    char job[40];
    if(pauseM.sel == PQuit)
        snprintf(job, sizeof(job), "LAST LANDING IS SAVED");
    else if(Quest::isActive())
        Quest::formatStatus(job, sizeof(job));
    else
        job[0] = '\0';
    footer(c, job[0] ? job : nullptr);
}

// ---------- System map ----------

void mapStep(int dir) {
    const auto& L = SystemFlight::layout;
    int total = L.numPOIs + NPCShip::MaxNPCs;
    for(int k = 1; k <= total; k++) {
        int v = ((mapSel + dir * k) % total + total) % total;
        if(v >= L.numPOIs && !NPCShip::ships[v - L.numPOIs].active) continue;
        mapSel = v;
        return;
    }
}

void updateMap(float dt) {
    World::animT += dt;
    if(nav(InputKeyUp) || nav(InputKeyLeft)) {
        mapStep(-1);
        Audio::uiMove();
    }
    if(nav(InputKeyDown) || nav(InputKeyRight)) {
        mapStep(1);
        Audio::uiMove();
    }
    const auto& L = SystemFlight::layout;
    if(mapSel >= L.numPOIs && !NPCShip::ships[mapSel - L.numPOIs].active) mapStep(1);
    World::drawSystemMap(mapSel);
    if(pressed(InputKeyOk)) {
        auto& st = SystemFlight::state;
        if(mapSel < L.numPOIs && L.poi[mapSel].type == SolarSystem::POIType::Star) {
            Audio::deny();
            setNotice("CAN'T TARGET THE STAR");
        } else {
            SystemFlight::disengage();
            if(mapSel < L.numPOIs) {
                st.targetIdx = mapSel;
                st.lockedNPC = -1;
            } else {
                st.lockedNPC = mapSel - L.numPOIs;
                st.targetIdx = -1;
            }
            char nm[20];
            SystemFlight::targetName(nm, sizeof(nm));
            SystemFlight::toast(nm);
            Audio::lockOn();
            resume();
        }
    }
    if(pressed(InputKeyBack)) {
        Audio::uiBack();
        setMode(Mode::Pause);
    }
}

void drawMap(Canvas* c) {
    blit(c);
    const auto& L = SystemFlight::layout;
    char nm[20], line[40];
    float d;
    if(mapSel < L.numPOIs) {
        const auto& p = L.poi[mapSel];
        SolarSystem::displayName(currentSystem, p, nm, sizeof(nm));
        d = SystemFlight::distTo((float)p.x, (float)p.y, (float)p.z);
    } else {
        const auto& s = NPCShip::ships[mapSel - L.numPOIs];
        snprintf(nm, sizeof(nm), "%s", SystemFlight::roleName(s.role));
        d = SystemFlight::distTo(s.wx, s.wy, s.wz);
    }
    char ds[12];
    World::formatDist(ds, sizeof(ds), d);
    snprintf(line, sizeof(line), "%s  %s", nm, ds);
    canvas_set_color(c, Bg());
    canvas_draw_box(c, 0, 54, 128, 10);
    canvas_set_color(c, Fg());
    canvas_draw_line(c, 0, 54, 127, 54);
    if(noticeT > 0.0f) {
        tinyCenter(c, 64, 57, notice);
    } else {
        canvas_set_font(c, FontSecondary);
        canvas_draw_str(c, 2, 63, line);
        tinyRight(c, 126, 57, "OK:SET");
    }
}

// ---------- Galaxy chart ----------

int chartX(int i) {
    return 4 + (int)Galaxy::systems[i].x * 120 / 255;
}
int chartY(int i) {
    return 2 + (int)Galaxy::systems[i].y * 40 / 255;
}

// Nearest system in a direction on the chart (0 up, 1 down, 2 left, 3 right).
int chartStep(int from, int dir) {
    int best = from;
    int bestScore = 1 << 30;
    for(int j = 0; j < Galaxy::NumSystems; j++) {
        if(j == from) continue;
        int dx = chartX(j) - chartX(from), dy = chartY(j) - chartY(from);
        int along = 0, across = 0;
        switch(dir) {
        case 0:
            along = -dy;
            across = dx;
            break;
        case 1:
            along = dy;
            across = dx;
            break;
        case 2:
            along = -dx;
            across = dy;
            break;
        default:
            along = dx;
            across = dy;
            break;
        }
        if(along <= 0) continue;
        int score = along + 2 * (across < 0 ? -across : across);
        if(score < bestScore) {
            bestScore = score;
            best = j;
        }
    }
    return best;
}

void leaveChart() {
    if(chartFromGate) {
        SystemFlight::bumpOffGate();
        game.speed = 0.0f;
        game.pitchInput = game.rollInput = game.yawInput = 0.0f;
        game.pitchRate = game.rollRate = game.yawRate = 0.0f;
        setMode(Mode::Flight);
    } else {
        setMode(Mode::Pause);
    }
}

void updateChart() {
    static const InputKey dirs[4] = {InputKeyUp, InputKeyDown, InputKeyLeft, InputKeyRight};
    for(int d = 0; d < 4; d++) {
        if(nav(dirs[d])) {
            int n = chartStep(chartSel, d);
            if(n != chartSel) {
                chartSel = n;
                Audio::uiMove();
            }
        }
    }
    if(pressed(InputKeyOk)) {
        if(!chartFromGate) {
            Audio::deny();
            setNotice("FLY INTO THE GATE TO JUMP");
        } else if(chartSel == currentSystem) {
            Audio::deny();
            setNotice("YOU ARE HERE");
        } else if(!Hyperspace::inJumpRange(currentSystem, chartSel)) {
            Audio::deny();
            setNotice("NO GATE LINK THERE");
        } else {
            int cost = Hyperspace::jumpCostTenths(currentSystem, chartSel);
            if(game.credits < cost) {
                Audio::deny();
                char cr[12], msg[28];
                fmtCredits(cr, sizeof(cr), cost);
                snprintf(msg, sizeof(msg), "JUMP COSTS %s CR", cr);
                setNotice(msg);
            } else {
                game.credits -= cost;
                jumpTarget = chartSel;
                Audio::warpWhoosh();
                setMode(Mode::Hyperspace);
            }
        }
    }
    if(pressed(InputKeyBack)) {
        Audio::uiBack();
        leaveChart();
    }
}

void drawChart(Canvas* c) {
    // Gate links.
    for(int i = 0; i < Galaxy::NumSystems; i++) {
        for(int t = 0; t < Galaxy::gateCount[i]; t++) {
            int j = Galaxy::gates[i][t];
            if(j < i) continue;
            int x0 = chartX(i), y0 = chartY(i), x1 = chartX(j), y1 = chartY(j);
            // Dotted, so the systems stand out.
            int n = (x1 - x0) * (x1 - x0) + (y1 - y0) * (y1 - y0);
            int steps = (int)sqrtf((float)n) / 2;
            for(int s = 1; s < steps; s++) {
                canvas_draw_dot(c, x0 + (x1 - x0) * s / steps, y0 + (y1 - y0) * s / steps);
            }
        }
    }
    for(int i = 0; i < Galaxy::NumSystems; i++) {
        int x = chartX(i), y = chartY(i);
        canvas_draw_box(c, x - 1, y - 1, 3, 3);
        if(i == currentSystem) canvas_draw_circle(c, x, y, 4);
    }
    int sx = chartX(chartSel), sy = chartY(chartSel);
    if((int)(modeT * 3.0f) % 3 != 2) {
        canvas_draw_line(c, sx - 6, sy - 6, sx - 4, sy - 6);
        canvas_draw_line(c, sx - 6, sy - 6, sx - 6, sy - 4);
        canvas_draw_line(c, sx + 4, sy - 6, sx + 6, sy - 6);
        canvas_draw_line(c, sx + 6, sy - 6, sx + 6, sy - 4);
        canvas_draw_line(c, sx - 6, sy + 6, sx - 4, sy + 6);
        canvas_draw_line(c, sx - 6, sy + 4, sx - 6, sy + 6);
        canvas_draw_line(c, sx + 4, sy + 6, sx + 6, sy + 6);
        canvas_draw_line(c, sx + 6, sy + 4, sx + 6, sy + 6);
    }
    canvas_set_color(c, Bg());
    canvas_draw_box(c, 0, 45, 128, 19);
    canvas_set_color(c, Fg());
    canvas_draw_line(c, 0, 45, 127, 45);

    const auto& s = Galaxy::systems[chartSel];
    char line[40];
    float ly = Galaxy::distanceLY(currentSystem, chartSel);
    int lyT = (int)(ly * 10.0f + 0.5f);
    canvas_set_font(c, FontPrimary);
    canvas_draw_str(c, 1, 55, s.name);
    snprintf(line, sizeof(line), "TL%d  %d.%dLY", s.techLevel + 1, lyT / 10, lyT % 10);
    tinyRight(c, 126, 49, line);
    if(noticeT > 0.0f) {
        canvas_draw_box(c, 0, 57, 128, 7);
        canvas_set_color(c, Bg());
        tinyCenter(c, 64, 58, notice);
        canvas_set_color(c, Fg());
    } else if(
        chartFromGate && chartSel != currentSystem &&
        Hyperspace::inJumpRange(currentSystem, chartSel)) {
        char cr[12];
        fmtCredits(cr, sizeof(cr), Hyperspace::jumpCostTenths(currentSystem, chartSel));
        snprintf(line, sizeof(line), "OK: JUMP FOR %s CR", cr);
        tiny(c, 1, 58, line);
    } else {
        // Economy and government, upper case for the small font.
        char buf[40];
        snprintf(
            buf,
            sizeof(buf),
            "%s / %s",
            Galaxy::economyName(s.economy),
            Galaxy::govName(s.government));
        tiny(c, 1, 58, buf);
    }
}

// ---------- Hyperspace ----------

constexpr float HyperTime = 1.8f;

void updateHyperspace() {
    World::drawHyperspace(modeT / HyperTime);
    char msg[24];
    snprintf(msg, sizeof(msg), "TO %s", Galaxy::systems[jumpTarget].name);
    Fb::labelCenter(64, 56, msg);
    if(modeT >= HyperTime) {
        currentSystem = jumpTarget;
        Galaxy::marketEpoch++;
        localSys = -1;
        SystemFlight::enter(currentSystem);
        SystemFlight::state.zoneToast = SystemFlight::ZoneToastTime;
        game.speed = 0.0f;
        game.pitchInput = game.rollInput = game.yawInput = 0.0f;
        World::scatterDust();
        setMode(Mode::Flight);
    }
}

// ---------- Landed ----------

enum LandedItem {
    LMarket,
    LJobs,
    LEquip,
    LStatus,
    LLaunch,
    LSaveQuit,
    LCount
};

void launch() {
    Save::write(game, currentSystem, landedPOI, localQty);
    SystemFlight::enterNearPOI(currentSystem, landedPOI);
    game.speed = 1.0f; // launch at full throttle
    game.pitchInput = game.rollInput = game.yawInput = 0.0f;
    World::scatterDust();
    Audio::launchRoar();
    // A fresh pirate-hunting job puts its quarry in the sky once.
    if(Quest::needsPirateSpawn()) {
        NPCShip::ensurePirates((int)Quest::pirateSpawnQty(), currentSystem, SystemFlight::layout);
        Quest::consumePirateSpawn();
    }
    int jt = jobTargetPOI();
    if(jt >= 0) {
        SystemFlight::state.targetIdx = jt;
        SystemFlight::state.lockedNPC = -1;
    }
    setMode(Mode::Flight);
}

Quest::Slot board[Quest::BoardSize];

void updateLanded() {
    if(Quest::completionPending) {
        if(pressed(InputKeyOk) || pressed(InputKeyBack)) {
            Audio::uiSelect();
            Quest::dismissCompletion();
        }
        return;
    }
    listNav(landedM, LCount, 5);
    if(pressed(InputKeyBack)) {
        launch();
        return;
    } // BACK leaves the planet
    if(pressed(InputKeyOk)) {
        switch(landedM.sel) {
        case LMarket:
            Audio::uiSelect();
            marketM = {0, 0};
            setMode(Mode::Market);
            break;
        case LJobs:
            Audio::uiSelect();
            Quest::buildBoard(currentSystem, landedPOI, board);
            jobsM = {0, 0};
            setMode(Mode::Jobs);
            break;
        case LEquip:
            Audio::uiSelect();
            equipM = {0, 0};
            setMode(Mode::Equip);
            break;
        case LStatus:
            Audio::uiSelect();
            statusReturn = Mode::Landed;
            statusM = {0, 0};
            setMode(Mode::Status);
            break;
        case LLaunch:
            launch();
            break;
        case LSaveQuit:
            Audio::uiSelect();
            Save::write(game, currentSystem, landedPOI, localQty);
            goTitle();
            break;
        }
    }
}

void drawLanded(Canvas* c) {
    char nm[20];
    SolarSystem::displayName(currentSystem, SystemFlight::layout.poi[landedPOI], nm, sizeof(nm));
    char cr[16], right[24];
    fmtCredits(cr, sizeof(cr), game.credits);
    snprintf(right, sizeof(right), "%s CR", cr);
    header(c, nm, right);
    static const char* const labels[LCount] = {
        "MARKET", "JOBS", "EQUIP", "STATUS", "LAUNCH", "SAVE & QUIT"};
    for(int r = 0; r < 5; r++) {
        int i = landedM.scroll + r;
        if(i >= LCount) break;
        const char* right = nullptr;
        if(i == LJobs && Quest::isActive()) right = "1 ACTIVE";
        row(c, RowY0 + r * RowH, i == landedM.sel, labels[i], right);
    }
    scrollMarks(c, landedM, 5, LCount, RowY0);
    footer(c, nullptr);
    if(Quest::completionPending) {
        canvas_set_color(c, Bg());
        canvas_draw_box(c, 8, 16, 112, 34);
        canvas_set_color(c, Fg());
        canvas_draw_frame(c, 8, 16, 112, 34);
        canvas_draw_frame(c, 10, 18, 108, 30);
        canvas_set_font(c, FontPrimary);
        canvas_draw_str_aligned(c, 64, 29, AlignCenter, AlignBottom, "JOB DONE");
        tinyCenter(c, 64, 33, Quest::completionMsg);
        tinyCenter(c, 64, 41, "OK");
    }
}

// ---------- Jobs ----------

// Small-font text over two lines, broken at a space.
void tinyWrap(Canvas* c, int x, int y, const char* s) {
    constexpr int Cols = 31;
    int n = (int)strlen(s);
    if(n <= Cols) {
        tiny(c, x, y, s);
        return;
    }
    int cut = Cols;
    while(cut > 0 && s[cut] != ' ')
        cut--;
    if(cut == 0) cut = Cols;
    char a[Cols + 1];
    memcpy(a, s, (size_t)cut);
    a[cut] = '\0';
    tiny(c, x, y, a);
    const char* rest = s + cut;
    while(*rest == ' ')
        rest++;
    tiny(c, x, y + 7, rest);
}

int jobsCount() {
    return Quest::BoardSize + (Quest::isActive() ? 1 : 0);
}

void updateJobs() {
    listNav(jobsM, jobsCount(), 4);
    if(pressed(InputKeyOk)) {
        if(jobsM.sel >= Quest::BoardSize) {
            Quest::discard(game);
            setNotice("JOB DROPPED");
            if(jobsM.sel >= jobsCount()) jobsM.sel = jobsCount() - 1;
        } else if(Quest::isActive()) {
            Audio::deny();
            setNotice("FINISH OR DROP YOUR JOB");
        } else if(Quest::accept(game, board[jobsM.sel])) {
            // Its goods can't be bought here now.
            marketEnter(currentSystem, landedPOI);
            setNotice("JOB TAKEN");
        } else {
            Audio::deny();
        }
    }
    if(pressed(InputKeyBack)) {
        Audio::uiBack();
        setMode(Mode::Landed);
    }
}

void drawJobs(Canvas* c) {
    header(c, "JOBS", Quest::difficultyLabel(Quest::systemDifficulty(currentSystem)));
    for(int r = 0; r < 4; r++) {
        int i = jobsM.scroll + r;
        if(i >= jobsCount()) break;
        char right[16];
        if(i < Quest::BoardSize) {
            const Quest::Slot& s = board[i];
            char cr[12];
            fmtCredits(cr, sizeof(cr), s.rewardTenthsCR);
            snprintf(right, sizeof(right), "+%s CR", cr);
            row(c, RowY0 + r * RowH, i == jobsM.sel, Quest::typeShort(s.type), right);
        } else {
            row(c, RowY0 + r * RowH, i == jobsM.sel, "DROP ACTIVE JOB", nullptr);
        }
    }
    scrollMarks(c, jobsM, 4, jobsCount(), RowY0);
    char text[48];
    if(jobsM.sel < Quest::BoardSize)
        Quest::formatObjective(board[jobsM.sel], text, sizeof(text));
    else
        Quest::formatStatus(text, sizeof(text));
    if(noticeT > 0.0f) {
        canvas_draw_box(c, 0, 50, 128, 14);
        canvas_set_color(c, Bg());
        tinyCenter(c, 64, 54, notice);
        canvas_set_color(c, Fg());
    } else {
        canvas_draw_line(c, 0, 49, 127, 49);
        tinyWrap(c, 1, 51, text);
    }
}

// ---------- Market ----------

constexpr int MarketRows = 4;
constexpr int MarketY0 = 19;

void updateMarket() {
    listNav(marketM, (int)Market::N, MarketRows);
    int i = marketM.sel;
    int scale = Faction::priceScalePermille(game, currentSystem);
    int price = Market::priceWithScale(currentSystem, i, scale);
    if(nav(InputKeyRight) || nav(InputKeyOk)) {
        if(game.buyOne(i, localQty[i], price)) {
            localQty[i]--;
            Faction::applyTrade(game, currentSystem);
            Audio::cash();
        } else {
            Audio::deny();
            setNotice(
                localQty[i] == 0      ? "NONE FOR SALE" :
                game.cargoFree() <= 0 ? "HOLD FULL" :
                                        "NOT ENOUGH CREDITS");
        }
    }
    if(nav(InputKeyLeft)) {
        if(game.sellOne(i, price)) {
            Faction::applyTrade(game, currentSystem);
            Audio::cash();
        } else {
            Audio::deny();
            setNotice("NONE IN THE HOLD");
        }
    }
    if(pressed(InputKeyBack)) {
        Audio::uiBack();
        setMode(Mode::Landed);
    }
}

void drawMarket(Canvas* c) {
    char cr[16], right[24];
    fmtCredits(cr, sizeof(cr), game.credits);
    snprintf(right, sizeof(right), "%s CR", cr);
    header(c, "MARKET", right);
    tiny(c, 3, 13, "GOODS");
    tinyRight(c, 84, 13, "PRICE");
    tinyRight(c, 104, 13, "STK");
    tinyRight(c, 125, 13, "HELD");
    int scale = Faction::priceScalePermille(game, currentSystem);
    canvas_set_font(c, FontSecondary);
    for(int r = 0; r < MarketRows; r++) {
        int i = marketM.scroll + r;
        if(i >= (int)Market::N) break;
        int y0 = MarketY0 + r * RowH;
        bool sel = (i == marketM.sel);
        if(sel) {
            canvas_draw_box(c, 0, y0, 128, RowH);
            canvas_set_color(c, Bg());
        }
        char buf[12];
        canvas_draw_str(c, 3, y0 + 8, Market::items[i].name);
        fmtCredits(buf, sizeof(buf), Market::priceWithScale(currentSystem, i, scale));
        canvas_draw_str_aligned(c, 84, y0 + 8, AlignRight, AlignBottom, buf);
        snprintf(buf, sizeof(buf), "%d", localQty[i]);
        canvas_draw_str_aligned(c, 104, y0 + 8, AlignRight, AlignBottom, buf);
        snprintf(buf, sizeof(buf), "%d", game.cargo[i]);
        canvas_draw_str_aligned(c, 125, y0 + 8, AlignRight, AlignBottom, buf);
        canvas_set_color(c, Fg());
    }
    scrollMarks(c, marketM, MarketRows, (int)Market::N, MarketY0);
    char hint[32];
    snprintf(hint, sizeof(hint), "HOLD %d/%dT  >BUY  <SELL", game.cargoTotal(), game.cargoMax);
    footer(c, hint);
}

// ---------- Equipment ----------

enum EquipItem {
    EHull,
    EShield,
    EMissile,
    EEcm,
    EHold,
    EBeam,
    EMil,
    ECount
};
struct EquipDef {
    const char* label;
    int priceCR;
};
const EquipDef equipDefs[ECount] = {
    {"REPAIR HULL", 10},
    {"REPAIR SHIELD", 100},
    {"MISSILE", 30},
    {"ECM SYSTEM", 600},
    {"LARGE HOLD", 400},
    {"BEAM LASER", 1000},
    {"MIL LASER", 6000},
};
constexpr float RepairStepHull = 0.10f;

void equipStatus(int idx, char* out, size_t cap) {
    out[0] = '\0';
    switch(idx) {
    case EHull:
        if(game.hull >= 0.999f)
            snprintf(out, cap, "FULL");
        else
            snprintf(out, cap, "%d%%", (int)(game.hull * 100.0f + 0.5f));
        break;
    case EShield:
        if(game.shield >= 0.999f)
            snprintf(out, cap, "FULL");
        else
            snprintf(out, cap, "%d%%", (int)(game.shield * 100.0f + 0.5f));
        break;
    case EMissile:
        if(game.missiles >= 4) snprintf(out, cap, "MAX 4");
        break;
    case EEcm:
        if(game.ecm) snprintf(out, cap, "OWNED");
        break;
    case EHold:
        if(game.cargoMax >= GameState::CargoMaxLarge) snprintf(out, cap, "OWNED");
        break;
    case EBeam:
        if(game.laserTier >= 1) snprintf(out, cap, "OWNED");
        break;
    case EMil:
        if(game.laserTier >= 2)
            snprintf(out, cap, "OWNED");
        else if(game.laserTier < 1)
            snprintf(out, cap, "NEED BEAM");
        break;
    }
}

void equipBuy(int idx) {
    const char* why = nullptr;
    switch(idx) {
    case EHull:
        if(game.hull >= 0.999f) why = "HULL ALREADY FULL";
        break;
    case EShield:
        if(game.shield >= 0.999f) why = "SHIELD ALREADY FULL";
        break;
    case EMissile:
        if(game.missiles >= 4) why = "MISSILE RACK FULL";
        break;
    case EEcm:
        if(game.ecm) why = "ALREADY FITTED";
        break;
    case EHold:
        if(game.cargoMax >= GameState::CargoMaxLarge) why = "ALREADY FITTED";
        break;
    case EBeam:
        if(game.laserTier >= 1) why = "ALREADY FITTED";
        break;
    case EMil:
        if(game.laserTier >= 2)
            why = "ALREADY FITTED";
        else if(game.laserTier < 1)
            why = "FIT A BEAM LASER FIRST";
        break;
    }
    int price = equipDefs[idx].priceCR * 10;
    if(!why && game.credits < price) why = "NOT ENOUGH CREDITS";
    if(why) {
        Audio::deny();
        setNotice(why);
        return;
    }
    game.credits -= price;
    switch(idx) {
    case EHull:
        game.hull += RepairStepHull;
        if(game.hull > 1.0f) game.hull = 1.0f;
        break;
    case EShield:
        game.shield = 1.0f;
        break;
    case EMissile:
        game.missiles++;
        break;
    case EEcm:
        game.ecm = true;
        break;
    case EHold:
        game.cargoMax = GameState::CargoMaxLarge;
        break;
    case EBeam:
        game.laserTier = 1;
        break;
    case EMil:
        game.laserTier = 2;
        break;
    }
    Audio::cash();
}

void updateEquip() {
    listNav(equipM, ECount, 5);
    if(nav(InputKeyOk) || nav(InputKeyRight)) equipBuy(equipM.sel);
    if(pressed(InputKeyBack)) {
        Audio::uiBack();
        setMode(Mode::Landed);
    }
}

void drawEquip(Canvas* c) {
    char cr[16], right[24];
    fmtCredits(cr, sizeof(cr), game.credits);
    snprintf(right, sizeof(right), "%s CR", cr);
    header(c, "EQUIP", right);
    for(int r = 0; r < 5; r++) {
        int i = equipM.scroll + r;
        if(i >= ECount) break;
        char st[16], rt[20];
        equipStatus(i, st, sizeof(st));
        if(st[0])
            snprintf(rt, sizeof(rt), "%s", st);
        else
            snprintf(rt, sizeof(rt), "%d CR", equipDefs[i].priceCR);
        row(c, RowY0 + r * RowH, i == equipM.sel, equipDefs[i].label, rt);
    }
    scrollMarks(c, equipM, 5, ECount, RowY0);
    footer(c, equipM.sel == EHull ? "OK: BUY 10% HULL" : "OK: BUY");
}

// ---------- Status ----------

constexpr int StatusMax = 32;

int buildStatus(char lines[][2][20]) {
    int n = 0;
    auto add = [&](const char* a, const char* b) {
        if(n >= StatusMax) return;
        snprintf(lines[n][0], 20, "%s", a);
        snprintf(lines[n][1], 20, "%s", b);
        n++;
    };
    char b[20];
    add("RANK", Rank::nameFor(Rank::forKills(game.kills)));
    snprintf(b, sizeof(b), "%d", game.kills);
    add("KILLS", b);
    char cr[16];
    fmtCredits(cr, sizeof(cr), game.credits);
    snprintf(b, sizeof(b), "%s CR", cr);
    add("CREDITS", b);
    snprintf(b, sizeof(b), "%d / %d T", game.cargoTotal(), game.cargoMax);
    add("CARGO", b);
    snprintf(b, sizeof(b), "%d%%", (int)(game.shield * 100.0f + 0.5f));
    add("SHIELD", b);
    snprintf(b, sizeof(b), "%d%%", (int)(game.hull * 100.0f + 0.5f));
    add("HULL", b);
    static const char* const lasers[3] = {"PULSE", "BEAM", "MILITARY"};
    add("LASER", lasers[game.laserTier < 3 ? game.laserTier : 2]);
    snprintf(b, sizeof(b), "%d", game.missiles);
    add("MISSILES", b);
    add("ECM", game.ecm ? "FITTED" : "NONE");
    add("JOB", Quest::isActive() ? Quest::typeShort(Quest::active.type) : "NONE");
    for(int f = 0; f < Faction::Count; f++) {
        snprintf(b, sizeof(b), "%+d", game.standing[f]);
        add(Faction::longName((Faction::Id)f), b);
    }
    for(int i = 0; i < (int)Market::N; i++) {
        if(game.cargo[i] == 0) continue;
        snprintf(b, sizeof(b), "%d %s", game.cargo[i], Market::items[i].unit);
        add(Market::items[i].name, b);
    }
    return n;
}

char statusLines[StatusMax][2][20];
int statusCount = 0;

void updateStatus() {
    statusCount = buildStatus(statusLines);
    if(nav(InputKeyUp) && statusM.scroll > 0) statusM.scroll--;
    if(nav(InputKeyDown) && statusM.scroll < statusCount - 5) statusM.scroll++;
    if(pressed(InputKeyBack) || pressed(InputKeyOk)) {
        Audio::uiBack();
        setMode(statusReturn);
    }
}

void drawStatus(Canvas* c) {
    header(c, "STATUS", game.commanderName);
    canvas_set_font(c, FontSecondary);
    for(int r = 0; r < 5; r++) {
        int i = statusM.scroll + r;
        if(i >= statusCount) break;
        int y = RowY0 + r * RowH + 8;
        canvas_draw_str(c, 2, y, statusLines[i][0]);
        canvas_draw_str_aligned(c, 126, y, AlignRight, AlignBottom, statusLines[i][1]);
    }
    scrollMarks(c, statusM, 5, statusCount, RowY0);
}

// ---------- Game over ----------

void updateGameOver() {
    if(modeT > 0.8f && (pressed(InputKeyOk) || pressed(InputKeyBack))) {
        Audio::uiSelect();
        goTitle();
    }
}

void drawGameOver(Canvas* c) {
    canvas_set_font(c, FontPrimary);
    canvas_draw_str_aligned(c, 64, 14, AlignCenter, AlignBottom, "GAME OVER");
    canvas_set_font(c, FontSecondary);
    char b[32], cr[16];
    snprintf(b, sizeof(b), "RANK  %s", Rank::nameFor(Rank::forKills(game.kills)));
    canvas_draw_str_aligned(c, 64, 28, AlignCenter, AlignBottom, b);
    snprintf(b, sizeof(b), "KILLS  %d", game.kills);
    canvas_draw_str_aligned(c, 64, 38, AlignCenter, AlignBottom, b);
    fmtCredits(cr, sizeof(cr), game.credits);
    snprintf(b, sizeof(b), "CREDITS  %s", cr);
    canvas_draw_str_aligned(c, 64, 48, AlignCenter, AlignBottom, b);
    if(modeT > 0.8f)
        tinyCenter(c, 64, 57, hasSave ? "OK: TITLE (LAST LANDING SAVED)" : "OK: TITLE");
}

// ---------- Frame ----------

void step(float dt) {
    modeT += dt;
    if(noticeT > 0.0f) noticeT -= dt;
    switch(mode) {
    case Mode::Title:
        updateTitle();
        break;
    case Mode::Help:
        updateHelp();
        break;
    case Mode::Flight:
        updateFlight(dt);
        break;
    case Mode::Pause:
        updatePause();
        break;
    case Mode::Map:
        updateMap(dt);
        break;
    case Mode::Chart:
        updateChart();
        break;
    case Mode::Hyperspace:
        updateHyperspace();
        break;
    case Mode::Landed:
        updateLanded();
        break;
    case Mode::Market:
        updateMarket();
        break;
    case Mode::Equip:
        updateEquip();
        break;
    case Mode::Status:
        updateStatus();
        break;
    case Mode::GameOver:
        updateGameOver();
        break;
    case Mode::Jobs:
        updateJobs();
        break;
    }
}

void drawCb(Canvas* c, void* ctx) {
    App* a = (App*)ctx;
    furi_mutex_acquire(a->mutex, FuriWaitForever);
    canvas_set_color(c, Bg());
    canvas_draw_box(c, 0, 0, 128, 64);
    canvas_set_color(c, Fg());
    switch(mode) {
    case Mode::Title:
        drawTitle(c);
        break;
    case Mode::Help:
        drawHelp(c);
        break;
    case Mode::Flight:
    case Mode::Hyperspace:
        blit(c);
        break;
    case Mode::Pause:
        drawPause(c);
        break;
    case Mode::Map:
        drawMap(c);
        break;
    case Mode::Chart:
        drawChart(c);
        break;
    case Mode::Landed:
        drawLanded(c);
        break;
    case Mode::Market:
        drawMarket(c);
        break;
    case Mode::Equip:
        drawEquip(c);
        break;
    case Mode::Status:
        drawStatus(c);
        break;
    case Mode::GameOver:
        drawGameOver(c);
        break;
    case Mode::Jobs:
        drawJobs(c);
        break;
    }
    furi_mutex_release(a->mutex);
    Capture::onDraw(c);
}

// Runs on the GUI thread, so it must never block.
void inputCb(InputEvent* e, void* ctx) {
    App* a = (App*)ctx;
    if(furi_message_queue_put(a->queue, e, 0) != FuriStatusOk) a->dropped = a->dropped + 1;
}

} // namespace

extern "C" int32_t hazke_app(void* p) {
    (void)p;
    app.mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    app.queue = furi_message_queue_alloc(32, sizeof(InputEvent));
    app.vp = view_port_alloc();
    view_port_draw_callback_set(app.vp, drawCb, &app);
    view_port_input_callback_set(app.vp, inputCb, &app);
    app.gui = (Gui*)furi_record_open(RECORD_GUI);
    gui_add_view_port(app.gui, app.vp, GuiLayerFullscreen);
    NotificationApp* notif = (NotificationApp*)furi_record_open(RECORD_NOTIFICATION);
    notification_message(notif, &sequence_display_backlight_enforce_on);

    Audio::mode = Save::readSettings(Audio::All, lightScreen);
    if(Audio::mode >= Audio::ModeCount) Audio::mode = Audio::All;
    Audio::begin();
    Capture::begin();
    Galaxy::generate();
    memset(&keys, 0, sizeof(keys));
    goTitle();

    app.running = true;
    uint32_t last = furi_get_tick();
    uint32_t frame = 0;
    while(app.running) {
        uint32_t start = furi_get_tick();
        furi_mutex_acquire(app.mutex, FuriWaitForever);
        InputEvent e;
        bool gotInput = false;
        while(furi_message_queue_get(app.queue, &e, 0) == FuriStatusOk) {
            handleEvent(e);
            gotInput = true;
        }
        if(app.dropped) {
            // Lost events may include releases: let go of everything.
            app.dropped = 0;
            for(int k = 0; k < InputKeyMAX; k++)
                if(keys.held[k]) releaseKey(k);
        }
        releaseStaleKeys();
        float dt = (float)(start - last) / 1000.0f;
        if(dt > 0.1f) dt = 0.1f;
        last = start;
        step(dt);
        clearEdges();
        // Frame-buffer screens redraw every frame; text menus less often.
        bool fbMode = mode == Mode::Flight || mode == Mode::Hyperspace || mode == Mode::Title ||
                      mode == Mode::Map;
        furi_mutex_release(app.mutex);
        if(fbMode || gotInput || frame % Config::MenuFrameDiv == 0) view_port_update(app.vp);
        frame++;
        Capture::flush();
        uint32_t spent = furi_get_tick() - start;
        furi_delay_ms(spent < Config::FrameMs ? Config::FrameMs - spent : 1);
    }

    Audio::end();
    Capture::end();
    notification_message(notif, &sequence_display_backlight_enforce_auto);
    furi_record_close(RECORD_NOTIFICATION);
    view_port_enabled_set(app.vp, false);
    gui_remove_view_port(app.gui, app.vp);
    view_port_free(app.vp);
    furi_record_close(RECORD_GUI);
    furi_message_queue_free(app.queue);
    furi_mutex_free(app.mutex);
    return 0;
}
