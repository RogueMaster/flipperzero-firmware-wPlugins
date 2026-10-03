#include "subghz_wardriving_gps.h"

#if SUBGHZ_GPS_HAS_RPC

#define RPC_STREAM_FREQ       10
#define RPC_POLL_PERIOD_MS    1000
#define RPC_STREAM_TIMEOUT_MS 3000

static void subghz_gps_rpc_callback(GpsStatus status, const GpsLocation* location, void* context) {
    SubGhzGPS* subghz_gps = context;
    subghz_gps->rpc_last_rx = furi_get_tick();

    if(status == GpsStatusOk && location) {
        subghz_gps->latitude = location->latitude * 1e-7f;
        subghz_gps->longitude = location->longitude * 1e-7f;
        subghz_gps->satellites = location->satellites;
        DateTime datetime;
        furi_hal_rtc_get_datetime(&datetime);
        subghz_gps->fix_hour = datetime.hour;
        subghz_gps->fix_minute = datetime.minute;
        subghz_gps->fix_second = datetime.second;
    } else {
        subghz_gps->latitude = NAN;
        subghz_gps->longitude = NAN;
        subghz_gps->satellites = 0;
        subghz_gps->fix_hour = 0;
        subghz_gps->fix_minute = 0;
        subghz_gps->fix_second = 0;
    }
}

static void subghz_gps_rpc_poll(void* context) {
    SubGhzGPS* subghz_gps = context;
    if(furi_get_tick() - subghz_gps->rpc_last_rx >= furi_ms_to_ticks(RPC_STREAM_TIMEOUT_MS)) {
        gps_request_stream(subghz_gps->rpc, RPC_STREAM_FREQ);
    }
}

SubGhzGPS* subghz_gps_rpc_start(void) {
    SubGhzGPS* subghz_gps = malloc(sizeof(SubGhzGPS));
    memset(subghz_gps, 0, sizeof(SubGhzGPS));

    subghz_gps->plugin_app = NULL;
    subghz_gps->protocol = SubGhzGpsProtocolRpc;
    subghz_gps->latitude = NAN;
    subghz_gps->longitude = NAN;

    subghz_gps->rpc = furi_record_open(RECORD_GPS);
    gps_set_location_callback(subghz_gps->rpc, subghz_gps_rpc_callback, subghz_gps);

    gps_request_stream(subghz_gps->rpc, RPC_STREAM_FREQ);
    subghz_gps->rpc_timer =
        furi_timer_alloc(subghz_gps_rpc_poll, FuriTimerTypePeriodic, subghz_gps);
    furi_timer_start(subghz_gps->rpc_timer, furi_ms_to_ticks(RPC_POLL_PERIOD_MS));

    return subghz_gps;
}

void subghz_gps_rpc_stop(SubGhzGPS* subghz_gps) {
    furi_assert(subghz_gps);

    furi_timer_stop(subghz_gps->rpc_timer);
    furi_timer_free(subghz_gps->rpc_timer);

    gps_stop_stream(subghz_gps->rpc);
    gps_set_location_callback(subghz_gps->rpc, NULL, NULL);
    furi_record_close(RECORD_GPS);

    free(subghz_gps);
}

#else

SubGhzGPS* subghz_gps_rpc_start(void) {
    return NULL;
}

void subghz_gps_rpc_stop(SubGhzGPS* subghz_gps) {
    UNUSED(subghz_gps);
}

#endif
