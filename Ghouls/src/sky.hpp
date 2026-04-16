#pragma once
#include "pico-game-engine/engine/draw.hpp"
#include "general.hpp"

typedef enum
{
    SKY_SUNNY,  // clear/bright sky
    SKY_CLOUDY, // overcast sky
    SKY_DARK    // stormy/night sky
} SkyType;

class Sky
{
public:
    Sky(SkyType type);
    ~Sky();

    void render(Draw *draw);          // Render the sky using the provided Draw object
    void setSkyType(SkyType newType); // Change the sky type
    void tick();                      // advance the sky's internal time

private:
    SkyType type;
    uint32_t time;
};