#pragma once
#include "camera.hpp"
#include "draw.hpp"
#include "level.hpp"
#include "vector.hpp"
#include "callback.hpp"

#define MAX_LEVELS 10

class Game
{
public:
    Game(
        const char *name,                // Name of the game
        Vector size,                     // game/world size
        Draw *draw,                      // drawing object for rendering
        uint16_t fg_color = 0x0000,      // Foreground color
        uint16_t bg_color = 0xFFFF,      // Background color
        Camera *cameraContext = nullptr, // Camera context for rendering
        CallbackVoid start = {},         // Callback function for when the game starts
        CallbackVoid stop = {},          // Callback function for when the game stops
        CallbackVoid update = {});       // Callback function for when the game updates
    ~Game();

    void clamp(float &value, float min, float max); // Clamp a value between a lower and upper bound.
    Camera *getCamera() const { return camera; };   // Get current camera
    void level_add(Level *level);                   // Add a level to the game
    void level_remove(Level *level);                // Remove a level from the game
    void level_switch(const char *name);            // Switch to a level by name
    void level_switch(int index);                   // Switch to a level by index
    void render();                                  // Called every frame to render the game
    void setCamera(const Camera &cameraContext);    // Set the current camera
    void start();                                   // Called when the game starts
    void stop();                                    // Called when the game stops
    void update();                                  // Called every frame to update the game

    const char *name;          // Name of the game
    Level *levels[MAX_LEVELS]; // Array of levels
    Level *current_level;      // Current level
    Draw *draw;                // Draw object for rendering
    int input;                 // Last input (e.g., one of the BUTTON_ constants)
    Camera *camera;            // Camera context
    Vector pos;                // Player position
    Vector old_pos;            // Previous position
    Vector size;               // Game/World size
    bool is_active;            // Whether the game is active
    uint16_t bg_color;         // Background color
    uint16_t fg_color;         // Foreground color
    CallbackVoid _start;
    CallbackVoid _stop;
    CallbackVoid _update;

private:
};
