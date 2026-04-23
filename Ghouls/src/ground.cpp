#include "ground.hpp"

Ground::Ground(GroundType type) : time(0), type(type)
{
}

Ground::~Ground()
{
}

void Ground::drawGradientGround(Draw *draw,
                                uint8_t horizR, uint8_t horizG, uint8_t horizB,
                                uint8_t botR, uint8_t botG, uint8_t botB)
{
    Vector screen = draw->getDisplaySize();
    uint16_t groundHeight = (uint16_t)screen.y - GROUND_HORIZON_HEIGHT;

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
    switch (this->type)
    {
    case GROUND_GRASS:
        drawGradientGround(draw,
                           80, 110, 50,
                           30, 55, 15);
        break;
    case GROUND_DIRT:
        drawGradientGround(draw,
                           200, 140, 70,
                           140, 90, 40);
        break;
    case GROUND_DARK:
        drawGradientGround(draw,
                           60, 45, 25,
                           22, 16, 8);
        break;
    default:
        break;
    }
}

void Ground::setGroundType(GroundType newType)
{
    this->type = newType;
}

void Ground::tick()
{
    time++;
}