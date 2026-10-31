#pragma once
#include <storage/storage.h>
#include <toolbox/saved_struct.h>
#include <stddef.h>
#include "Platform.h"
#include "GameState.h"
#include "Galaxy.h"
#include "Market.h"
#include "Quest.h"

// One save slot on the SD card, written every time the ship lands and when
// it launches, plus a separate settings file.

namespace Save {

constexpr uint8_t Magic = 0x48; // 'H'
constexpr uint8_t Version = 2;
// Older layouts stay frozen below so their saves still load.
constexpr uint8_t VersionV1 = 1;
constexpr uint8_t SettingsVersion = 1;
#define HAZKE_SAVE_PATH     APP_DATA_PATH("save.dat")
#define HAZKE_SETTINGS_PATH APP_DATA_PATH("settings.dat")

// Version 1: before jobs. Frozen.
struct DataV1 {
    int32_t credits;
    int32_t kills;
    float shield;
    float hull;
    uint32_t marketEpoch;
    uint8_t cargo[Market::N];
    uint8_t localQty[Market::N];
    int8_t standing[GameState::NumFactions];
    uint8_t cargoMax;
    uint8_t missiles;
    uint8_t ecm;
    uint8_t laserTier;
    uint8_t lastSeenRank;
    uint8_t system;
    uint8_t planetPOI;
    uint8_t _pad;
};

// Version 2: version 1 plus the active job.
struct Data {
    int32_t credits;
    int32_t kills;
    float shield;
    float hull;
    uint32_t marketEpoch;
    uint8_t cargo[Market::N];
    uint8_t localQty[Market::N]; // what the planet has left to sell
    int8_t standing[GameState::NumFactions];
    uint8_t cargoMax;
    uint8_t missiles;
    uint8_t ecm;
    uint8_t laserTier;
    uint8_t lastSeenRank;
    uint8_t system;
    uint8_t planetPOI;
    uint8_t _pad;
    // Active job.
    int16_t questReward;
    uint16_t questProgress;
    uint8_t questType;
    uint8_t questStatus;
    uint8_t questFromSys;
    uint8_t questFromPOI;
    uint8_t questToPOI;
    uint8_t questCommodity;
    uint8_t questQty;
    uint8_t questFaction;
    int8_t questFactionDelta;
    uint8_t questDifficulty;
    uint8_t questSpawnPending;
    uint8_t _pad2;
};

struct Settings {
    uint8_t fx;
    uint8_t light; // 1 = light screen, 0 = dark
    uint8_t _pad[2];
};

inline void ensureDir() {
    Storage* st = (Storage*)furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(st, STORAGE_APP_DATA_PATH_PREFIX);
    furi_record_close(RECORD_STORAGE);
}

inline bool write(const GameState& g, int sys, int poi, const uint8_t localQty[Market::N]) {
    Data d;
    memset(&d, 0, sizeof(d));
    d.credits = g.credits;
    d.kills = g.kills;
    d.shield = g.shield;
    d.hull = g.hull;
    d.marketEpoch = Galaxy::marketEpoch;
    memcpy(d.cargo, g.cargo, sizeof(d.cargo));
    memcpy(d.localQty, localQty, sizeof(d.localQty));
    memcpy(d.standing, g.standing, sizeof(d.standing));
    d.cargoMax = g.cargoMax;
    d.missiles = g.missiles;
    d.ecm = g.ecm ? 1 : 0;
    d.laserTier = g.laserTier;
    d.lastSeenRank = g.lastSeenRank;
    d.system = (uint8_t)sys;
    d.planetPOI = (uint8_t)poi;
    const Quest::Slot& q = Quest::active;
    d.questType = (uint8_t)q.type;
    d.questStatus = (uint8_t)Quest::status;
    d.questReward = q.rewardTenthsCR;
    d.questProgress = q.progress;
    d.questFromSys = q.fromSys;
    d.questFromPOI = q.fromPOI;
    d.questToPOI = q.toPOI;
    d.questCommodity = q.commodity;
    d.questQty = q.qty;
    d.questFaction = q.faction;
    d.questFactionDelta = q.factionDelta;
    d.questDifficulty = q.difficulty;
    d.questSpawnPending = Quest::pirateSpawnPending ? 1 : 0;
    ensureDir();
    return saved_struct_save(HAZKE_SAVE_PATH, &d, sizeof(d), Magic, Version);
}

inline bool read(Data& d) {
    if(!saved_struct_load(HAZKE_SAVE_PATH, &d, sizeof(d), Magic, Version)) {
        DataV1 v1;
        if(!saved_struct_load(HAZKE_SAVE_PATH, &v1, sizeof(v1), Magic, VersionV1)) return false;
        memset(&d, 0, sizeof(d));
        // Version 2 starts with the version 1 fields.
        memcpy(&d, &v1, offsetof(DataV1, _pad) + 1);
    }
    if(d.questType >= (uint8_t)Quest::Type::Count || d.questStatus > 2) {
        d.questType = 0;
        d.questStatus = 0;
    }
    return d.system < Galaxy::NumSystems && d.laserTier < 3 &&
           d.cargoMax >= GameState::CargoMaxDefault;
}

inline bool exists() {
    Data d;
    return read(d);
}

// Into a freshly reset GameState.
inline void apply(const Data& d, GameState& g) {
    g.credits = d.credits;
    g.kills = d.kills;
    g.shield = d.shield;
    g.hull = d.hull;
    Galaxy::marketEpoch = d.marketEpoch;
    memcpy(g.cargo, d.cargo, sizeof(g.cargo));
    memcpy(g.standing, d.standing, sizeof(g.standing));
    g.cargoMax = d.cargoMax;
    g.missiles = d.missiles;
    g.ecm = d.ecm != 0;
    g.laserTier = d.laserTier;
    g.lastSeenRank = d.lastSeenRank;
    Quest::resetAll();
    if(d.questType != 0 && d.questStatus != 0) {
        Quest::Slot& q = Quest::active;
        q.type = (Quest::Type)d.questType;
        q.rewardTenthsCR = d.questReward;
        q.progress = d.questProgress;
        q.fromSys = d.questFromSys;
        q.fromPOI = d.questFromPOI;
        q.toPOI = d.questToPOI;
        q.commodity = d.questCommodity;
        q.qty = d.questQty;
        q.faction = d.questFaction;
        q.factionDelta = d.questFactionDelta;
        q.difficulty = d.questDifficulty;
        Quest::status = (Quest::Status)d.questStatus;
        Quest::pirateSpawnPending = d.questSpawnPending != 0;
    }
}

inline void writeSettings(uint8_t fx, bool light) {
    Settings s = {fx, (uint8_t)(light ? 1 : 0), {0, 0}};
    ensureDir();
    saved_struct_save(HAZKE_SETTINGS_PATH, &s, sizeof(s), Magic, SettingsVersion);
}

inline uint8_t readSettings(uint8_t fallback, bool& light) {
    Settings s;
    if(!saved_struct_load(HAZKE_SETTINGS_PATH, &s, sizeof(s), Magic, SettingsVersion))
        return fallback;
    light = s.light != 0;
    return s.fx;
}

} // namespace Save
