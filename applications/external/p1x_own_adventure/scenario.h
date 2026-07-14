#ifndef SCENARIO_H
#define SCENARIO_H

#include <stdint.h>

// Maximum number of options per scene
#define MAX_OPTIONS    4
// Maximum number of text lines per scene
#define MAX_TEXT_LINES 10

// Option structure
typedef struct {
    char* text; // Text shown for this option
    uint8_t target_scene; // Scene ID this option leads to
} Option;

// Scene structure
typedef struct {
    char* text_lines[MAX_TEXT_LINES]; // Array of text lines for this scene
    uint8_t num_lines; // Number of text lines
    uint8_t num_options; // Number of available options
    Option options[MAX_OPTIONS]; // Available options
} Scene;

// Overall scenario structure
typedef struct {
    char* title; // Title of the adventure
    uint8_t num_scenes; // Number of scenes
    Scene* scenes; // Array of scenes
} Scenario;

// Define your scenario here
// This is an example scenario with a simple adventure
static Scene adventure_scenes[] = {
    // Scene 0: Title Screen
    {.text_lines =
         {"Welcome to the mysterious", "tower adventure! You must", "find your way to escape..."},
     .num_lines = 3,
     .num_options = 1,
     .options = {{.text = "Start adventure", .target_scene = 1}}},

    // Scene 1: Start (was Scene 0 before)
    {.text_lines =
         {"You wake up in a dark room.",
          "You can't see much, but you",
          "feel a door and a window."},
     .num_lines = 3,
     .num_options = 2,
     .options =
         {{.text = "Try the door", .target_scene = 2},
          {.text = "Look out the window", .target_scene = 3}}},

    // Scene 2: Door (was Scene 1 before)
    {.text_lines = {"The door is locked. You hear", "something scratching from", "the other side."},
     .num_lines = 3,
     .num_options = 2,
     .options =
         {{.text = "Try to force the door", .target_scene = 4},
          {.text = "Go back and check the window", .target_scene = 3}}},

    // Scene 3: Window (was Scene 2 before)
    {.text_lines =
         {"The window shows you're in a",
          "tall tower. It's a long way",
          "down, but you see a ledge",
          "nearby."},
     .num_lines = 4,
     .num_options = 2,
     .options =
         {{.text = "Try to reach the ledge", .target_scene = 5},
          {.text = "Go back to the door", .target_scene = 2}}},

    // Scene 4: Force the door (was Scene 3 before)
    {.text_lines = {"You break through the door!", "But a monster was waiting.", "GAME OVER."},
     .num_lines = 3,
     .num_options = 1,
     .options = {{.text = "Start again", .target_scene = 0}}},

    // Scene 5: Ledge (was Scene 4 before)
    {.text_lines =
         {"You climb onto the ledge", "and find a hidden passage.", "You've escaped!", "YOU WIN!"},
     .num_lines = 4,
     .num_options = 1,
     .options = {{.text = "Play again", .target_scene = 0}}}};

// The complete scenario definition
static const Scenario SCENARIO = {
    .title = "Tower Escape by P1X",
    .num_scenes = 6,
    .scenes = adventure_scenes};

#endif // SCENARIO_H
