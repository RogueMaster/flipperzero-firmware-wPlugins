/*
 * SPDX-FileCopyrightText: 2020-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "linux_port.h"
#include "esp_loader_io.h"
#include "esp_loader.h"
#include "loader_port_stdio_log.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <termios.h>
#include <time.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>

#if defined(LINUX_PORT_GPIO)
#include <gpiod.h>
#endif

/* ─── USB JTAG Serial detection ──────────────────────────────────────────── */

#define ESPRESSIF_USB_JTAG_VID 0x303A
#define ESPRESSIF_USB_JTAG_PID 0x1001

/* ─── monotonic wall-clock helper ───────────────────────────────────────── */

static int64_t time_now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000LL + (int64_t)ts.tv_nsec / 1000000LL;
}

/* ─── UART helpers ───────────────────────────────────────────────────────── */

static speed_t convert_baudrate(uint32_t baud)
{
    switch (baud) {
    case 50:      return B50;
    case 75:      return B75;
    case 110:     return B110;
    case 134:     return B134;
    case 150:     return B150;
    case 200:     return B200;
    case 300:     return B300;
    case 600:     return B600;
    case 1200:    return B1200;
    case 1800:    return B1800;
    case 2400:    return B2400;
    case 4800:    return B4800;
    case 9600:    return B9600;
    case 19200:   return B19200;
    case 38400:   return B38400;
    case 57600:   return B57600;
    case 115200:  return B115200;
    case 230400:  return B230400;
    case 460800:  return B460800;
    case 500000:  return B500000;
    case 576000:  return B576000;
    case 921600:  return B921600;
    case 1000000: return B1000000;
    case 1152000: return B1152000;
    case 1500000: return B1500000;
    case 2000000: return B2000000;
    case 2500000: return B2500000;
    case 3000000: return B3000000;
    case 3500000: return B3500000;
    case 4000000: return B4000000;
    default:      return (speed_t) - 1;
    }
}

static int serial_open(const char *device, uint32_t baudrate)
{
    struct termios options;
    int status, fd;

    fd = open(device, O_RDWR | O_NOCTTY | O_NDELAY | O_NONBLOCK);
    if (fd == -1) {
        perror("serial_open: open");
        return -1;
    }

    fcntl(fd, F_SETFL, O_RDWR);

    tcgetattr(fd, &options);
    speed_t baud = convert_baudrate(baudrate);
    if (baud == (speed_t) - 1) {
        fprintf(stderr, "serial_open: unsupported baudrate %u\n", baudrate);
        close(fd);
        return -1;
    }

    cfmakeraw(&options);
    cfsetispeed(&options, baud);
    cfsetospeed(&options, baud);

    options.c_cflag |= (CLOCAL | CREAD);
    options.c_cflag &= ~(PARENB | CSTOPB | CSIZE);
    options.c_cflag |= CS8;
    options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    options.c_oflag &= ~OPOST;
    options.c_iflag &= ~(IXON | IXOFF | IXANY);
    options.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL);

    options.c_cc[VMIN]  = 0;
    options.c_cc[VTIME] = 0; /* timeouts managed via select(), not termios */

    tcsetattr(fd, TCSANOW, &options);

    ioctl(fd, TIOCMGET, &status);
    status |= TIOCM_DTR;
    status |= TIOCM_RTS;
    ioctl(fd, TIOCMSET, &status);

    usleep(10000); /* 10 ms settling */

    return fd;
}

static esp_loader_error_t change_baudrate(int fd, uint32_t baudrate)
{
    struct termios options;
    speed_t baud = convert_baudrate(baudrate);

    if (baud == (speed_t) - 1) {
        return ESP_LOADER_ERROR_INVALID_PARAM;
    }

    tcgetattr(fd, &options);
    cfmakeraw(&options);
    cfsetispeed(&options, baud);
    cfsetospeed(&options, baud);
    tcsetattr(fd, TCSANOW, &options);

    return ESP_LOADER_SUCCESS;
}

/* ─── USB JTAG Serial helpers ────────────────────────────────────────────── */

/*
 * Read a hex value from a sysfs file (e.g. "303a\n") into *out.
 * Returns true on success.
 */
static bool read_sysfs_hex(const char *path, unsigned int *out)
{
    FILE *f = fopen(path, "r");
    if (!f) {
        return false;
    }
    int rc = fscanf(f, "%x", out);
    fclose(f);
    return (rc == 1);
}

/*
 * Return true when the serial device at `device` is an Espressif USB JTAG
 * Serial interface (VID=0x303A, PID=0x1001), detected via sysfs.
 *
 * Typical sysfs layout for /dev/ttyACM0:
 *   /sys/class/tty/ttyACM0/device  → symlink to USB interface
 *   ../idVendor                    → one level up = USB device node
 *   ../idProduct
 */
static bool is_usb_jtag_serial(const char *device)
{
    /* Extract the basename, e.g. "ttyACM0" from "/dev/ttyACM0" */
    const char *name = strrchr(device, '/');
    name = name ? name + 1 : device;

    char vid_path[256], pid_path[256];
    snprintf(vid_path, sizeof(vid_path),
             "/sys/class/tty/%s/device/../idVendor", name);
    snprintf(pid_path, sizeof(pid_path),
             "/sys/class/tty/%s/device/../idProduct", name);

    unsigned int vid = 0, pid = 0;
    if (!read_sysfs_hex(vid_path, &vid) || !read_sysfs_hex(pid_path, &pid)) {
        return false;
    }

    return (vid == ESPRESSIF_USB_JTAG_VID && pid == ESPRESSIF_USB_JTAG_PID);
}

/*
 * After a USB JTAG Serial device re-enumerates, the port disappears briefly.
 * Close the current fd and poll serial_open() for up to `timeout_ms`.
 */
static esp_loader_error_t wait_for_port_reopen(linux_port_t *p, uint32_t timeout_ms)
{
    if (p->_serial >= 0) {
        close(p->_serial);
        p->_serial = -1;
    }

    int64_t deadline = time_now_ms() + (int64_t)timeout_ms;

    while (time_now_ms() < deadline) {
        usleep(100000); /* 100 ms between attempts */
        int fd = serial_open(p->device, p->baudrate);
        if (fd >= 0) {
            p->_serial = fd;
            return ESP_LOADER_SUCCESS;
        }
    }

    fprintf(stderr, "linux_port: timed out waiting for %s to reappear after USB reset\n",
            p->device);
    return ESP_LOADER_ERROR_TIMEOUT;
}

/* ─── DTR/RTS helpers (LINUX_GPIO_DTR_RTS) ───────────────────────────────── */

static void set_dtr_rts(int fd, bool dtr, bool rts)
{
    int status;
    ioctl(fd, TIOCMGET, &status);
    if (dtr) {
        status |= TIOCM_DTR;
    } else {
        status &= ~TIOCM_DTR;
    }
    if (rts) {
        status |= TIOCM_RTS;
    } else {
        status &= ~TIOCM_RTS;
    }
    ioctl(fd, TIOCMSET, &status);
}

/* ─── GPIO helpers (LINUX_GPIO_GPIOD) ──────────────────────────────────── */

#if defined(LINUX_PORT_GPIO)

/* Drives a single GPIO line high (active=true) or low (active=false). */
static void set_gpio_line(linux_port_t *p, uint32_t pin, bool active)
{
    gpiod_line_request_set_value(p->_gpio_request, (unsigned int)pin,
                                 active ? GPIOD_LINE_VALUE_ACTIVE : GPIOD_LINE_VALUE_INACTIVE);
}

static esp_loader_error_t gpiod_init(linux_port_t *p)
{
    struct gpiod_chip           *chip     = NULL;
    struct gpiod_line_settings  *settings = NULL;
    struct gpiod_line_config    *line_cfg = NULL;
    struct gpiod_request_config *req_cfg  = NULL;
    esp_loader_error_t           ret      = ESP_LOADER_ERROR_FAIL;

    chip = gpiod_chip_open(p->gpio_chip_path);
    if (!chip) {
        fprintf(stderr, "linux_port: cannot open GPIO chip %s: %s\n",
                p->gpio_chip_path, strerror(errno));
        return ESP_LOADER_ERROR_FAIL;
    }

    settings = gpiod_line_settings_new();
    line_cfg  = gpiod_line_config_new();
    req_cfg   = gpiod_request_config_new();

    if (!settings || !line_cfg || !req_cfg) {
        fprintf(stderr, "linux_port: out of memory allocating gpiod config\n");
        goto out;
    }

    gpiod_line_settings_set_direction(settings, GPIOD_LINE_DIRECTION_OUTPUT);
    gpiod_line_settings_set_output_value(settings, GPIOD_LINE_VALUE_ACTIVE); /* deasserted high */

    unsigned int offsets[2] = { p->reset_pin, p->boot_pin };
    if (gpiod_line_config_add_line_settings(line_cfg, offsets, 2, settings) < 0) {
        fprintf(stderr, "linux_port: cannot add GPIO line settings: %s\n", strerror(errno));
        goto out;
    }

    gpiod_request_config_set_consumer(req_cfg, "esp-serial-flasher");

    p->_gpio_request = gpiod_chip_request_lines(chip, req_cfg, line_cfg);
    if (!p->_gpio_request) {
        fprintf(stderr, "linux_port: cannot request GPIO lines: %s\n", strerror(errno));
        goto out;
    }

    ret = ESP_LOADER_SUCCESS;

out:
    if (settings) {
        gpiod_line_settings_free(settings);
    }
    if (line_cfg) {
        gpiod_line_config_free(line_cfg);
    }
    if (req_cfg) {
        gpiod_request_config_free(req_cfg);
    }
    /* The request is self-contained; the chip handle is no longer needed. */
    gpiod_chip_close(chip);
    return ret;
}

static void gpiod_deinit(linux_port_t *p)
{
    if (p->_gpio_request) {
        gpiod_line_request_release(p->_gpio_request);
        p->_gpio_request = NULL;
    }
}
#endif /* LINUX_PORT_GPIO */

/* ─── port ops ───────────────────────────────────────────────────────────── */

static esp_loader_error_t linux_port_init(esp_loader_port_t *port)
{
    linux_port_t *p = container_of(port, linux_port_t, port);

    p->_time_end    = 0;
    p->_is_usb_jtag = false;
#if defined(LINUX_PORT_GPIO)
    p->_gpio_request = NULL;
#endif

#ifndef LINUX_PORT_GPIO
    if (p->gpio_mode == LINUX_GPIO_GPIOD) {
        fprintf(stderr, "linux_port: gpio_mode=LINUX_GPIO_GPIOD requested but "
                "gpio support was not compiled in.\n"
                "  Rebuild with -DLINUX_PORT_GPIO=ON to enable it.\n");
        return ESP_LOADER_ERROR_FAIL;
    }
#endif

    p->_serial = serial_open(p->device, p->baudrate);
    if (p->_serial < 0) {
        fprintf(stderr, "linux_port: could not open %s\n", p->device);
        return ESP_LOADER_ERROR_FAIL;
    }

    switch (p->gpio_mode) {
#if defined(LINUX_PORT_GPIO)
    case LINUX_GPIO_GPIOD:
        return gpiod_init(p);
#endif
    case LINUX_GPIO_DTR_RTS:
        p->_is_usb_jtag = is_usb_jtag_serial(p->device);
        if (p->_is_usb_jtag) {
            printf("linux_port: detected USB JTAG Serial device on %s "
                   "(VID=0x%04X PID=0x%04X) — re-enumeration after reset will be handled automatically\n",
                   p->device, ESPRESSIF_USB_JTAG_VID, ESPRESSIF_USB_JTAG_PID);
        }
        return ESP_LOADER_SUCCESS;
    default:
        return ESP_LOADER_SUCCESS;
    }
}

static void linux_port_deinit(esp_loader_port_t *port)
{
    linux_port_t *p = container_of(port, linux_port_t, port);

    if (p->_serial >= 0) {
        close(p->_serial);
        p->_serial = -1;
    }

#if defined(LINUX_PORT_GPIO)
    gpiod_deinit(p);
#endif
}

static esp_loader_error_t read_char(linux_port_t *p, char *c, uint32_t timeout_ms)
{
    if (timeout_ms == 0) {
        return ESP_LOADER_ERROR_TIMEOUT;
    }

    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(p->_serial, &fds);

    struct timeval tv = {
        .tv_sec  = timeout_ms / 1000,
        .tv_usec = (timeout_ms % 1000) * 1000,
    };

    int ret = select(p->_serial + 1, &fds, NULL, NULL, &tv);
    if (ret < 0) {
        return (errno == EINTR) ? ESP_LOADER_ERROR_TIMEOUT : ESP_LOADER_ERROR_FAIL;
    } else if (ret == 0) {
        return ESP_LOADER_ERROR_TIMEOUT;
    }

    int n = (int)read(p->_serial, c, 1);
    if (n == 1) {
        return ESP_LOADER_SUCCESS;
    } else if (n == 0) {
        return ESP_LOADER_ERROR_TIMEOUT;
    } else {
        return ESP_LOADER_ERROR_FAIL;
    }
}

static esp_loader_error_t read_data(linux_port_t *p, uint8_t *buffer, uint16_t size)
{
    for (uint16_t i = 0; i < size; i++) {
        int64_t remaining = p->_time_end - time_now_ms();
        uint32_t timeout  = (remaining > 0) ? (uint32_t)remaining : 0;
        RETURN_ON_ERROR(read_char(p, (char *)&buffer[i], timeout));
    }
    return ESP_LOADER_SUCCESS;
}

static esp_loader_error_t linux_uart_write(esp_loader_port_t *port, const uint8_t *data, uint16_t size, uint32_t timeout)
{
    linux_port_t *p = container_of(port, linux_port_t, port);
    (void)timeout;
    ssize_t written = write(p->_serial, data, size);

    if (written < 0) {
        return ESP_LOADER_ERROR_FAIL;
    } else if ((uint16_t)written < size) {
        return ESP_LOADER_ERROR_TIMEOUT;
    }

    return ESP_LOADER_SUCCESS;
}

static esp_loader_error_t linux_uart_read(esp_loader_port_t *port, uint8_t *data, uint16_t size, uint32_t timeout)
{
    linux_port_t *p = container_of(port, linux_port_t, port);
    (void)timeout;
    RETURN_ON_ERROR(read_data(p, data, size));

    return ESP_LOADER_SUCCESS;
}

static void linux_delay_ms_raw(uint32_t ms)
{
    usleep((useconds_t)ms * 1000u);
}

static void linux_delay_ms(esp_loader_port_t *port, uint32_t ms)
{
    (void)port;
    linux_delay_ms_raw(ms);
}

static void linux_start_timer(esp_loader_port_t *port, uint32_t ms)
{
    linux_port_t *p = container_of(port, linux_port_t, port);
    p->_time_end = time_now_ms() + (int64_t)ms;
}

static uint32_t linux_remaining_time(esp_loader_port_t *port)
{
    linux_port_t *p = container_of(port, linux_port_t, port);
    int64_t remaining = p->_time_end - time_now_ms();
    return (remaining > 0) ? (uint32_t)remaining : 0;
}


static esp_loader_error_t linux_change_rate(esp_loader_port_t *port, uint32_t baudrate)
{
    linux_port_t *p = container_of(port, linux_port_t, port);
    return change_baudrate(p->_serial, baudrate);
}

/* ─── reset / bootloader entry ───────────────────────────────────────────── */

/*
 * DTR/RTS signal mapping on the standard esptool auto-reset circuit
 * (CP2102/CH340/FT232 boards):
 *
 *   DTR asserted (HIGH) → BOOT pin LOW  (via inverting transistor)
 *   RTS asserted (HIGH) → RESET pin LOW (via inverting transistor)
 *
 * SERIAL_FLASHER_BOOT_INVERT / SERIAL_FLASHER_RESET_INVERT flip these
 * polarities for boards that use a non-inverting circuit.
 */

/* Logical signal levels — independent of the hardware polarity setting */
#define DTR_BOOT_ASSERT    (!SERIAL_FLASHER_BOOT_INVERT)   /* DTR value that drives BOOT LOW  */
#define DTR_BOOT_DEASSERT  ( SERIAL_FLASHER_BOOT_INVERT)   /* DTR value that drives BOOT HIGH */
#define RTS_RESET_ASSERT   (!SERIAL_FLASHER_RESET_INVERT)  /* RTS value that drives RESET LOW  */
#define RTS_RESET_DEASSERT ( SERIAL_FLASHER_RESET_INVERT)  /* RTS value that drives RESET HIGH */

static void linux_reset_target(esp_loader_port_t *port)
{
    linux_port_t *p = container_of(port, linux_port_t, port);

    switch (p->gpio_mode) {

#if defined(LINUX_PORT_GPIO)
    case LINUX_GPIO_GPIOD:
        set_gpio_line(p, p->reset_pin, SERIAL_FLASHER_RESET_INVERT != 0); /* assert RESET */
        linux_delay_ms_raw(SERIAL_FLASHER_RESET_HOLD_TIME_MS);
        set_gpio_line(p, p->reset_pin, SERIAL_FLASHER_RESET_INVERT == 0); /* deassert RESET */
        break;
#endif

    case LINUX_GPIO_DTR_RTS:
        /* esptool HardReset: pulse RESET low, leave BOOT (DTR) alone */
        set_dtr_rts(p->_serial, false, RTS_RESET_ASSERT);
        linux_delay_ms_raw(SERIAL_FLASHER_RESET_HOLD_TIME_MS);
        set_dtr_rts(p->_serial, false, RTS_RESET_DEASSERT);
        if (p->_is_usb_jtag) {
            wait_for_port_reopen(p, 3000);
        }
        break;

    default:
        break;
    }
}

static void linux_enter_bootloader(esp_loader_port_t *port)
{
    linux_port_t *p = container_of(port, linux_port_t, port);

    switch (p->gpio_mode) {

#if defined(LINUX_PORT_GPIO)
    case LINUX_GPIO_GPIOD:
        set_gpio_line(p, p->boot_pin, SERIAL_FLASHER_BOOT_INVERT  != 0); /* assert BOOT */
        linux_delay_ms_raw(SERIAL_FLASHER_RESET_HOLD_TIME_MS);
        set_gpio_line(p, p->reset_pin, SERIAL_FLASHER_RESET_INVERT != 0); /* assert RESET */
        linux_delay_ms_raw(SERIAL_FLASHER_RESET_HOLD_TIME_MS);
        set_gpio_line(p, p->reset_pin, SERIAL_FLASHER_RESET_INVERT == 0); /* deassert RESET */
        linux_delay_ms_raw(SERIAL_FLASHER_BOOT_HOLD_TIME_MS);
        set_gpio_line(p, p->boot_pin, SERIAL_FLASHER_BOOT_INVERT  == 0); /* deassert BOOT */
        tcflush(p->_serial, TCIFLUSH); /* discard ROM boot noise before connect */
        break;
#endif

    case LINUX_GPIO_DTR_RTS:
        if (p->_is_usb_jtag) {
            /*
             * esptool USBJTAGSerialReset — required for ESP32-C3/S3/C6/H2/P4
             * connected via their built-in USB JTAG Serial peripheral.
             *
             * The firmware latches BOOT at the moment RESET goes low, so BOOT must
             * be asserted BEFORE the reset pulse. After the pulse the lines
             * transition through (BOOT_low, RESET_low)=(1,1) → (BOOT_high, RESET_low)
             * to avoid the (0,0) glitch, then RESET is released.
             *
             * Step 1: idle
             * Step 2: assert BOOT (DTR)
             * Step 3: assert RESET (RTS) — chip enters reset with BOOT low
             * Step 4: release BOOT through (1,1) state
             * Step 5: release RESET — chip boots into bootloader
             */
            set_dtr_rts(p->_serial, DTR_BOOT_DEASSERT, RTS_RESET_DEASSERT); /* idle */
            linux_delay_ms_raw(SERIAL_FLASHER_RESET_HOLD_TIME_MS);
            set_dtr_rts(p->_serial, DTR_BOOT_ASSERT,   RTS_RESET_DEASSERT); /* assert BOOT */
            linux_delay_ms_raw(SERIAL_FLASHER_RESET_HOLD_TIME_MS);
            set_dtr_rts(p->_serial, DTR_BOOT_ASSERT,   RTS_RESET_ASSERT);   /* assert RESET */
            set_dtr_rts(p->_serial, DTR_BOOT_DEASSERT, RTS_RESET_ASSERT);   /* release BOOT via (1,1) */
            linux_delay_ms_raw(SERIAL_FLASHER_RESET_HOLD_TIME_MS);
            set_dtr_rts(p->_serial, DTR_BOOT_DEASSERT, RTS_RESET_DEASSERT); /* release RESET */
            wait_for_port_reopen(p, 3000);
        } else {
            /*
             * esptool UnixTightReset — both DTR and RTS are changed atomically
             * with a single TIOCMSET ioctl to avoid glitches that can occur when
             * the two lines are toggled by separate calls.
             *
             * Step 1: idle state
             * Step 2: through (1,1) — avoids (0,0)→(0,1) edge that can mis-trigger
             * Step 3: BOOT=HIGH, RESET=LOW  → chip held in reset, BOOT deasserted
             *         (sleep RESET_HOLD_TIME_MS)
             * Step 4: BOOT=LOW,  RESET=HIGH → RESET released while BOOT is low → bootloader
             *         (sleep BOOT_HOLD_TIME_MS)
             * Step 5: BOOT=HIGH, RESET=HIGH → release BOOT, chip running in bootloader
             */
            set_dtr_rts(p->_serial, DTR_BOOT_DEASSERT, RTS_RESET_DEASSERT);
            set_dtr_rts(p->_serial, DTR_BOOT_ASSERT,   RTS_RESET_ASSERT);
            set_dtr_rts(p->_serial, DTR_BOOT_DEASSERT, RTS_RESET_ASSERT);
            linux_delay_ms_raw(SERIAL_FLASHER_RESET_HOLD_TIME_MS);
            set_dtr_rts(p->_serial, DTR_BOOT_ASSERT,   RTS_RESET_DEASSERT);
            linux_delay_ms_raw(SERIAL_FLASHER_BOOT_HOLD_TIME_MS);
            set_dtr_rts(p->_serial, DTR_BOOT_DEASSERT, RTS_RESET_DEASSERT);
            tcflush(p->_serial, TCIFLUSH); /* discard ROM boot noise before connect */
        }
        break;

    default:
        break;
    }
}

/* ─── port ops vtable ────────────────────────────────────────────────────── */

const esp_loader_port_ops_t linux_uart_ops = {
    .init                     = linux_port_init,
    .deinit                   = linux_port_deinit,
    .enter_bootloader         = linux_enter_bootloader,
    .reset_target             = linux_reset_target,
    .start_timer              = linux_start_timer,
    .remaining_time           = linux_remaining_time,
    .delay_ms                 = linux_delay_ms,
    .log                      = loader_port_stdio_log,
    .log_hex                  = loader_port_stdio_log_hex,
    .change_transmission_rate = linux_change_rate,
    .write                    = linux_uart_write,
    .read                     = linux_uart_read,
};
