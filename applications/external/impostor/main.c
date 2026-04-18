#include "include/app/impostor_app.h"
#include <furi.h>

int32_t impostor_game_app(void *p) {
  UNUSED(p);
  return impostor_app_run();
}
