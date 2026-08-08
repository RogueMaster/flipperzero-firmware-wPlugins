// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt
//
// Host unit-test runner for FlipDeFlock's pure-logic modules. Builds with plain
// gcc (see Makefile) so the confidence-scoring / coincidence-gate / auth-grading
// contracts can be regression-tested off-device. Exits non-zero on any failure.
#include <stdio.h>

int g_checks = 0;
int g_fails = 0;

void suite_flock_db(void);
void suite_watchscore(void);
void suite_esp_parser(void);
void suite_detect_rules(void);
void suite_report_escape(void);
void suite_gps_parser(void);
void suite_report_fmt(void);
void suite_report_fmt_redact(void);
void suite_flock_store(void);
void suite_flock_ble(void);
void suite_oui_vendor(void);
void suite_marauder_scan(void);
void suite_fast_trig(void);
void suite_tracker_rules(void);

int main(void) {
    printf("FlipDeFlock host unit tests\n");
    suite_flock_db();
    suite_watchscore();
    suite_esp_parser();
    suite_detect_rules();
    suite_report_escape();
    suite_gps_parser();
    suite_report_fmt();
    suite_report_fmt_redact();
    suite_flock_store();
    suite_flock_ble();
    suite_oui_vendor();
    suite_marauder_scan();
    suite_fast_trig();
    suite_tracker_rules();

    printf("\n%d checks, %d failed\n", g_checks, g_fails);
    if(g_fails) {
        printf("RESULT: FAIL\n");
        return 1;
    }
    printf("RESULT: PASS\n");
    return 0;
}
