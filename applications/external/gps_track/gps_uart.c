#include <string.h>
#include <math.h>
#include <minmea.h>
#include "gps_uart.h"

typedef enum {
    WorkerEvtStop   = (1 << 0),
    WorkerEvtRxDone = (1 << 1),
} WorkerEvtFlags;
#define WORKER_ALL_RX_EVENTS (WorkerEvtStop | WorkerEvtRxDone)

static float safe_float(float v) {
    if(__builtin_isnan(v) || __builtin_isinf(v)) return 0.0f;
    return v;
}

static void gps_uart_on_irq_cb(FuriHalSerialHandle* handle, FuriHalSerialRxEvent ev, void* context) {
    GpsUart* gps_uart = (GpsUart*)context;
    if(ev == FuriHalSerialRxEventData) {
        uint8_t data = furi_hal_serial_async_rx(handle);
        furi_stream_buffer_send(gps_uart->rx_stream, &data, 1, 0);
        furi_thread_flags_set(furi_thread_get_id(gps_uart->thread), WorkerEvtRxDone);
    }
}
static void gps_uart_serial_init(GpsUart* gps_uart) {
    furi_assert(!gps_uart->serial_handle);
    gps_uart->serial_handle = furi_hal_serial_control_acquire(UART_CH);
    furi_check(gps_uart->serial_handle);
    furi_hal_serial_init(gps_uart->serial_handle, gps_uart->baudrate);
    furi_hal_serial_async_rx_start(gps_uart->serial_handle, gps_uart_on_irq_cb, gps_uart, false);
    furi_hal_serial_tx(gps_uart->serial_handle, (uint8_t*)"wakey wakey\r\n", strlen("wakey wakey\r\n"));
    // 1Hz обновление, только RMC+GGA
    furi_delay_ms(200);
    furi_hal_serial_tx(gps_uart->serial_handle,
        (uint8_t*)"$PMTK220,1000*1F\r\n", strlen("$PMTK220,1000*1F\r\n"));
    furi_hal_serial_tx(gps_uart->serial_handle,
        (uint8_t*)"$PMTK314,0,1,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0*28\r\n",
        strlen("$PMTK314,0,1,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0*28\r\n"));
}
static void gps_uart_serial_deinit(GpsUart* gps_uart) {
    furi_assert(gps_uart->serial_handle);
    furi_hal_serial_async_rx_stop(gps_uart->serial_handle);
    furi_hal_serial_deinit(gps_uart->serial_handle);
    furi_hal_serial_control_release(gps_uart->serial_handle);
    gps_uart->serial_handle = NULL;
}
static void gps_uart_parse_nmea(GpsUart* gps_uart, char* line) {
    switch(minmea_sentence_id(line, false)) {
    case MINMEA_SENTENCE_RMC: {
        struct minmea_sentence_rmc frame;
        if(minmea_parse_rmc(&frame, line)) {
            float lat = safe_float(minmea_tocoord(&frame.latitude));
            float lon = safe_float(minmea_tocoord(&frame.longitude));
            if(lat != 0.0f || lon != 0.0f) {
                gps_uart->status.latitude  = lat;
                gps_uart->status.longitude = lon;
            }
            gps_uart->status.valid        = frame.valid;
            gps_uart->status.speed_knots  = safe_float(minmea_tofloat(&frame.speed));
            gps_uart->status.course       = safe_float(minmea_tofloat(&frame.course));
            gps_uart->status.time_hours   = frame.time.hours;
            gps_uart->status.time_minutes = frame.time.minutes;
            gps_uart->status.time_seconds = frame.time.seconds;
        }
    } break;
    case MINMEA_SENTENCE_GGA: {
        struct minmea_sentence_gga frame;
        if(minmea_parse_gga(&frame, line)) {
            float lat = safe_float(minmea_tocoord(&frame.latitude));
            float lon = safe_float(minmea_tocoord(&frame.longitude));
            if(lat != 0.0f || lon != 0.0f) {
                gps_uart->status.latitude  = lat;
                gps_uart->status.longitude = lon;
            }
            gps_uart->status.fix_quality        = frame.fix_quality;
            gps_uart->status.satellites_tracked = frame.satellites_tracked;
            float alt = safe_float(minmea_tofloat(&frame.altitude));
            if(alt != 0.0f) gps_uart->status.altitude = alt;
            if(frame.altitude_units != 0) gps_uart->status.altitude_units = frame.altitude_units;
            gps_uart->status.time_hours   = frame.time.hours;
            gps_uart->status.time_minutes = frame.time.minutes;
            gps_uart->status.time_seconds = frame.time.seconds;
        }
    } break;
    case MINMEA_SENTENCE_GLL: {
        struct minmea_sentence_gll frame;
        if(minmea_parse_gll(&frame, line)) {
            float lat = safe_float(minmea_tocoord(&frame.latitude));
            float lon = safe_float(minmea_tocoord(&frame.longitude));
            if(lat != 0.0f || lon != 0.0f) {
                gps_uart->status.latitude  = lat;
                gps_uart->status.longitude = lon;
            }
            gps_uart->status.time_hours   = frame.time.hours;
            gps_uart->status.time_minutes = frame.time.minutes;
            gps_uart->status.time_seconds = frame.time.seconds;
        }
    } break;
    default: break;
    }
}
static int32_t gps_uart_worker(void* context) {
    GpsUart* gps_uart = (GpsUart*)context;
    size_t rx_offset = 0;
    while(1) {
        uint32_t events = furi_thread_flags_wait(WORKER_ALL_RX_EVENTS, FuriFlagWaitAny, FuriWaitForever);
        furi_check((events & FuriFlagError) == 0);
        if(events & WorkerEvtStop) break;
        if(events & WorkerEvtRxDone) {
            size_t len = 0;
            do {
                len = furi_stream_buffer_receive(gps_uart->rx_stream, gps_uart->rx_buf + rx_offset, RX_BUF_SIZE - 1 - rx_offset, 0);
                if(len > 0) {
                    rx_offset += len;
                    gps_uart->rx_buf[rx_offset] = '\0';
                    char* line = (char*)gps_uart->rx_buf;
                    while(1) {
                        while(*line == '\0' && line < (char*)gps_uart->rx_buf + rx_offset - 1) line++;
                        char* nl = strchr(line, '\n');
                        if(nl) { *nl = '\0'; gps_uart_parse_nmea(gps_uart, line); line = nl + 1; }
                        else {
                            if(line > (char*)gps_uart->rx_buf) {
                                rx_offset = 0;
                                while(*line) gps_uart->rx_buf[rx_offset++] = *(line++);
                            }
                            break;
                        }
                    }
                }
            } while(len > 0);
        }
    }
    gps_uart_serial_deinit(gps_uart);
    furi_stream_buffer_free(gps_uart->rx_stream);
    return 0;
}
void gps_uart_init_thread(GpsUart* gps_uart) {
    furi_assert(gps_uart);
    memset(&gps_uart->status, 0, sizeof(GpsStatus));
    gps_uart->status.altitude_units = 'M';
    gps_uart->rx_stream = furi_stream_buffer_alloc(RX_BUF_SIZE * 5, 1);
    gps_uart->thread    = furi_thread_alloc();
    furi_thread_set_name(gps_uart->thread, "GpsTrackWorker");
    furi_thread_set_stack_size(gps_uart->thread, 2048);
    furi_thread_set_context(gps_uart->thread, gps_uart);
    furi_thread_set_callback(gps_uart->thread, gps_uart_worker);
    furi_thread_start(gps_uart->thread);
    gps_uart_serial_init(gps_uart);
}
void gps_uart_deinit_thread(GpsUart* gps_uart) {
    furi_assert(gps_uart);
    furi_thread_flags_set(furi_thread_get_id(gps_uart->thread), WorkerEvtStop);
    furi_thread_join(gps_uart->thread);
    furi_thread_free(gps_uart->thread);
}
GpsUart* gps_uart_enable() {
    GpsUart* gps_uart        = malloc(sizeof(GpsUart));
    memset(gps_uart, 0, sizeof(GpsUart));
    gps_uart->notifications  = furi_record_open(RECORD_NOTIFICATION);
    gps_uart->baudrate       = gps_baudrates[current_gps_baudrate];
    gps_uart->speed_units    = MS;
    gps_uart->tz_offset      = 0;
    gps_uart->view_state     = VIEW_NORMAL;
    gps_uart_init_thread(gps_uart);
    return gps_uart;
}
void gps_uart_disable(GpsUart* gps_uart) {
    furi_assert(gps_uart);
    gps_uart_deinit_thread(gps_uart);
    furi_record_close(RECORD_NOTIFICATION);
    free(gps_uart);
}
