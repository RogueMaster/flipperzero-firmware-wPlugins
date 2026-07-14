#ifndef SCENE_MSF_HEADERS
#define SCENE_MSF_HEADERS

#include "flipper.h"
#include "logic_msf.h"

typedef enum {
    LCWMSFEventReceiveA0,
    LCWMSFEventReceiveA1,
    LCWMSFEventReceiveB0,
    LCWMSFEventReceiveB1,
    LCWMSFEventReceiveSync,
    LCWMSFEventReceiveDesync,
    LCWMSFEventReceiveInterrupt,
    LCWMSFEventReceiveUnknown
} LWCMSFEvent;

View* lwc_msf_scene_alloc();
void lwc_msf_scene_free(View* view);

void lwc_msf_scene_on_enter(void* context);
bool lwc_msf_scene_on_event(void* context, SceneManagerEvent event);
void lwc_msf_scene_on_exit(void* context);

#endif
