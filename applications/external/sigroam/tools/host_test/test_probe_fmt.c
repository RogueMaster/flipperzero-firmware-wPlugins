#include "sr_test.h"

#include "sr_probe_fmt.h"
#include "sr_types.h"

#include <string.h>

static void fw_clear(SrFirmwareInfo* fw) {
    memset(fw, 0, sizeof(*fw));
}

int test_probe_fmt_run(void) {
    SrFirmwareInfo fw;
    char out[448];
    size_t n;
    unsigned sigroam_ok = 0;
    unsigned sigroam_hw = 0;
    unsigned generic_ok = 0;
    unsigned scout_generic = 0;
    unsigned no_uart = 0;
    unsigned brand = 0;
    unsigned diag = 0;

    sr_test_failures = 0;

    fw_clear(&fw);
    sr_strlcpy(fw.firmware, sizeof(fw.firmware), "Marauder");
    sr_strlcpy(fw.version, sizeof(fw.version), "v1.14.1-sigroam-0");
    sr_strlcpy(fw.hardware, sizeof(fw.hardware), "Scout Lite (ESP32-C5)");
    sr_strlcpy(fw.esp_idf, sizeof(fw.esp_idf), "v5.5.1");
    n = sr_probe_fmt_ok(&fw, "0.4", "by PINGEQUA Lab", out, sizeof(out));
    CHECK(n > 0);
    CHECK(strstr(out, "\e#SigRoam") != NULL);
    CHECK(strstr(out, "v0.4") != NULL);
    CHECK(strstr(out, "Scout Lite") != NULL);
    CHECK(strstr(out, "by PINGEQUA Lab") != NULL);
    CHECK(strstr(out, "(ESP32-C5)") == NULL);
    sigroam_ok++;
    brand++;

    CHECK(strstr(out, "v1.14.1") == NULL);
    CHECK(strstr(out, "v1.14.1-sigroam-0") == NULL);
    CHECK(strstr(out, "Firmware: Marauder") == NULL);
    CHECK(strstr(out, "ESP-IDF") == NULL);
    CHECK(strstr(out, "passive 2.4") == NULL);
    CHECK(strstr(out, "Version:") == NULL);
    no_uart++;

    fw_clear(&fw);
    sr_strlcpy(fw.firmware, sizeof(fw.firmware), "Marauder");
    sr_strlcpy(fw.version, sizeof(fw.version), "v1.14.1-sigroam-0");
    sr_strlcpy(fw.hardware, sizeof(fw.hardware), "Cardputer ADV");
    n = sr_probe_fmt_ok(&fw, "0.4", "by PINGEQUA Lab", out, sizeof(out));
    CHECK(strstr(out, "\e#SigRoam") != NULL);
    CHECK(strstr(out, "v0.4") != NULL);
    CHECK(strstr(out, "Cardputer ADV") != NULL);
    CHECK(strstr(out, "v1.14.1") == NULL);
    sigroam_hw++;

    fw_clear(&fw);
    sr_strlcpy(fw.firmware, sizeof(fw.firmware), "Marauder");
    sr_strlcpy(fw.version, sizeof(fw.version), "v1.14.1");
    sr_strlcpy(fw.hardware, sizeof(fw.hardware), "ESP32-C5 DevKit");
    n = sr_probe_fmt_ok(&fw, "0.4", "by PINGEQUA Lab", out, sizeof(out));
    CHECK(strstr(out, "\e#Marauder") != NULL);
    CHECK(strstr(out, "ESP32-C5 DevKit") != NULL);
    CHECK(strstr(out, "\e#SigRoam") == NULL);
    CHECK(strstr(out, "v0.4") == NULL);
    CHECK(strstr(out, "v1.14.1") == NULL);
    CHECK(strstr(out, "Version:") == NULL);
    CHECK(strstr(out, "ESP-IDF") == NULL);
    generic_ok++;

    fw_clear(&fw);
    sr_strlcpy(fw.firmware, sizeof(fw.firmware), "Marauder");
    sr_strlcpy(fw.version, sizeof(fw.version), "v1.14.1");
    sr_strlcpy(fw.hardware, sizeof(fw.hardware), "Scout Lite (ESP32-C5)");
    n = sr_probe_fmt_ok(&fw, "0.4", "by PINGEQUA Lab", out, sizeof(out));
    CHECK(strstr(out, "\e#SigRoam") == NULL);
    CHECK(strstr(out, "Scout Lite") != NULL);
    CHECK(strstr(out, "v1.14.1") == NULL);
    CHECK(strstr(out, "v0.4") == NULL);
    scout_generic++;

    fw_clear(&fw);
    sr_strlcpy(fw.firmware, sizeof(fw.firmware), "Marauder");
    sr_strlcpy(fw.version, sizeof(fw.version), "v1.14.1-sigroam-0");
    sr_strlcpy(fw.hardware, sizeof(fw.hardware), "Scout Lite (ESP32-C5)");
    fw.diag_seen = true;
    fw.diag_state = 1;
    fw.diag_seal = 0x7f;
    fw.diag_hb[0] = 1;
    fw.diag_hb[1] = 2;
    fw.diag_hb[2] = 3;
    fw.diag_hb[3] = 4;
    fw.diag_hb[4] = 5;
    fw.diag_hb[5] = 6;
    n = sr_probe_fmt_ok(&fw, "0.4", "by PINGEQUA Lab", out, sizeof(out));
    CHECK(strstr(out, "state: SCANNING") != NULL);
    CHECK(strstr(out, "stuck: (all cleared)") != NULL);
    CHECK(strstr(out, "hb 1/2/3") != NULL);
    CHECK(strstr(out, "v1.14.1") == NULL);
    diag++;

    CHECK(sr_probe_fmt_ok(NULL, "0.4", "by PINGEQUA Lab", out, sizeof(out)) == 0);
    CHECK(sr_probe_fmt_ok(&fw, "0.4", "by PINGEQUA Lab", NULL, 10) == 0);

    printf(
        "probe_fmt cover: sigroam_ok=%u sigroam_hw=%u generic_ok=%u "
        "scout_generic=%u no_uart=%u brand=%u diag=%u\n",
        sigroam_ok,
        sigroam_hw,
        generic_ok,
        scout_generic,
        no_uart,
        brand,
        diag);

    CHECK(sigroam_ok == 1);
    CHECK(sigroam_hw == 1);
    CHECK(generic_ok == 1);
    CHECK(scout_generic == 1);
    CHECK(no_uart == 1);
    CHECK(brand == 1);
    CHECK(diag == 1);

    return sr_test_failures;
}
