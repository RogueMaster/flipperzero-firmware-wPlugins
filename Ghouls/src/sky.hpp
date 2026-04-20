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
    uint32_t time;
    SkyType type;

    void drawGradientSky(Draw *draw,
                         uint8_t topR, uint8_t topG, uint8_t topB,
                         uint8_t horizR, uint8_t horizG, uint8_t horizB);
    uint16_t makeRGB565(uint8_t r, uint8_t g, uint8_t b);
};