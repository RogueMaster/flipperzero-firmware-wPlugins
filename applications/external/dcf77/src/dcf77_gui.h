#ifndef DCF77_GUI_H
#define DCF77_GUI_H

#include "dcf77_app.h"

typedef struct {
    AppFSM* app_fsm;
} Dcf77AppViewModel;

void dcf77_gui_init(AppFSM* app_fsm);
void dcf77_gui_deinit(AppFSM* app_fsm);

#endif
