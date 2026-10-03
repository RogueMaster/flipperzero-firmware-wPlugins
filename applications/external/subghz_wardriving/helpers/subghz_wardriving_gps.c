#include "subghz_wardriving_gps.h"
#include "minmea.h"
#include "ubox.h"

typedef enum {
    WorkerEvtStop = (1 << 0),
    WorkerEvtRxDone = (1 << 1),
} WorkerEvtFlags;

#define WORKER_ALL_RX_EVENTS (WorkerEvtStop | WorkerEvtRxDone)

static void subghz_gps_uart_parse_nmea(SubGhzGPS* subghz_gps, char* line) {
    switch(minmea_sentence_id(line, false)) {
    case MINMEA_SENTENCE_RMC: {
        struct minmea_sentence_rmc frame;
        if(minmea_parse_rmc(&frame, line)) {
            subghz_gps->latitude = minmea_tocoord(&frame.latitude);
            subghz_gps->longitude = minmea_tocoord(&frame.longitude);
            subghz_gps->fix_second = frame.time.seconds;
            subghz_gps->fix_minute = frame.time.minutes;
            subghz_gps->fix_hour = frame.time.hours;
        }
    } break;

    case MINMEA_SENTENCE_GGA: {
        struct minmea_sentence_gga frame;
        if(minmea_parse_gga(&frame, line)) {
            subghz_gps->latitude = minmea_tocoord(&frame.latitude);
            subghz_gps->longitude = minmea_tocoord(&frame.longitude);
            subghz_gps->satellites = frame.satellites_tracked;
            subghz_gps->fix_second = frame.time.seconds;
            subghz_gps->fix_minute = frame.time.minutes;
            subghz_gps->fix_hour = frame.time.hours;
        }
    } break;

    case MINMEA_SENTENCE_GLL: {
        struct minmea_sentence_gll frame;
        if(minmea_parse_gll(&frame, line)) {
            subghz_gps->latitude = minmea_tocoord(&frame.latitude);
            subghz_gps->longitude = minmea_tocoord(&frame.longitude);
            subghz_gps->fix_second = frame.time.seconds;
            subghz_gps->fix_minute = frame.time.minutes;
            subghz_gps->fix_hour = frame.time.hours;
        }
    } break;

    default:
        break;
    }
}

static void subghz_gps_uart_on_irq_cb(
    FuriHalSerialHandle* handle,
    FuriHalSerialRxEvent event,
    void* context) {
    UNUSED(handle);
    SubGhzGPS* subghz_gps = (SubGhzGPS*)context;

    if(event == FuriHalSerialRxEventData) {
        uint8_t data = furi_hal_serial_async_rx(handle);
        furi_stream_buffer_send(subghz_gps->rx_stream, &data, 1, 0);
        furi_thread_flags_set(furi_thread_get_id(subghz_gps->thread), WorkerEvtRxDone);
    }
}

static void subghz_gps_uart_parse_ubox(SubGhzGPS* subghz_gps, const uint8_t* data, size_t len) {
    UboxPvt pvt;
    for(size_t i = 0; i < len; i++) {
        if(ubox_rx_byte(&subghz_gps->ubox, data[i], &pvt)) {
            subghz_gps->latitude = pvt.lat * 1e-7f;
            subghz_gps->longitude = pvt.lon * 1e-7f;
            subghz_gps->satellites = pvt.sats;
            subghz_gps->fix_hour = pvt.hour;
            subghz_gps->fix_minute = pvt.min;
            subghz_gps->fix_second = pvt.sec;
        }
    }
}

static int32_t subghz_gps_uart_worker(void* context) {
    SubGhzGPS* subghz_gps = (SubGhzGPS*)context;

    size_t rx_offset = 0;

    while(1) {
        uint32_t events =
            furi_thread_flags_wait(WORKER_ALL_RX_EVENTS, FuriFlagWaitAny, FuriWaitForever);
        furi_check((events & FuriFlagError) == 0);

        if(events & WorkerEvtStop) {
            break;
        }

        if(events & WorkerEvtRxDone) {
            size_t len = 0;
            do {
                len = furi_stream_buffer_receive(
                    subghz_gps->rx_stream,
                    subghz_gps->rx_buf + rx_offset,
                    RX_BUF_SIZE - rx_offset,
                    0);

                if(len == 0) continue;

                if(subghz_gps->protocol == SubGhzGpsProtocolUbox) {
                    subghz_gps_uart_parse_ubox(subghz_gps, subghz_gps->rx_buf + rx_offset, len);
                    rx_offset = 0;
                    continue;
                }

                rx_offset += len;
                subghz_gps->rx_buf[rx_offset] = '\0';

                char* current_line = (char*)subghz_gps->rx_buf;
                while(true) {
                    while(*current_line == '\0' &&
                          current_line < (char*)subghz_gps->rx_buf + rx_offset) {
                        current_line++;
                    }

                    char* next_line = strchr(current_line, '\n');
                    if(next_line) {
                        *next_line = '\0';
                        subghz_gps_uart_parse_nmea(subghz_gps, current_line);
                        current_line = next_line + 1;
                    } else {
                        if(current_line > (char*)subghz_gps->rx_buf) {
                            rx_offset = 0;
                            while(*current_line) {
                                subghz_gps->rx_buf[rx_offset++] = *(current_line++);
                            }
                        }
                        break;
                    }
                }
            } while(len > 0);
        }
    }

    return 0;
}

static void subghz_gps_deinit(SubGhzGPS* subghz_gps) {
    furi_assert(subghz_gps);

    furi_thread_flags_set(furi_thread_get_id(subghz_gps->thread), WorkerEvtStop);
    furi_thread_join(subghz_gps->thread);

    furi_hal_serial_async_rx_stop(subghz_gps->serial_handle);
    furi_hal_serial_deinit(subghz_gps->serial_handle);
    furi_hal_serial_control_release(subghz_gps->serial_handle);

    furi_thread_free(subghz_gps->thread);

    furi_stream_buffer_free(subghz_gps->rx_stream);
}

static void subghz_gps_init(
    SubGhzGPS* subghz_gps,
    SubGhzGpsProtocol protocol,
    uint32_t baudrate,
    SubGhzGpsPins pins) {
    subghz_gps->latitude = NAN;
    subghz_gps->longitude = NAN;
    subghz_gps->satellites = 0;
    subghz_gps->fix_hour = 0;
    subghz_gps->fix_minute = 0;
    subghz_gps->fix_second = 0;

    subghz_gps->protocol = protocol;
    subghz_gps->pins = pins;
    ubox_rx_init(&subghz_gps->ubox);

    subghz_gps->rx_stream = furi_stream_buffer_alloc(RX_BUF_SIZE, 1);

    subghz_gps->thread =
        furi_thread_alloc_ex("SubGhzGPSWorker", 1024, subghz_gps_uart_worker, subghz_gps);
    furi_thread_start(subghz_gps->thread);

    subghz_gps->serial_handle = furi_hal_serial_control_acquire(
        pins == SubGhzGpsPinsLpuart ? FuriHalSerialIdLpuart : FuriHalSerialIdUsart);
    furi_check(subghz_gps->serial_handle);
    furi_hal_serial_init(subghz_gps->serial_handle, baudrate);

    furi_hal_serial_async_rx_start(
        subghz_gps->serial_handle, subghz_gps_uart_on_irq_cb, subghz_gps, false);

    subghz_gps->deinit = &subghz_gps_deinit;
}

static const FlipperAppPluginDescriptor plugin_descriptor = {
    .appid = "subghz_plugin_gps",
    .ep_api_version = 3,
    .entry_point = &subghz_gps_init,
};

const FlipperAppPluginDescriptor* subghz_gps_plugin_ep(void) {
    return &plugin_descriptor;
}
