#include <stdio.h>

int test_internet_time_run(void);

/* Present when tests/test_clock_model.c is linked; otherwise skipped. */
int test_clock_model_run(void) __attribute__((weak));

int main(void) {
    int failures = 0;
    failures += test_internet_time_run();
    if(test_clock_model_run) {
        failures += test_clock_model_run();
    }

    if(failures != 0) {
        fprintf(stderr, "%d test(s) failed\n", failures);
        return 1;
    }
    puts("ok");
    return 0;
}
