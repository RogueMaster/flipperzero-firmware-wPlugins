#pragma once
#include "pico-game-engine/engine/draw.hpp"
#include "general.hpp"

typedef enum
{
    GROUND_GRASS, // green grassy ground
    GROUND_DIRT,  // muddy/earthy ground
    GROUND_DARK   // stormy/night ground
} GroundType;

class Ground
{
public:
    Ground(GroundType type);
    ~Ground();

    void render(Draw *draw);                // Render the ground using the provided Draw object
    void setGroundType(GroundType newType); // Change the ground type
    void tick();                            // advance the ground's internal time

private:
    uint32_t time;
    GroundType type;

    void drawGradientGround(Draw *draw,
                            uint8_t horizR, uint8_t horizG, uint8_t horizB,
                            uint8_t botR, uint8_t botG, uint8_t botB);
    uint16_t makeRGB565(uint8_t r, uint8_t g, uint8_t b);
};