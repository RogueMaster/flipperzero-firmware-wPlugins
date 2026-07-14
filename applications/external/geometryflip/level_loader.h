#pragma once
#include "constants.h"
#include "obstacles.h"

// Function prototypes for level loading
bool load_level_from_file(GeometryDashApp* app, const char* filename);
void free_level_files(LevelFileArray_t* level_files);
void scan_level_files(GeometryDashApp* app);
void reset_game(GeometryDashApp* app);
