#include <stdio.h>

int test_internet_time_run(void);

/* Present when the matching tests/test_*.c is linked; otherwise skipped. */
int test_clock_model_run(void) __attribute__((weak));
int test_dcf77_decode_run(void) __attribute__((weak));
int test_dcf77_time_run(void) __attribute__((weak));
int test_dcf77_gpio_run(void) __attribute__((weak));
int test_dcf77_auto_sync_run(void) __attribute__((weak));

int main(void) {
    int failures = 0;
    failures += test_internet_time_run();
    if(test_clock_model_run) {
        failures += test_clock_model_run();
    }
    if(test_dcf77_decode_run) {
        failures += test_dcf77_decode_run();
    }
    if(test_dcf77_time_run) {
        failures += test_dcf77_time_run();
    }
    if(test_dcf77_gpio_run) {
        failures += test_dcf77_gpio_run();
    }
    if(test_dcf77_auto_sync_run) {
        failures += test_dcf77_auto_sync_run();
    }

    if(failures != 0) {
        fprintf(stderr, "%d test(s) failed\n", failures);
        return 1;
    }
    puts("ok");
    return 0;
}
