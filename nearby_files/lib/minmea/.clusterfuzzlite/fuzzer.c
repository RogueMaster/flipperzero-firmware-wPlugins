#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "minmea_compat_ti-rtos.h"
#include "minmea_compat_windows.h"
#include "minmea.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 7) {
        return 0;
    }

    // Prepare the input data for minmea_parse_rmc
    char input[size + 1];
    memcpy(input, data, size);
    input[size] = '\0';

    // Prepare the frame
    struct minmea_sentence_rmc frame;
    const char *sentence = (const char *)input;

    // Call the target function
    bool result = minmea_parse_rmc(&frame, sentence);
    (void)result;

    return 0;
}
  
