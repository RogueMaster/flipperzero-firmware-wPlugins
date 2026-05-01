#include "wfr_evdev.h"

#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <syslog.h>
#include <sys/ioctl.h>
#include <linux/input.h>

/* Reverse bit order within a byte.
 * The FAB4 IR receiver delivers NEC scancodes with bit-reversed bytes
 * relative to the standard NEC encoding. */
static uint8_t reverse_bits(uint8_t b) {
    b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
    b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
    b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
    return b;
}

int wfr_evdev_open(const char* device) {
    int fd = open(device, O_RDONLY);
    if(fd < 0) {
        syslog(LOG_ERR, "infrafid: failed to open %s: %s", device, strerror(errno));
        return -1;
    }

    /* Verify the device supports EV_MSC */
    enum {
        EVBITS_WORDS = (EV_MAX / (8 * sizeof(unsigned long))) + 1
    };
    unsigned long evbits[EVBITS_WORDS];
    memset(evbits, 0, sizeof(evbits));
    if(ioctl(fd, EVIOCGBIT(0, sizeof(evbits)), evbits) < 0) {
        syslog(LOG_ERR, "infrafid: EVIOCGBIT failed on %s: %s", device, strerror(errno));
        close(fd);
        return -1;
    }

    if(!(evbits[EV_MSC / (8 * sizeof(unsigned long))] &
         (1UL << (EV_MSC % (8 * sizeof(unsigned long)))))) {
        syslog(LOG_ERR, "infrafid: %s does not support EV_MSC events", device);
        close(fd);
        return -1;
    }

    syslog(LOG_INFO, "infrafid: opened evdev %s (fd=%d)", device, fd);
    return fd;
}

void wfr_evdev_close(int fd) {
    if(fd >= 0) {
        close(fd);
    }
}

int wfr_evdev_read_scancode(int fd, uint8_t* address, uint8_t* command) {
    for(;;) {
        struct input_event ev;
        ssize_t n = read(fd, &ev, sizeof(ev));
        if(n != (ssize_t)sizeof(ev)) {
            if(n < 0 && errno != EINTR) {
                syslog(LOG_ERR, "infrafid: evdev read error: %s", strerror(errno));
            }
            return -1;
        }

        /* Only process MSC_RAW (code 3) events */
        if(ev.type != EV_MSC || ev.code != MSC_RAW) {
            continue;
        }

        uint32_t raw = (uint32_t)ev.value;

        syslog(LOG_DEBUG, "infrafid: evdev raw=0x%08x", raw);

        /* Bit-reverse each byte to recover the NEC frame.
         * NOTE: This assumes FAB4-style MSC_RAW bit ordering.
         * Other receivers may already report bytes in standard NEC order.
         *
         * byte 0 = address, byte 1 = ~address, byte 2 = command, byte 3 = ~command */
        uint8_t addr = reverse_bits((raw >> 24) & 0xFF);
        uint8_t addr_inv = reverse_bits((raw >> 16) & 0xFF);
        uint8_t cmd = reverse_bits((raw >> 8) & 0xFF);
        uint8_t cmd_inv = reverse_bits(raw & 0xFF);

        /* Validate NEC inverse bytes */
        if((addr ^ addr_inv) != 0xFF) {
            syslog(
                LOG_DEBUG,
                "infrafid: NEC address validation failed (0x%02x ^ 0x%02x != 0xFF)",
                addr,
                addr_inv);
            continue;
        }
        if((cmd ^ cmd_inv) != 0xFF) {
            syslog(
                LOG_DEBUG,
                "infrafid: NEC command validation failed (0x%02x ^ 0x%02x != 0xFF)",
                cmd,
                cmd_inv);
            continue;
        }

        syslog(LOG_DEBUG, "infrafid: NEC decoded addr=0x%02x cmd=0x%02x", addr, cmd);

        *address = addr;
        *command = cmd;
        return 0;
    }
}
