#ifndef SCENE_DCF77_HEADERS
#define SCENE_DCF77_HEADERS

#include "flipper.h"

#include "logic_dcf77.h"

typedef enum {
    LCWDCF77EventReceive0,
    LCWDCF77EventReceive1,
    LCWDCF77EventReceiveSync,
    LCWDCF77EventReceiveDesync,
    LCWDCF77EventReceiveInterrupt,
    LCWDCF77EventReceiveUnknown
} LWCDCF77Event;

View* lwc_dcf77_scene_alloc();
void lwc_dcf77_scene_free(View* view);

void lwc_dcf77_scene_on_enter(void* context);
bool lwc_dcf77_scene_on_event(void* context, SceneManagerEvent event);
void lwc_dcf77_scene_on_exit(void* context);

#endif
