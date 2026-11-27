#pragma once
/*
 * loop_health.h — how long loop() passes take, and where the time went.
 *
 * Everything that reads CAN runs on the loop task, so a long pass is a gap in
 * reception: the TWAI queue and the MCP2515's two buffers fill and then drop
 * frames (#176, #228). main.cpp marks each stage of loop() as it ends; a pass
 * runs from one loop() entry to the next, so time spent outside loop() (other
 * tasks on the core, the Arduino loop task itself) lands in "other". Each pass
 * keeps its longest stage, so the longest pass says where the stall came from.
 *
 * Kept since boot and since the CAN Bus card's reset (#252). Pure, so the host
 * tests run it (test/test_loop_health.cpp); main.cpp owns the instance.
 */

#include <stdint.h>

// A pass longer than this is counted as slow: at 2,000 frames/s it is 50
// frames, more than the MCP2515 holds by far and a real dent in a TWAI queue.
#define LOOP_SLOW_US 25000u

enum LoopStage : uint8_t {
    LOOP_STAGE_CAN_RX = 0,  // draining the controllers, process_frame()
    LOOP_STAGE_LOGIC,       // Continuous AP, precondition, serial status lines
    LOOP_STAGE_CAN_INIT,    // re-initialising a controller that failed
    LOOP_STAGE_LOGS,        // CAN dump, black box, tap check, power log, sleep
    LOOP_STAGE_WEB,         // dashboard, HTTP, WebSocket, capture stream
    LOOP_STAGE_OTHER,       // button, serial, LED, display, outside loop()
    LOOP_STAGE_COUNT,
};

static inline const char *loop_stage_name(uint8_t stage) {
    static const char *const kNames[LOOP_STAGE_COUNT] = {
        "can rx", "logic", "can init", "logs", "web", "other",
    };
    return stage < LOOP_STAGE_COUNT ? kNames[stage] : "?";
}

struct LoopPassStats {
    uint32_t max_us    = 0;                 // longest pass
    uint8_t  max_stage = LOOP_STAGE_OTHER;  // its longest stage
    uint32_t slow      = 0;                 // passes over LOOP_SLOW_US

    void note(uint32_t pass_us, uint8_t stage) {
        if (pass_us > LOOP_SLOW_US) slow++;
        if (pass_us > max_us) {
            max_us = pass_us;
            max_stage = stage;
        }
    }
};

struct LoopHealth {
    LoopPassStats boot;   // since boot
    LoopPassStats since;  // since reset_window()
    uint32_t stage_us[LOOP_STAGE_COUNT] = {};
    uint32_t start_us = 0;
    uint32_t mark_us  = 0;
    bool     running  = false;

    // Top of loop(): close the previous pass (the time since its last mark
    // was spent outside loop()) and start this one. Unsigned differences:
    // micros() wraps every 71 minutes and no pass is that long.
    void begin_pass(uint32_t now_us) {
        if (running) {
            stage_us[LOOP_STAGE_OTHER] += now_us - mark_us;
            uint8_t worst = 0;
            for (uint8_t i = 1; i < LOOP_STAGE_COUNT; i++)
                if (stage_us[i] > stage_us[worst]) worst = i;
            uint32_t pass = now_us - start_us;
            boot.note(pass, worst);
            since.note(pass, worst);
        }
        for (uint8_t i = 0; i < LOOP_STAGE_COUNT; i++) stage_us[i] = 0;
        start_us = mark_us = now_us;
        running  = true;
    }

    // A stage of loop() just ended: the time since the last mark was its.
    void mark(uint8_t stage, uint32_t now_us) {
        if (stage < LOOP_STAGE_COUNT) stage_us[stage] += now_us - mark_us;
        mark_us = now_us;
    }

    void reset_window() { since = LoopPassStats(); }
};

// main.cpp's instance, for the dashboard and its CAN counter reset.
const LoopHealth &loop_health();
void loop_health_reset_window();
