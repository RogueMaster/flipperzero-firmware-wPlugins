#include "fmtx_audio.h"

int16_t u8pcm(uint8_t s)
{
    return ((int16_t)s - 128) * 256;
}
