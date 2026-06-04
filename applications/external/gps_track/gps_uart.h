#pragma once
#include <furi_hal.h>
#include <notification/notification_messages.h>

#define RX_BUF_SIZE 1024
#define UART_CH     (FuriHalSerialIdUsart)

static const int gps_baudrates[6] = {4800, 9600, 19200, 38400, 57600, 115200};
static int current_gps_baudrate   = 1;

typedef struct {
    bool  valid;
    float latitude;
    float longitude;
    float speed_knots;
    float course;
    float altitude;
    char  altitude_units;
    int   fix_quality;
    int   satellites_tracked;
    int   time_hours;
    int   time_minutes;
    int   time_seconds;
} GpsStatus;

typedef enum { MS, KPH, KNOTS, MPH } SpeedUnit;

typedef enum {
    VIEW_NORMAL,
    VIEW_SETTINGS,
    VIEW_CHANGE_BAUDRATE,
    VIEW_CHANGE_DEEPSLEEP,
    VIEW_CHANGE_SPEEDUNIT,
    VIEW_NAV,
    VIEW_NAV_LIST,
    VIEW_CRUMBS,
    VIEW_CRUMBS_MENU,
    VIEW_POI_COORD,
    VIEW_NAME_INPUT,
    VIEW_POI_RENAME,
} AppView;

typedef struct {
    FuriMutex*           mutex;
    FuriThread*          thread;
    FuriStreamBuffer*    rx_stream;
    uint8_t              rx_buf[RX_BUF_SIZE];
    NotificationApp*     notifications;
    uint32_t             baudrate;
    bool                 deep_sleep_enabled;
    SpeedUnit            speed_units;
    int                  tz_offset;
    AppView              view_state;
    FuriHalSerialHandle* serial_handle;
    GpsStatus            status;
} GpsUart;

void     gps_uart_init_thread(GpsUart* gps_uart);
void     gps_uart_deinit_thread(GpsUart* gps_uart);
GpsUart* gps_uart_enable();
void     gps_uart_disable(GpsUart* gps_uart);
