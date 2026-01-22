#include <furi.h>
#include <gui/gui.h>

typedef struct {
  FuriMessageQueue* input_queue;
  ViewPort* view_port;
  Gui* gui;
} Sonicare;
