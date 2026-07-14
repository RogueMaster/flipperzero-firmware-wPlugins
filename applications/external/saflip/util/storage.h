#pragma once

#include "../saflip.h"

#define SAFLIP_APP_FOLDER           ANY_PATH("saflip")
#define SAFLIP_APP_EXTENSION        ".saflip"
#define SAFLIP_APP_FILE_PREFIX      "Saflip"
#define SAFLIP_APP_SHADOW_EXTENSION ".saf"

bool saflip_load_file(SaflipApp* app, const char* path);
bool saflip_save_file(SaflipApp* app, const char* path);
