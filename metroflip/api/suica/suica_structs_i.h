#include <datetime.h>
#include <stdbool.h>
#include <furi.h>

typedef enum {
    SuicaKeikyu,
    SuicaTokyoMetro,
    SuicaToei,
    SuicaJREast,
    SuicaJRCentral,
    SuicaJRWest,
    SuicaJRKyushu,
    SuicaTokyu,
    SuicaMobile,
    SuicaTWR,
    SuicaYurikamome,
    SuicaTokyoMonorail,
    SuicaOsakaMetro,
    SuicaRailwayTypeMax,
} SuicaRailwayCompany;

typedef enum {
    SuicaBalanceAdd,
    SuicaBalanceSub,
    SuicaBalanceEqual,
} SuicaBalanceChangeSign;

typedef struct {
    uint8_t station_code;
    uint8_t station_number;
    FuriString* name;
    FuriString* jr_header;
} Station;

typedef struct {
    uint8_t line_code;
    int logo_offset[2];
    const char* long_name;
    uint8_t station_num;
    SuicaRailwayCompany type;
    const char* short_name;
    const Icon* logo_icon;
} Railway;

typedef struct {
    Railway entry_line;
    Station entry_station;
    Railway exit_line;
    Station exit_station;
    uint8_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint16_t balance;
    uint8_t history_type;
    uint8_t area_code;
    uint16_t previous_balance;
    uint16_t balance_change;
    SuicaBalanceChangeSign balance_sign;
    uint8_t shop_code[2];
} SuicaHistory;

typedef struct {
    uint8_t entry; // Which entry we are currently viewing
    uint8_t page; // Which vertial page we are on
    uint8_t* travel_history; // Dynamic array for raw 16-byte entries
    size_t size; // Number of entries currently stored
    size_t capacity; // Allocated capacity
    uint8_t animator_tick; // Counter for the animations
    SuicaHistory history; // Current history entry
} SuicaHistoryViewModel;
