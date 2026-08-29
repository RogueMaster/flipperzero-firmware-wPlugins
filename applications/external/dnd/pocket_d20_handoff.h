#pragma once

/* Shared cross-FAP launch contract. Bestiary launches Dungeons & Dolphins by
   full FAP path. Optional monster records follow the initiative token as:
   initiative;Name,HP,AC;Name,HP,AC;... */
#define POCKET_D20_HANDOFF_LAUNCH_ARG    "initiative"
#define POCKET_D20_HANDOFF_LAUNCH_PREFIX "initiative;"
#define POCKET_D20_APP_FAP_PATH          "/ext/apps/Games/dungeons_and_dolphins.fap"
#define POCKET_D20_LAUNCH_ARGS_MAX       1536U
#define POCKET_D20_TRANSFER_MAX          23U
