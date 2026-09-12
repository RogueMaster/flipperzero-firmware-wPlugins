/* What a string is actually this wide, in the font the device draws it
   in. The fake canvas in test/ estimates per character; this asks u8g2. */
#define BB_HOST_TEST 1
#include <stdio.h>
#include <string.h>
#include "beepback.h"
#include "real_canvas.h"

int main(int argc, char** argv) {
    rc_init();
    for(int i = 1; i < argc; i++) {
        canvas_set_font(NULL, FontSecondary);
        int sec = canvas_string_width(NULL, argv[i]);
        canvas_set_font(NULL, FontPrimary);
        printf(
            "%-22s secondary %3d   primary %3d\n",
            argv[i],
            sec,
            canvas_string_width(NULL, argv[i]));
    }
    return 0;
}
