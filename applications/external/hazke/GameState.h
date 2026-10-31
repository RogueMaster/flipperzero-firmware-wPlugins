#pragma once
#include "Platform.h"
#include <string.h>
#include "Input.h"
#include "Market.h"

struct GameState {
    // Throttle 0..1
    float speed;

    // Smoothed input axes -1..1
    float pitchInput;
    float rollInput;
    float yawInput;

    // Angular rates (rad/sec) — used by the starfield to simulate flight
    float pitchRate;
    float rollRate;
    float yawRate; // Z / X keys — flat nose-left/right turn

    // HUD readouts (0..1)
    float shield;
    float hull; // 0..1 direct ship integrity; lost to collisions / spillover

    // Wallet
    int credits; // stored as tenths of CR (Elite style)

    // Commander identity + progression
    static constexpr int NameCap = 12;
    char commanderName[NameCap];
    int kills;

    // Cargo hold — `cargoMax` is mutable so the equipment shop's LARGE
    // HOLD upgrade can raise it (20 → 35 t).
    static constexpr int CargoMaxDefault = 20;
    static constexpr int CargoMaxLarge = 35;
    uint8_t cargoMax;
    uint8_t cargo[Market::N];

    // R18 equipment loadout. Set by EquipScreen, read by Combat / Cockpit /
    // Status. Bools default false; counters default 0; laserTier 0 = Pulse.
    uint8_t missiles; // 0..4
    bool ecm;
    uint8_t laserTier; // 0 Pulse, 1 Beam, 2 Military
    bool autolock; // AUTOLOCK module fitted — F steers onto the marker

    // R21: ECM is unlimited but throttled. `ecmCooldown` decrements toward
    // zero; the input handler refuses to fire while > 0.
    float ecmCooldown;

    // R22: per-faction standing, indexed by Faction::Id (-100..+100). 0
    // means neutral. Kills, patrol attacks, and trade events nudge these.
    static constexpr int NumFactions = 4;
    int8_t standing[NumFactions];

    // R24: highest rank index the player has been shown a promotion for.
    // Bumped by Rank::checkPromotion when kills crosses a tier so each
    // promotion fires its banner exactly once.
    uint8_t lastSeenRank;

    // R26: sun-proximity heat. Builds up while skimming the star (rate
    // scales with closeness); decays whenever the player is clear.
    // ≥ 1.0 means actively damaging the ship. SystemFlight owns the
    // physics; Cockpit & HUD read this for the warning banners.
    float hullHeat;

    // R27: Imperium-vs-Cartel arc.
    //   arcStage: 0 not started, 1..4 in progress, 5 complete.
    //   arcSide:  0 undecided, 1 Imperium loyal, 2 Cartel defector.
    // Arc::advance bumps these from Mission::payOut when an arc slot
    // completes; UI in MissionScreen reads them to gate offer rows.
    uint8_t arcStage;
    uint8_t arcSide;

    int cargoTotal() const {
        int t = 0;
        for(int i = 0; i < (int)Market::N; i++)
            t += cargo[i];
        return t;
    }
    int cargoFree() const {
        return (int)cargoMax - cargoTotal();
    }

    // Returns false if the trade can't happen (no funds, no space, no stock).
    bool buyOne(int commodity, int sysQtyAvailable, int unitPriceTenths) {
        if(sysQtyAvailable <= 0) return false;
        if(cargoFree() <= 0) return false;
        if(credits < unitPriceTenths) return false;
        credits -= unitPriceTenths;
        cargo[commodity]++;
        return true;
    }

    bool sellOne(int commodity, int unitPriceTenths) {
        if(cargo[commodity] == 0) return false;
        cargo[commodity]--;
        credits += unitPriceTenths;
        return true;
    }

    void reset() {
        speed = 0.0f;
        pitchInput = rollInput = yawInput = 0.0f;
        pitchRate = rollRate = yawRate = 0.0f;
        shield = 1.0f;
        hull = 1.0f;
        credits = 1000; // shows as 100.0 CR
        kills = 0;
        strncpy(commanderName, "JAMESON", NameCap);
        commanderName[NameCap - 1] = '\0';
        memset(cargo, 0, sizeof(cargo));
        cargoMax = CargoMaxDefault;
        missiles = 0;
        ecm = false;
        laserTier = 0;
        autolock = false;
        ecmCooldown = 0.0f;
        for(int i = 0; i < NumFactions; i++)
            standing[i] = 0;
        lastSeenRank = 0; // Harmless
        hullHeat = 0.0f;
        arcStage = 0;
        arcSide = 0;
    }

    static float clamp01(float v) {
        if(v < 0.0f) return 0.0f;
        if(v > 1.0f) return 1.0f;
        return v;
    }

    void update(const InputState& in, float dt) {
        // Quick to answer a button and quicker to settle when it's let go,
        // so turning stops where the player stops.
        auto axis = [&](float& v, bool pos, bool neg) {
            float target = pos ? 1.0f : (neg ? -1.0f : 0.0f);
            float k = (target == 0.0f ? 30.0f : 16.0f) * dt;
            if(k > 1.0f) k = 1.0f;
            v += (target - v) * k;
            if(target == 0.0f && fabsf(v) < 0.02f) v = 0.0f;
        };
        axis(pitchInput, in.pitchUp, in.pitchDown);
        axis(rollInput, in.rollRight, in.rollLeft);
        axis(yawInput, in.yawRight, in.yawLeft);

        pitchRate = pitchInput * 1.5f;
        rollRate = rollInput * 2.4f;
        yawRate = yawInput * 1.1f;
    }
};
