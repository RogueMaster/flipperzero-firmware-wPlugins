#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <gui/modules/widget.h>
#include <loader/loader.h>
#include <storage/storage.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ck42x_dopewars_icons.h>

#define DW_TAG                "DopeFlipper"
#define DW_DRUGS              8
#define DW_LOCS               10
#define DW_LABELS             32
#define DW_LABEL_LEN          64
#define DW_MSG_LEN            384
#define DW_SAVE_MAGIC         0x44573432UL /* DW42 */
#define DW_SAVE_VERSION       2
#define DW_APP_DATA_ROOT      EXT_PATH("apps_data")
#define DW_SAVE_DIR           EXT_PATH("apps_data/ck42x_dopewars")
#define DW_SAVE_PATH          EXT_PATH("apps_data/ck42x_dopewars/save.bin")
#define DW_PROFILE_PATH       EXT_PATH("apps_data/ck42x_dopewars/profile.txt")
#define DW_STATS_PROFILE_PATH EXT_PATH("apps_data/ck42x_dopewars/stats.ck42x")
#define DW_BADUSB_WIN_PATH    EXT_PATH("apps_data/ck42x_dopewars/global_windows.txt")
#define DW_BADUSB_LINUX_PATH  EXT_PATH("apps_data/ck42x_dopewars/global_linux.txt")
#define DW_BADUSB_MAC_PATH    EXT_PATH("apps_data/ck42x_dopewars/global_mac.txt")
#define DW_LEADERBOARD_URL    "https://www.ck42x.com/dopeflipper"
#define DW_STATS_MAGIC        0x44575354UL /* DWST */
#define DW_STATS_VERSION      3
#define DW_STATS_PATH         EXT_PATH("apps_data/ck42x_dopewars/stats.bin")
#define DW_LEADERBOARD_SIZE   5
#define DW_FIGHT_SEQ_MAX      8
#define DW_RUN_BULLETS_MAX    4

#define DW_PRODUCT_BASE 100
#define DW_TRAVEL_BASE  200

typedef enum {
    DwViewMenu = 0,
    DwViewWidget,
    DwViewArt,
    DwViewHub,
} DwView;

typedef enum {
    DwModeLoading = 0,
    DwModeTitle,
    DwModeMain,
    DwModeBuyList,
    DwModeSellList,
    DwModeProduct,
    DwModeTravel,
    DwModeTravelArt,
    DwModeLoan,
    DwModeStats,
    DwModeLeaderboard,
    DwModeSaveReset,
    DwModeCop,
    DwModeCopFightGame,
    DwModeCopRunGame,
    DwModeEnd,
} DwMode;

typedef enum {
    DwEventTravel = 1,
    DwEventBankDeposit = 2,
    DwEventBankWithdraw = 3,
    DwEventLoanHalf = 4,
    DwEventLoanAll = 5,
    DwEventStatus = 6,
    DwEventAbout = 7,
    DwEventRestart = 8,
    DwEventCoatBuy = 9,
    DwEventBackMain = 10,
    DwEventSaveNow = 11,
    DwEventResetSave = 12,
    DwEventBuyScreen = 13,
    DwEventSellScreen = 14,
    DwEventLoanScreen = 15,
    DwEventLeaderboard = 16,
    DwEventStartGame = 17,
    DwEventSaveResetScreen = 18,
    DwEventStatsScreen = 19,
    DwEventBuy1 = 20,
    DwEventBuy5 = 21,
    DwEventBuy10 = 22,
    DwEventBuyMax = 23,
    DwEventSell1 = 24,
    DwEventSell5 = 25,
    DwEventSell10 = 26,
    DwEventSellAll = 27,
    DwEventCopFight = 30,
    DwEventCopRun = 31,
    DwEventCopSkillTick = 32,
    DwEventGlobalWindows = 33,
    DwEventGlobalLinux = 34,
    DwEventGlobalMac = 35,
    DwEventGlobalInfo = 36,
    DwEventGlobalFallback = 37,
    DwEventWidgetLeft = 40,
    DwEventWidgetRight = 41,
    DwEventWidgetCenter = 42,
} DwEventId;

typedef enum {
    DwMarketNone = 0,
    DwMarketSpike,
    DwMarketCrash,
} DwMarketEventType;

typedef struct {
    const char* name;
    const char* icon;
    uint16_t min;
    uint16_t max;
    uint16_t e_min;
    uint16_t e_max;
} DwDrug;

typedef struct {
    const char* name;
    const char* flavor;
    uint16_t bias[DW_DRUGS]; /* percent, 100 = normal */
} DwLocation;

typedef struct {
    DwMarketEventType type;
    uint8_t drug;
    const char* msg;
    uint8_t probability;
} DwMarketEvent;

typedef struct {
    int32_t net;
    int32_t profit;
    int32_t cash;
    int32_t debt;
    int32_t biggest_deal;
    uint32_t score_code;
    uint8_t heat;
    uint8_t cops_fought;
    uint8_t cops_ran;
    uint8_t days_left;
} DwScoreEntry;

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t size;
    uint32_t checksum;
    uint32_t games_played;
    uint32_t wins;
    uint32_t losses;
    int32_t best_net;
    int32_t biggest_deal;
    int32_t lifetime_bought;
    int32_t lifetime_sold;
    uint32_t cops_fought;
    uint32_t cops_ran;
    uint8_t highest_heat;
} DwStatsV1;

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t size;
    uint32_t checksum;
    uint32_t games_played;
    uint32_t wins;
    uint32_t losses;
    int32_t best_net;
    int32_t worst_net;
    int32_t biggest_deal;
    int32_t best_profit;
    int32_t best_cash;
    int32_t best_bank;
    int32_t lifetime_bought;
    int32_t lifetime_sold;
    uint32_t cops_fought;
    uint32_t cops_ran;
    uint32_t current_streak;
    uint32_t best_streak;
    uint32_t travel_days;
    uint8_t highest_heat;
    DwScoreEntry leaderboard[DW_LEADERBOARD_SIZE];
} DwStats;

typedef struct {
    Gui* gui;
    ViewDispatcher* dispatcher;
    Submenu* submenu;
    Widget* widget;
    View* art_view;
    View* hub_view;
    DwView current_view;
    DwMode mode;

    int32_t cash;
    int32_t debt;
    int32_t bank;
    uint8_t day;
    uint16_t max_coat;
    uint8_t heat;
    uint8_t location;
    uint16_t inventory[DW_DRUGS];
    int32_t prices[DW_DRUGS];
    int32_t total_bought;
    int32_t total_sold;
    int32_t biggest_deal;
    uint32_t buy_qty[DW_DRUGS];
    uint32_t sell_qty[DW_DRUGS];
    int32_t buy_value[DW_DRUGS];
    int32_t sell_value[DW_DRUGS];
    uint8_t cops_fought;
    uint8_t cops_ran;
    uint8_t cop_streak;
    uint8_t fight_wins;
    uint8_t fight_messy;
    uint8_t fight_losses;
    uint8_t run_clean;
    uint8_t run_messy;
    uint8_t run_caught;
    uint32_t action_count;
    uint32_t run_chain;
    uint32_t run_started_tick;

    uint8_t selected_product;
    uint8_t pending_officers;
    uint8_t cop_skill_target;
    uint8_t cop_skill_cursor;
    uint8_t cop_skill_moves;
    uint8_t cop_skill_width;
    uint8_t cop_skill_speed;
    uint32_t cop_skill_started;
    bool cop_skill_timer_running;
    FuriTimer* cop_skill_timer;
    uint8_t cop_game_phase;
    uint8_t cop_sequence[DW_FIGHT_SEQ_MAX];
    uint8_t cop_sequence_len;
    uint8_t cop_sequence_pos;
    uint8_t run_lane;
    uint8_t run_hits;
    uint8_t run_bullet_count;
    uint8_t run_bullet_x[DW_RUN_BULLETS_MAX];
    uint8_t run_bullet_lane[DW_RUN_BULLETS_MAX];
    uint16_t run_ticks_goal;
    bool coat_offer;
    uint8_t coat_increase;
    bool loaded_from_save;
    bool trade_sell;
    uint16_t trade_qty;
    uint8_t hub_selected;
    DwStats stats;

    char header[DW_LABEL_LEN];
    char labels[DW_LABELS][DW_LABEL_LEN];
    char text[DW_MSG_LEN];
    char last_msg[DW_MSG_LEN];
} DwApp;

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t size;
    uint32_t checksum;
    int32_t cash;
    int32_t debt;
    int32_t bank;
    uint8_t day;
    uint16_t max_coat;
    uint8_t heat;
    uint8_t location;
    uint16_t inventory[DW_DRUGS];
    int32_t prices[DW_DRUGS];
    int32_t total_bought;
    int32_t total_sold;
    int32_t biggest_deal;
    uint8_t cops_fought;
    uint8_t cops_ran;
    uint32_t action_count;
    uint32_t run_chain;
    uint8_t selected_product;
    uint8_t pending_officers;
    bool coat_offer;
    uint8_t coat_increase;
    char last_msg[DW_MSG_LEN];
} DwSave;

static const DwDrug dw_drugs[DW_DRUGS] = {
    {"Weed", "{L}", 15, 90, 10, 140},
    {"Shrooms", "oo", 20, 120, 8, 200},
    {"Acid", "[*]", 100, 500, 50, 1400},
    {"Ecstasy", "(D)", 150, 600, 60, 1800},
    {"Speed", ">>", 70, 350, 30, 900},
    {"Cocaine", "^^", 1200, 6000, 800, 18000},
    {"Heroin", "H!", 1500, 8000, 1000, 22000},
    {"Oxy", "[O]", 3000, 15000, 2000, 40000},
};

static const char* dw_drug_keys[DW_DRUGS] =
    {"weed", "shrooms", "acid", "ecstasy", "speed", "cocaine", "heroin", "oxy"};

static const DwLocation dw_locations[DW_LOCS] = {
    {"Bronx", "Burnt-out buildings and opportunity.", {70, 80, 90, 85, 75, 100, 100, 110}},
    {"Manhattan", "Money talks. Everything costs more.", {140, 130, 120, 130, 120, 140, 150, 130}},
    {"Brooklyn", "Gentrified blocks and raw corners.", {100, 110, 110, 120, 100, 90, 95, 100}},
    {"Queens", "Diverse. Unpredictable.", {100, 100, 100, 100, 110, 100, 100, 100}},
    {"Staten", "Quiet borough. Good connects.", {110, 110, 120, 110, 110, 80, 85, 80}},
    {"Harlem", "Legendary streets. OGs move different.", {60, 70, 80, 90, 80, 130, 120, 140}},
    {"Coney", "Boardwalk noise and fast flips.", {90, 120, 130, 150, 110, 105, 95, 90}},
    {"Jersey", "Tunnel tax. Weird supply.", {130, 95, 85, 100, 120, 115, 120, 110}},
    {"Newark", "Hot corners. Heavy pressure.", {75, 85, 100, 90, 95, 125, 135, 130}},
    {"Yonkers", "Quiet routes. Thin inventory.", {115, 105, 95, 85, 100, 90, 95, 125}},
};

static const DwMarketEvent dw_market_events[] = {
    {DwMarketSpike, 0, "Weed demand jumped. Prices spiked.", 8},
    {DwMarketSpike, 2, "Acid got scarce. Prices are wild.", 6},
    {DwMarketSpike, 3, "Rave tonight. Ecstasy demand is insane.", 7},
    {DwMarketSpike, 5, "Cocaine supply dried up. Prices mooning.", 5},
    {DwMarketSpike, 6, "Heroin pipeline got hit. Prices exploding.", 6},
    {DwMarketSpike, 7, "Pill mills raided. Oxy is scarce.", 5},
    {DwMarketCrash, 0, "Huge weed shipment landed. Prices tanked.", 8},
    {DwMarketCrash, 1, "Shrooms flooded the block. Bottom fell out.", 7},
    {DwMarketCrash, 4, "Speed lab evidence leaked to the street.", 6},
    {DwMarketCrash, 5, "Cocaine everywhere. Prices crashed.", 5},
    {DwMarketCrash, 6, "Heroin seized, then leaked. Streets flooded.", 4},
    {DwMarketCrash, 7, "Generic oxy hit. Prices cratered.", 4},
};

static const uint8_t dw_title_xbm[1024] = {
    0xC0, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x01,
    0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
    0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
    0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
    0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFE, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
    0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0xE0, 0x07, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
    0x40, 0x00, 0x00, 0x00, 0x00, 0x01, 0x3C, 0x00, 0xE0, 0xF9, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x01,
    0x40, 0x00, 0x00, 0x00, 0x00, 0x01, 0x06, 0x00, 0x00, 0x0F, 0x60, 0x00, 0x00, 0x00, 0x00, 0x01,
    0x40, 0x00, 0x00, 0x00, 0x00, 0x80, 0x01, 0x00, 0x00, 0x0C, 0x40, 0x00, 0x00, 0x00, 0x00, 0x01,
    0x40, 0x00, 0x00, 0x00, 0x00, 0xC0, 0x00, 0x00, 0x00, 0x38, 0x40, 0x00, 0x00, 0xC0, 0x01, 0x01,
    0x40, 0x00, 0x00, 0x00, 0x00, 0x60, 0x00, 0x00, 0x00, 0x60, 0x60, 0x00, 0x00, 0xE0, 0x03, 0x01,
    0x40, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0x00, 0xC0, 0x20, 0x00, 0x00, 0xE0, 0x03, 0x01,
    0x40, 0x00, 0x00, 0x80, 0x00, 0x08, 0x00, 0x00, 0x00, 0x80, 0x31, 0x00, 0x00, 0xE0, 0x03, 0x01,
    0x40, 0x00, 0x00, 0xC0, 0x03, 0x0C, 0x00, 0x00, 0x00, 0x00, 0x13, 0x00, 0x00, 0xE0, 0x01, 0x01,
    0x40, 0x00, 0x00, 0x80, 0x00, 0x06, 0x00, 0x00, 0x00, 0x00, 0x12, 0x00, 0x00, 0x00, 0x00, 0x01,
    0x40, 0x00, 0x00, 0x80, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x00, 0x06, 0x00, 0x00, 0x01,
    0x40, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x00, 0x02, 0x00, 0x00, 0x01,
    0x40, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x18, 0x00, 0x00, 0x00, 0x00, 0x01,
    0x40, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x01,
    0x40, 0x00, 0x00, 0x00, 0xB0, 0x01, 0x00, 0x00, 0xE0, 0x0F, 0x30, 0x00, 0x00, 0x00, 0x00, 0x01,
    0x40, 0x80, 0x01, 0x00, 0xF0, 0x00, 0x00, 0xF0, 0xFF, 0x0F, 0x20, 0x00, 0x00, 0x00, 0x00, 0x01,
    0x40, 0xC0, 0x07, 0x00, 0xF0, 0x01, 0xC0, 0xFF, 0xFF, 0x0F, 0x20, 0xC0, 0x00, 0x00, 0x00, 0x01,
    0x40, 0xE0, 0x07, 0x00, 0xF0, 0x03, 0xFC, 0xBF, 0xFF, 0x07, 0x60, 0xC0, 0x00, 0x00, 0x00, 0x01,
    0x40, 0xE0, 0x07, 0x00, 0xF0, 0x8F, 0xFF, 0xFF, 0x7F, 0x07, 0x40, 0x00, 0x00, 0x00, 0x00, 0x01,
    0x40, 0xE0, 0x07, 0x00, 0xF0, 0xFF, 0xFF, 0xDF, 0x7F, 0x03, 0x40, 0x00, 0x00, 0x00, 0x00, 0x01,
    0x40, 0xE0, 0x07, 0x00, 0xF0, 0xC3, 0xFF, 0xFF, 0xBF, 0x03, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x01,
    0x40, 0x00, 0x00, 0x00, 0xF0, 0x80, 0xFF, 0xFF, 0xCC, 0x01, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x01,
    0x40, 0x00, 0x80, 0x00, 0x30, 0x00, 0xFF, 0x7F, 0xF8, 0x00, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x01,
    0x40, 0x00, 0x80, 0x00, 0x1C, 0x00, 0xFE, 0xFF, 0x3F, 0x00, 0x80, 0x00, 0x18, 0x00, 0x00, 0x01,
    0x40, 0x00, 0x00, 0x00, 0x07, 0x00, 0xF8, 0x1F, 0x00, 0x00, 0x80, 0x01, 0x10, 0x00, 0x00, 0x01,
    0x40, 0x00, 0x00, 0xE0, 0x01, 0x00, 0xC0, 0x0F, 0x00, 0x00, 0x30, 0x01, 0x00, 0x00, 0x00, 0x01,
    0x40, 0x00, 0x00, 0x7C, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x18, 0x01, 0x00, 0x00, 0x00, 0x01,
    0x40, 0x00, 0x00, 0x86, 0x00, 0x00, 0x00, 0x78, 0x00, 0x00, 0x0C, 0x01, 0x00, 0x00, 0x00, 0x01,
    0x40, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x0F, 0x00, 0x00, 0x07, 0x01, 0x00, 0x00, 0x00, 0x01,
    0x40, 0x00, 0x00, 0x02, 0x00, 0x00, 0xE0, 0x01, 0x00, 0x80, 0x01, 0x01, 0x00, 0x00, 0x00, 0x01,
    0x40, 0x00, 0x00, 0x02, 0x00, 0x00, 0x3C, 0x00, 0x00, 0xC0, 0x00, 0x03, 0x00, 0x00, 0x00, 0x01,
    0x40, 0x00, 0x00, 0x06, 0x00, 0x80, 0x07, 0x00, 0x00, 0x30, 0x00, 0x03, 0x00, 0x00, 0x00, 0x01,
    0x40, 0x00, 0x00, 0xFC, 0x03, 0xFC, 0x00, 0x00, 0x00, 0x18, 0x00, 0x03, 0x00, 0x00, 0x00, 0x01,
    0x40, 0x00, 0x00, 0x88, 0xFF, 0x0F, 0x00, 0x00, 0x00, 0x0E, 0x00, 0x03, 0x00, 0x00, 0x00, 0x01,
    0x40, 0x00, 0x00, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x33, 0x0E, 0x00, 0x00, 0x01,
    0x40, 0x00, 0x00, 0x70, 0x00, 0x00, 0x00, 0x00, 0x80, 0x01, 0x00, 0x7B, 0x0F, 0x00, 0x00, 0x01,
    0x40, 0x00, 0x00, 0xC0, 0xFF, 0xFF, 0x1F, 0x00, 0xC0, 0x00, 0x80, 0xC6, 0x07, 0x00, 0x00, 0x01,
    0x40, 0x00, 0x00, 0x00, 0x00, 0x80, 0xFF, 0x01, 0x30, 0x00, 0xC0, 0x26, 0x03, 0x00, 0x00, 0x01,
    0x40, 0x00, 0x00, 0x00, 0x00, 0xC0, 0x18, 0x04, 0x18, 0x00, 0x60, 0x43, 0x06, 0x00, 0x00, 0x01,
    0x40, 0x00, 0x00, 0x00, 0x00, 0x60, 0x20, 0x00, 0x0C, 0x00, 0xB0, 0x86, 0x0C, 0x00, 0x00, 0x01,
    0x40, 0x00, 0x00, 0x00, 0x00, 0x30, 0x40, 0x00, 0x06, 0x00, 0xD8, 0x0E, 0x19, 0x00, 0x00, 0x01,
    0x40, 0x00, 0x00, 0x00, 0x00, 0x18, 0x80, 0x00, 0x01, 0x00, 0x6C, 0x1F, 0x12, 0x00, 0x00, 0x01,
    0x40, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x80, 0x01, 0x0D, 0x00, 0xB4, 0x3F, 0x18, 0x00, 0x00, 0x01,
    0x40, 0x00, 0x00, 0x00, 0x00, 0x06, 0xC0, 0x81, 0x1D, 0x00, 0xDA, 0x7F, 0x1E, 0x00, 0x00, 0x01,
    0x40, 0x00, 0x00, 0x00, 0x00, 0x03, 0x47, 0x81, 0x38, 0x00, 0x0F, 0xFF, 0x0E, 0x00, 0x00, 0x01,
    0x40, 0x00, 0x00, 0x00, 0x80, 0xFF, 0x47, 0xC1, 0x78, 0x80, 0x07, 0x7E, 0x07, 0x00, 0x00, 0x01,
    0x40, 0x00, 0x00, 0x00, 0x80, 0xDF, 0x61, 0x60, 0xF0, 0xC0, 0xC3, 0xBD, 0x03, 0x00, 0x00, 0x01,
    0x40, 0x00, 0x00, 0x00, 0x00, 0xEB, 0x20, 0x60, 0xE0, 0xC1, 0x6D, 0xD9, 0x01, 0x00, 0x00, 0x01,
    0x40, 0x00, 0x00, 0x00, 0x00, 0x3B, 0x20, 0x20, 0x20, 0xE7, 0xFE, 0xC1, 0x01, 0x00, 0x00, 0x01,
    0x40, 0x00, 0x00, 0x00, 0x80, 0x1F, 0x30, 0x10, 0x40, 0x3C, 0xFF, 0xE2, 0x01, 0x00, 0x00, 0x01,
    0x40, 0x00, 0x00, 0x00, 0x80, 0x1D, 0x10, 0x18, 0xC0, 0xB8, 0xFF, 0x72, 0x01, 0x00, 0x00, 0x01,
    0x40, 0x00, 0x00, 0x00, 0x80, 0x34, 0x10, 0x0C, 0x80, 0x98, 0xFF, 0xF9, 0x03, 0x00, 0x00, 0x01,
    0x40, 0x00, 0x00, 0x00, 0xC0, 0x26, 0x10, 0x06, 0x00, 0xE9, 0xFA, 0x3D, 0x03, 0x00, 0x00, 0x01,
    0x40, 0x00, 0x00, 0x00, 0x40, 0x67, 0x10, 0x03, 0x00, 0xDB, 0xFF, 0x9E, 0x02, 0x00, 0x00, 0x01,
    0x40, 0x00, 0x00, 0x00, 0x60, 0x46, 0x98, 0x01, 0xC0, 0x29, 0xFF, 0x4E, 0x02, 0x00, 0x00, 0x01,
    0x40, 0x00, 0x00, 0x00, 0x20, 0xC6, 0xD8, 0x00, 0x78, 0x58, 0x7A, 0x03, 0x03, 0x00, 0x00, 0x01,
    0x40, 0x00, 0x00, 0x00, 0x10, 0x86, 0x78, 0x00, 0x1E, 0xA4, 0xEC, 0x01, 0x02, 0x00, 0x00, 0x01,
    0xC0, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x01,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static void dw_show_loading(DwApp* app);
static void dw_show_title(DwApp* app);
static void dw_show_main(DwApp* app);
static void dw_show_buy_list(DwApp* app);
static void dw_show_sell_list(DwApp* app);
static void dw_show_loan(DwApp* app);
static void dw_show_leaderboard(DwApp* app);
static void dw_show_global_leaderboard(DwApp* app);
static void dw_show_save_reset(DwApp* app);
static void dw_show_stats(DwApp* app);
static void dw_show_product(DwApp* app, uint8_t drug_idx);
static void dw_show_travel(DwApp* app);
static void dw_show_travel_art(DwApp* app);
static void dw_show_cop(DwApp* app);
static void dw_start_cop_fight(DwApp* app);
static void dw_start_cop_run(DwApp* app);
static void dw_cop_fight(DwApp* app, uint8_t skill_score);
static void dw_cop_run(DwApp* app, uint8_t skill_score);
static void dw_end_game(DwApp* app);
static void dw_handle_event(DwApp* app, uint32_t event);
static void dw_save_game(DwApp* app);
static bool dw_load_game(DwApp* app);
static void dw_delete_save(void);
static void dw_show_run_status(DwApp* app);
static void dw_show_status(DwApp* app, const char* title, const char* body);
static void dw_load_stats(DwApp* app);
static void dw_save_stats(DwApp* app);
static void dw_append(char* dst, size_t size, const char* line);
static uint32_t dw_run_play_seconds(DwApp* app);
static uint32_t dw_score_code(DwApp* app, int32_t net, int32_t profit);
static uint32_t dw_profile_hash(DwApp* app, int32_t net, int32_t profit, int32_t inv_value);
static void dw_export_profile(
    DwApp* app,
    int32_t net,
    int32_t profit,
    int32_t inv_value,
    uint32_t score_code,
    bool finished);
static void dw_export_stats_profile(
    DwApp* app,
    int32_t net,
    int32_t profit,
    int32_t inv_value,
    uint32_t score_code,
    uint32_t profile_hash,
    bool finished);
static void dw_autosync_stats_profile(DwApp* app, bool finished);
static int32_t dw_run_inventory_value(DwApp* app, bool local_prices);
static int32_t dw_run_net(DwApp* app, bool local_prices);
static void dw_update_stats(DwApp* app, int32_t net, int32_t profit, int32_t inv_value);
static void dw_do_buy(DwApp* app, uint16_t requested);
static void dw_do_sell(DwApp* app, uint16_t requested);
static uint32_t dw_hash_u32(uint32_t seed, uint32_t value);
static const char* dw_rank_for(int32_t net);
static void dw_record_action(DwApp* app, uint8_t tag, int32_t value, uint16_t detail);

static void dw_ensure_data_dir(Storage* storage) {
    storage_common_mkdir(storage, DW_APP_DATA_ROOT);
    storage_common_mkdir(storage, DW_SAVE_DIR);
}

static uint32_t dw_rand(uint32_t max) {
    if(max == 0) return 0;
    return furi_hal_random_get() % max;
}

static uint32_t dw_rand_range(uint32_t min, uint32_t max) {
    if(max <= min) return min;
    return min + dw_rand(max - min + 1);
}

static uint16_t dw_coat_used(DwApp* app) {
    uint16_t used = 0;
    for(uint8_t i = 0; i < DW_DRUGS; i++)
        used += app->inventory[i];
    return used;
}

static int32_t dw_run_inventory_value(DwApp* app, bool local_prices) {
    int32_t inv_value = 0;
    for(uint8_t i = 0; i < DW_DRUGS; i++) {
        int32_t price = local_prices ? app->prices[i] :
                                       (int32_t)((dw_drugs[i].min + dw_drugs[i].max) / 2);
        inv_value += app->inventory[i] * price;
    }
    return inv_value;
}

static int32_t dw_run_net(DwApp* app, bool local_prices) {
    return app->cash + app->bank + dw_run_inventory_value(app, local_prices) - app->debt;
}

static uint8_t dw_days_elapsed(DwApp* app) {
    return app->day > 30 ? 0 : (uint8_t)(30 - app->day);
}

static uint8_t dw_run_pressure(DwApp* app) {
    uint16_t pressure =
        app->heat + (uint16_t)dw_days_elapsed(app) * 2U + (uint16_t)app->cop_streak * 3U;
    return pressure > 100 ? 100 : (uint8_t)pressure;
}

static uint8_t dw_heat_drop(DwApp* app, uint8_t base_drop) {
    uint8_t drop = base_drop + MIN((uint8_t)8, app->cop_streak / 2);
    if(drop > app->heat) drop = app->heat;
    app->heat -= drop;
    return drop;
}

static uint32_t dw_score_code(DwApp* app, int32_t net, int32_t profit) {
    uint32_t code = 2166136261UL;
    const int32_t values[] = {
        net,
        profit,
        app->cash,
        app->bank,
        app->debt,
        app->biggest_deal,
        app->total_bought,
        app->total_sold,
    };
    for(size_t i = 0; i < COUNT_OF(values); i++) {
        uint32_t value = (uint32_t)values[i];
        for(uint8_t b = 0; b < 4; b++) {
            code ^= (value >> (b * 8)) & 0xFF;
            code *= 16777619UL;
        }
    }
    code ^= app->heat;
    code *= 16777619UL;
    code ^= app->cops_fought;
    code *= 16777619UL;
    code ^= app->cops_ran;
    code *= 16777619UL;
    code ^= app->day;
    code *= 16777619UL;
    return code ? code : 1;
}

static uint32_t dw_profile_hash(DwApp* app, int32_t net, int32_t profit, int32_t inv_value) {
    uint32_t code = 2166136261UL;
    const int32_t values[] = {
        net,
        profit,
        inv_value,
        app->cash,
        app->bank,
        app->debt,
        app->total_bought,
        app->total_sold,
        app->biggest_deal,
        (int32_t)app->stats.games_played,
        app->stats.best_net,
        app->stats.biggest_deal,
    };
    for(size_t i = 0; i < COUNT_OF(values); i++) {
        uint32_t value = (uint32_t)values[i];
        for(uint8_t b = 0; b < 4; b++) {
            code ^= (value >> (b * 8)) & 0xFF;
            code *= 16777619UL;
        }
    }
    const uint8_t bytes[] = {
        app->day,
        app->heat,
        app->cops_fought,
        app->cops_ran,
        (uint8_t)(app->max_coat & 0xFF),
        (uint8_t)(app->max_coat >> 8),
    };
    for(size_t i = 0; i < COUNT_OF(bytes); i++) {
        code ^= bytes[i];
        code *= 16777619UL;
    }
    code = dw_hash_u32(code, app->action_count);
    code = dw_hash_u32(code, app->run_chain);
    code ^= 0xC42D0E42UL;
    code *= 16777619UL;
    return code ? code : 1;
}

static uint32_t dw_hash_u32(uint32_t seed, uint32_t value) {
    uint32_t code = seed;
    for(uint8_t b = 0; b < 4; b++) {
        code ^= (value >> (b * 8)) & 0xFF;
        code *= 16777619UL;
    }
    return code ? code : 1;
}

static void dw_record_action(DwApp* app, uint8_t tag, int32_t value, uint16_t detail) {
    if(!app) return;
    uint32_t code = app->run_chain ? app->run_chain : 2166136261UL;
    code = dw_hash_u32(code, tag);
    code = dw_hash_u32(code, (uint32_t)value);
    code = dw_hash_u32(code, detail);
    code = dw_hash_u32(code, app->day);
    code = dw_hash_u32(code, app->cash);
    code = dw_hash_u32(code, app->bank);
    code = dw_hash_u32(code, app->debt);
    code = dw_hash_u32(code, dw_coat_used(app));
    code = dw_hash_u32(code, app->heat);
    code ^= 0xA17C4E42UL;
    code *= 16777619UL;
    app->run_chain = code ? code : 1;
    if(app->action_count < 240) app->action_count++;
}

static uint32_t dw_hash_cstr_seed(uint32_t seed, const char* value) {
    uint32_t code = seed ? seed : 2166136261UL;
    if(!value) value = "";
    while(*value) {
        code ^= (uint8_t)*value++;
        code *= 16777619UL;
    }
    return code ? code : 1;
}

static void dw_sanitize_device_name(char* out, size_t out_size, const char* name) {
    if(!out || out_size == 0) return;
    if(!name || !*name) name = "Flipper";
    size_t j = 0;
    for(size_t i = 0; name[i] && j + 1 < out_size; i++) {
        char c = name[i];
        bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                  c == '_' || c == '-' || c == '.';
        out[j++] = ok ? c : '_';
    }
    out[j] = '\0';
    if(j == 0) strlcpy(out, "Flipper", out_size);
}

static uint32_t dw_stats_seed_for_device(const char* device_name) {
    uint32_t seed =
        dw_hash_cstr_seed(2166136261UL, device_name && *device_name ? device_name : "Flipper");
    seed ^= 0xA7D042C3UL;
    seed *= 16777619UL;
    return seed ? seed : 0x6D2B79F5UL;
}

static uint32_t dw_stats_stream_next(uint32_t state) {
    if(!state) state = 0x6D2B79F5UL;
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state ? state : 0x6D2B79F5UL;
}

static void dw_encode_stats_hex(
    const char* payload,
    char* encoded,
    size_t encoded_size,
    const char* device_name) {
    static const char hex[] = "0123456789ABCDEF";
    if(!payload || !encoded || encoded_size == 0) return;
    uint32_t state = dw_stats_seed_for_device(device_name);
    size_t out = 0;
    for(size_t i = 0; payload[i] && out + 2 < encoded_size; i++) {
        state = dw_stats_stream_next(state);
        uint8_t b = ((uint8_t)payload[i]) ^ (uint8_t)(state & 0xFF);
        encoded[out++] = hex[b >> 4];
        encoded[out++] = hex[b & 0x0F];
    }
    encoded[out] = '\0';
}

static uint32_t dw_device_lock(
    const char* device_name,
    uint32_t score_code,
    uint32_t profile_hash,
    uint32_t games_played) {
    uint32_t code =
        dw_hash_cstr_seed(2166136261UL, device_name && *device_name ? device_name : "Flipper");
    code = dw_hash_u32(code, score_code);
    code = dw_hash_u32(code, profile_hash);
    code = dw_hash_u32(code, games_played);
    code ^= 0xD0E57A75UL;
    code *= 16777619UL;
    return code ? code : 1;
}

static uint32_t dw_encoded_profile_seal(
    const char* encoded,
    const char* device_name,
    uint32_t score_code,
    uint32_t profile_hash) {
    uint32_t code = dw_hash_cstr_seed(2166136261UL, encoded);
    code = dw_hash_u32(
        code,
        dw_hash_cstr_seed(2166136261UL, device_name && *device_name ? device_name : "Flipper"));
    code = dw_hash_u32(code, score_code);
    code = dw_hash_u32(code, profile_hash);
    code ^= 0x51A7C0DEUL;
    code *= 16777619UL;
    return code ? code : 1;
}

static uint32_t dw_run_play_seconds(DwApp* app) {
    if(!app || app->day == 0 || app->run_started_tick == 0) return 0;
    uint32_t elapsed_ticks = furi_get_tick() - app->run_started_tick;
    return elapsed_ticks / furi_kernel_get_tick_frequency();
}

static int dw_format_profile(
    DwApp* app,
    char* profile,
    size_t profile_size,
    int32_t net,
    int32_t profit,
    int32_t inv_value,
    uint32_t score_code,
    bool finished) {
    if(!app || !profile || profile_size == 0) return -1;
    uint32_t profile_hash = dw_profile_hash(app, net, profit, inv_value);
    return snprintf(
        profile,
        profile_size,
        "ck42x_dopewars_profile_v1\n"
        "app=ck42x_dopewars\n"
        "version=%u\n"
        "finished=%u\n"
        "score=%ld\n"
        "net=%ld\n"
        "profit=%ld\n"
        "cash=%ld\n"
        "bank=%ld\n"
        "debt=%ld\n"
        "inventory_value=%ld\n"
        "total_bought=%ld\n"
        "total_sold=%ld\n"
        "biggest_deal=%ld\n"
        "cops_fought=%u\n"
        "cops_ran=%u\n"
        "heat=%u\n"
        "day=%u\n"
        "max_coat=%u\n"
        "games_played=%lu\n"
        "best_net=%ld\n"
        "best_streak=%lu\n"
        "stats_biggest_deal=%ld\n"
        "action_count=%lu\n"
        "run_chain=%08lX\n"
        "score_code=%08lX\n"
        "profile_hash=%08lX\n",
        (unsigned)DW_STATS_VERSION,
        finished ? 1U : 0U,
        (long)net,
        (long)net,
        (long)profit,
        (long)app->cash,
        (long)app->bank,
        (long)app->debt,
        (long)inv_value,
        (long)app->total_bought,
        (long)app->total_sold,
        (long)app->biggest_deal,
        app->cops_fought,
        app->cops_ran,
        app->heat,
        app->day,
        app->max_coat,
        (unsigned long)app->stats.games_played,
        (long)app->stats.best_net,
        (unsigned long)app->stats.best_streak,
        (long)app->stats.biggest_deal,
        (unsigned long)app->action_count,
        (unsigned long)app->run_chain,
        (unsigned long)score_code,
        (unsigned long)profile_hash);
}

static void dw_hex_encode(const char* input, size_t input_len, char* output, size_t output_size) {
    static const char hex[] = "0123456789ABCDEF";
    if(!input || !output || output_size == 0) return;
    size_t out = 0;
    for(size_t i = 0; i < input_len && out + 2 < output_size; i++) {
        uint8_t b = (uint8_t)input[i];
        output[out++] = hex[b >> 4];
        output[out++] = hex[b & 0x0F];
    }
    output[out] = '\0';
}

static void dw_export_stats_profile(
    DwApp* app,
    int32_t net,
    int32_t profit,
    int32_t inv_value,
    uint32_t score_code,
    uint32_t profile_hash,
    bool finished) {
    static char device_name[32];
    static char payload[2304];
    static char product_lines[1024];
    static char encoded[4609];
    static char package[5120];
    dw_sanitize_device_name(
        device_name, sizeof(device_name), furi_hal_version_get_device_name_ptr());
    product_lines[0] = '\0';
    for(uint8_t i = 0; i < DW_DRUGS; i++) {
        char line[128];
        snprintf(
            line,
            sizeof(line),
            "buy_qty_%s=%lu\nsell_qty_%s=%lu\nbuy_value_%s=%ld\nsell_value_%s=%ld\n",
            dw_drug_keys[i],
            (unsigned long)app->buy_qty[i],
            dw_drug_keys[i],
            (unsigned long)app->sell_qty[i],
            dw_drug_keys[i],
            (long)app->buy_value[i],
            dw_drug_keys[i],
            (long)app->sell_value[i]);
        dw_append(product_lines, sizeof(product_lines), line);
    }
    uint32_t device_lock =
        dw_device_lock(device_name, score_code, profile_hash, app->stats.games_played);
    int payload_len = snprintf(
        payload,
        sizeof(payload),
        "ck42x_dopewars_device_stats_v1\n"
        "app=ck42x_dopewars\n"
        "format=device_stats_v1\n"
        "version=%u\n"
        "finished=%u\n"
        "flipper_name=%s\n"
        "device_lock=%08lX\n"
        "score=%ld\n"
        "net=%ld\n"
        "profit=%ld\n"
        "cash=%ld\n"
        "bank=%ld\n"
        "debt=%ld\n"
        "inventory_value=%ld\n"
        "total_bought=%ld\n"
        "total_sold=%ld\n"
        "biggest_deal=%ld\n"
        "cops_fought=%u\n"
        "cops_ran=%u\n"
        "fight_wins=%u\n"
        "fight_messy=%u\n"
        "fight_losses=%u\n"
        "run_clean=%u\n"
        "run_messy=%u\n"
        "run_caught=%u\n"
        "heat=%u\n"
        "day=%u\n"
        "max_coat=%u\n"
        "games_played=%lu\n"
        "best_net=%ld\n"
        "best_streak=%lu\n"
        "stats_biggest_deal=%ld\n"
        "play_seconds=%lu\n"
        "%s"
        "action_count=%lu\n"
        "run_chain=%08lX\n"
        "score_code=%08lX\n"
        "profile_hash=%08lX\n"
        "share=ck42x.com/dopeflipper\n",
        (unsigned)DW_STATS_VERSION,
        finished ? 1U : 0U,
        device_name,
        (unsigned long)device_lock,
        (long)net,
        (long)net,
        (long)profit,
        (long)app->cash,
        (long)app->bank,
        (long)app->debt,
        (long)inv_value,
        (long)app->total_bought,
        (long)app->total_sold,
        (long)app->biggest_deal,
        app->cops_fought,
        app->cops_ran,
        app->fight_wins,
        app->fight_messy,
        app->fight_losses,
        app->run_clean,
        app->run_messy,
        app->run_caught,
        app->heat,
        app->day,
        app->max_coat,
        (unsigned long)app->stats.games_played,
        (long)app->stats.best_net,
        (unsigned long)app->stats.best_streak,
        (long)app->stats.biggest_deal,
        (unsigned long)dw_run_play_seconds(app),
        product_lines,
        (unsigned long)app->action_count,
        (unsigned long)app->run_chain,
        (unsigned long)score_code,
        (unsigned long)profile_hash);
    if(payload_len <= 0 || (size_t)payload_len >= sizeof(payload)) return;
    dw_encode_stats_hex(payload, encoded, sizeof(encoded), device_name);
    uint32_t seal = dw_encoded_profile_seal(encoded, device_name, score_code, profile_hash);
    int package_len = snprintf(
        package,
        sizeof(package),
        "CK42X_DWSTAT_V1\n"
        "encoding=xor_hex_fnv1a_device\n"
        "device_hint=%08lX\n"
        "payload=%s\n"
        "seal=%08lX\n",
        (unsigned long)dw_hash_cstr_seed(2166136261UL, device_name),
        encoded,
        (unsigned long)seal);
    if(package_len <= 0) return;
    size_t bytes = (size_t)package_len;
    if(bytes >= sizeof(package)) bytes = sizeof(package) - 1;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    dw_ensure_data_dir(storage);
    File* file = storage_file_alloc(storage);
    if(storage_file_open(file, DW_STATS_PROFILE_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        size_t written = storage_file_write(file, package, bytes);
        storage_file_sync(file);
        if(written != bytes)
            FURI_LOG_W(DW_TAG, "Stats profile write short: %u", (unsigned)written);
    } else {
        FURI_LOG_W(DW_TAG, "Could not open stats profile for write");
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

static void dw_autosync_stats_profile(DwApp* app, bool finished) {
    if(!app || app->day == 0) return;
    int32_t inv_value = dw_run_inventory_value(app, true);
    int32_t net = dw_run_net(app, true);
    int32_t profit = app->total_sold - app->total_bought;
    uint32_t score_code = dw_score_code(app, net, profit);
    uint32_t profile_hash = dw_profile_hash(app, net, profit, inv_value);
    dw_export_stats_profile(app, net, profit, inv_value, score_code, profile_hash, finished);
}

static bool dw_write_text_file(const char* path, const char* text) {
    if(!path || !text) return false;
    Storage* storage = furi_record_open(RECORD_STORAGE);
    dw_ensure_data_dir(storage);
    File* file = storage_file_alloc(storage);
    bool ok = false;
    if(storage_file_open(file, path, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        size_t bytes = strlen(text);
        size_t written = storage_file_write(file, text, bytes);
        storage_file_sync(file);
        ok = written == bytes;
        if(!ok) FURI_LOG_W(DW_TAG, "Text write short: %u", (unsigned)written);
    } else {
        FURI_LOG_W(DW_TAG, "Could not open text file for write");
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return ok;
}

static bool dw_build_global_url(DwApp* app, char* url, size_t url_size) {
    if(!app || !url || url_size == 0) return false;
    bool finished = app->day == 0;
    int32_t inv_value = dw_run_inventory_value(app, !finished);
    int32_t net = dw_run_net(app, !finished);
    int32_t profit = app->total_sold - app->total_bought;
    uint32_t score_code = dw_score_code(app, net, profit);
    char profile[1024];
    char encoded[2049];
    int len = dw_format_profile(
        app, profile, sizeof(profile), net, profit, inv_value, score_code, finished);
    if(len <= 0 || (size_t)len >= sizeof(profile)) return false;
    dw_hex_encode(profile, (size_t)len, encoded, sizeof(encoded));
    int url_len =
        snprintf(url, url_size, "%s?profile_hex=%s&submit=1", DW_LEADERBOARD_URL, encoded);
    return url_len > 0 && (size_t)url_len < url_size;
}

static bool dw_prepare_badusb_script(DwApp* app, const char* path, const char* os_name) {
    if(!app || !path || !os_name) return false;
    if(app->day != 0) return false;
    char url[2300];
    char script[2600];
    if(!dw_build_global_url(app, url, sizeof(url))) return false;
    if(strcmp(os_name, "windows") == 0) {
        snprintf(script, sizeof(script), "DELAY 700\nGUI r\nDELAY 450\nSTRING %s\nENTER\n", url);
    } else if(strcmp(os_name, "linux") == 0) {
        snprintf(
            script,
            sizeof(script),
            "DELAY 700\nALT F2\nDELAY 450\nSTRING xdg-open %s\nENTER\n",
            url);
    } else {
        snprintf(
            script, sizeof(script), "DELAY 700\nGUI SPACE\nDELAY 600\nSTRING %s\nENTER\n", url);
    }
    return dw_write_text_file(path, script);
}

static void dw_run_global_badusb(DwApp* app, const char* path, const char* os_name) {
    if(app->day != 0) {
        dw_show_status(
            app,
            "Global Board",
            "Finish a run first.\n\nFallback site:\nhttps://www.ck42x.com/dopeflipper");
        return;
    }
    if(!dw_prepare_badusb_script(app, path, os_name)) {
        dw_show_status(
            app,
            "Global Board",
            "Could not build BadUSB script.\n\nFallback site:\nhttps://www.ck42x.com/dopeflipper\nProfile file:\n/ext/apps_data/ck42x_dopewars/profile.txt");
        return;
    }
    Loader* loader = furi_record_open(RECORD_LOADER);
    loader_enqueue_launch(loader, "bad_usb", path, LoaderDeferredLaunchFlagGui);
    furi_record_close(RECORD_LOADER);
    view_dispatcher_stop(app->dispatcher);
}

static void dw_export_profile(
    DwApp* app,
    int32_t net,
    int32_t profit,
    int32_t inv_value,
    uint32_t score_code,
    bool finished) {
    char profile[1024];
    int len = dw_format_profile(
        app, profile, sizeof(profile), net, profit, inv_value, score_code, finished);
    if(len <= 0) return;
    size_t bytes = (size_t)len;
    if(bytes >= sizeof(profile)) bytes = sizeof(profile) - 1;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    dw_ensure_data_dir(storage);
    File* file = storage_file_alloc(storage);
    if(storage_file_open(file, DW_PROFILE_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        size_t written = storage_file_write(file, profile, bytes);
        storage_file_sync(file);
        if(written != bytes) FURI_LOG_W(DW_TAG, "Profile write short: %u", (unsigned)written);
    } else {
        FURI_LOG_W(DW_TAG, "Could not open profile for write");
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    uint32_t profile_hash = dw_profile_hash(app, net, profit, inv_value);
    dw_export_stats_profile(app, net, profit, inv_value, score_code, profile_hash, finished);
}

static void dw_tone(float frequency, uint16_t duration_ms, float volume) {
    if(furi_hal_speaker_acquire(30)) {
        furi_hal_speaker_start(frequency, volume);
        furi_delay_ms(duration_ms);
        furi_hal_speaker_stop();
        furi_hal_speaker_release();
    } else {
        furi_delay_ms(duration_ms);
    }
}

static void dw_pause(uint16_t duration_ms) {
    furi_delay_ms(duration_ms);
}

static void dw_sfx_good(void) {
    furi_hal_vibro_on(true);
    dw_tone(660.0f, 35, 0.35f);
    furi_hal_vibro_on(false);
    dw_pause(20);
    dw_tone(880.0f, 45, 0.35f);
}

static void dw_sfx_bad(void) {
    furi_hal_vibro_on(true);
    dw_tone(220.0f, 90, 0.4f);
    furi_hal_vibro_on(false);
    dw_pause(25);
    dw_tone(160.0f, 120, 0.35f);
}

static void dw_sfx_buy(void) {
    dw_tone(520.0f, 28, 0.3f);
    dw_pause(15);
    dw_tone(740.0f, 32, 0.3f);
}

static void dw_sfx_sell(void) {
    dw_tone(880.0f, 28, 0.3f);
    dw_pause(15);
    dw_tone(660.0f, 32, 0.3f);
}

static void dw_sfx_tick(bool up) {
    furi_hal_vibro_on(true);
    dw_tone(up ? 1175.0f : 740.0f, 34, 0.45f);
    furi_hal_vibro_on(false);
}

static void dw_sfx_travel(void) {
    dw_tone(330.0f, 35, 0.25f);
    dw_pause(18);
    dw_tone(392.0f, 35, 0.25f);
    dw_pause(18);
    dw_tone(494.0f, 45, 0.25f);
}

static void dw_sfx_cop(void) {
    for(uint8_t i = 0; i < 3; i++) {
        dw_tone(1040.0f, 45, 0.38f);
        dw_pause(25);
        dw_tone(760.0f, 45, 0.38f);
        dw_pause(25);
    }
}

static void dw_sfx_game_over(bool win) {
    if(win) {
        dw_tone(500.0f, 70, 0.35f);
        dw_pause(30);
        dw_tone(660.0f, 70, 0.35f);
        dw_pause(30);
        dw_tone(880.0f, 120, 0.35f);
    } else {
        dw_tone(360.0f, 100, 0.35f);
        dw_pause(35);
        dw_tone(260.0f, 130, 0.35f);
        dw_pause(35);
        dw_tone(160.0f, 180, 0.35f);
    }
}

static uint32_t dw_save_checksum(DwSave* save) {
    uint32_t old = save->checksum;
    save->checksum = 0;
    const uint8_t* bytes = (const uint8_t*)save;
    uint32_t sum = 2166136261UL;
    for(size_t i = 0; i < sizeof(DwSave); i++) {
        sum ^= bytes[i];
        sum *= 16777619UL;
    }
    save->checksum = old;
    return sum;
}

static bool dw_save_is_valid(DwSave* save) {
    if(save->magic != DW_SAVE_MAGIC) return false;
    if(save->version != DW_SAVE_VERSION) return false;
    if(save->size != sizeof(DwSave)) return false;
    if(dw_save_checksum(save) != save->checksum) return false;
    if(save->day == 0 || save->day > 30) return false;
    if(save->location >= DW_LOCS) return false;
    if(save->selected_product >= DW_DRUGS) return false;
    if(save->heat > 100) return false;
    if(save->max_coat < 20 || save->max_coat > 500) return false;
    uint16_t used = 0;
    for(uint8_t i = 0; i < DW_DRUGS; i++) {
        used += save->inventory[i];
        if(save->prices[i] <= 0) return false;
    }
    if(used > save->max_coat) return false;
    save->last_msg[DW_MSG_LEN - 1] = '\0';
    return true;
}

static void dw_fill_save(DwSave* save, DwApp* app) {
    memset(save, 0, sizeof(DwSave));
    save->magic = DW_SAVE_MAGIC;
    save->version = DW_SAVE_VERSION;
    save->size = sizeof(DwSave);
    save->cash = app->cash;
    save->debt = app->debt;
    save->bank = app->bank;
    save->day = app->day;
    save->max_coat = app->max_coat;
    save->heat = app->heat;
    save->location = app->location;
    memcpy(save->inventory, app->inventory, sizeof(save->inventory));
    memcpy(save->prices, app->prices, sizeof(save->prices));
    save->total_bought = app->total_bought;
    save->total_sold = app->total_sold;
    save->biggest_deal = app->biggest_deal;
    save->cops_fought = app->cops_fought;
    save->cops_ran = app->cops_ran;
    save->action_count = app->action_count;
    save->run_chain = app->run_chain;
    save->selected_product = app->selected_product;
    save->pending_officers = app->pending_officers;
    save->coat_offer = app->coat_offer;
    save->coat_increase = app->coat_increase;
    strncpy(save->last_msg, app->last_msg, sizeof(save->last_msg) - 1);
    save->checksum = dw_save_checksum(save);
}

static void dw_apply_save(DwApp* app, DwSave* save) {
    app->cash = save->cash;
    app->debt = save->debt;
    app->bank = save->bank;
    app->day = save->day;
    app->max_coat = save->max_coat;
    app->heat = save->heat;
    app->location = save->location;
    memcpy(app->inventory, save->inventory, sizeof(app->inventory));
    memcpy(app->prices, save->prices, sizeof(app->prices));
    app->total_bought = save->total_bought;
    app->total_sold = save->total_sold;
    app->biggest_deal = save->biggest_deal;
    app->cops_fought = save->cops_fought;
    app->cops_ran = save->cops_ran;
    app->action_count = save->action_count;
    app->run_chain = save->run_chain;
    app->selected_product = save->selected_product;
    app->pending_officers = save->pending_officers;
    app->coat_offer = save->coat_offer;
    app->coat_increase = save->coat_increase;
    strncpy(app->last_msg, save->last_msg, sizeof(app->last_msg) - 1);
    app->last_msg[sizeof(app->last_msg) - 1] = '\0';
    app->loaded_from_save = true;
}

static void dw_save_game(DwApp* app) {
    if(app->day == 0) return;
    Storage* storage = furi_record_open(RECORD_STORAGE);
    dw_ensure_data_dir(storage);
    File* file = storage_file_alloc(storage);
    if(storage_file_open(file, DW_SAVE_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        DwSave save;
        dw_fill_save(&save, app);
        size_t written = storage_file_write(file, &save, sizeof(save));
        storage_file_sync(file);
        if(written != sizeof(save)) FURI_LOG_W(DW_TAG, "Save write short: %u", (unsigned)written);
    } else {
        FURI_LOG_W(DW_TAG, "Could not open save for write");
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    dw_autosync_stats_profile(app, false);
}

static bool dw_load_game(DwApp* app) {
    bool loaded = false;
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    if(storage_file_open(file, DW_SAVE_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        DwSave save;
        size_t read = storage_file_read(file, &save, sizeof(save));
        if(read == sizeof(save) && dw_save_is_valid(&save)) {
            dw_apply_save(app, &save);
            loaded = true;
        } else {
            FURI_LOG_W(DW_TAG, "Ignoring invalid save");
        }
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return loaded;
}

static void dw_delete_save(void) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_common_remove(storage, DW_SAVE_PATH);
    furi_record_close(RECORD_STORAGE);
}

static uint32_t dw_stats_checksum(DwStats* stats) {
    uint32_t old = stats->checksum;
    stats->checksum = 0;
    const uint8_t* bytes = (const uint8_t*)stats;
    uint32_t sum = 2166136261UL;
    for(size_t i = 0; i < sizeof(DwStats); i++) {
        sum ^= bytes[i];
        sum *= 16777619UL;
    }
    stats->checksum = old;
    return sum;
}

static uint32_t dw_stats_v1_checksum(DwStatsV1* stats) {
    uint32_t old = stats->checksum;
    stats->checksum = 0;
    const uint8_t* bytes = (const uint8_t*)stats;
    uint32_t sum = 2166136261UL;
    for(size_t i = 0; i < sizeof(DwStatsV1); i++) {
        sum ^= bytes[i];
        sum *= 16777619UL;
    }
    stats->checksum = old;
    return sum;
}

static void dw_init_stats(DwApp* app) {
    memset(&app->stats, 0, sizeof(app->stats));
    app->stats.magic = DW_STATS_MAGIC;
    app->stats.version = DW_STATS_VERSION;
    app->stats.size = sizeof(DwStats);
    app->stats.checksum = dw_stats_checksum(&app->stats);
}

static bool dw_stats_is_valid(DwStats* stats) {
    if(stats->magic != DW_STATS_MAGIC) return false;
    if(stats->version != DW_STATS_VERSION) return false;
    if(stats->size != sizeof(DwStats)) return false;
    if(dw_stats_checksum(stats) != stats->checksum) return false;
    if(stats->highest_heat > 100) return false;
    for(uint8_t i = 0; i < DW_LEADERBOARD_SIZE; i++) {
        if(stats->leaderboard[i].heat > 100) return false;
    }
    return true;
}

static bool dw_stats_v1_is_valid(DwStatsV1* stats) {
    if(stats->magic != DW_STATS_MAGIC) return false;
    if(stats->version != 1) return false;
    if(stats->size != sizeof(DwStatsV1)) return false;
    if(dw_stats_v1_checksum(stats) != stats->checksum) return false;
    if(stats->highest_heat > 100) return false;
    return true;
}

static void dw_migrate_stats_v1(DwApp* app, DwStatsV1* old) {
    dw_init_stats(app);
    app->stats.games_played = old->games_played;
    app->stats.wins = old->wins;
    app->stats.losses = old->losses;
    app->stats.best_net = old->best_net;
    app->stats.worst_net = old->games_played ? old->best_net : 0;
    app->stats.biggest_deal = old->biggest_deal;
    app->stats.best_profit = 0;
    app->stats.lifetime_bought = old->lifetime_bought;
    app->stats.lifetime_sold = old->lifetime_sold;
    app->stats.cops_fought = old->cops_fought;
    app->stats.cops_ran = old->cops_ran;
    app->stats.highest_heat = old->highest_heat;
    if(old->games_played) {
        app->stats.leaderboard[0].net = old->best_net;
        app->stats.leaderboard[0].biggest_deal = old->biggest_deal;
        app->stats.leaderboard[0].heat = old->highest_heat;
        app->stats.leaderboard[0].score_code = old->best_net ? (uint32_t)old->best_net : 1;
    }
}

static void dw_load_stats(DwApp* app) {
    bool loaded = false;
    bool migrated = false;
    dw_init_stats(app);
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    if(storage_file_open(file, DW_STATS_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        DwStats stats;
        memset(&stats, 0, sizeof(stats));
        size_t read = storage_file_read(file, &stats, sizeof(stats));
        if(read == sizeof(stats) && dw_stats_is_valid(&stats)) {
            app->stats = stats;
            loaded = true;
        } else if(read == sizeof(DwStatsV1)) {
            DwStatsV1* old = (DwStatsV1*)&stats;
            if(dw_stats_v1_is_valid(old)) {
                dw_migrate_stats_v1(app, old);
                loaded = true;
                migrated = true;
            } else {
                FURI_LOG_W(DW_TAG, "Ignoring invalid v1 stats file");
            }
        } else {
            FURI_LOG_W(DW_TAG, "Ignoring invalid stats file");
        }
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    if(!loaded) dw_save_stats(app);
    if(migrated) {
        dw_save_stats(app);
        FURI_LOG_I(DW_TAG, "Migrated stats to v2");
    }
}

static void dw_save_stats(DwApp* app) {
    app->stats.magic = DW_STATS_MAGIC;
    app->stats.version = DW_STATS_VERSION;
    app->stats.size = sizeof(DwStats);
    app->stats.checksum = dw_stats_checksum(&app->stats);
    Storage* storage = furi_record_open(RECORD_STORAGE);
    dw_ensure_data_dir(storage);
    File* file = storage_file_alloc(storage);
    if(storage_file_open(file, DW_STATS_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        size_t written = storage_file_write(file, &app->stats, sizeof(app->stats));
        storage_file_sync(file);
        if(written != sizeof(app->stats))
            FURI_LOG_W(DW_TAG, "Stats write short: %u", (unsigned)written);
    } else {
        FURI_LOG_W(DW_TAG, "Could not open stats for write");
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

static void dw_insert_leaderboard(DwApp* app, int32_t net, int32_t profit) {
    DwScoreEntry entry;
    memset(&entry, 0, sizeof(entry));
    entry.net = net;
    entry.profit = profit;
    entry.cash = app->cash;
    entry.debt = app->debt;
    entry.biggest_deal = app->biggest_deal;
    entry.score_code = dw_score_code(app, net, profit);
    entry.heat = app->heat;
    entry.cops_fought = app->cops_fought;
    entry.cops_ran = app->cops_ran;
    entry.days_left = app->day;

    uint8_t pos = DW_LEADERBOARD_SIZE;
    for(uint8_t i = 0; i < DW_LEADERBOARD_SIZE; i++) {
        if(app->stats.leaderboard[i].score_code == 0 || net > app->stats.leaderboard[i].net) {
            pos = i;
            break;
        }
    }
    if(pos >= DW_LEADERBOARD_SIZE) return;
    for(uint8_t i = DW_LEADERBOARD_SIZE - 1; i > pos; i--) {
        app->stats.leaderboard[i] = app->stats.leaderboard[i - 1];
    }
    app->stats.leaderboard[pos] = entry;
}

static void dw_update_stats(DwApp* app, int32_t net, int32_t profit, int32_t inv_value) {
    UNUSED(inv_value);
    bool first_game = app->stats.games_played == 0;
    app->stats.games_played++;
    if(net >= 0) {
        app->stats.wins++;
        app->stats.current_streak++;
        if(app->stats.current_streak > app->stats.best_streak)
            app->stats.best_streak = app->stats.current_streak;
    } else {
        app->stats.losses++;
        app->stats.current_streak = 0;
    }
    if(first_game || net > app->stats.best_net) app->stats.best_net = net;
    if(first_game || net < app->stats.worst_net) app->stats.worst_net = net;
    if(app->biggest_deal > app->stats.biggest_deal) app->stats.biggest_deal = app->biggest_deal;
    if(first_game || profit > app->stats.best_profit) app->stats.best_profit = profit;
    if(first_game || app->cash > app->stats.best_cash) app->stats.best_cash = app->cash;
    if(first_game || app->bank > app->stats.best_bank) app->stats.best_bank = app->bank;
    if(app->total_bought > 0) app->stats.lifetime_bought += app->total_bought;
    if(app->total_sold > 0) app->stats.lifetime_sold += app->total_sold;
    app->stats.cops_fought += app->cops_fought;
    app->stats.cops_ran += app->cops_ran;
    app->stats.travel_days += 30 - app->day;
    if(app->heat > app->stats.highest_heat) app->stats.highest_heat = app->heat;
    dw_insert_leaderboard(app, net, profit);
    dw_save_stats(app);
}

static void dw_append(char* dst, size_t size, const char* line) {
    size_t used = strlen(dst);
    if(used + 1 >= size) return;
    snprintf(dst + used, size - used, "%s", line);
}

static void dw_new_game(DwApp* app) {
    app->cash = 1500;
    app->debt = 6500;
    app->bank = 0;
    app->day = 30;
    app->max_coat = 100;
    app->heat = 0;
    app->location = 0;
    app->total_bought = 0;
    app->total_sold = 0;
    app->biggest_deal = 0;
    memset(app->buy_qty, 0, sizeof(app->buy_qty));
    memset(app->sell_qty, 0, sizeof(app->sell_qty));
    memset(app->buy_value, 0, sizeof(app->buy_value));
    memset(app->sell_value, 0, sizeof(app->sell_value));
    app->cops_fought = 0;
    app->cops_ran = 0;
    app->cop_streak = 0;
    app->fight_wins = 0;
    app->fight_messy = 0;
    app->fight_losses = 0;
    app->run_clean = 0;
    app->run_messy = 0;
    app->run_caught = 0;
    app->action_count = 0;
    app->run_started_tick = furi_get_tick();
    app->run_chain =
        dw_hash_u32(furi_hal_random_get() ^ 0xD042A17CUL, app->stats.games_played + 1);
    app->selected_product = 0;
    app->pending_officers = 0;
    app->coat_offer = false;
    app->coat_increase = 0;
    app->loaded_from_save = false;
    memset(app->inventory, 0, sizeof(app->inventory));
    snprintf(app->last_msg, sizeof(app->last_msg), "30 days. Pay the shark. No handouts.");
}

static void dw_generate_prices(DwApp* app, int8_t override[DW_DRUGS]) {
    const DwLocation* loc = &dw_locations[app->location];
    for(uint8_t i = 0; i < DW_DRUGS; i++) {
        const DwDrug* drug = &dw_drugs[i];
        uint32_t min = drug->min;
        uint32_t max = drug->max;
        if(override && override[i] == 1) {
            min = drug->max;
            max = drug->e_max;
        } else if(override && override[i] == -1) {
            min = drug->e_min;
            max = drug->min;
        }
        min = (min * loc->bias[i]) / 100;
        max = (max * loc->bias[i]) / 100;
        if(max < min) max = min;
        app->prices[i] = (int32_t)dw_rand_range(min, max);
        if(app->prices[i] < 1) app->prices[i] = 1;
    }
}

static void dw_start_round(DwApp* app) {
    int8_t overrides[DW_DRUGS];
    memset(overrides, 0, sizeof(overrides));
    snprintf(
        app->last_msg,
        sizeof(app->last_msg),
        "%s\n%s\n",
        dw_locations[app->location].name,
        dw_locations[app->location].flavor);

    uint8_t market_hits = 0;
    for(size_t i = 0; i < COUNT_OF(dw_market_events); i++) {
        const DwMarketEvent* evt = &dw_market_events[i];
        if(dw_rand(100) < evt->probability) {
            overrides[evt->drug] = (evt->type == DwMarketSpike) ? 1 : -1;
            dw_append(app->last_msg, sizeof(app->last_msg), evt->msg);
            dw_append(app->last_msg, sizeof(app->last_msg), "\n");
            market_hits++;
            if(market_hits >= 2) break;
        }
    }
    if(market_hits == 0 && dw_rand(100) < 25) {
        dw_append(app->last_msg, sizeof(app->last_msg), "Quiet market. Read the board.\n");
    }

    dw_generate_prices(app, overrides);

    if(app->debt > 20000 && dw_rand(100) < 15) {
        int32_t squeeze = MIN(app->cash, app->debt / 20);
        if(squeeze > 0) {
            app->cash -= squeeze;
            app->heat = MIN(100, app->heat + 5);
            char line[96];
            snprintf(
                line,
                sizeof(line),
                "Shark pressure. Paid $%ld to stay breathing.\n",
                (long)squeeze);
            dw_append(app->last_msg, sizeof(app->last_msg), line);
        }
    }

    if(dw_rand(100) < 9 && app->cash > 0) {
        int32_t loss = (app->cash * (int32_t)dw_rand_range(8, 24)) / 100;
        app->cash -= loss;
        char line[96];
        snprintf(line, sizeof(line), "You got jumped. Lost $%ld.\n", (long)loss);
        dw_append(app->last_msg, sizeof(app->last_msg), line);
    }

    if(dw_rand(100) < 3) {
        uint8_t drug = dw_rand(5);
        uint16_t qty = dw_rand_range(2, 15);
        uint16_t fit = app->max_coat - dw_coat_used(app);
        if(fit > 0) {
            if(qty > fit) qty = fit;
            app->inventory[drug] += qty;
            char line[96];
            snprintf(line, sizeof(line), "Found %u %s in an alley.\n", qty, dw_drugs[drug].name);
            dw_append(app->last_msg, sizeof(app->last_msg), line);
        }
    }

    if(!app->coat_offer && dw_rand(100) < 4 && app->cash >= 750) {
        app->coat_offer = true;
        app->coat_increase = dw_rand_range(15, 40);
        dw_append(
            app->last_msg,
            sizeof(app->last_msg),
            "Coat dealer found. Buy deeper pockets from main menu.\n");
    }
}

static bool dw_cop_check(DwApp* app) {
    uint8_t pressure = dw_run_pressure(app);
    uint8_t chance = 16 + ((pressure * 5) / 10);
    if(dw_days_elapsed(app) >= 20) chance += 5;
    if(dw_coat_used(app) > app->max_coat / 2) chance += 6;
    if(app->cash > 20000) chance += 6;
    if(app->debt > 20000) chance += 4;
    if(chance > 72) chance = 72;
    if(dw_rand(100) < chance) {
        app->pending_officers = 1 + dw_rand((app->heat / 20) + 1);
        return true;
    }
    return false;
}

static void dw_set_topbar(DwApp* app, const char* screen) {
    snprintf(
        app->header,
        sizeof(app->header),
        "%s D%u $%ld D%ld H%u C%u/%u",
        screen,
        app->day,
        (long)app->cash,
        (long)app->debt,
        app->heat,
        dw_coat_used(app),
        app->max_coat);
}

static void dw_draw_building(Canvas* canvas, int32_t x, int32_t y, int32_t w, int32_t h) {
    canvas_draw_frame(canvas, x, y, w, h);
    for(int32_t wy = y + 4; wy < y + h - 2; wy += 6) {
        for(int32_t wx = x + 3; wx < x + w - 2; wx += 7) {
            canvas_draw_box(canvas, wx, wy, 2, 2);
        }
    }
}

static void dw_draw_location_art(Canvas* canvas, uint8_t loc) {
    if(loc >= DW_LOCS) loc = 0;

    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_box(canvas, 0, 0, 128, 10);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 8, "ARRIVED");
    canvas_draw_str_aligned(canvas, 126, 8, AlignRight, AlignBottom, dw_locations[loc].name);
    canvas_set_color(canvas, ColorBlack);

    switch(loc) {
    case 0: /* Bronx */
        canvas_draw_line(canvas, 0, 47, 127, 47);
        canvas_draw_line(canvas, 0, 21, 84, 21);
        canvas_draw_line(canvas, 4, 18, 88, 18);
        for(int32_t x = 8; x < 82; x += 14)
            canvas_draw_line(canvas, x, 18, x + 5, 47);
        dw_draw_building(canvas, 92, 22, 18, 25);
        canvas_draw_line(canvas, 93, 31, 109, 24);
        canvas_draw_line(canvas, 93, 24, 109, 31);
        dw_draw_building(canvas, 112, 16, 14, 31);
        canvas_draw_str(canvas, 9, 35, "EL LINE");
        break;
    case 1: /* Manhattan */
        canvas_draw_line(canvas, 0, 50, 127, 50);
        dw_draw_building(canvas, 6, 27, 12, 23);
        dw_draw_building(canvas, 23, 18, 16, 32);
        dw_draw_building(canvas, 45, 25, 12, 25);
        dw_draw_building(canvas, 63, 13, 18, 37);
        canvas_draw_line(canvas, 72, 13, 72, 8);
        dw_draw_building(canvas, 89, 21, 13, 29);
        dw_draw_building(canvas, 108, 29, 14, 21);
        break;
    case 2: /* Brooklyn */
        canvas_draw_line(canvas, 0, 51, 127, 51);
        canvas_draw_frame(canvas, 23, 21, 11, 30);
        canvas_draw_frame(canvas, 94, 21, 11, 30);
        canvas_draw_line(canvas, 0, 31, 28, 21);
        canvas_draw_line(canvas, 28, 21, 64, 42);
        canvas_draw_line(canvas, 64, 42, 99, 21);
        canvas_draw_line(canvas, 99, 21, 127, 31);
        for(int32_t x = 10; x < 120; x += 12)
            canvas_draw_line(canvas, x, 33, x + 2, 51);
        canvas_draw_str_aligned(canvas, 64, 18, AlignCenter, AlignBottom, "BRIDGE");
        break;
    case 3: /* Queens */
        canvas_draw_line(canvas, 0, 24, 127, 24);
        canvas_draw_line(canvas, 0, 27, 127, 27);
        for(int32_t x = 6; x < 122; x += 15)
            canvas_draw_line(canvas, x, 24, x - 4, 50);
        canvas_draw_frame(canvas, 18, 15, 42, 9);
        canvas_draw_frame(canvas, 68, 15, 42, 9);
        for(int32_t x = 5; x < 122; x += 20) {
            canvas_draw_frame(canvas, x, 41, 15, 10);
            canvas_draw_line(canvas, x, 41, x + 7, 34);
            canvas_draw_line(canvas, x + 7, 34, x + 15, 41);
        }
        break;
    case 4: /* Staten */
        for(int32_t y = 45; y < 55; y += 4) {
            for(int32_t x = 0; x < 128; x += 16)
                canvas_draw_line(canvas, x, y, x + 8, y + 2);
        }
        canvas_draw_line(canvas, 24, 38, 93, 38);
        canvas_draw_line(canvas, 31, 28, 85, 28);
        canvas_draw_line(canvas, 24, 38, 31, 28);
        canvas_draw_line(canvas, 93, 38, 85, 28);
        canvas_draw_box(canvas, 43, 18, 26, 10);
        canvas_draw_line(canvas, 69, 28, 77, 18);
        canvas_draw_str(canvas, 45, 26, "FERRY");
        break;
    default: /* Harlem */
        canvas_draw_line(canvas, 0, 51, 127, 51);
        for(int32_t x = 6; x < 92; x += 23) {
            dw_draw_building(canvas, x, 23, 18, 28);
            canvas_draw_box(canvas, x + 6, 43, 6, 8);
        }
        canvas_draw_frame(canvas, 96, 24, 24, 15);
        canvas_draw_str(canvas, 100, 34, "JAZZ");
        canvas_draw_line(canvas, 108, 39, 108, 51);
        canvas_draw_circle(canvas, 108, 18, 4);
        break;
    }

    canvas_draw_box(canvas, 0, 55, 128, 9);
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_str(canvas, 2, 63, "new prices loaded");
    canvas_draw_str_aligned(canvas, 126, 63, AlignRight, AlignBottom, "SELL next");
    canvas_set_color(canvas, ColorBlack);
}

static void dw_art_draw_callback(Canvas* canvas, void* model) {
    DwApp* app = model ? *(DwApp**)model : NULL;
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    if(!app || app->mode == DwModeLoading) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 16, AlignCenter, AlignCenter, "CK42X");
        canvas_draw_frame(canvas, 18, 22, 92, 20);
        canvas_draw_str_aligned(canvas, 64, 36, AlignCenter, AlignCenter, "DOPE WARS");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(
            canvas, 64, 56, AlignCenter, AlignCenter, "loading street intel...");
        return;
    }

    if(app->mode == DwModeTravelArt) {
        dw_draw_location_art(canvas, app->location);
        return;
    }

    canvas_draw_xbm(canvas, 0, 0, 128, 64, dw_title_xbm);
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_box(canvas, 0, 54, 128, 10);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 62, "BACK Exit");
    canvas_draw_str_aligned(canvas, 126, 62, AlignRight, AlignBottom, "OK Start");
    canvas_set_color(canvas, ColorBlack);
}

static bool dw_art_input_callback(InputEvent* event, void* context) {
    if(event->type != InputTypeShort) return false;
    DwApp* app = context;
    if(app->mode == DwModeTitle) {
        if(event->key == InputKeyOk || event->key == InputKeyRight) {
            view_dispatcher_send_custom_event(app->dispatcher, DwEventStartGame);
            return true;
        } else if(event->key == InputKeyBack || event->key == InputKeyLeft) {
            view_dispatcher_stop(app->dispatcher);
            return true;
        }
    } else if(app->mode == DwModeTravelArt) {
        if(event->key == InputKeyOk || event->key == InputKeyRight) {
            view_dispatcher_send_custom_event(app->dispatcher, DwEventSellScreen);
            return true;
        } else if(event->key == InputKeyBack || event->key == InputKeyLeft) {
            view_dispatcher_send_custom_event(app->dispatcher, DwEventBackMain);
            return true;
        }
    }
    return false;
}

static void dw_show_loading(DwApp* app) {
    app->mode = DwModeLoading;
    app->current_view = DwViewArt;
    view_dispatcher_switch_to_view(app->dispatcher, DwViewArt);
}

static void dw_show_title(DwApp* app) {
    app->mode = DwModeTitle;
    app->current_view = DwViewArt;
    view_dispatcher_switch_to_view(app->dispatcher, DwViewArt);
}

static void dw_show_travel_art(DwApp* app) {
    app->mode = DwModeTravelArt;
    app->current_view = DwViewArt;
    view_dispatcher_switch_to_view(app->dispatcher, DwViewArt);
}

static const uint32_t dw_hub_events[] = {
    DwEventBuyScreen,
    DwEventSellScreen,
    DwEventTravel,
    DwEventLoanScreen,
    DwEventStatsScreen,
    DwEventLeaderboard,
    DwEventSaveResetScreen,
    DwEventAbout,
};

static const char* const dw_hub_labels[] = {
    "BUY",
    "SELL",
    "TRAVEL",
    "LOAN",
    "STATS",
    "BOARD",
    "SAVE/RST",
    "ABOUT",
};

static const uint8_t* dw_edge_glyph(char c) {
    static const uint8_t a[] = {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11};
    static const uint8_t b[] = {0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E};
    static const uint8_t d[] = {0x1E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1E};
    static const uint8_t e[] = {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F};
    static const uint8_t l[] = {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F};
    static const uint8_t n[] = {0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11};
    static const uint8_t o[] = {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E};
    static const uint8_t r[] = {0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11};
    static const uint8_t s[] = {0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E};
    static const uint8_t t[] = {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04};
    static const uint8_t u[] = {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E};
    static const uint8_t v[] = {0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04};
    static const uint8_t y[] = {0x11, 0x0A, 0x04, 0x04, 0x04, 0x04, 0x04};
    static const uint8_t slash[] = {0x01, 0x02, 0x04, 0x08, 0x10, 0x00, 0x00};

    switch(c) {
    case 'A':
        return a;
    case 'B':
        return b;
    case 'D':
        return d;
    case 'E':
        return e;
    case 'L':
        return l;
    case 'N':
        return n;
    case 'O':
        return o;
    case 'R':
        return r;
    case 'S':
        return s;
    case 'T':
        return t;
    case 'U':
        return u;
    case 'V':
        return v;
    case 'Y':
        return y;
    case '/':
        return slash;
    default:
        return NULL;
    }
}

static uint8_t dw_edge_label_width(const char* label) {
    uint8_t width = 0;
    for(size_t i = 0; label[i]; i++) {
        width += label[i] == ' ' ? 3 : 5;
        if(label[i + 1]) width += 1;
    }
    return width;
}

static void dw_draw_edge_label(Canvas* canvas, int32_t x, int32_t y, const char* label) {
    for(size_t i = 0; label[i]; i++) {
        const uint8_t* rows = dw_edge_glyph(label[i]);
        if(rows) {
            for(uint8_t row = 0; row < 7; row++) {
                for(uint8_t col = 0; col < 5; col++) {
                    if(rows[row] & (1 << (4 - col))) canvas_draw_dot(canvas, x + col, y + row);
                }
            }
        }
        x += label[i] == ' ' ? 4 : 6;
    }
}

static void dw_draw_button(
    Canvas* canvas,
    int32_t x,
    int32_t y,
    size_t w,
    size_t h,
    const char* label,
    bool active,
    bool primary) {
    canvas_set_color(canvas, ColorBlack);
    if(active) {
        canvas_draw_rbox(canvas, x, y, w, h, 3);
        canvas_set_color(canvas, ColorWhite);
    } else {
        canvas_draw_rframe(canvas, x, y, w, h, 3);
        if(primary) {
            canvas_draw_line(
                canvas, x + 3, y + (int32_t)h - 3, x + (int32_t)w - 4, y + (int32_t)h - 3);
        }
    }

    uint8_t label_width = dw_edge_label_width(label);
    int32_t label_x = x + ((int32_t)w - label_width) / 2;
    int32_t label_y = y + ((int32_t)h - 7) / 2;
    dw_draw_edge_label(canvas, label_x, label_y, label);

    if(active) {
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_dot(canvas, x + 2, y + 2);
        canvas_draw_dot(canvas, x + (int32_t)w - 3, y + (int32_t)h - 3);
    }
    canvas_set_color(canvas, ColorBlack);
}

static uint16_t dw_trade_max_qty(DwApp* app) {
    uint8_t i = app->selected_product;
    if(app->trade_sell) return app->inventory[i];
    int32_t price = app->prices[i];
    uint16_t fit = app->max_coat - dw_coat_used(app);
    uint16_t affordable = price > 0 ? app->cash / price : 0;
    return MIN(fit, affordable);
}

static void dw_trade_clamp_qty(DwApp* app, bool default_max) {
    uint16_t max_qty = dw_trade_max_qty(app);
    if(max_qty == 0) {
        app->trade_qty = 0;
    } else if(default_max || app->trade_qty == 0 || app->trade_qty > max_qty) {
        app->trade_qty = max_qty;
    }
}

static void dw_trade_draw_callback(Canvas* canvas, DwApp* app) {
    uint8_t i = app->selected_product;
    uint16_t max_qty = dw_trade_max_qty(app);
    int32_t total = app->prices[i] * app->trade_qty;
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_box(canvas, 0, 0, 128, 10);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontSecondary);
    snprintf(
        app->header,
        sizeof(app->header),
        "%s %s",
        app->trade_sell ? "SELL" : "BUY",
        dw_locations[app->location].name);
    canvas_draw_str(canvas, 1, 8, app->header);
    canvas_set_color(canvas, ColorBlack);

    canvas_set_font(canvas, FontPrimary);
    snprintf(
        app->labels[0],
        sizeof(app->labels[0]),
        "%s = $%ld",
        dw_drugs[i].name,
        (long)app->prices[i]);
    canvas_draw_str(canvas, 4, 23, app->labels[0]);
    canvas_set_font(canvas, FontSecondary);
    snprintf(
        app->labels[1],
        sizeof(app->labels[1]),
        "%s/%s inv %u%s",
        app->trade_sell ? "buy" : "[BUY]",
        app->trade_sell ? "[SELL]" : "sell",
        app->inventory[i],
        max_qty == 0 ? " blocked" : "");
    canvas_draw_str(canvas, 4, 35, app->labels[1]);
    snprintf(
        app->labels[2],
        sizeof(app->labels[2]),
        "Qty %u/%u total $%ld",
        app->trade_qty,
        max_qty,
        (long)total);
    canvas_draw_str(canvas, 4, 47, app->labels[2]);
    canvas_draw_str(canvas, 1, 62, "UD item  LR qty  OK trade");
}

static bool dw_trade_input_callback(InputEvent* event, DwApp* app) {
    if(event->type != InputTypeShort && event->type != InputTypeRepeat) return false;
    if(event->key == InputKeyBack) {
        dw_show_main(app);
        return true;
    }
    if(event->key == InputKeyOk) {
        if(app->trade_sell) {
            dw_do_sell(app, app->trade_qty);
        } else {
            dw_do_buy(app, app->trade_qty);
        }
        return true;
    }
    if(event->key == InputKeyUp || event->key == InputKeyDown) {
        if(event->key == InputKeyUp) {
            app->selected_product = app->selected_product == 0 ? (DW_DRUGS - 1) :
                                                                 app->selected_product - 1;
        } else {
            app->selected_product = (app->selected_product + 1) % DW_DRUGS;
        }
        dw_trade_clamp_qty(app, true);
        dw_sfx_tick(event->key == InputKeyUp);
        view_commit_model(app->hub_view, true);
        return true;
    }
    if(event->key == InputKeyLeft || event->key == InputKeyRight) {
        uint16_t max_qty = dw_trade_max_qty(app);
        if(max_qty > 0) {
            if(event->key == InputKeyLeft) {
                app->trade_qty = app->trade_qty <= 1 ? max_qty : app->trade_qty - 1;
            } else {
                app->trade_qty = app->trade_qty >= max_qty ? 1 : app->trade_qty + 1;
            }
            dw_sfx_tick(event->key == InputKeyRight);
            view_commit_model(app->hub_view, true);
        }
        return true;
    }
    return false;
}

static void dw_stop_cop_skill_timer(DwApp* app) {
    if(app->cop_skill_timer && app->cop_skill_timer_running) {
        furi_timer_stop(app->cop_skill_timer);
        app->cop_skill_timer_running = false;
    }
}

static void dw_start_cop_skill_timer(DwApp* app) {
    if(app->cop_skill_timer && !app->cop_skill_timer_running) {
        furi_timer_start(app->cop_skill_timer, furi_ms_to_ticks(55));
        app->cop_skill_timer_running = true;
    }
}

static uint16_t dw_fight_reveal_ms(DwApp* app) {
    UNUSED(app);
    return 700;
}

static const char* dw_fight_move_label(uint8_t move) {
    switch(move & 3) {
    case 0:
        return "UP  JAB";
    case 1:
        return "RIGHT HOOK";
    case 2:
        return "DOWN KICK";
    default:
        return "LEFT BLOCK";
    }
}

static const char* dw_fight_move_arrow(uint8_t move) {
    switch(move & 3) {
    case 0:
        return "^";
    case 1:
        return ">";
    case 2:
        return "v";
    default:
        return "<";
    }
}

static bool dw_input_to_fight_move(InputKey key, uint8_t* move) {
    if(key == InputKeyUp) {
        *move = 0;
        return true;
    } else if(key == InputKeyRight) {
        *move = 1;
        return true;
    } else if(key == InputKeyDown) {
        *move = 2;
        return true;
    } else if(key == InputKeyLeft) {
        *move = 3;
        return true;
    }
    return false;
}

static void
    dw_draw_bar(Canvas* canvas, int32_t x, int32_t y, int32_t w, uint16_t value, uint16_t max) {
    if(max == 0) max = 1;
    if(value > max) value = max;
    canvas_draw_frame(canvas, x, y, w, 5);
    int32_t fill = ((w - 2) * (int32_t)value) / (int32_t)max;
    if(fill > 0) canvas_draw_box(canvas, x + 1, y + 1, fill, 3);
}

static void
    dw_draw_fight_arrow(Canvas* canvas, int32_t cx, int32_t cy, uint8_t move, bool active) {
    canvas_set_color(canvas, ColorBlack);
    if(active) {
        canvas_draw_rbox(canvas, cx - 8, cy - 8, 16, 16, 2);
        canvas_set_color(canvas, ColorWhite);
    } else {
        canvas_draw_rframe(canvas, cx - 8, cy - 8, 16, 16, 2);
    }

    switch(move & 3) {
    case 0:
        canvas_draw_line(canvas, cx, cy - 5, cx - 5, cy);
        canvas_draw_line(canvas, cx, cy - 5, cx + 5, cy);
        canvas_draw_line(canvas, cx, cy - 5, cx, cy + 6);
        break;
    case 1:
        canvas_draw_line(canvas, cx + 5, cy, cx, cy - 5);
        canvas_draw_line(canvas, cx + 5, cy, cx, cy + 5);
        canvas_draw_line(canvas, cx - 6, cy, cx + 5, cy);
        break;
    case 2:
        canvas_draw_line(canvas, cx, cy + 5, cx - 5, cy);
        canvas_draw_line(canvas, cx, cy + 5, cx + 5, cy);
        canvas_draw_line(canvas, cx, cy - 6, cx, cy + 5);
        break;
    default:
        canvas_draw_line(canvas, cx - 5, cy, cx, cy - 5);
        canvas_draw_line(canvas, cx - 5, cy, cx, cy + 5);
        canvas_draw_line(canvas, cx + 6, cy, cx - 5, cy);
        break;
    }
    canvas_set_color(canvas, ColorBlack);
}

static void
    dw_draw_fighter(Canvas* canvas, int32_t x, int32_t ground, bool cop, uint8_t move, bool active) {
    int32_t dir = cop ? -1 : 1;
    if(active) {
        canvas_draw_line(canvas, x - 10, ground - 27, x - 7, ground - 24);
        canvas_draw_line(canvas, x + 10, ground - 27, x + 7, ground - 24);
    }
    canvas_draw_circle(canvas, x, ground - 24, 3);
    if(cop) {
        canvas_draw_line(canvas, x - 4, ground - 28, x + 4, ground - 28);
        canvas_draw_box(canvas, x - 3, ground - 31, 6, 2);
        canvas_draw_dot(canvas, x - 1, ground - 23);
    } else {
        canvas_draw_line(canvas, x - 3, ground - 28, x + 3, ground - 27);
    }
    canvas_draw_box(canvas, x - 2, ground - 20, 5, 10);
    canvas_draw_line(canvas, x - 2, ground - 10, x - 7, ground - 1);
    canvas_draw_line(canvas, x + 2, ground - 10, x + 7, ground - 1);

    if((move & 3) == 0) {
        canvas_draw_line(canvas, x + (dir * 2), ground - 18, x + (dir * 15), ground - 21);
    } else if((move & 3) == 1) {
        canvas_draw_line(canvas, x + (dir * 2), ground - 18, x + (dir * 11), ground - 16);
        canvas_draw_line(canvas, x + (dir * 11), ground - 16, x + (dir * 16), ground - 20);
    } else if((move & 3) == 2) {
        canvas_draw_line(canvas, x + (dir * 2), ground - 10, x + (dir * 15), ground - 6);
        canvas_draw_line(canvas, x + (dir * 15), ground - 6, x + (dir * 19), ground - 8);
    } else {
        canvas_draw_line(canvas, x + (dir * 2), ground - 18, x + (dir * 8), ground - 25);
        canvas_draw_line(canvas, x + (dir * 8), ground - 25, x + (dir * 12), ground - 18);
    }
    canvas_draw_line(canvas, x - (dir * 2), ground - 18, x - (dir * 9), ground - 15);
}

static void dw_draw_run_player(Canvas* canvas, int32_t x, int32_t y) {
    canvas_draw_circle(canvas, x + 4, y - 7, 3);
    canvas_draw_line(canvas, x + 4, y - 4, x + 4, y + 4);
    canvas_draw_line(canvas, x + 4, y - 2, x + 11, y - 6);
    canvas_draw_line(canvas, x + 4, y - 1, x - 2, y + 3);
    canvas_draw_line(canvas, x + 4, y + 4, x + 10, y + 8);
    canvas_draw_line(canvas, x + 4, y + 4, x - 1, y + 8);
}

static void dw_draw_run_bullet(Canvas* canvas, int32_t x, int32_t y) {
    canvas_draw_box(canvas, x, y - 2, 6, 4);
    canvas_draw_line(canvas, x + 6, y - 2, x + 10, y);
    canvas_draw_line(canvas, x + 6, y + 1, x + 10, y);
    canvas_draw_line(canvas, x + 11, y, x + 15, y);
    canvas_draw_line(canvas, x - 5, y - 2, x - 2, y - 2);
    canvas_draw_line(canvas, x - 7, y + 2, x - 3, y + 2);
}

static void dw_draw_cop_car(Canvas* canvas, int32_t x, int32_t y) {
    canvas_draw_frame(canvas, x, y + 4, 24, 8);
    canvas_draw_line(canvas, x + 4, y + 4, x + 8, y);
    canvas_draw_line(canvas, x + 8, y, x + 16, y);
    canvas_draw_line(canvas, x + 16, y, x + 20, y + 4);
    canvas_draw_box(canvas, x + 10, y - 2, 4, 2);
    canvas_draw_circle(canvas, x + 5, y + 12, 2);
    canvas_draw_circle(canvas, x + 19, y + 12, 2);
    canvas_draw_str(canvas, x + 7, y + 11, "PD");
}

static void dw_run_reset_bullet(DwApp* app, uint8_t idx) {
    uint8_t other = idx == 0 ? 1 : 0;
    app->run_bullet_x[idx] = (uint8_t)(112 + dw_rand(13));
    app->run_bullet_lane[idx] = dw_rand(3);
    if(app->run_bullet_count > 1 && app->run_bullet_x[other] > 80 &&
       app->run_bullet_lane[idx] == app->run_bullet_lane[other]) {
        app->run_bullet_lane[idx] = (app->run_bullet_lane[idx] + 1 + dw_rand(2)) % 3;
    }
}

static uint8_t dw_cop_skill_score(DwApp* app) {
    uint16_t goal = app->run_ticks_goal ? app->run_ticks_goal : 1;
    uint16_t ticks = app->cop_skill_moves > goal ? goal : app->cop_skill_moves;
    uint8_t score = (uint8_t)((ticks * 100U) / goal);
    uint8_t hit_penalty = app->run_hits * 22U;
    return hit_penalty >= score ? 0 : score - hit_penalty;
}

static void dw_finish_cop_run_game(DwApp* app) {
    uint8_t skill_score = dw_cop_skill_score(app);
    dw_stop_cop_skill_timer(app);
    dw_sfx_tick(skill_score >= 70);
    dw_cop_run(app, skill_score);
}

static void dw_cop_fight_draw_callback(Canvas* canvas, DwApp* app) {
    uint32_t reveal_ticks = furi_ms_to_ticks(dw_fight_reveal_ms(app));
    if(reveal_ticks == 0) reveal_ticks = 1;
    uint32_t elapsed_ticks = furi_get_tick() - app->cop_skill_started;
    uint8_t reveal_idx = elapsed_ticks / reveal_ticks;
    bool watching = app->cop_game_phase == 0;

    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_box(canvas, 0, 0, 128, 10);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontSecondary);
    snprintf(
        app->header,
        sizeof(app->header),
        "COP FIGHT x%u H%u S%u",
        app->pending_officers ? app->pending_officers : 1,
        app->heat,
        app->cop_streak);
    canvas_draw_str(canvas, 1, 8, app->header);
    canvas_set_color(canvas, ColorBlack);

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_line(canvas, 15, 52, 113, 52);
    canvas_draw_line(canvas, 26, 49, 102, 49);
    canvas_draw_str(canvas, 56, 45, "VS");

    if(watching) {
        if(reveal_idx >= app->cop_sequence_len) reveal_idx = app->cop_sequence_len - 1;
        uint8_t move = app->cop_sequence[reveal_idx];
        canvas_draw_str(canvas, 2, 20, "WATCH COMBO");
        snprintf(
            app->labels[0],
            sizeof(app->labels[0]),
            "%u/%u",
            (uint8_t)(reveal_idx + 1),
            app->cop_sequence_len);
        canvas_draw_str_aligned(canvas, 126, 20, AlignRight, AlignBottom, app->labels[0]);
        dw_draw_bar(canvas, 36, 14, 56, reveal_idx + 1, app->cop_sequence_len);
        dw_draw_fighter(canvas, 31, 51, false, move, true);
        dw_draw_fighter(canvas, 98, 51, true, move, false);
        dw_draw_fight_arrow(canvas, 64, 30, move, true);
        canvas_draw_str_aligned(
            canvas, 64, 42, AlignCenter, AlignBottom, dw_fight_move_label(move));
        for(uint8_t i = 0; i <= reveal_idx && i < app->cop_sequence_len; i++) {
            canvas_draw_str(canvas, 36 + (i * 7), 62, dw_fight_move_arrow(app->cop_sequence[i]));
        }
    } else {
        canvas_draw_str(canvas, 2, 20, "REPEAT COMBO");
        snprintf(
            app->labels[0],
            sizeof(app->labels[0]),
            "%u/%u",
            app->cop_sequence_pos,
            app->cop_sequence_len);
        canvas_draw_str_aligned(canvas, 126, 20, AlignRight, AlignBottom, app->labels[0]);
        dw_draw_bar(canvas, 36, 14, 56, app->cop_sequence_pos, app->cop_sequence_len);
        dw_draw_fighter(canvas, 31, 51, false, app->cop_sequence_pos, false);
        dw_draw_fighter(canvas, 98, 51, true, 3, true);
        dw_draw_fight_arrow(canvas, 64, 25, 0, false);
        dw_draw_fight_arrow(canvas, 80, 38, 1, false);
        dw_draw_fight_arrow(canvas, 64, 51, 2, false);
        dw_draw_fight_arrow(canvas, 48, 38, 3, false);
        for(uint8_t i = 0; i < app->cop_sequence_len; i++) {
            int32_t x = 36 + (i * 7);
            if(i < app->cop_sequence_pos) {
                canvas_draw_box(canvas, x, 58, 5, 4);
            } else {
                canvas_draw_frame(canvas, x, 58, 5, 4);
            }
        }
    }
}

static void dw_cop_run_draw_callback(Canvas* canvas, DwApp* app) {
    static const uint8_t lanes[3] = {24, 39, 54};
    uint16_t goal = app->run_ticks_goal ? app->run_ticks_goal : 1;
    uint8_t time_left = app->cop_skill_moves >= goal ?
                            0 :
                            (uint8_t)(((goal - app->cop_skill_moves) * 55U) / 1000U + 1U);
    uint8_t max_hits = app->heat > 75 ? 3 : 4;

    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_box(canvas, 0, 0, 128, 10);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontSecondary);
    snprintf(
        app->header,
        sizeof(app->header),
        "COP RUN x%u H%u S%u",
        app->pending_officers ? app->pending_officers : 1,
        app->heat,
        app->cop_streak);
    canvas_draw_str(canvas, 1, 8, app->header);
    canvas_set_color(canvas, ColorBlack);

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 18, "RUN");
    dw_draw_bar(canvas, 25, 13, 61, app->cop_skill_moves, goal);
    snprintf(app->labels[0], sizeof(app->labels[0]), "%us", time_left);
    canvas_draw_str(canvas, 91, 18, app->labels[0]);
    snprintf(app->labels[1], sizeof(app->labels[1]), "HIT %u/%u", app->run_hits, max_hits);
    canvas_draw_str_aligned(canvas, 126, 18, AlignRight, AlignBottom, app->labels[1]);

    canvas_draw_frame(canvas, 7, 20, 116, 39);
    for(uint8_t lane = 0; lane < 3; lane++) {
        canvas_draw_line(canvas, 8, lanes[lane], 122, lanes[lane]);
        for(int32_t dash = 23; dash < 120; dash += 20) {
            canvas_draw_line(canvas, dash, lanes[lane] + 6, dash + 6, lanes[lane] + 6);
        }
    }
    dw_draw_cop_car(canvas, 95, 22);
    dw_draw_run_player(canvas, 15, lanes[app->run_lane]);
    for(uint8_t i = 0; i < app->run_bullet_count; i++) {
        uint8_t y = lanes[app->run_bullet_lane[i]];
        dw_draw_run_bullet(canvas, app->run_bullet_x[i], y);
    }
    canvas_draw_str(canvas, 1, 64, "UP/DN lanes        dodge fire");
}

static void dw_cop_skill_draw_callback(Canvas* canvas, DwApp* app) {
    if(app->mode == DwModeCopRunGame) {
        dw_cop_run_draw_callback(canvas, app);
    } else {
        dw_cop_fight_draw_callback(canvas, app);
    }
}

static void dw_update_cop_fight_game(DwApp* app) {
    if(app->mode != DwModeCopFightGame || app->cop_game_phase != 0) return;
    uint32_t elapsed_ticks = furi_get_tick() - app->cop_skill_started;
    uint32_t total_ticks =
        furi_ms_to_ticks(((uint32_t)app->cop_sequence_len * dw_fight_reveal_ms(app)) + 450U);
    if(elapsed_ticks >= total_ticks) {
        app->cop_game_phase = 1;
        app->cop_sequence_pos = 0;
        dw_sfx_tick(true);
    }
}

static void dw_update_cop_run_game(DwApp* app) {
    if(app->mode != DwModeCopRunGame) return;
    uint8_t max_hits = app->heat > 75 ? 3 : 4;
    app->cop_skill_moves++;
    for(uint8_t i = 0; i < app->run_bullet_count; i++) {
        uint8_t x = app->run_bullet_x[i];
        if(x <= app->cop_skill_speed + 2) {
            dw_run_reset_bullet(app, i);
            continue;
        }
        app->run_bullet_x[i] = x - app->cop_skill_speed;
        if(app->run_bullet_lane[i] == app->run_lane && app->run_bullet_x[i] <= 16 &&
           app->run_bullet_x[i] >= 8) {
            app->run_hits++;
            dw_sfx_bad();
            dw_run_reset_bullet(app, i);
            if(app->run_hits >= max_hits) {
                dw_finish_cop_run_game(app);
                return;
            }
        }
    }
    if(app->cop_skill_moves >= app->run_ticks_goal) {
        dw_finish_cop_run_game(app);
    }
}

static void dw_cop_skill_tick_callback(void* context) {
    DwApp* app = context;
    if(app && (app->mode == DwModeCopFightGame || app->mode == DwModeCopRunGame)) {
        view_dispatcher_send_custom_event(app->dispatcher, DwEventCopSkillTick);
    }
}

static bool dw_cop_skill_input_callback(InputEvent* event, DwApp* app) {
    if(event->type != InputTypeShort && event->type != InputTypeRepeat) return false;
    if(event->key == InputKeyBack) {
        dw_stop_cop_skill_timer(app);
        dw_show_cop(app);
        return true;
    }
    if(app->mode == DwModeCopRunGame) {
        if(event->key == InputKeyUp && app->run_lane > 0) {
            app->run_lane--;
            dw_sfx_tick(true);
            view_commit_model(app->hub_view, true);
        } else if(event->key == InputKeyDown && app->run_lane < 2) {
            app->run_lane++;
            dw_sfx_tick(true);
            view_commit_model(app->hub_view, true);
        }
        return true;
    }

    uint8_t move = 0;
    if(app->cop_game_phase == 0 || !dw_input_to_fight_move(event->key, &move)) return true;
    if(move == app->cop_sequence[app->cop_sequence_pos]) {
        app->cop_sequence_pos++;
        dw_sfx_tick(true);
        if(app->cop_sequence_pos >= app->cop_sequence_len) {
            dw_stop_cop_skill_timer(app);
            dw_sfx_good();
            dw_cop_fight(app, 100);
        } else {
            view_commit_model(app->hub_view, true);
        }
    } else {
        dw_stop_cop_skill_timer(app);
        dw_sfx_bad();
        dw_cop_fight(app, 0);
    }
    return true;
}

static void dw_hub_draw_callback(Canvas* canvas, void* model) {
    DwApp* app = model ? *(DwApp**)model : NULL;
    if(!app) return;
    if(app->mode == DwModeCopFightGame || app->mode == DwModeCopRunGame) {
        dw_cop_skill_draw_callback(canvas, app);
        return;
    }
    if(app->mode == DwModeBuyList || app->mode == DwModeSellList) {
        dw_trade_draw_callback(canvas, app);
        return;
    }
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_box(canvas, 0, 0, 128, 10);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontSecondary);
    snprintf(
        app->header,
        sizeof(app->header),
        "D%u $%ld D%ld H%u C%u/%u",
        app->day,
        (long)app->cash,
        (long)app->debt,
        app->heat,
        dw_coat_used(app),
        app->max_coat);
    canvas_draw_str(canvas, 1, 8, app->header);
    canvas_set_color(canvas, ColorBlack);

    dw_draw_button(canvas, 2, 13, 39, 15, dw_hub_labels[0], app->hub_selected == 0, true);
    dw_draw_button(canvas, 44, 13, 39, 15, dw_hub_labels[1], app->hub_selected == 1, true);
    dw_draw_button(canvas, 86, 13, 40, 15, dw_hub_labels[2], app->hub_selected == 2, true);
    dw_draw_button(canvas, 2, 32, 39, 12, dw_hub_labels[3], app->hub_selected == 3, false);
    dw_draw_button(canvas, 44, 32, 39, 12, dw_hub_labels[4], app->hub_selected == 4, false);
    dw_draw_button(canvas, 86, 32, 40, 12, dw_hub_labels[5], app->hub_selected == 5, false);
    dw_draw_button(canvas, 2, 48, 60, 13, dw_hub_labels[6], app->hub_selected == 6, false);
    dw_draw_button(canvas, 66, 48, 60, 13, dw_hub_labels[7], app->hub_selected == 7, false);
}

static void dw_hub_select(DwApp* app, uint8_t selected) {
    app->hub_selected = selected % COUNT_OF(dw_hub_events);
    view_commit_model(app->hub_view, true);
}

static bool dw_hub_input_callback(InputEvent* event, void* context) {
    if(event->type != InputTypeShort && event->type != InputTypeRepeat) return false;
    DwApp* app = context;
    if(app->mode == DwModeCopFightGame || app->mode == DwModeCopRunGame) {
        return dw_cop_skill_input_callback(event, app);
    }
    if(app->mode == DwModeBuyList || app->mode == DwModeSellList) {
        return dw_trade_input_callback(event, app);
    }
    if(event->key == InputKeyOk) {
        view_dispatcher_send_custom_event(app->dispatcher, dw_hub_events[app->hub_selected]);
        return true;
    } else if(event->key == InputKeyBack) {
        view_dispatcher_stop(app->dispatcher);
        return true;
    } else if(event->key == InputKeyLeft) {
        uint8_t next = app->hub_selected == 0 ? (uint8_t)(COUNT_OF(dw_hub_events) - 1) :
                                                app->hub_selected - 1;
        dw_hub_select(app, next);
        return true;
    } else if(event->key == InputKeyRight) {
        dw_hub_select(app, app->hub_selected + 1);
        return true;
    } else if(event->key == InputKeyUp) {
        if(app->hub_selected >= 6) {
            dw_hub_select(app, app->hub_selected - 3);
        } else if(app->hub_selected >= 3) {
            dw_hub_select(app, app->hub_selected - 3);
        } else {
            dw_hub_select(app, app->hub_selected == 2 ? 7 : 6);
        }
        return true;
    } else if(event->key == InputKeyDown) {
        if(app->hub_selected < 3) {
            dw_hub_select(app, app->hub_selected + 3);
        } else if(app->hub_selected < 6) {
            dw_hub_select(app, app->hub_selected >= 4 ? 7 : 6);
        } else {
            dw_hub_select(app, app->hub_selected == 7 ? 2 : 0);
        }
        return true;
    }
    return false;
}

static void dw_submenu_callback(void* context, uint32_t index) {
    DwApp* app = context;
    view_dispatcher_send_custom_event(app->dispatcher, index);
}

static void dw_widget_button_callback(GuiButtonType result, InputType type, void* context) {
    if(type != InputTypeShort) return;
    DwApp* app = context;
    if(result == GuiButtonTypeLeft) {
        view_dispatcher_send_custom_event(app->dispatcher, DwEventWidgetLeft);
    } else if(result == GuiButtonTypeRight) {
        view_dispatcher_send_custom_event(app->dispatcher, DwEventWidgetRight);
    } else if(result == GuiButtonTypeCenter) {
        view_dispatcher_send_custom_event(app->dispatcher, DwEventWidgetCenter);
    }
}

static bool dw_custom_event_callback(void* context, uint32_t event) {
    dw_handle_event(context, event);
    return true;
}

static bool dw_navigation_callback(void* context) {
    DwApp* app = context;
    if((app->current_view == DwViewMenu && app->mode == DwModeMain) || app->mode == DwModeTitle) {
        view_dispatcher_stop(app->dispatcher);
    } else {
        dw_show_main(app);
    }
    return true;
}

static void dw_show_main(DwApp* app) {
    app->mode = DwModeMain;
    app->current_view = DwViewHub;
    view_dispatcher_switch_to_view(app->dispatcher, DwViewHub);
}

static void dw_show_buy_list(DwApp* app) {
    app->mode = DwModeBuyList;
    app->trade_sell = false;
    if(app->selected_product >= DW_DRUGS) app->selected_product = 0;
    dw_trade_clamp_qty(app, true);
    app->current_view = DwViewHub;
    view_commit_model(app->hub_view, true);
    view_dispatcher_switch_to_view(app->dispatcher, DwViewHub);
}

static void dw_show_sell_list(DwApp* app) {
    app->mode = DwModeSellList;
    app->trade_sell = true;
    if(app->selected_product >= DW_DRUGS) app->selected_product = 0;
    dw_trade_clamp_qty(app, true);
    app->current_view = DwViewHub;
    view_commit_model(app->hub_view, true);
    view_dispatcher_switch_to_view(app->dispatcher, DwViewHub);
}

static void dw_show_loan(DwApp* app) {
    app->mode = DwModeLoan;
    submenu_reset(app->submenu);
    dw_set_topbar(app, "LOAN");
    submenu_set_header(app->submenu, app->header);
    snprintf(app->labels[0], sizeof(app->labels[0]), "Debt: $%ld", (long)app->debt);
    submenu_add_item(app->submenu, app->labels[0], DwEventStatus, dw_submenu_callback, app);
    submenu_add_item(app->submenu, "Repay half", DwEventLoanHalf, dw_submenu_callback, app);
    submenu_add_item(app->submenu, "Repay max", DwEventLoanAll, dw_submenu_callback, app);
    submenu_add_item(app->submenu, "Deposit all", DwEventBankDeposit, dw_submenu_callback, app);
    submenu_add_item(app->submenu, "Withdraw all", DwEventBankWithdraw, dw_submenu_callback, app);
    if(app->coat_offer) {
        snprintf(app->labels[1], sizeof(app->labels[1]), "Buy coat +%u $500", app->coat_increase);
        submenu_add_item(app->submenu, app->labels[1], DwEventCoatBuy, dw_submenu_callback, app);
    }
    submenu_add_item(app->submenu, "Back", DwEventBackMain, dw_submenu_callback, app);
    app->current_view = DwViewMenu;
    view_dispatcher_switch_to_view(app->dispatcher, DwViewMenu);
}

static void dw_show_save_reset(DwApp* app) {
    app->mode = DwModeSaveReset;
    submenu_reset(app->submenu);
    dw_set_topbar(app, "SAVE");
    submenu_set_header(app->submenu, app->header);
    submenu_add_item(app->submenu, "Save run", DwEventSaveNow, dw_submenu_callback, app);
    submenu_add_item(app->submenu, "Restart run", DwEventRestart, dw_submenu_callback, app);
    submenu_add_item(app->submenu, "Reset save + run", DwEventResetSave, dw_submenu_callback, app);
    submenu_add_item(app->submenu, "Back", DwEventBackMain, dw_submenu_callback, app);
    app->current_view = DwViewMenu;
    view_dispatcher_switch_to_view(app->dispatcher, DwViewMenu);
}

static void dw_show_stats(DwApp* app) {
    int32_t inv_value = dw_run_inventory_value(app, true);
    int32_t net = dw_run_net(app, true);
    int32_t profit = app->total_sold - app->total_bought;
    char body[DW_MSG_LEN];
    snprintf(
        body,
        sizeof(body),
        "Current run\nDay: %u/30\nNet: $%ld\nProfit: $%ld\nInv local: $%ld\nCash/Bank: $%ld/$%ld\nDebt: $%ld\nBig deal: $%ld\nBought/Sold: $%ld/$%ld\nCops F/R: %u/%u\nHeat: %u%%\n\nAll-time\nGames: %lu W/L %lu/%lu\nBest net: $%ld\nRank: %s\nLife B/S: $%ld/$%ld",
        app->day,
        (long)net,
        (long)profit,
        (long)inv_value,
        (long)app->cash,
        (long)app->bank,
        (long)app->debt,
        (long)app->biggest_deal,
        (long)app->total_bought,
        (long)app->total_sold,
        app->cops_fought,
        app->cops_ran,
        app->heat,
        (unsigned long)app->stats.games_played,
        (unsigned long)app->stats.wins,
        (unsigned long)app->stats.losses,
        (long)app->stats.best_net,
        dw_rank_for(app->stats.best_net),
        (long)app->stats.lifetime_bought,
        (long)app->stats.lifetime_sold);
    app->mode = DwModeStats;
    dw_show_status(app, "Stats", body);
}

static void dw_show_global_leaderboard(DwApp* app) {
    app->mode = DwModeLeaderboard;
    submenu_reset(app->submenu);
    dw_set_topbar(app, "GLOBAL");
    submenu_set_header(app->submenu, app->header);
    submenu_add_item(
        app->submenu, "Windows BadUSB", DwEventGlobalWindows, dw_submenu_callback, app);
    submenu_add_item(app->submenu, "Linux BadUSB", DwEventGlobalLinux, dw_submenu_callback, app);
    submenu_add_item(app->submenu, "macOS BadUSB", DwEventGlobalMac, dw_submenu_callback, app);
    submenu_add_item(
        app->submenu, "Fallback URL", DwEventGlobalFallback, dw_submenu_callback, app);
    submenu_add_item(app->submenu, "Back", DwEventLeaderboard, dw_submenu_callback, app);
    app->current_view = DwViewMenu;
    view_dispatcher_switch_to_view(app->dispatcher, DwViewMenu);
}

static void dw_show_leaderboard(DwApp* app) {
    app->mode = DwModeLeaderboard;
    submenu_reset(app->submenu);
    dw_set_topbar(app, "BOARD");
    submenu_set_header(app->submenu, app->header);
    snprintf(app->labels[0], sizeof(app->labels[0]), "Best net $%ld", (long)app->stats.best_net);
    snprintf(app->labels[1], sizeof(app->labels[1]), "Rank %s", dw_rank_for(app->stats.best_net));
    snprintf(
        app->labels[2], sizeof(app->labels[2]), "Biggest $%ld", (long)app->stats.biggest_deal);
    snprintf(
        app->labels[3],
        sizeof(app->labels[3]),
        "Games %lu",
        (unsigned long)app->stats.games_played);
    submenu_add_item(app->submenu, app->labels[0], DwEventStatus, dw_submenu_callback, app);
    submenu_add_item(app->submenu, app->labels[1], DwEventStatus, dw_submenu_callback, app);
    submenu_add_item(app->submenu, app->labels[2], DwEventStatus, dw_submenu_callback, app);
    submenu_add_item(app->submenu, app->labels[3], DwEventStatus, dw_submenu_callback, app);
    submenu_add_item(
        app->submenu, "Global leaderboard", DwEventGlobalInfo, dw_submenu_callback, app);
    submenu_add_item(app->submenu, "Back", DwEventBackMain, dw_submenu_callback, app);
    app->current_view = DwViewMenu;
    view_dispatcher_switch_to_view(app->dispatcher, DwViewMenu);
}

static void dw_show_product(DwApp* app, uint8_t drug_idx) {
    app->mode = DwModeProduct;
    app->selected_product = drug_idx;
    submenu_reset(app->submenu);
    snprintf(
        app->header,
        sizeof(app->header),
        "%s %s $%ld inv %u",
        app->trade_sell ? "SELL" : "BUY",
        dw_drugs[drug_idx].name,
        (long)app->prices[drug_idx],
        app->inventory[drug_idx]);
    submenu_set_header(app->submenu, app->header);
    int32_t avg = ((dw_drugs[drug_idx].min + dw_drugs[drug_idx].max) / 2) *
                  dw_locations[app->location].bias[drug_idx] / 100;
    snprintf(app->labels[0], sizeof(app->labels[0]), "Avg here: $%ld", (long)avg);
    submenu_add_item(app->submenu, app->labels[0], DwEventStatus, dw_submenu_callback, app);
    if(app->trade_sell) {
        submenu_add_item(app->submenu, "Sell 1", DwEventSell1, dw_submenu_callback, app);
        submenu_add_item(app->submenu, "Sell 5", DwEventSell5, dw_submenu_callback, app);
        submenu_add_item(app->submenu, "Sell 10", DwEventSell10, dw_submenu_callback, app);
        submenu_add_item(app->submenu, "Sell all", DwEventSellAll, dw_submenu_callback, app);
        submenu_add_item(
            app->submenu, "Back to SELL", DwEventSellScreen, dw_submenu_callback, app);
    } else {
        submenu_add_item(app->submenu, "Buy 1", DwEventBuy1, dw_submenu_callback, app);
        submenu_add_item(app->submenu, "Buy 5", DwEventBuy5, dw_submenu_callback, app);
        submenu_add_item(app->submenu, "Buy 10", DwEventBuy10, dw_submenu_callback, app);
        submenu_add_item(app->submenu, "Buy max", DwEventBuyMax, dw_submenu_callback, app);
        submenu_add_item(app->submenu, "Back to BUY", DwEventBuyScreen, dw_submenu_callback, app);
    }
    submenu_add_item(app->submenu, "Hub", DwEventBackMain, dw_submenu_callback, app);
    app->current_view = DwViewMenu;
    view_dispatcher_switch_to_view(app->dispatcher, DwViewMenu);
}

static void dw_show_travel(DwApp* app) {
    app->mode = DwModeTravel;
    submenu_reset(app->submenu);
    dw_set_topbar(app, "TRAVEL");
    submenu_set_header(app->submenu, app->header);
    for(uint8_t i = 0; i < DW_LOCS; i++) {
        snprintf(
            app->labels[i],
            sizeof(app->labels[i]),
            "%s%s",
            i == app->location ? "* " : "",
            dw_locations[i].name);
        submenu_add_item(
            app->submenu, app->labels[i], DW_TRAVEL_BASE + i, dw_submenu_callback, app);
    }
    submenu_add_item(app->submenu, "Back", DwEventBackMain, dw_submenu_callback, app);
    app->current_view = DwViewMenu;
    view_dispatcher_switch_to_view(app->dispatcher, DwViewMenu);
}

static void dw_show_cop(DwApp* app) {
    app->mode = DwModeCop;
    submenu_reset(app->submenu);
    snprintf(
        app->header,
        sizeof(app->header),
        "COPS! %u officer%s  STREAK %u",
        app->pending_officers,
        app->pending_officers == 1 ? "" : "s",
        app->cop_streak);
    submenu_set_header(app->submenu, app->header);
    submenu_add_item(
        app->submenu, "Fight: loot + heat", DwEventCopFight, dw_submenu_callback, app);
    submenu_add_item(app->submenu, "Run: cool heat", DwEventCopRun, dw_submenu_callback, app);
    app->current_view = DwViewMenu;
    view_dispatcher_switch_to_view(app->dispatcher, DwViewMenu);
}

static void dw_show_status(DwApp* app, const char* title, const char* body) {
    widget_reset(app->widget);
    snprintf(app->text, sizeof(app->text), "\e#%s\n%s", title, body);
    widget_add_text_scroll_element(app->widget, 0, 0, 128, 52, app->text);
    widget_add_button_element(
        app->widget, GuiButtonTypeLeft, "Back", dw_widget_button_callback, app);
    app->current_view = DwViewWidget;
    view_dispatcher_switch_to_view(app->dispatcher, DwViewWidget);
}

static void dw_show_run_status(DwApp* app) {
    int32_t inv_value = dw_run_inventory_value(app, true);
    int32_t net = dw_run_net(app, true);
    int32_t profit = app->total_sold - app->total_bought;
    char body[DW_MSG_LEN];
    snprintf(
        body,
        sizeof(body),
        "Day: %u/30\nLoc: %s\nCash/Bank: $%ld/$%ld\nDebt: $%ld\nInv local: $%ld\nNet: $%ld\nRank: %s\nProfit: $%ld\nCoat: %u/%u\nHeat: %u%%  Cop streak: %u\nBig deal: $%ld\nCops F/R: %u/%u\nAutosave: on\n\nIntel:\n%.80s",
        app->day,
        dw_locations[app->location].name,
        (long)app->cash,
        (long)app->bank,
        (long)app->debt,
        (long)inv_value,
        (long)net,
        dw_rank_for(net),
        (long)profit,
        dw_coat_used(app),
        app->max_coat,
        app->heat,
        app->cop_streak,
        (long)app->biggest_deal,
        app->cops_fought,
        app->cops_ran,
        app->last_msg);
    dw_show_status(app, "Run Status", body);
}

static void dw_do_buy(DwApp* app, uint16_t requested) {
    uint8_t i = app->selected_product;
    int32_t price = app->prices[i];
    uint16_t fit = app->max_coat - dw_coat_used(app);
    uint16_t affordable = price > 0 ? app->cash / price : 0;
    uint16_t qty = requested;
    if(qty > fit) qty = fit;
    if(qty > affordable) qty = affordable;
    if(qty == 0) {
        dw_show_status(app, "No Deal", "Not enough cash or coat space.");
        return;
    }
    int32_t cost = price * qty;
    app->cash -= cost;
    app->inventory[i] += qty;
    app->total_bought += cost;
    app->buy_qty[i] += qty;
    app->buy_value[i] += cost;
    if(cost > app->biggest_deal) app->biggest_deal = cost;
    dw_record_action(app, 1, cost, ((uint16_t)i << 8) | qty);
    if(cost > 5000) app->heat = MIN(100, app->heat + (cost / 5000));
    snprintf(
        app->last_msg,
        sizeof(app->last_msg),
        "Bought %u %s for $%ld.",
        qty,
        dw_drugs[i].name,
        (long)cost);
    dw_sfx_buy();
    dw_save_game(app);
    dw_trade_clamp_qty(app, true);
    app->current_view = DwViewHub;
    view_commit_model(app->hub_view, true);
    view_dispatcher_switch_to_view(app->dispatcher, DwViewHub);
}

static void dw_do_sell(DwApp* app, uint16_t requested) {
    uint8_t i = app->selected_product;
    uint16_t owned = app->inventory[i];
    uint16_t qty = requested;
    if(qty > owned) qty = owned;
    if(qty == 0) {
        dw_show_status(app, "No Stock", "You do not have any to sell.");
        return;
    }
    int32_t revenue = app->prices[i] * qty;
    app->cash += revenue;
    app->inventory[i] -= qty;
    app->total_sold += revenue;
    app->sell_qty[i] += qty;
    app->sell_value[i] += revenue;
    bool new_big_deal = revenue > app->biggest_deal;
    if(new_big_deal) app->biggest_deal = revenue;
    dw_record_action(app, 2, revenue, ((uint16_t)i << 8) | qty);
    if(revenue > 5000) app->heat = MIN(100, app->heat + (revenue / 8000));
    if(new_big_deal) {
        snprintf(
            app->last_msg,
            sizeof(app->last_msg),
            "NEW BIG DEAL! Sold %u %s for $%ld.",
            qty,
            dw_drugs[i].name,
            (long)revenue);
        dw_sfx_good();
    } else {
        snprintf(
            app->last_msg,
            sizeof(app->last_msg),
            "Sold %u %s for $%ld.",
            qty,
            dw_drugs[i].name,
            (long)revenue);
    }
    dw_sfx_sell();
    dw_save_game(app);
    dw_trade_clamp_qty(app, true);
    app->current_view = DwViewHub;
    view_commit_model(app->hub_view, true);
    view_dispatcher_switch_to_view(app->dispatcher, DwViewHub);
}

static void dw_travel_to(DwApp* app, uint8_t loc) {
    if(loc == app->location) {
        dw_show_main(app);
        return;
    }
    app->location = loc;
    if(app->day > 0) app->day--;
    if(app->debt > 0) app->debt = (app->debt * 112) / 100;
    dw_record_action(app, 3, app->prices[0], loc);
    if(app->heat > 0) {
        uint8_t decay = dw_rand_range(1, 4);
        app->heat = app->heat > decay ? app->heat - decay : 0;
    }
    dw_sfx_travel();
    if(app->day == 0) {
        dw_end_game(app);
        return;
    }
    dw_start_round(app);
    if(dw_cop_check(app)) {
        dw_save_game(app);
        dw_sfx_cop();
        dw_show_cop(app);
    } else {
        dw_save_game(app);
        dw_show_travel_art(app);
        furi_delay_ms(900);
        dw_show_sell_list(app);
    }
}

static void dw_start_cop_fight(DwApp* app) {
    uint8_t officers = app->pending_officers ? app->pending_officers : 1;
    uint8_t pressure = dw_run_pressure(app);
    uint8_t len =
        3 + (pressure / 28) + ((officers + 1) / 2) + MIN((uint8_t)1, app->cop_streak / 4);
    if(pressure >= 75) len++;
    if(len < 4) len = 4;
    if(len > DW_FIGHT_SEQ_MAX) len = DW_FIGHT_SEQ_MAX;
    app->cop_sequence_len = len;
    app->cop_sequence_pos = 0;
    app->cop_game_phase = 0;
    for(uint8_t i = 0; i < app->cop_sequence_len; i++) {
        app->cop_sequence[i] = dw_rand(4);
        if(i > 0 && app->cop_sequence[i] == app->cop_sequence[i - 1] && dw_rand(100) < 45) {
            app->cop_sequence[i] = (app->cop_sequence[i] + 1 + dw_rand(3)) & 3;
        }
    }
    app->cop_skill_started = furi_get_tick();
    app->mode = DwModeCopFightGame;
    app->current_view = DwViewHub;
    dw_start_cop_skill_timer(app);
    view_commit_model(app->hub_view, true);
    view_dispatcher_switch_to_view(app->dispatcher, DwViewHub);
}

static void dw_start_cop_run(DwApp* app) {
    uint8_t officers = app->pending_officers ? app->pending_officers : 1;
    app->cop_game_phase = 0;
    app->cop_skill_moves = 0;
    uint8_t pressure = dw_run_pressure(app);
    app->cop_skill_speed = MIN(3, 1 + (pressure / 50) + (officers / 4));
    app->cop_skill_width = 34;
    app->run_lane = 1;
    app->run_hits = 0;
    app->run_bullet_count = MIN((uint8_t)2, 1 + (pressure / 70) + (officers / 4));
    app->run_ticks_goal = 60 + (pressure / 4) + (officers * 4) + MIN((uint8_t)10, app->cop_streak);
    for(uint8_t i = 0; i < DW_RUN_BULLETS_MAX; i++) {
        app->run_bullet_lane[i] = i % 3;
        app->run_bullet_x[i] = (uint8_t)(92 + (i * 30));
        if(app->run_bullet_x[i] > 124) app->run_bullet_x[i] = 124;
    }
    app->cop_skill_started = furi_get_tick();
    app->mode = DwModeCopRunGame;
    app->current_view = DwViewHub;
    dw_start_cop_skill_timer(app);
    view_commit_model(app->hub_view, true);
    view_dispatcher_switch_to_view(app->dispatcher, DwViewHub);
}

static void dw_cop_fight(DwApp* app, uint8_t skill_score) {
    uint8_t officers = app->pending_officers ? app->pending_officers : 1;
    int32_t pressure = (officers * 14) + (app->heat / 12);
    int32_t result = (int32_t)skill_score - pressure;
    app->cops_fought++;

    if(result >= 55) {
        app->fight_wins++;
        app->cop_streak = MIN(99, app->cop_streak + 1);
        int32_t combo_bonus =
            ((int32_t)app->cop_streak * officers * (skill_score >= 95 ? 140 : 80));
        if((app->cop_streak % 3) == 0) combo_bonus += 750 * officers;
        int32_t loot = ((int32_t)dw_rand_range(150, 550) * officers) +
                       ((int32_t)skill_score * officers * 5) + combo_bonus;
        app->cash += loot;
        app->heat = MIN(100, app->heat + 12 + (officers * 2));
        snprintf(
            app->last_msg,
            sizeof(app->last_msg),
            "Clean fight! Streak %u. Combo bonus $%ld. Dropped %u cop%s and grabbed $%ld.",
            app->cop_streak,
            (long)combo_bonus,
            officers,
            officers == 1 ? "" : "s",
            (long)loot);
        dw_sfx_good();
    } else if(result >= 20) {
        app->fight_messy++;
        app->cop_streak = 0;
        int32_t loss = (app->cash * (int32_t)(6 + officers * 3)) / 100;
        app->cash -= loss;
        app->heat = MIN(100, app->heat + 15 + (officers * 2));
        snprintf(
            app->last_msg,
            sizeof(app->last_msg),
            "Messy win. Skill %u%%. You escaped but lost $%ld.",
            skill_score,
            (long)loss);
        dw_sfx_good();
    } else {
        app->fight_losses++;
        app->cop_streak = 0;
        uint8_t loss_pct = 18 + (officers * 8);
        if(result < 0) loss_pct += (uint8_t)MIN(30, -result / 2);
        if(loss_pct > 78) loss_pct = 78;
        int32_t loss = (app->cash * (int32_t)loss_pct) / 100;
        uint8_t drug = 0;
        for(uint8_t i = 1; i < DW_DRUGS; i++) {
            if(app->inventory[i] > app->inventory[drug]) drug = i;
        }
        uint16_t lost = (app->inventory[drug] * (uint16_t)loss_pct) / 100;
        app->cash -= loss;
        app->inventory[drug] -= lost;
        app->heat = MIN(100, app->heat + 22 + (officers * 3));
        snprintf(
            app->last_msg,
            sizeof(app->last_msg),
            "Lost the fight. Skill %u%%. Cops took $%ld and %u %s.",
            skill_score,
            (long)loss,
            lost,
            dw_drugs[drug].name);
        dw_sfx_bad();
    }
    app->pending_officers = 0;
    dw_record_action(app, 4, app->cash, ((uint16_t)officers << 8) | skill_score);
    dw_save_game(app);
    dw_show_status(app, "Aftermath", app->last_msg);
}

static void dw_cop_run(DwApp* app, uint8_t skill_score) {
    uint8_t officers = app->pending_officers ? app->pending_officers : 1;
    int32_t pressure = (officers * 12) + (app->heat / 14);
    int32_t result = (int32_t)skill_score - pressure;
    app->cops_ran++;
    if(result >= 45) {
        app->run_clean++;
        app->cop_streak = MIN(99, app->cop_streak + 1);
        uint8_t drop = dw_heat_drop(app, 3 + officers + (skill_score / 35));
        int32_t stash_bonus =
            75 + ((int32_t)skill_score * officers) + ((int32_t)app->cop_streak * 50);
        app->cash += stash_bonus;
        snprintf(
            app->last_msg,
            sizeof(app->last_msg),
            "Clean getaway! Streak %u. Heat -%u. Stash saved +$%ld.",
            app->cop_streak,
            drop,
            (long)stash_bonus);
        dw_sfx_good();
    } else if(result >= 10) {
        app->run_messy++;
        app->cop_streak = 0;
        int32_t fine = dw_rand_range(100, 550) * officers;
        app->cash = app->cash > fine ? app->cash - fine : 0;
        app->heat = MIN(100, app->heat + 4 + officers);
        snprintf(
            app->last_msg,
            sizeof(app->last_msg),
            "Messy run. Skill %u%%. Ditched them but dropped $%ld.",
            skill_score,
            (long)fine);
        dw_sfx_good();
    } else {
        app->run_caught++;
        app->cop_streak = 0;
        int32_t fine = dw_rand_range(500, 2000) * officers;
        uint8_t drug = dw_rand(DW_DRUGS);
        uint16_t lost = (app->inventory[drug] * (uint16_t)dw_rand_range(20, 60)) / 100;
        app->cash = app->cash > fine ? app->cash - fine : 0;
        app->inventory[drug] -= lost;
        app->heat = MIN(100, app->heat + 12 + (officers * 2));
        snprintf(
            app->last_msg,
            sizeof(app->last_msg),
            "Caught running. Skill %u%%. Lost $%ld and %u %s.",
            skill_score,
            (long)fine,
            lost,
            dw_drugs[drug].name);
        dw_sfx_bad();
    }
    app->pending_officers = 0;
    dw_record_action(app, 5, app->cash, ((uint16_t)officers << 8) | skill_score);
    dw_save_game(app);
    dw_show_status(app, "Aftermath", app->last_msg);
}

static const char* dw_rank_for(int32_t net) {
    if(net >= 1000000) return "The Plug";
    if(net >= 500000) return "Kingpin";
    if(net >= 250000) return "Borough Boss";
    if(net >= 100000) return "Neighborhood Connect";
    if(net >= 50000) return "Block Captain";
    if(net >= 10000) return "Corner Boy";
    if(net >= 0) return "Small Time Hustler";
    if(net >= -10000) return "Street Corner Bum";
    return "Dead Broke";
}

static void dw_end_game(DwApp* app) {
    app->mode = DwModeEnd;
    int32_t inv_value = dw_run_inventory_value(app, false);
    int32_t net = dw_run_net(app, false);
    int32_t profit = app->total_sold - app->total_bought;
    uint32_t code = dw_score_code(app, net, profit);
    dw_update_stats(app, net, profit, inv_value);
    dw_export_profile(app, net, profit, inv_value, code, true);
    dw_autosync_stats_profile(app, true);
    dw_delete_save();
    dw_sfx_game_over(net >= 0);
    widget_reset(app->widget);
    snprintf(
        app->text,
        sizeof(app->text),
        "\e#%s\nRank: %s\nNet: $%ld\nScore code: #%08lX\nCash: $%ld\nBank: $%ld\nDebt: $%ld\nInv value: $%ld\nProfit: $%ld\nBought: $%ld\nSold: $%ld\nBig deal: $%ld\nCops fought: %u\nCops ran: %u\nHeat: %u%%",
        net >= 0 ? "YOU MADE IT" : "GAME OVER",
        dw_rank_for(net),
        (long)net,
        (unsigned long)code,
        (long)app->cash,
        (long)app->bank,
        (long)app->debt,
        (long)inv_value,
        (long)profit,
        (long)app->total_bought,
        (long)app->total_sold,
        (long)app->biggest_deal,
        app->cops_fought,
        app->cops_ran,
        app->heat);
    widget_add_text_scroll_element(app->widget, 0, 0, 128, 52, app->text);
    widget_add_button_element(
        app->widget, GuiButtonTypeLeft, "Exit", dw_widget_button_callback, app);
    widget_add_button_element(
        app->widget, GuiButtonTypeCenter, "Global", dw_widget_button_callback, app);
    widget_add_button_element(
        app->widget, GuiButtonTypeRight, "New", dw_widget_button_callback, app);
    app->current_view = DwViewWidget;
    view_dispatcher_switch_to_view(app->dispatcher, DwViewWidget);
}

static void dw_handle_bank_deposit(DwApp* app) {
    if(app->cash <= 0) {
        dw_show_status(app, "Bank", "No cash to deposit.");
        return;
    }
    int32_t amount = app->cash;
    app->bank += amount;
    app->cash = 0;
    dw_record_action(app, 6, amount, 0);
    snprintf(
        app->last_msg,
        sizeof(app->last_msg),
        "Deposited $%ld. Bank balance $%ld.",
        (long)amount,
        (long)app->bank);
    dw_save_game(app);
    dw_show_loan(app);
}

static void dw_handle_bank_withdraw(DwApp* app) {
    if(app->bank <= 0) {
        dw_show_status(app, "Bank", "No bank balance to withdraw.");
        return;
    }
    int32_t amount = app->bank;
    app->cash += amount;
    app->bank = 0;
    dw_record_action(app, 7, amount, 0);
    snprintf(app->last_msg, sizeof(app->last_msg), "Withdrew $%ld.", (long)amount);
    dw_save_game(app);
    dw_show_loan(app);
}

static void dw_handle_loan(DwApp* app, bool all) {
    if(app->debt <= 0) {
        dw_show_status(app, "Loan Shark", "Debt is paid. Stay sharp.");
        return;
    }
    int32_t target = all ? app->debt : (app->debt / 2);
    int32_t amount = MIN(app->cash, target);
    if(amount <= 0) {
        dw_show_status(app, "Loan Shark", "No cash to repay.");
        return;
    }
    app->cash -= amount;
    app->debt -= amount;
    dw_record_action(app, 8, amount, all ? 1 : 0);
    snprintf(
        app->last_msg,
        sizeof(app->last_msg),
        "Repaid $%ld. Debt now $%ld.",
        (long)amount,
        (long)app->debt);
    dw_save_game(app);
    dw_show_loan(app);
}

static void dw_handle_event(DwApp* app, uint32_t event) {
    if(event == DwEventCopSkillTick) {
        if(app->mode == DwModeCopFightGame) {
            dw_update_cop_fight_game(app);
            view_commit_model(app->hub_view, true);
        } else if(app->mode == DwModeCopRunGame) {
            dw_update_cop_run_game(app);
            if(app->mode == DwModeCopRunGame) view_commit_model(app->hub_view, true);
        }
    } else if(event >= DW_PRODUCT_BASE && event < DW_PRODUCT_BASE + DW_DRUGS) {
        dw_show_product(app, event - DW_PRODUCT_BASE);
    } else if(event >= DW_TRAVEL_BASE && event < DW_TRAVEL_BASE + DW_LOCS) {
        dw_travel_to(app, event - DW_TRAVEL_BASE);
    } else if(event == DwEventTravel) {
        dw_show_travel(app);
    } else if(event == DwEventBuyScreen) {
        dw_show_buy_list(app);
    } else if(event == DwEventSellScreen) {
        dw_show_sell_list(app);
    } else if(event == DwEventLoanScreen) {
        dw_show_loan(app);
    } else if(event == DwEventLeaderboard) {
        dw_show_leaderboard(app);
    } else if(event == DwEventGlobalInfo) {
        dw_show_global_leaderboard(app);
    } else if(event == DwEventGlobalFallback) {
        dw_show_status(
            app,
            "Global Board",
            "Fallback site:\nhttps://www.ck42x.com/dopeflipper\n\nIf BadUSB fails, connect USB and use Sync Flipper, or upload profile.txt from /ext/apps_data/ck42x_dopewars/.");
    } else if(event == DwEventGlobalWindows) {
        dw_run_global_badusb(app, DW_BADUSB_WIN_PATH, "windows");
    } else if(event == DwEventGlobalLinux) {
        dw_run_global_badusb(app, DW_BADUSB_LINUX_PATH, "linux");
    } else if(event == DwEventGlobalMac) {
        dw_run_global_badusb(app, DW_BADUSB_MAC_PATH, "mac");
    } else if(event == DwEventSaveResetScreen) {
        dw_show_save_reset(app);
    } else if(event == DwEventStartGame) {
        dw_show_main(app);
    } else if(event == DwEventBankDeposit) {
        dw_handle_bank_deposit(app);
    } else if(event == DwEventBankWithdraw) {
        dw_handle_bank_withdraw(app);
    } else if(event == DwEventLoanHalf) {
        dw_handle_loan(app, false);
    } else if(event == DwEventLoanAll) {
        dw_handle_loan(app, true);
    } else if(event == DwEventStatus) {
        dw_show_run_status(app);
    } else if(event == DwEventStatsScreen) {
        dw_show_stats(app);
    } else if(event == DwEventAbout) {
        dw_show_status(
            app,
            "DopeFlipper",
            "CK42X pocket market game.\n\nBuy low. Sell high. Pay the shark before 30 days run out.");
    } else if(event == DwEventSaveNow) {
        dw_save_game(app);
        snprintf(app->last_msg, sizeof(app->last_msg), "Run saved to SD card.");
        dw_show_status(app, "Saved", app->last_msg);
    } else if(event == DwEventRestart) {
        dw_delete_save();
        dw_new_game(app);
        dw_start_round(app);
        dw_save_game(app);
        dw_show_main(app);
    } else if(event == DwEventResetSave) {
        dw_delete_save();
        dw_new_game(app);
        dw_start_round(app);
        dw_save_game(app);
        dw_show_status(app, "Reset", "Saved run deleted. New run started.");
    } else if(event == DwEventCoatBuy) {
        if(app->coat_offer && app->cash >= 500) {
            app->cash -= 500;
            app->max_coat += app->coat_increase;
            dw_record_action(app, 9, 500, app->coat_increase);
            snprintf(
                app->last_msg,
                sizeof(app->last_msg),
                "Bought deeper pockets. Capacity now %u.",
                app->max_coat);
            app->coat_offer = false;
            app->coat_increase = 0;
            dw_sfx_good();
            dw_save_game(app);
        }
        dw_show_main(app);
    } else if(event == DwEventBackMain) {
        dw_show_main(app);
    } else if(event == DwEventBuy1) {
        dw_do_buy(app, 1);
    } else if(event == DwEventBuy5) {
        dw_do_buy(app, 5);
    } else if(event == DwEventBuy10) {
        dw_do_buy(app, 10);
    } else if(event == DwEventBuyMax) {
        uint8_t i = app->selected_product;
        int32_t price = app->prices[i];
        uint16_t fit = app->max_coat - dw_coat_used(app);
        uint16_t affordable = price > 0 ? app->cash / price : 0;
        dw_do_buy(app, MIN(fit, affordable));
    } else if(event == DwEventSell1) {
        dw_do_sell(app, 1);
    } else if(event == DwEventSell5) {
        dw_do_sell(app, 5);
    } else if(event == DwEventSell10) {
        dw_do_sell(app, 10);
    } else if(event == DwEventSellAll) {
        dw_do_sell(app, app->inventory[app->selected_product]);
    } else if(event == DwEventCopFight) {
        dw_start_cop_fight(app);
    } else if(event == DwEventCopRun) {
        dw_start_cop_run(app);
    } else if(event == DwEventWidgetLeft) {
        if(app->mode == DwModeEnd) {
            view_dispatcher_stop(app->dispatcher);
        } else {
            dw_show_main(app);
        }
    } else if(event == DwEventWidgetRight) {
        if(app->mode == DwModeEnd) {
            dw_delete_save();
            dw_new_game(app);
            dw_start_round(app);
            dw_save_game(app);
            dw_show_main(app);
        }
    } else if(event == DwEventWidgetCenter) {
        if(app->mode == DwModeEnd) {
            dw_show_global_leaderboard(app);
        }
    }
}

static DwApp* dw_app_alloc(void) {
    DwApp* app = malloc(sizeof(DwApp));
    furi_assert(app);
    memset(app, 0, sizeof(DwApp));

    app->dispatcher = view_dispatcher_alloc();
    app->submenu = submenu_alloc();
    app->widget = widget_alloc();
    app->art_view = view_alloc();
    app->hub_view = view_alloc();
    app->cop_skill_timer =
        furi_timer_alloc(dw_cop_skill_tick_callback, FuriTimerTypePeriodic, app);

    view_dispatcher_set_event_callback_context(app->dispatcher, app);
    view_dispatcher_set_custom_event_callback(app->dispatcher, dw_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(app->dispatcher, dw_navigation_callback);
    view_set_draw_callback(app->art_view, dw_art_draw_callback);
    view_set_input_callback(app->art_view, dw_art_input_callback);
    view_set_context(app->art_view, app);
    view_set_draw_callback(app->hub_view, dw_hub_draw_callback);
    view_set_input_callback(app->hub_view, dw_hub_input_callback);
    view_set_context(app->hub_view, app);
    view_allocate_model(app->hub_view, ViewModelTypeLockFree, sizeof(DwApp*));
    DwApp** hub_model = view_get_model(app->hub_view);
    *hub_model = app;
    view_commit_model(app->hub_view, false);
    view_allocate_model(app->art_view, ViewModelTypeLockFree, sizeof(DwApp*));
    DwApp** art_model = view_get_model(app->art_view);
    *art_model = app;
    view_commit_model(app->art_view, false);
    view_dispatcher_add_view(app->dispatcher, DwViewMenu, submenu_get_view(app->submenu));
    view_dispatcher_add_view(app->dispatcher, DwViewWidget, widget_get_view(app->widget));
    view_dispatcher_add_view(app->dispatcher, DwViewArt, app->art_view);
    view_dispatcher_add_view(app->dispatcher, DwViewHub, app->hub_view);

    dw_load_stats(app);

    if(!dw_load_game(app)) {
        dw_new_game(app);
        dw_start_round(app);
        dw_save_game(app);
    }
    return app;
}

static void dw_app_free(DwApp* app) {
    if(!app) return;
    dw_stop_cop_skill_timer(app);
    if(app->cop_skill_timer) furi_timer_free(app->cop_skill_timer);
    view_dispatcher_remove_view(app->dispatcher, DwViewMenu);
    view_dispatcher_remove_view(app->dispatcher, DwViewWidget);
    view_dispatcher_remove_view(app->dispatcher, DwViewArt);
    view_dispatcher_remove_view(app->dispatcher, DwViewHub);
    view_free(app->art_view);
    view_free(app->hub_view);
    submenu_free(app->submenu);
    widget_free(app->widget);
    dw_save_game(app);
    view_dispatcher_free(app->dispatcher);
    free(app);
}

int32_t ck42x_dopewars_app(void* p) {
    UNUSED(p);
    FURI_LOG_I(DW_TAG, "Starting DopeFlipper");

    DwApp* app = dw_app_alloc();
    app->gui = furi_record_open(RECORD_GUI);
    view_dispatcher_attach_to_gui(app->dispatcher, app->gui, ViewDispatcherTypeFullscreen);
    dw_show_loading(app);
    dw_sfx_good();
    furi_delay_ms(850);
    dw_show_title(app);
    view_dispatcher_run(app->dispatcher);

    furi_record_close(RECORD_GUI);
    dw_app_free(app);
    FURI_LOG_I(DW_TAG, "Stopped DopeFlipper");
    return 0;
}
