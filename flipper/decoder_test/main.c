/* Прогон декодера из tpms_bridge на обычном компьютере: тот же
 * тестовый вектор, что и в host/tests/test_decoder.py.
 * Сборка и запуск: ./run.sh */
#include "tpms_renault.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int frames_seen = 0;
static uint8_t last_raw[9];

static void on_frame(const uint8_t* raw, void* ctx) {
    (void)ctx;
    memcpy(last_raw, raw, 9);
    frames_seen++;
}

/* Собрать поток чипов кадра и скормить декодеру как интервалы. */
static void feed_chips(TpmsRenaultDecoder* d, const char* chips, int chip_us, int invert) {
    int i = 0;
    while(chips[i]) {
        char level = chips[i];
        int run = 0;
        while(chips[i] == level) { run++; i++; }
        int lvl = (level == '1');
        if(invert) lvl = !lvl;
        tpms_renault_decoder_feed(d, lvl, run * chip_us);
    }
}

static const char* VECTOR =
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

int main(void) {
    const uint8_t expected[9] = {0xd9,0x45,0x34,0x79,0xd7,0x7a,0xff,0xff,0xbf};
    int failures = 0;

    for(int invert = 0; invert <= 1; invert++) {
        frames_seen = 0;
        TpmsRenaultDecoder* d = tpms_renault_decoder_alloc(on_frame, NULL);
        feed_chips(d, VECTOR, TPMS_CHIP_US, invert);
        if(frames_seen != 1 || memcmp(last_raw, expected, 9) != 0) {
            printf("FAIL: polarity=%d frames=%d\n", invert, frames_seen);
            failures++;
        } else {
            TpmsRenaultFrame f;
            tpms_renault_parse(last_raw, &f);
            printf("OK  polarity=%lu id=%06lx pressure=%lu.%02lu kPa temp=%d flags=0x%02x\n",
                (unsigned long)invert, (unsigned long)f.id,
                (unsigned long)(f.pressure_raw*75UL/100), (unsigned long)(f.pressure_raw*75UL%100),
                f.temperature_c, f.flags);
        }
        tpms_renault_decoder_free(d);
    }

    /* Живой датчик 407003VU0B: sync приходит в прямой полярности, а пары
     * Manchester — в обратной. Проверяем, что такое сочетание ловится. */
    {
        char mixed[512];
        size_t sync_len = 20;
        strcpy(mixed, VECTOR);
        for(size_t i = sync_len; i < strlen(mixed); i++) {
            mixed[i] = mixed[i] == '0' ? '1' : '0';
        }

        frames_seen = 0;
        TpmsRenaultDecoder* dm = tpms_renault_decoder_alloc(on_frame, NULL);
        feed_chips(dm, mixed, TPMS_CHIP_US, 0);
        if(frames_seen != 1 || memcmp(last_raw, expected, 9) != 0) {
            printf("FAIL: sync прямой + данные инвертированные, frames=%d\n", frames_seen);
            failures++;
        } else {
            printf("OK  sync прямой + данные инвертированные\n");
        }
        tpms_renault_decoder_free(dm);
    }

    /* Джиттер длительностей +-15% */
    srand(42);
    for(int trial = 0; trial < 20; trial++) {
        frames_seen = 0;
        TpmsRenaultDecoder* d = tpms_renault_decoder_alloc(on_frame, NULL);
        int i = 0;
        while(VECTOR[i]) {
            char level = VECTOR[i];
            int run = 0;
            while(VECTOR[i] == level) { run++; i++; }
            int nominal = run * TPMS_CHIP_US;
            int jittered = nominal * (85 + rand() % 31) / 100;
            tpms_renault_decoder_feed(d, level == '1', jittered);
        }
        if(frames_seen != 1 || memcmp(last_raw, expected, 9) != 0) {
            printf("FAIL jitter trial %d: frames=%d\n", trial, frames_seen);
            failures++;
        }
        tpms_renault_decoder_free(d);
    }
    if(!failures) printf("OK  jitter +-15%%: 20/20 trials decoded\n");

    /* Битый кадр не должен пролезать */
    frames_seen = 0;
    TpmsRenaultDecoder* d = tpms_renault_decoder_alloc(on_frame, NULL);
    char broken[512];
    strcpy(broken, VECTOR);
    broken[40] = broken[40] == '0' ? '1' : '0';
    feed_chips(d, broken, TPMS_CHIP_US, 0);
    if(frames_seen != 0) { printf("FAIL: corrupted frame accepted\n"); failures++; }
    else printf("OK  corrupted frame rejected\n");
    tpms_renault_decoder_free(d);

    printf(failures ? "\nFAILURES: %d\n" : "\nall good\n", failures);
    return failures != 0;
}
