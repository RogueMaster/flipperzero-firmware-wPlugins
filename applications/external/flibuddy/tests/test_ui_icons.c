#include "minitest.h"

#include "ui_icons.h"

static void test_icons_have_the_design_sizes(void) {
    CHECK_EQ_INT(ui_icon_usb.width, 9);
    CHECK_EQ_INT(ui_icon_usb.height, 5);
    CHECK_EQ_INT(ui_icon_bt.width, 5);
    CHECK_EQ_INT(ui_icon_bt.height, 9);
    CHECK_EQ_INT(ui_icon_check.width, 7);
    CHECK_EQ_INT(ui_icon_warn.width, 9);
    CHECK_EQ_INT(ui_icon_warn.height, 8);
    CHECK_EQ_INT(ui_icon_arrow_left.width, 3);
    CHECK_EQ_INT(ui_icon_arrow_up.width, 5);
    CHECK_EQ_INT(ui_icon_app.width, 10);
    CHECK_EQ_INT(ui_icon_app_2x.width, 20);
    CHECK_EQ_INT(ui_icon_app_2x.height, 20);
    CHECK_EQ_INT(ui_icon_ok_button.width, 7);
    CHECK_EQ_INT(ui_icon_ok_button.height, 7);
}

/* Version 3 (29 modules) at 2 px: 58 px, within the 64 px screen. */
static void test_the_qr_code_fits_the_screen(void) {
    CHECK_EQ_INT(ui_icon_qr_get_app.width, 58);
    CHECK_EQ_INT(ui_icon_qr_get_app.height, 58);
    /* the top-left finder pattern: 7 dark modules, 14 px */
    CHECK_EQ_INT(ui_icon_qr_get_app.bits[0], 0xFF);
    CHECK_EQ_INT(ui_icon_qr_get_app.bits[1], 0x3F);
}

static void test_rows_are_lsb_first(void) {
    /* usb row 2 is "#####.#.#": bits 0-4, 6 and 8 */
    CHECK_EQ_INT(ui_icon_usb.bits[4], 0x5F);
    CHECK_EQ_INT(ui_icon_usb.bits[5], 0x01);
    /* app_2x row 0 is ".." "######" doubled: pixels 4..15 */
    CHECK_EQ_INT(ui_icon_app_2x.bits[0], 0xF0);
    CHECK_EQ_INT(ui_icon_app_2x.bits[1], 0xFF);
    CHECK_EQ_INT(ui_icon_app_2x.bits[2], 0x00);
}

int main(void) {
    RUN_TEST(test_icons_have_the_design_sizes);
    RUN_TEST(test_rows_are_lsb_first);
    RUN_TEST(test_the_qr_code_fits_the_screen);
    return MINITEST_RESULT();
}
