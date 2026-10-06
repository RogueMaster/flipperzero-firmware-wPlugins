#pragma once
#include <stdbool.h>
#include <stdint.h>

typedef struct MToolsApp MToolsApp;
typedef struct MToolsUidChanger MToolsUidChanger;
MToolsUidChanger* mtools_uid_changer_alloc(MToolsApp* app);
void mtools_uid_changer_free(MToolsUidChanger* instance);
void mtools_uid_changer_enter(MToolsUidChanger* instance);
void mtools_uid_changer_exit(MToolsUidChanger* instance);
bool mtools_uid_changer_event(MToolsUidChanger* instance, uint32_t event);
bool mtools_uid_changer_back(MToolsUidChanger* instance);
