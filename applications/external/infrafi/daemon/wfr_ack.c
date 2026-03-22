#include "wfr_ack.h"

#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <syslog.h>
#include <sys/ioctl.h>
#include <linux/lirc.h>
#include <time.h>

int wfr_ack_open(const char* device) {
    int fd = open(device, O_WRONLY);
    if(fd < 0) {
        syslog(LOG_ERR, "infrafid: failed to open %s for TX: %s", device, strerror(errno));
        return -1;
    }

    /* Set send mode to SCANCODE — kernel encodes RC-6 for us */
    unsigned int mode = LIRC_MODE_SCANCODE;
    if(ioctl(fd, LIRC_SET_SEND_MODE, &mode) < 0) {
        syslog(LOG_ERR, "infrafid: LIRC_SET_SEND_MODE SCANCODE failed: %s", strerror(errno));
        close(fd);
        return -1;
    }

    syslog(LOG_DEBUG, "infrafid: opened %s for IR TX (fd=%d)", device, fd);
    return fd;
}

void wfr_ack_close(int fd) {
    if(fd >= 0) {
        close(fd);
    }
}

/* Send one RC-6 scancode via LIRC */
static bool send_rc6(int fd, uint8_t address, uint8_t command) {
    struct lirc_scancode sc;
    memset(&sc, 0, sizeof(sc));
    sc.rc_proto = RC_PROTO_RC6_0;
    sc.scancode = ((uint32_t)address << 8) | command;

    ssize_t n = write(fd, &sc, sizeof(sc));
    if(n != (ssize_t)sizeof(sc)) {
        syslog(LOG_ERR, "infrafid: IR TX write failed: %s", strerror(errno));
        return false;
    }
    return true;
}

static void delay_ms(int ms) {
    struct timespec ts = {
        .tv_sec = ms / 1000,
        .tv_nsec = (ms % 1000) * 1000000L,
    };
    nanosleep(&ts, NULL);
}

bool wfr_ack_send(int fd, bool success, const char* ip_str) {
    if(fd < 0) return false;

    /* Build ACK payload */
    char payload[WFR_MAX_TOTAL_PAYLOAD + 1];
    size_t payload_len;

    if(success && ip_str) {
        payload_len = (size_t)snprintf(payload, sizeof(payload), "%s%s", WFR_ACK_PREFIX_OK, ip_str);
    } else {
        payload_len = (size_t)snprintf(payload, sizeof(payload), "%s", WFR_ACK_PREFIX_FAIL);
    }

    if(payload_len == 0 || payload_len > 255) {
        syslog(LOG_ERR, "infrafid: ACK payload too large (%zu bytes)", payload_len);
        return false;
    }

    const uint8_t* data = (const uint8_t*)payload;
    uint8_t crc = wfr_crc8(data, payload_len);

    syslog(LOG_INFO, "infrafid: sending ACK via IR: %s", payload);

    /* Send using same framing as credentials: START → DATA × N → END */
    uint8_t pass = 0;

    /* START */
    if(!send_rc6(fd, WFR_RC6_MAGIC | WFR_RC6_TYPE_START | pass, (uint8_t)payload_len)) {
        return false;
    }
    delay_ms(WFR_RC6_INTER_MSG_MS);

    /* DATA — one byte per message */
    for(size_t i = 0; i < payload_len; i++) {
        if(!send_rc6(fd, WFR_RC6_MAGIC | WFR_RC6_TYPE_DATA | pass, data[i])) {
            return false;
        }
        delay_ms(WFR_RC6_INTER_MSG_MS);
    }

    /* END — CRC-8 */
    if(!send_rc6(fd, WFR_RC6_MAGIC | WFR_RC6_TYPE_END | pass, crc)) {
        return false;
    }

    syslog(LOG_INFO, "infrafid: ACK transmitted (%zu bytes)", payload_len);
    return true;
}
