#ifndef yo3gnd_scenes_a12c
#define yo3gnd_scenes_a12c

#include "fmtx_app.h"

typedef enum
{
    ScMain,
    ScPlay,
    ScCount,
} Scn;

typedef enum
{
    VMain,
    VPlay,
} ViewId;

typedef enum
{
    MStart,
    MFile,
    MSet,
} MenuId;

typedef struct
{
    uint32_t elapsed_ms;
} PlayModel;

extern const SceneManagerHandlers scenes;

void playdraw(Canvas *canvas, void *model);

#endif
