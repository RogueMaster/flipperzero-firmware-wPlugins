#include "ground.hpp"

Ground::Ground() : gradient{}, time(0)
{
    memset(&gradient, 0, sizeof(gradient_color_t));
}

Ground::~Ground()
{
    // nothing to do...
}

void Ground::drawGradientGround(Draw *draw,
                                uint8_t horizR, uint8_t horizG, uint8_t horizB,
                                uint8_t botR, uint8_t botG, uint8_t botB)
{
    const uint16_t groundHeight = (uint16_t)draw->getDisplaySize().y - GROUND_HORIZON_HEIGHT;

    const int drdy = ((botR - horizR) * FIXED_POINT_SCALE) / groundHeight;
    const int dgdy = ((botG - horizG) * FIXED_POINT_SCALE) / groundHeight;
    const int dbdy = ((botB - horizB) * FIXED_POINT_SCALE) / groundHeight;

    int r = horizR * FIXED_POINT_SCALE;
    int g = horizG * FIXED_POINT_SCALE;
    int b = horizB * FIXED_POINT_SCALE;

    for (int y = 0; y < groundHeight; y += GROUND_ROWS)
    {
        uint16_t color = makeRGB565(r >> 8, g >> 8, b >> 8);

        int height = (y + GROUND_ROWS < groundHeight) ? GROUND_ROWS : groundHeight - y;
        draw->fillRectangle(0, GROUND_HORIZON_HEIGHT + y, ENGINE_LCD_WIDTH, height, color);

        r += drdy * GROUND_ROWS;
        g += dgdy * GROUND_ROWS;
        b += dbdy * GROUND_ROWS;
    }
}

uint16_t Ground::makeRGB565(uint8_t r, uint8_t g, uint8_t b)
{
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
}

void Ground::render(Draw *draw)
{
    drawGradientGround(draw,
                       gradient.horizR, gradient.horizG, gradient.horizB,
                       gradient.layerR, gradient.layerG, gradient.layerB);
}

void Ground::setGround(gradient_color_t groundGradient)
{
    this->gradient = groundGradient;
}

void Ground::setGroundType(GroundType groundType)
{
    switch (groundType)
    {
    case GROUND_GRASS:
        this->gradient = {
            .horizR = 80,
            .horizG = 110,
            .horizB = 50,
            .layerR = 30,
            .layerG = 55,
            .layerB = 15,
        };
        break;
    case GROUND_DIRT:
        this->gradient = {
            .horizR = 200,
            .horizG = 140,
            .horizB = 70,
            .layerR = 140,
            .layerG = 90,
            .layerB = 40,
        };
        break;
    case GROUND_DARK:
        this->gradient = {
            .horizR = 60,
            .horizG = 45,
            .horizB = 25,
            .layerR = 22,
            .layerG = 16,
            .layerB = 8,
        };
        break;
    default:
        break;
    };
}

void Ground::tick()
{
    time++;
}