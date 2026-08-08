#ifndef yo3gnd_playback_90aa
#define yo3gnd_playback_90aa

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    char path[256];
    uint32_t hz;
} PlayReq;

typedef struct Play Play;

void fmtx_playback_request_init(PlayReq* request, const char* path, uint32_t hz);
Play* fmtx_playback_alloc(void);
void fmtx_playback_free(Play* playback);
bool fmtx_playback_start(Play* playback, const PlayReq* request);
bool fmtx_playback_start_paused(Play* playback, const PlayReq* request);
void fmtx_playback_stop(Play* playback);
bool fmtx_playback_is_running(const Play* playback);
bool fmtx_playback_is_transmitting(const Play* playback);
bool fmtx_playback_is_paused(const Play* playback);
bool fmtx_playback_toggle_pause(Play* playback);
bool fmtx_playback_seek_frames(Play* playback, int32_t frames);
uint32_t fmtx_playback_position_ms(const Play* playback);
uint32_t fmtx_playback_duration_ms(const Play* playback);
uint8_t fmtx_playback_gain(const Play* playback);
uint8_t fmtx_playback_cycle_gain(Play* playback);
bool fmtx_playback_filter_enabled(const Play* playback);
bool fmtx_playback_toggle_filter(Play* playback);

#endif
