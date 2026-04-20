#include "sky.hpp"

Sky::Sky(SkyType type) : time(0), type(type)
{
}

Sky::~Sky()
{
}

void Sky::drawGradientSky(Draw *draw,
                          uint8_t topR, uint8_t topG, uint8_t topB,
                          uint8_t horizR, uint8_t horizG, uint8_t horizB)
{
    const int drdy = ((horizR - topR) * FIXED_POINT_SCALE) / SKY_HORIZON_HEIGHT;
    const int dgdy = ((horizG - topG) * FIXED_POINT_SCALE) / SKY_HORIZON_HEIGHT;
    const int dbdy = ((horizB - topB) * FIXED_POINT_SCALE) / SKY_HORIZON_HEIGHT;

    int r = topR * FIXED_POINT_SCALE;
    int g = topG * FIXED_POINT_SCALE;
    int b = topB * FIXED_POINT_SCALE;

    for (int y = 0; y < SKY_HORIZON_HEIGHT; y += SKY_HORIZON_ROWS)
    {
        uint16_t color = makeRGB565(r >> 8, g >> 8, b >> 8);

        // Draw SKY_HORIZON_ROWS rows with the same color
        int height = (y + SKY_HORIZON_ROWS < SKY_HORIZON_HEIGHT) ? SKY_HORIZON_ROWS : SKY_HORIZON_HEIGHT - y;
        draw->fillRectangle(0, y, ENGINE_LCD_WIDTH, height, color);

        r += drdy * SKY_HORIZON_ROWS;
        g += dgdy * SKY_HORIZON_ROWS;
        b += dbdy * SKY_HORIZON_ROWS;
    }
}

uint16_t Sky::makeRGB565(uint8_t r, uint8_t g, uint8_t b)
{
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
}

void Sky::render(Draw *draw)
{
    switch (this->type)
    {
    case SKY_SUNNY:
        drawGradientSky(draw,
                        100, 160, 255,
                        180, 220, 255);
        break;
    case SKY_CLOUDY:
        drawGradientSky(draw,
                        60, 70, 90,
                        130, 140, 150);
        break;
    case SKY_DARK:
        drawGradientSky(draw,
                        10, 15, 50,
                        40, 50, 120);
        break;
    default:
        break;
    }
}

void Sky::setSkyType(SkyType newType)
{
    this->type = newType;
}

void Sky::tick()
{
    time++;
}