/**
*  ▌  ▞▚ ▛▚ ▌  ▞▚ ▟  Copyright© 2025 LAB401 GPLv3
*  ▌  ▛▜ ▛▚ ▙▙ ▌▐ ▐  This program is free software
*  ▀▀ ▘▝ ▀▘  ▘ ▝▘ ▀▘ See LICENSE.txt - lab401.com
*    + Tixlegeek
*/
#ifndef L401_ERR_H
#define L401_ERR_H

typedef enum {
    L401_OK,
    L401_ERR_STORAGE,
    L401_ERR_PARSE,
    L401_ERR_MALFORMED,
    L401_ERR_INTERNAL,
    L401_ERR_FILESYSTEM,
    L401_ERR_FILE_DOESNT_EXISTS,
    L401_ERR_BMP_FILE,
    L401_ERR_NULLPTR,
    L401_ERR_HARDWARE,
    L401_ERR_WIDTH,
} l401_err;

#endif /* end of include guard: L401_ERR_H */
