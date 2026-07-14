#pragma once

#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <input/input.h>
#include <stdlib.h>
#include <storage/storage.h>
#include "p1x_network_defender_icons.h"

// Game constants
#define SCREEN_WIDTH               128
#define SCREEN_HEIGHT              64
#define MAX_PACKETS                16 // Increased DDOS limit to 16 (4 per computer)
#define MAX_PACKETS_PER_COMPUTER   4 // Maximum packets per computer before overflow
#define DDOS_LIMIT                 17 // Game over when total packets reaches this number
#define COMPUTER_OVERFLOW          5 // Game over when one computer reaches this many packets
#define HACK_WARN_TIME             2000 // 2 seconds warning (in ms)
#define HACK_TIME                  5000 // 5 seconds to fully hack (in ms)
#define PATCH_TIME                 2000 // 2 seconds to patch (in ms)
#define PACKET_ANIMATION_TIME      300 // Faster animation for accepted packets
#define PACKET_SPAWN_CHANCE        1 // Reduced packet spawn chance for slower gameplay
#define BASE_PACKET_SPAWN_INTERVAL 1800 // Base interval between packet spawns (ms)
#define ACCELERATION_FACTOR        0.5 // Each dead computer makes game faster (halves the interval)
#define HISCORE_FILENAME           APP_DATA_PATH("hiscore.save") // High score save file

// Add upcoming packets queue to game state
#define MAX_UPCOMING_PACKETS 3

// Define game states
#define GAME_STATE_TITLE 0
#define GAME_STATE_MENU  1
#define GAME_STATE_HELP  2
#define GAME_STATE_PLAY  3

// Game over reasons
#define GAME_OVER_HACK 1
#define GAME_OVER_DDOS 2

// System positions (4 corners)
typedef struct {
    int x, y;
} Position;

// Game state
typedef struct {
    int player_pos; // 0-3 for the 4 positions
    bool hacking[4]; // Is system being hacked?
    uint32_t hack_start[4]; // When hacking began (ms)
    uint32_t warn_start[4]; // When warning began (ms)
    int packets[4]; // Packets waiting at each position
    bool patching; // Currently patching?
    uint32_t patch_start; // When patching began (ms)
    bool game_over;
    int game_over_reason; // 1=hack, 2=DDOS
    bool packet_animation; // Packet moving to server
    uint32_t anim_start; // When animation began
    int anim_from; // Which system accepted the packet
    int score; // Player's score
    int upcoming_packets[4][MAX_UPCOMING_PACKETS]; // Queue of upcoming packets for each system
    uint32_t last_packet_spawn_time; // Last packet spawn time
    int current_state; // Current game state
    int help_page; // Help page number
    int menu_selection; // Menu selection index
    bool computer_dead[4]; // Is the computer dead (hacked)
    int high_score; // High score from previous games
    bool new_high_score; // Flag to indicate if current score is a new high score
} GameState;

// Forward declaration for system positions
extern const Position SYSTEM_POSITIONS[4];
extern const Position PLAYER_POSITIONS[4];
extern const Position SERVER_POSITION;
