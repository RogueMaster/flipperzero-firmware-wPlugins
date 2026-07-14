#define _USE_MATH_DEFINES

#include <math.h>
#include <stdio.h>

int main()
{
    // sin table
    for(int i = 0; i < 65; ++i)
    {
        double x = M_PI / 128 * i;
        printf("%3d,%c", (int)roundf(sin(x) * 127), i % 8 < 7 ? ' ' : '\n');
    }

    printf("\n\n");

    // sin16 table
    for(int i = 0; i < 65; ++i)
    {
        double x = M_PI / 128 * i;
        printf("%5d,%c", (int)roundf(sin(x) * 32767), i % 8 < 7 ? ' ' : '\n');
    }

    printf("\n\n");

    // inv table
    for(int i = 0; i < 256; ++i)
    {
        int d = i == 0 ? 65535 : int(1.0 / i * 65535.5);
        printf("%5d,%c", d, i % 8 < 7 ? ' ' : '\n');
    }

    printf("\n\n");

    // atan table
    for (int i = 0; i < 33; ++i)
    {
        int d = (int)round(atan(double(i) / 32) * 8192 / (M_PI / 4));
        printf("%4d,%c", d, i % 8 < 7 ? ' ' : '\n');
    }

    return 0;
}
