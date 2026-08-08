#ifndef yo3gnd_vfo_43d1
#define yo3gnd_vfo_43d1

#include <stdbool.h>
#include <stdint.h>
#include <gui/canvas.h>
#include <input/input.h>

typedef struct FmtxVfo FmtxVfo;

FmtxVfo* fmtx_vfo_alloc(void);
void fmtx_vfo_free(FmtxVfo* vfo);
void fmtx_vfo_begin(FmtxVfo* vfo, uint32_t frequency_hz);
bool fmtx_vfo_input(FmtxVfo* vfo, const InputEvent* event, bool* accepted);
void fmtx_vfo_draw(const FmtxVfo* vfo, Canvas* canvas);
uint32_t fmtx_vfo_accept(FmtxVfo* vfo);
uint32_t fmtx_vfo_frequency(const FmtxVfo* vfo);
bool fmtx_vfo_frequency_valid(uint32_t frequency_hz);
uint32_t fmtx_vfo_default_frequency(void);
bool mf_radio_frequency_in_vfo(uint32_t frequency_hz);

#endif
