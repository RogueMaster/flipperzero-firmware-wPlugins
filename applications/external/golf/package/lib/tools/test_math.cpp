#define _USE_MATH_DEFINES 1

#include <stdio.h>
#include <cmath>

#include "../game.hpp"

uint8_t poll_btns() { return 0; }
void save_audio_on_off() {}
void toggle_audio() {}
bool audio_enabled() { return false; }

int main()
{
    // test div
    {
        float worst_fdiff = 0;
        uint16_t worst_x = 0;
        for(int i = 256; i < 65536; ++i)
        {
            uint16_t x = uint16_t(i);
            uint16_t y = inv16(x);

            int act_y = (1 << 24) / x;
            uint32_t diff = std::abs(y - act_y);
            float fdiff = float(diff) / x;
            if(fdiff > worst_fdiff)
                worst_fdiff = fdiff, worst_x = x;
        }

        uint16_t x = worst_x;
        uint16_t y = inv16(x);
        int act_y = (1 << 24) / x;
        uint32_t diff = std::abs(y - act_y);
        float fdiff = float(diff) / x;
        printf("x     = %d\n", (int)x);
        printf("y     = %d\n", (int)y);
        printf("act y = %d\n", act_y);
        printf("diff  = %d\n", (int)diff);
        printf("fdiff = %f\n", fdiff);
        printf("\n\n");
    }

    // test fsin16
    {
        int worst_diff = 0;
        uint16_t worst_x = 0;
        for(int i = 0; i < 65536; ++i)
        {
            uint16_t x = uint16_t(i);
            int16_t y = fsin16(x);
            int16_t act_y = int16_t(round(sin(M_PI * x / 32768) * 32767));
            int diff = std::abs(y - act_y);
            if(diff > worst_diff)
                worst_diff = diff, worst_x = x;
        }

        uint16_t x = worst_x;
        int16_t y = fsin16(x);
        int16_t act_y = int16_t(round(sin(M_PI * x / 32768) * 32767));
        int diff = std::abs(y - act_y);
        printf("x     = %d\n", (int)x);
        printf("y     = %d\n", (int)y);
        printf("act y = %d\n", act_y);
        printf("diff  = %d\n", (int)diff);
        printf("\n\n");
    }

    // test atan2
    {
        int worst_diff = 0;
        int16_t worst_x = 0;
        int16_t worst_y = 0;
        for(int i = 0; i < 1024; ++i)
        {
            for(int j = 0; j < 1024; ++j)
            {
                int16_t x = (i - 512) * 63;
                int16_t y = (j - 512) * 63 + 31;
                uint16_t z = atan2(y, x);
                uint16_t act_z = uint16_t(floor(atan2(double(y), double(x)) / (M_PI * 2) * 65535.9));
                int diff = std::abs(z - act_z);
                if(diff > worst_diff)
                    worst_diff = diff, worst_x = x, worst_y = y;
            }
        }

        int16_t x = worst_x;
        int16_t y = worst_y;
        uint16_t act_z = uint16_t(floor(atan2(double(y), double(x)) / (M_PI * 2) * 65535.9));
        uint16_t z = atan2(y, x);
        int diff = std::abs(z - act_z);
        printf("x     = %d\n", (int)x);
        printf("y     = %d\n", (int)y);
        printf("z     = %d\n", (int)z);
        printf("act z = %d\n", act_z);
        printf("diff  = %d\n", (int)diff);
        printf("\n\n");
    }

    return 0;
}
