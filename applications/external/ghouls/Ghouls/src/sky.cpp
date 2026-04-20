#include "sky.hpp"

Sky::Sky(SkyType type) : type(type)
{
}

Sky::~Sky()
{
}

void Sky::render(Draw *draw)
{
    (void)draw; // for now
    switch (this->type)
    {
    case SKY_SUNNY:
        break;
    case SKY_CLOUDY:
        break;
    case SKY_DARK:
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