#include "wfr_lirc.h"

#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <syslog.h>
#include <sys/ioctl.h>
#include <linux/lirc.h>

int wfr_lirc_open(const char* device) {
    int fd = open(device, O_RDONLY);
    if(fd < 0) {
        syslog(LOG_ERR, "infrafid: failed to open %s: %s", device, strerror(errno));
        return -1;
    }

    /* Set receive mode to SCANCODE — kernel decodes RC-6 for us */
    unsigned int mode = LIRC_MODE_SCANCODE;
    if(ioctl(fd, LIRC_SET_REC_MODE, &mode) < 0) {
        syslog(LOG_ERR, "infrafid: LIRC_SET_REC_MODE SCANCODE failed: %s", strerror(errno));
        close(fd);
        return -1;
    }
    syslog(LOG_INFO, "infrafid: set receive mode to SCANCODE");

    syslog(LOG_INFO, "infrafid: opened %s (fd=%d)", device, fd);
    return fd;
}

void wfr_lirc_close(int fd) {
    if(fd >= 0) {
        close(fd);
    }
}

int wfr_lirc_read_scancode(int fd, uint8_t* address, uint8_t* command) {
    for(;;) {
        struct lirc_scancode sc;
        ssize_t n = read(fd, &sc, sizeof(sc));
        if(n != (ssize_t)sizeof(sc)) {
            if(n < 0 && errno != EINTR) {
                syslog(LOG_ERR, "infrafid: scancode read error: %s", strerror(errno));
            }
            return -1;
        }

        syslog(
            LOG_DEBUG,
            "infrafid: scancode proto=%u flags=0x%x scancode=0x%llx",
            (unsigned)sc.rc_proto,
            (unsigned)sc.flags,
            (unsigned long long)sc.scancode);

        /* Accept RC-6 Mode 0 or standard NEC — both encode as (address << 8 | command) */
        if(sc.rc_proto != RC_PROTO_RC6_0 && sc.rc_proto != RC_PROTO_NEC) {
            continue;
        }

        /* Scancode layout: bits [15:8] = address, bits [7:0] = command */
        *address = (uint8_t)((sc.scancode >> 8) & 0xFF);
        *command = (uint8_t)(sc.scancode & 0xFF);
        return 0;
    }
}
