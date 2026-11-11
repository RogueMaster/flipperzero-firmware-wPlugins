#pragma once

#define NAMESPOOF_HEADER  "Flipper Name File"
// Keep at 1: namespoof_init() hard-rejects a mismatching version, so bumping
// would silently drop every existing user's name spoof. The Mac/Serial keys are
// optional additions a v1 reader simply ignores if absent (forward/backward ok).
#define NAMESPOOF_VERSION 1
#define NAMESPOOF_PATH    EXT_PATH("dolphin/name.settings")

#define NAMESPOOF_KEY_NAME   "Name"
#define NAMESPOOF_KEY_MAC    "Mac"
#define NAMESPOOF_KEY_SERIAL "Serial"
#define NAMESPOOF_MAC_LEN    6
#define NAMESPOOF_SERIAL_LEN 8

void namespoof_init(void);
