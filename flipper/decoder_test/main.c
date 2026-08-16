/* Runs the firmware decoders on an ordinary computer. No Flipper needed.
 *
 * Two kinds of check:
 *
 * 1. Every protocol gets one synthetic frame from vectors.h, whose
 *    expected values come from rtl_433 itself (see gen_vectors.py). That
 *    is what proves a port reads the same fields as its original.
 * 2. The Renault decoder, the one protocol verified against a real
 *    sensor, is put through the cases real hardware produced: mixed
 *    polarity, timing jitter, a corrupted frame.
 *
 * Build and run: ./run.sh
 */
#include "tpms_decoder.h"
#include "vectors.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_FRAMES 8

static TpmsFrame frames[MAX_FRAMES];
static int frame_count;

static void on_frame(const TpmsFrame* frame, void* context) {
    (void)context;
    if(frame_count < MAX_FRAMES) frames[frame_count] = *frame;
    frame_count++;
}

/** Turn a chip string into intervals and feed them to the decoder. */
static void feed_chips(TpmsDecoder* decoder, const char* chips, int chip_us, int invert) {
    size_t i = 0;
    while(chips[i]) {
        const char level = chips[i];
        int run = 0;
        while(chips[i] == level) {
            run++;
            i++;
        }
        int high = (level == '1');
        if(invert) high = !high;
        tpms_decoder_feed(decoder, high, (uint32_t)(run * chip_us));
    }
    /* A gap flushes whatever the last capture was waiting for. */
    tpms_decoder_feed(decoder, false, 20000);
}

static int protocol_index(const char* id) {
    for(uint8_t i = 0; i < tpms_protocol_count; i++) {
        if(strcmp(tpms_protocols[i].id, id) == 0) return i;
    }
    return -1;
}

static int check_vector(const TpmsTestVector* vector, int invert) {
    const int index = protocol_index(vector->protocol);
    if(index < 0) {
        printf("FAIL %-14s no such protocol in the table\n", vector->protocol);
        return 1;
    }

    frame_count = 0;
    TpmsDecoder* decoder = tpms_decoder_alloc(on_frame, NULL);
    /* The radio can only be set up for one modulation at a time, so the
     * decoder looks for one set of protocols at a time too. */
    tpms_decoder_set_modulation(decoder, tpms_protocols[index].modulation);
    feed_chips(decoder, vector->chips, vector->chip_us, invert);
    tpms_decoder_free(decoder);

    const TpmsFrame* found = NULL;
    for(int i = 0; i < frame_count && i < MAX_FRAMES; i++) {
        if(frames[i].protocol == index) found = &frames[i];
    }

    if(!found) {
        printf("FAIL %-14s polarity=%d: not decoded;", vector->protocol, invert);
        for(int i = 0; i < frame_count && i < MAX_FRAMES; i++) {
            printf(" %s", tpms_protocol_label(frames[i].protocol));
        }
        printf(" %d other frames\n", frame_count);
        return 1;
    }

    int failures = 0;
    if(found->id != vector->id) {
        printf("FAIL %-14s id %08lx, expected %08lx\n",
               vector->protocol, (unsigned long)found->id, (unsigned long)vector->id);
        failures++;
    }

    /* Half a kPa of slack: rtl_433 works in floating point and the
     * firmware in hundredths of a kPa. A misread field is out by far
     * more than that. */
    const int32_t delta = found->pressure_kpa_x100 - vector->pressure_kpa_x100;
    if(delta > 50 || delta < -50) {
        printf("FAIL %-14s pressure %ld, expected %ld (hundredths of a kPa)\n",
               vector->protocol,
               (long)found->pressure_kpa_x100,
               (long)vector->pressure_kpa_x100);
        failures++;
    }

    if(vector->has_temperature && found->temperature_c != vector->temperature_c) {
        printf("FAIL %-14s temperature %d, expected %d\n",
               vector->protocol, found->temperature_c, vector->temperature_c);
        failures++;
    }

    if(!failures && !invert) {
        printf("ok   %-14s id=%0*lx  %ld.%02ld kPa  %d C\n",
               vector->protocol,
               tpms_protocols[index].id_digits,
               (unsigned long)found->id,
               (long)(found->pressure_kpa_x100 / 100),
               (long)(found->pressure_kpa_x100 % 100),
               found->temperature_c);
    }
    return failures;
}

/* --- the Renault sensor, the one checked against real hardware --------- */

static const char* RENAULT_VECTOR =
    "01010101010101010110"
    "010110010110"
    "10011001101010011001"
    "1010010110011010"
    "1001010101101001"
    "0101100110010101"
    "1001010101100110"
    "0101010101010101"
    "0101010101010101"
    "0110010101010101";

static const uint8_t RENAULT_RAW[9] = {0xd9, 0x45, 0x34, 0x79, 0xd7, 0x7a, 0xff, 0xff, 0xbf};

static int renault_decoded(void) {
    const int index = protocol_index("renault");
    for(int i = 0; i < frame_count && i < MAX_FRAMES; i++) {
        if(frames[i].protocol == index && memcmp(frames[i].raw, RENAULT_RAW, 9) == 0) return 1;
    }
    return 0;
}

static int renault_hardware_cases(void) {
    int failures = 0;

    for(int invert = 0; invert <= 1; invert++) {
        frame_count = 0;
        TpmsDecoder* decoder = tpms_decoder_alloc(on_frame, NULL);
        feed_chips(decoder, RENAULT_VECTOR, 52, invert);
        tpms_decoder_free(decoder);

        if(!renault_decoded()) {
            printf("FAIL renault polarity=%d\n", invert);
            failures++;
        }
    }
    if(!failures) printf("ok   renault        both stream polarities\n");

    /* A real 407003VU0B sends the sync word in normal polarity while the
     * Manchester pairs come inverted. */
    {
        char mixed[512];
        strcpy(mixed, RENAULT_VECTOR);
        for(size_t i = 20; i < strlen(mixed); i++) mixed[i] = mixed[i] == '0' ? '1' : '0';

        frame_count = 0;
        TpmsDecoder* decoder = tpms_decoder_alloc(on_frame, NULL);
        feed_chips(decoder, mixed, 52, 0);
        tpms_decoder_free(decoder);

        if(!renault_decoded()) {
            printf("FAIL renault normal sync with inverted data\n");
            failures++;
        } else {
            printf("ok   renault        normal sync with inverted data\n");
        }
    }

    /* Duration jitter of +-15%. */
    srand(42);
    int jitter_failures = 0;
    for(int trial = 0; trial < 20; trial++) {
        frame_count = 0;
        TpmsDecoder* decoder = tpms_decoder_alloc(on_frame, NULL);

        size_t i = 0;
        while(RENAULT_VECTOR[i]) {
            const char level = RENAULT_VECTOR[i];
            int run = 0;
            while(RENAULT_VECTOR[i] == level) {
                run++;
                i++;
            }
            const int nominal = run * 52;
            tpms_decoder_feed(
                decoder, level == '1', (uint32_t)(nominal * (85 + rand() % 31) / 100));
        }
        tpms_decoder_feed(decoder, false, 20000);
        tpms_decoder_free(decoder);

        if(!renault_decoded()) jitter_failures++;
    }
    if(jitter_failures) {
        printf("FAIL renault jitter: %d of 20 trials lost\n", jitter_failures);
        failures += jitter_failures;
    } else {
        printf("ok   renault        +-15%% jitter, 20 of 20 trials\n");
    }

    /* A corrupted frame must not get through. */
    {
        char broken[512];
        strcpy(broken, RENAULT_VECTOR);
        broken[40] = broken[40] == '0' ? '1' : '0';

        frame_count = 0;
        TpmsDecoder* decoder = tpms_decoder_alloc(on_frame, NULL);
        feed_chips(decoder, broken, 52, 0);
        tpms_decoder_free(decoder);

        if(renault_decoded()) {
            printf("FAIL renault corrupted frame accepted\n");
            failures++;
        } else {
            printf("ok   renault        corrupted frame rejected\n");
        }
    }

    return failures;
}

/** Noise must not turn into readings. */
static int noise_case(void) {
    frame_count = 0;
    TpmsDecoder* decoder = tpms_decoder_alloc(on_frame, NULL);

    srand(7);
    for(int i = 0; i < 400000; i++) {
        tpms_decoder_feed(decoder, rand() & 1, (uint32_t)(20 + rand() % 140));
    }
    tpms_decoder_free(decoder);

    printf(
        frame_count ? "WARN 400k noise intervals produced %d frames\n" :
                      "ok   noise          400k random intervals, no frames\n",
        frame_count);
    return 0;
}

int main(void) {
    int failures = 0;

    printf("protocols in the table: %u\n\n", (unsigned)tpms_protocol_count);

    for(size_t i = 0; i < TPMS_TEST_VECTOR_COUNT; i++) {
        failures += check_vector(&tpms_test_vectors[i], 0);
        failures += check_vector(&tpms_test_vectors[i], 1);
    }

    printf("\n");
    failures += renault_hardware_cases();
    printf("\n");
    failures += noise_case();

    if(failures) {
        printf("\nFAILURES: %d\n", failures);
        return 1;
    }
    printf("\nall good\n");
    return 0;
}
