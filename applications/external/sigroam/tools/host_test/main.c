#include <stdio.h>

int test_source_iface_run(void);
int test_line_run(void);
int test_bloom_run(void);
int test_model_run(void);
int test_settings_run(void);
int test_parse_marauder_run(void);
int test_rx_policy_run(void);
int test_handshake_run(void);
int test_scan_ctl_run(void);
int test_rawlog_run(void);
int test_view_fmt_run(void);
int test_gps_sample_run(void);
int test_wait_stage_run(void);
int test_resync_run(void);
int test_alert_run(void);
int test_poi_run(void);

int main(void) {
    int fails = 0;

    fails += test_source_iface_run();
    fails += test_line_run();
    fails += test_bloom_run();
    fails += test_model_run();
    fails += test_settings_run();
    fails += test_parse_marauder_run();
    fails += test_rx_policy_run();
    fails += test_handshake_run();
    fails += test_scan_ctl_run();
    fails += test_rawlog_run();
    fails += test_view_fmt_run();
    fails += test_gps_sample_run();
    fails += test_wait_stage_run();
    fails += test_resync_run();
    fails += test_alert_run();
    fails += test_poi_run();

    if(fails != 0) {
        printf("FAILED: %d assertion(s)\n", fails);
        return 1;
    }

    printf("ALL TESTS PASSED\n");
    return 0;
}
