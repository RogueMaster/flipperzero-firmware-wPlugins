#ifndef __LOG_USER_H_
#define __LOG_USER_H_

#include <furi.h>
#include <furi/core/log.h>
#include <furi_hal.h>

#define LOG_TAG  "INFO_WRITTEN"
#define LOG_SHOW true

#define log_info(format, ...) \
    if(LOG_SHOW) FURI_LOG_I(LOG_TAG, format, ##__VA_ARGS__)

#define log_exception(format, ...) \
    if(LOG_SHOW) FURI_LOG_E(LOG_TAG, format, ##__VA_ARGS__)

#define log_warning(format, ...) \
    if(LOG_SHOW) FURI_LOG_W(LOG_TAG, format, ##__VA_ARGS__)

#endif
