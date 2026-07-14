#pragma once

#ifndef AUTOCONNECT_TRY_NUM
#  define AUTOCONNECT_TRY_NUM 1000
#endif

// Activate I2S Support (legacy i2s)
#ifndef A2DP_LEGACY_I2S_SUPPORT
#  define A2DP_LEGACY_I2S_SUPPORT (ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 0, 0))
#endif

// Use https://pschatzmann.github.io/arduino-audio-tools for output
#ifndef A2DP_I2S_AUDIOTOOLS
#  if __has_include("AudioTools.h")
#    define A2DP_I2S_AUDIOTOOLS 1
#  endif
#endif

// Activate SPP Support
#ifndef A2DP_SPP_SUPPORT
#  define A2DP_SPP_SUPPORT 1
#endif

// Maximum write size
#ifndef A2DP_I2S_MAX_WRITE_SIZE 
#  define A2DP_I2S_MAX_WRITE_SIZE 1024 * 5
#endif

#ifndef A2DP_I2S_MAX_WRITE_DELAY_MS 
#  define A2DP_I2S_MAX_WRITE_DELAY_MS 0
#endif

// Maximum wait time for status change in 100 ms when calling end()
#ifndef A2DP_DISCONNECT_LIMIT 
#  define A2DP_DISCONNECT_LIMIT 20
#endif
