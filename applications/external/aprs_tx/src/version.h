#pragma once

#define APP_VER_MAJOR 1
#define APP_VER_MINOR 3
#define APP_VER_REV 17

#define APP_VER_S2(x) #x
#define APP_VER_S(x) APP_VER_S2(x)
#define APP_VER_TEXT APP_VER_S(APP_VER_MAJOR) "." APP_VER_S(APP_VER_MINOR) "." APP_VER_S(APP_VER_REV)

#ifndef APP_BUILD_COMMIT
#define APP_BUILD_COMMIT "unknown"
#endif

#ifndef APP_BUILD_HOST
#define APP_BUILD_HOST "unknown"
#endif

#ifndef APP_BUILD_TIME
#define APP_BUILD_TIME "unknown"
#endif
