#include <string.h>
#include <minmea.h>
#include "gps_reader.h"

typedef enum {
    WorkerEvtStop = (1 << 0),
    WorkerEvtRxDone = (1 << 1),
} WorkerEvtFlags;

#define WORKER_ALL_RX_EVENTS (WorkerEvtStop | WorkerEvtRxDone)

static const int gps_baudrates[] = {4800, 9600, 19200, 38400, 57600, 115200};
static const int default_baudrate_index = 1; // 9600

static bool gps_reader_is_supported_baudrate(uint32_t baudrate) {
    for(size_t i = 0; i < sizeof(gps_baudrates) / sizeof(gps_baudrates[0]); i++) {
        if((uint32_t)gps_baudrates[i] == baudrate) {
            return true;
        }
    }

    return false;
}

// Forward declarations
static void gps_reader_parse_nmea(GpsReader* gps_reader, char* line);
static void gps_reader_start_worker(GpsReader* gps_reader);
static void gps_reader_stop_worker(GpsReader* gps_reader);

static void
    gps_reader_on_irq_cb(FuriHalSerialHandle* handle, FuriHalSerialRxEvent ev, void* context) {
    GpsReader* gps_reader = (GpsReader*)context;

    if(ev == FuriHalSerialRxEventData) {
        uint8_t data = furi_hal_serial_async_rx(handle);
        furi_stream_buffer_send(gps_reader->rx_stream, &data, 1, 0);
        furi_thread_flags_set(furi_thread_get_id(gps_reader->thread), WorkerEvtRxDone);
    }
}

static bool gps_reader_serial_init(GpsReader* gps_reader) {
    furi_assert(!gps_reader->serial_handle);

    furi_mutex_acquire(gps_reader->mutex, FuriWaitForever);
    const uint32_t baudrate = gps_reader->baudrate;
    furi_mutex_release(gps_reader->mutex);

    FURI_LOG_D("GPS", "Attempting to acquire GPS UART channel %d", GPS_UART_CH);

    // Try to acquire GPS UART channel with error handling
    gps_reader->serial_handle = furi_hal_serial_control_acquire(GPS_UART_CH);
    if(!gps_reader->serial_handle) {
        FURI_LOG_E(
            "GPS",
            "Failed to acquire GPS UART channel %d - may be in use by another app",
            GPS_UART_CH);
        return false;
    }

    FURI_LOG_I(
        "GPS",
        "GPS UART channel %d acquired, initializing at %lu baud",
        GPS_UART_CH,
        (unsigned long)baudrate);
    furi_hal_serial_init(gps_reader->serial_handle, baudrate);
    furi_hal_serial_async_rx_start(
        gps_reader->serial_handle, gps_reader_on_irq_cb, gps_reader, false);

    // Send a wake-up command like the GPS NMEA app does
    furi_hal_serial_tx(gps_reader->serial_handle, (uint8_t*)"$PMTK000*32\r\n", 14);

    FURI_LOG_I("GPS", "GPS serial initialization complete, sent wake-up command");
    return true;
}

static void gps_reader_serial_deinit(GpsReader* gps_reader) {
    if(gps_reader->serial_handle) {
        FURI_LOG_I("GPS", "Stopping serial async RX");
        furi_hal_serial_async_rx_stop(gps_reader->serial_handle);

        FURI_LOG_I("GPS", "Deinitializing serial");
        furi_hal_serial_deinit(gps_reader->serial_handle);

        FURI_LOG_I("GPS", "Releasing serial control");
        furi_hal_serial_control_release(gps_reader->serial_handle);
        gps_reader->serial_handle = NULL;

        FURI_LOG_I("GPS", "Serial cleanup complete");
    }
}

static void gps_reader_parse_nmea(GpsReader* gps_reader, char* line) {
    FURI_LOG_D("GPS", "Parsing NMEA: %s", line);

    // Mark GPS module as detected when any valid NMEA sentence is received
    furi_mutex_acquire(gps_reader->mutex, FuriWaitForever);
    gps_reader->coordinates.module_detected = true;
    furi_mutex_release(gps_reader->mutex);

    switch(minmea_sentence_id(line, false)) {
    case MINMEA_SENTENCE_RMC: {
        struct minmea_sentence_rmc frame;
        if(minmea_parse_rmc(&frame, line) && frame.valid) {
            FURI_LOG_I("GPS", "Valid RMC sentence parsed, updating coordinates");
            furi_mutex_acquire(gps_reader->mutex, FuriWaitForever);
            gps_reader->coordinates.valid = true;
            gps_reader->coordinates.latitude = minmea_tocoord(&frame.latitude);
            gps_reader->coordinates.longitude = minmea_tocoord(&frame.longitude);
            furi_mutex_release(gps_reader->mutex);
        } else {
            FURI_LOG_D("GPS", "RMC sentence invalid or parse failed");
        }
    } break;

    case MINMEA_SENTENCE_GGA: {
        struct minmea_sentence_gga frame;
        if(minmea_parse_gga(&frame, line)) {
            furi_mutex_acquire(gps_reader->mutex, FuriWaitForever);
            // Update satellite count from GGA sentence
            gps_reader->coordinates.satellite_count = frame.satellites_tracked;
            if(frame.fix_quality > 0) {
                FURI_LOG_I(
                    "GPS",
                    "Valid GGA sentence parsed, updating coordinates (sats: %d)",
                    frame.satellites_tracked);
                gps_reader->coordinates.valid = true;
                gps_reader->coordinates.latitude = minmea_tocoord(&frame.latitude);
                gps_reader->coordinates.longitude = minmea_tocoord(&frame.longitude);
            } else {
                FURI_LOG_D(
                    "GPS", "GGA sentence parsed, no fix (sats: %d)", frame.satellites_tracked);
            }
            furi_mutex_release(gps_reader->mutex);
        } else {
            FURI_LOG_D("GPS", "GGA sentence invalid or parse failed");
        }
    } break;

    case MINMEA_SENTENCE_GLL: {
        struct minmea_sentence_gll frame;
        if(minmea_parse_gll(&frame, line) && frame.status == 'A') {
            FURI_LOG_I("GPS", "Valid GLL sentence parsed, updating coordinates");
            furi_mutex_acquire(gps_reader->mutex, FuriWaitForever);
            gps_reader->coordinates.valid = true;
            gps_reader->coordinates.latitude = minmea_tocoord(&frame.latitude);
            gps_reader->coordinates.longitude = minmea_tocoord(&frame.longitude);
            furi_mutex_release(gps_reader->mutex);
        } else {
            FURI_LOG_D("GPS", "GLL sentence invalid or not active");
        }
    } break;

    default:
        break;
    }
}

static int32_t gps_reader_worker(void* context) {
    GpsReader* gps_reader = (GpsReader*)context;

    FURI_LOG_I("GPS", "GPS worker thread started");

    if(!gps_reader_serial_init(gps_reader)) {
        FURI_LOG_E("GPS", "Failed to initialize GPS serial");
        return -1;
    }

    FURI_LOG_I("GPS", "GPS worker thread entering main loop");
    size_t rx_offset = 0;

    while(1) {
        uint32_t events =
            furi_thread_flags_wait(WORKER_ALL_RX_EVENTS, FuriFlagWaitAny, FuriWaitForever);
        furi_check((events & FuriFlagError) == 0);

        if(events & WorkerEvtStop) {
            FURI_LOG_I("GPS", "GPS worker thread received stop signal");
            break;
        }

        if(events & WorkerEvtRxDone) {
            size_t len = 0;
            do {
                len = furi_stream_buffer_receive(
                    gps_reader->rx_stream,
                    gps_reader->rx_buf + rx_offset,
                    GPS_RX_BUF_SIZE - 1 - rx_offset,
                    0);

                if(len > 0) {
                    rx_offset += len;
                    gps_reader->rx_buf[rx_offset] = '\0';

                    char* line_current = (char*)gps_reader->rx_buf;
                    while(1) {
                        // Skip null characters
                        while(*line_current == '\0' &&
                              line_current < (char*)gps_reader->rx_buf + rx_offset - 1) {
                            line_current++;
                        }

                        // Find next newline
                        char* newline = strchr(line_current, '\n');
                        if(newline) {
                            *newline = '\0';
                            gps_reader_parse_nmea(gps_reader, line_current);
                            line_current = newline + 1;
                        } else {
                            if(line_current > (char*)gps_reader->rx_buf) {
                                // Move leftover bytes to start of buffer
                                rx_offset = 0;
                                while(*line_current) {
                                    gps_reader->rx_buf[rx_offset++] = *(line_current++);
                                }
                            }
                            break;
                        }
                    }
                }
            } while(len > 0);
        }
    }

    FURI_LOG_I("GPS", "GPS worker thread cleaning up serial");
    gps_reader_serial_deinit(gps_reader);
    furi_stream_buffer_free(gps_reader->rx_stream);

    FURI_LOG_I("GPS", "GPS worker thread exiting");
    return 0;
}

static void gps_reader_start_worker(GpsReader* gps_reader) {
    gps_reader->rx_stream = furi_stream_buffer_alloc(GPS_RX_BUF_SIZE, 1);
    gps_reader->thread =
        furi_thread_alloc_ex("GpsReaderWorker", 1024, gps_reader_worker, gps_reader);
    furi_thread_start(gps_reader->thread);
}

static void gps_reader_stop_worker(GpsReader* gps_reader) {
    furi_thread_flags_set(furi_thread_get_id(gps_reader->thread), WorkerEvtStop);
    furi_thread_join(gps_reader->thread);
    furi_thread_free(gps_reader->thread);
    gps_reader->thread = NULL;
    gps_reader->rx_stream = NULL;
}

GpsReader* gps_reader_alloc(uint32_t initial_baudrate) {
    GpsReader* gps_reader = malloc(sizeof(GpsReader));

    gps_reader->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    gps_reader->rx_stream = NULL;
    gps_reader->thread = NULL;
    gps_reader->serial_handle = NULL;
    if(gps_reader_is_supported_baudrate(initial_baudrate)) {
        gps_reader->baudrate = initial_baudrate;
    } else {
        gps_reader->baudrate = gps_baudrates[default_baudrate_index];
    }

    // Initialize coordinates as invalid
    gps_reader->coordinates.valid = false;
    gps_reader->coordinates.latitude = 0.0f;
    gps_reader->coordinates.longitude = 0.0f;
    gps_reader->coordinates.module_detected = false;
    gps_reader->coordinates.satellite_count = 0;

    gps_reader_start_worker(gps_reader);

    return gps_reader;
}

void gps_reader_free(GpsReader* gps_reader) {
    furi_assert(gps_reader);

    gps_reader_stop_worker(gps_reader);

    furi_mutex_free(gps_reader->mutex);

    free(gps_reader);
}

GpsCoordinates gps_reader_get_coordinates(GpsReader* gps_reader) {
    furi_assert(gps_reader);

    furi_mutex_acquire(gps_reader->mutex, FuriWaitForever);
    GpsCoordinates coords = gps_reader->coordinates;
    furi_mutex_release(gps_reader->mutex);

    return coords;
}

uint32_t gps_reader_get_baudrate(GpsReader* gps_reader) {
    furi_assert(gps_reader);

    furi_mutex_acquire(gps_reader->mutex, FuriWaitForever);
    const uint32_t baudrate = gps_reader->baudrate;
    furi_mutex_release(gps_reader->mutex);

    return baudrate;
}

bool gps_reader_set_baudrate(GpsReader* gps_reader, uint32_t baudrate) {
    furi_assert(gps_reader);

    if(!gps_reader_is_supported_baudrate(baudrate)) {
        FURI_LOG_E("GPS", "Unsupported baudrate: %lu", (unsigned long)baudrate);
        return false;
    }

    furi_mutex_acquire(gps_reader->mutex, FuriWaitForever);
    if(gps_reader->baudrate == baudrate) {
        furi_mutex_release(gps_reader->mutex);
        return true;
    }

    gps_reader->baudrate = baudrate;
    gps_reader->coordinates.valid = false;
    gps_reader->coordinates.module_detected = false;
    gps_reader->coordinates.satellite_count = 0;
    furi_mutex_release(gps_reader->mutex);

    // Full worker restart ensures the new baudrate takes effect immediately
    // without stale serial or buffered RX state from the previous session.
    gps_reader_stop_worker(gps_reader);
    gps_reader_start_worker(gps_reader);

    return true;
}
