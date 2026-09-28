/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <tiny_crypto/tiny_crypto.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static volatile uint8_t sink;

/* Fixed public inputs keep repeated measurements comparable. */
static int measure(void)
{
    /* KMAC256 with a zero key and 64 zero input bytes, checked with OpenSSL. */
    static const uint8_t expected[48] = {
        0x48,0x73,0x94,0xc2,0xd0,0x21,0xa6,0xb4,0x9d,0x22,0x1f,0x0e,
        0x9c,0xb6,0x9d,0x42,0xcf,0x52,0xbe,0xbd,0xef,0x7f,0x20,0x20,
        0x18,0xbb,0x38,0xb4,0x53,0xb9,0x73,0x65,0x4e,0xdf,0x41,0x14,
        0xba,0x6d,0x24,0x42,0xd2,0x16,0x19,0x86,0x2d,0xa1,0x87,0x73
    };
    uint8_t key[32] = {0}, data[64] = {0}, digest[48];
    int64_t start = esp_timer_get_time();
    for (unsigned i = 0; i < 100; ++i) {
        if (TC_KMAC256_digest(key, sizeof key, data, sizeof data, NULL, 0,
                              digest, sizeof digest) != TC_OK) return 1;
        sink ^= digest[0];
    }
    int64_t elapsed = esp_timer_get_time() - start;
    if (memcmp(digest, expected, sizeof expected) != 0) return 1;
    printf("KMAC256 64-byte input, 48-byte output: %" PRId64 " us / 100 operations\n",
           elapsed);
#if TC_ENABLE_EC
    static TC_EC_workspace workspace;
    uint8_t scalar[48] = {0}, point[97];
    for (unsigned width = 32; width <= 48; width += 16) {
        if ((width == 32 && !TC_EC_ENABLE_P256) ||
            (width == 48 && !TC_EC_ENABLE_P384)) continue;
        memset(scalar, 0, sizeof scalar);
        scalar[width - 1] = 1;
        start = esp_timer_get_time();
        if (TC_EC_public_key(width == 32 ? TC_EC_P256 : TC_EC_P384,
                            scalar, width, point, 1 + 2 * width, &workspace) != TC_OK) return 1;
        printf("P-%u public key: %" PRId64 " us\n", width * 8, esp_timer_get_time() - start);
        sink ^= point[1];
        vTaskDelay(1);
    }
#endif
    TC_secure_zero(key, sizeof key);
    return 0;
}

void app_main(void)
{
    printf("tiny_crypto_c %s, resource profile %d\n", TC_APPLICATION_TARGET, TC_RESOURCE_PROFILE);
    if (measure()) {
        puts("Benchmark failed");
        abort();
    }
    printf("Internal heap free: %zu bytes; minimum: %zu bytes; task stack unused: %u bytes\n",
           heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
           heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
           (unsigned)uxTaskGetStackHighWaterMark(NULL));
}
