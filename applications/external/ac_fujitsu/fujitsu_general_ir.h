#pragma once

#include <stdint.h>

typedef enum {
    Fuji56ProtocolCommand_PowerOff,
    Fuji56ProtocolCommand_Set,
    Fuji56ProtocolCommand_Economy,
    Fuji56ProtocolCommand_Powerfull
} Fuji56ProtocolCommand;

void send_fuji56_command(Fuji56ProtocolCommand cmd);

typedef struct {
    uint8_t temperature; // [20,44]
    uint8_t power; // 1 only when the turn on signal is sent, otherwise 0
    uint8_t timer_type; // 0 -> clean all timers, 1 -> sleep, 2 -> off, 3 -> on, 4 -> off->on/on->off
    uint8_t mode; // 0 Auto, 1 Cool, 2 Dry, 3 Fan, 4 Heat
    uint8_t swing; // 0 or 1
    uint8_t fan_speed; // Auto -> 0 , Speed 4 -> 1, Speed 3 -> 2, Speed 2 -> 3, speed 1 -> 4
    uint32_t timer_off; // minutes for off or sleep.
        // Allowed values for sleep - 30, 60, 120, 180, 300, 420, 540
        // Allowed values for off - 30, 60, 90, 120, 150, ..., 600, 660, 720
    uint8_t flag_timer_off; // 1 when sleep or off -> on, 0 otherwise
    uint32_t timer_on; // Same as allowed values for on
} Fuji128Signal;

void send_fuji128_state(Fuji128Signal* sig);
