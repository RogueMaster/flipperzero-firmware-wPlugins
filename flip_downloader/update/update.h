#pragma once
#include "flipper_http/flipper_http.h"
#include "easy_flipper/easy_flipper.h"
#define BUILD_ID "686b725db51b89169a226aa1" // 67f379d625a4a6f1fb4a57f7

#define FAP_ID "flip_downloader"
#define APP_FOLDER "GPIO"
#define MOM_FOLDER "FlipperHTTP"

#ifndef APP_ID
#define APP_ID "flip_downloader"
#endif

#ifndef TAG
#define TAG "FlipDownloader"
#endif

#ifdef __cplusplus
extern "C"
{
#endif

    bool update_is_ready(FlipperHTTP *fhttp, bool use_flipper_api);

#ifdef __cplusplus
}
#endif
