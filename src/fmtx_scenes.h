#ifndef yo3gnd_scenes_a12c
#define yo3gnd_scenes_a12c

#include "fmtx_app.h"

typedef enum {
    ScBoot,
    ScMain,
    ScPlay,
    FmtxSceneSettings,
    FmtxSceneVfo,
    ScAbout,
    ScCount,
} Scn;

typedef enum {
    VMain,
    VPlay,
    FmtxViewSettings,
    FmtxViewVfo,
    VAbout,
} ViewId;

typedef enum {
    MStart,
    MFile,
    MSet,
    MAbout,
} MenuId;

typedef enum {
    FmtxSettingsSetFrequency,
} FmtxSettingsItem;

typedef enum {
    FmtxVfoDone,
} FmtxVfoEvent;

typedef enum {
    FmtxAboutNext,
    FmtxAboutBack,
} FmtxAboutEvent;

typedef struct {
    uint32_t elapsed_ms;
    uint32_t pause_ms;
    uint32_t frequency_hz;
    uint32_t scroll;
    uint8_t gain;
    bool filter;
    bool tx;
    bool paused;
    char filename[256];
} PlayModel;

typedef struct {
    FmtxVfo* vfo;
} FmtxVfoViewModel;

extern const SceneManagerHandlers scenes;
extern const char abttext[];

void playdraw(Canvas* canvas, void* model);
bool playinput(InputEvent* ev, void* ctx);
void vfodraw(Canvas* canvas, void* model);
bool vfoinput(InputEvent* ev, void* ctx);
void abtback(GuiButtonType b, InputType t, void* ctx);

#endif
