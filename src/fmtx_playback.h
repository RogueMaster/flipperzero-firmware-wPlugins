#ifndef yo3gnd_playback_90aa
#define yo3gnd_playback_90aa

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    char path[256];
    uint32_t hz;
} PlayReq;

typedef struct Play Play;

void playreq(PlayReq *request, const char *path, uint32_t hz);
Play *playnew(void);
void playfree(Play *playback);
bool playstart(Play *playback, const PlayReq *request);
void playstop(Play *playback);
bool playon(const Play *playback);
uint32_t playms(const Play *playback);

#endif
