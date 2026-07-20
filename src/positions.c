#include "flipperham_i.h"
#include "aprs.h"

#include <stdio.h>

uint8_t pos_ok(FlipperHamApp* app, uint8_t i) {
    char a[POS_LEN];
    char b[POS_LEN];

    if(i >= TXT_N) return 0;
    if(!app->pos_name[i][0]) return 0;
    if(!app->pos_lat[i][0]) return 0;
    if(!app->pos_lon[i][0]) return 0;
    if(!aprs_ll_clamp(a, sizeof(a), app->pos_lat[i], 0)) return 0;
    if(!aprs_ll_clamp(b, sizeof(b), app->pos_lon[i], 1)) return 0;

    /* Keep stored positions normalized so edits and re-saves stay predictable. */
    snprintf(app->pos_lat[i], sizeof(app->pos_lat[i]), "%s", a);
    snprintf(app->pos_lon[i], sizeof(app->pos_lon[i]), "%s", b);
    return 1;
}
