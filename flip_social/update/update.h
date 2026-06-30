#pragma once
#include "flipper_http/flipper_http.h"
#include "easy_flipper/easy_flipper.h"
#define BUILD_ID "6714d6326cb04a64f7ec2328"
#define FAP_ID "flip_social"
#define APP_FOLDER "GPIO"
#define MOM_FOLDER "FlipperHTTP"

#ifndef APP_ID
#define APP_ID "flip_social"
#endif

#ifndef TAG
#define TAG "FlipSocial"
#endif

#ifdef __cplusplus
extern "C"
{
#endif

    bool update_is_ready(FlipperHTTP *fhttp, bool use_flipper_api);

#ifdef __cplusplus
}
#endif
