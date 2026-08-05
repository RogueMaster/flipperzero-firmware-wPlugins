#ifndef yo3gnd_scenes_a12c
#define yo3gnd_scenes_a12c

#include "fmtx_app.h"

typedef enum
{
    ScMain,
    ScPlay,
    FmtxSceneSettings,
    FmtxSceneVfo,
    ScCount,
} Scn;

typedef enum
{
    VMain,
    VPlay,
    FmtxViewSettings,
    FmtxViewVfo,
} ViewId;

typedef enum
{
    MStart,
    MFile,
    MSet,
} MenuId;

typedef enum
{
    FmtxSettingsSetFrequency,
} FmtxSettingsItem;

typedef enum
{
    FmtxVfoDone,
} FmtxVfoEvent;

typedef struct
{
    uint32_t elapsed_ms;
    uint8_t gain;
    bool filter;
    char filename[256];
} PlayModel;

typedef struct
{
    FmtxVfo *vfo;
} FmtxVfoViewModel;

extern const SceneManagerHandlers scenes;

void playdraw(Canvas *canvas, void *model);
bool playinput(InputEvent *ev, void *ctx);
void vfodraw(Canvas *canvas, void *model);
bool vfoinput(InputEvent *ev, void *ctx);

#endif
