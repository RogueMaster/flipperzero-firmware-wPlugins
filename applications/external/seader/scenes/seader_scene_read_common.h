#pragma once

#include <stdint.h>

typedef struct Seader Seader;

void seader_sam_check_worker_callback(uint32_t event, void* context);
void seader_scene_read_prepare(Seader* seader);
void seader_scene_read_cleanup(Seader* seader);
