#include "sky.hpp"

Sky::Sky() : gradient{}, time(0)
{
    memset(&gradient, 0, sizeof(gradient_color_t));
}

Sky::~Sky()
{
    // nothing to do...
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
    drawGradientSky(draw,
                    gradient.layerR, gradient.layerG, gradient.layerB,
                    gradient.horizR, gradient.horizG, gradient.horizB);
}

void Sky::setSky(gradient_color_t skyGradient)
{
    this->gradient = skyGradient;
}

void Sky::setSkyType(SkyType skyType)
{
    switch (skyType)
    {
    case SKY_SUNNY:
        this->gradient = {
            .horizR = 180,
            .horizG = 220,
            .horizB = 255,
            .layerR = 100,
            .layerG = 160,
            .layerB = 255,
        };
        break;
    case SKY_CLOUDY:
        this->gradient = {
            .horizR = 130,
            .horizG = 140,
            .horizB = 150,
            .layerR = 60,
            .layerG = 70,
            .layerB = 90,
        };
        break;
    case SKY_DARK:
        this->gradient = {
            .horizR = 40,
            .horizG = 50,
            .horizB = 120,
            .layerR = 10,
            .layerG = 15,
            .layerB = 50,
        };
        break;
    default:
        break;
    };
}

void Sky::tick()
{
    time++;
}