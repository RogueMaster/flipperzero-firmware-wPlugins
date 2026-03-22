#include "wfr_lirc.h"
#include "wfr_decode.h"
#include "wfr_network.h"
#include "wfr_ack.h"
#include "../flipper/protocol/version.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <syslog.h>
#include <stdbool.h>
#include <getopt.h>

static volatile bool running = true;

static void signal_handler(int sig) {
    (void)sig;
    running = false;
}

static void print_usage(const char* prog) {
    fprintf(stderr, "Usage: %s [options]\n", prog);
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  -d, --device PATH       LIRC device for RX (default: /dev/lirc0)\n");
    fprintf(stderr, "  -a, --ack-device PATH   LIRC device for ACK TX (default: same as --device)\n");
    fprintf(stderr, "  -f, --foreground        Run in foreground (don't daemonize)\n");
    fprintf(stderr, "  -v, --verbose           Verbose logging\n");
    fprintf(stderr, "  -V, --version           Show version\n");
    fprintf(stderr, "  -h, --help              Show this help\n");
}

static int ack_fd = -1;

static void send_ack(bool success) {
    if(ack_fd < 0) return;

    char ip[32] = {0};
    if(success) {
        wfr_net_get_ip(ip, sizeof(ip));
    }

    /* Brief delay to let Flipper switch to receive mode */
    sleep(1);

    wfr_ack_send(ack_fd, success, ip[0] ? ip : NULL);
}

static void handle_credentials(const char* payload) {
    syslog(LOG_INFO, "received credentials: %s", payload);

    WfrWifiCreds creds;
    if(!wfr_parse_wifi_string(payload, &creds)) {
        syslog(LOG_ERR, "failed to parse WiFi string: %s", payload);
        return;
    }

    syslog(
        LOG_INFO,
        "parsed: SSID=%s, security=%d, hidden=%d",
        creds.ssid,
        creds.security,
        creds.hidden);

    /* Save current connection for rollback */
    char prev_ssid[WFR_SSID_MAX_LEN + 1] = {0};
    wfr_net_get_current(prev_ssid, sizeof(prev_ssid));

    if(prev_ssid[0]) {
        syslog(LOG_INFO, "current connection: %s (saved for rollback)", prev_ssid);
    }

    /* Attempt connection */
    if(wfr_net_connect(&creds)) {
        syslog(LOG_INFO, "successfully connected to %s", creds.ssid);
        send_ack(true);
    } else {
        syslog(LOG_ERR, "failed to connect to %s", creds.ssid);
        send_ack(false);
        if(prev_ssid[0]) {
            syslog(LOG_INFO, "attempting rollback to %s", prev_ssid);
            if(wfr_net_rollback(prev_ssid)) {
                syslog(LOG_INFO, "rollback successful");
            } else {
                syslog(LOG_ERR, "rollback failed — manual intervention may be needed");
            }
        }
    }
}

int main(int argc, char* argv[]) {
    const char* device = "/dev/lirc0";
    const char* ack_device = NULL;
    bool foreground = false;
    int log_level = LOG_INFO;

    static struct option long_opts[] = {
        {"device", required_argument, NULL, 'd'},
        {"ack-device", required_argument, NULL, 'a'},
        {"foreground", no_argument, NULL, 'f'},
        {"verbose", no_argument, NULL, 'v'},
        {"version", no_argument, NULL, 'V'},
        {"help", no_argument, NULL, 'h'},
        {NULL, 0, NULL, 0},
    };

    int opt;
    while((opt = getopt_long(argc, argv, "d:a:fvVh", long_opts, NULL)) != -1) {
        switch(opt) {
        case 'd':
            device = optarg;
            break;
        case 'a':
            ack_device = optarg;
            break;
        case 'f':
            foreground = true;
            break;
        case 'v':
            log_level = LOG_DEBUG;
            break;
        case 'V':
            printf("infrafid %s\n", INFRAFI_VERSION);
            return 0;
        case 'h':
            print_usage(argv[0]);
            return 0;
        default:
            print_usage(argv[0]);
            return 1;
        }
    }

    openlog("infrafid", LOG_PID | (foreground ? LOG_PERROR : 0), LOG_DAEMON);
    setlogmask(LOG_UPTO(log_level));

    /* Default ACK device to same as RX device */
    if(!ack_device) ack_device = device;

    syslog(LOG_INFO, "infrafid %s starting (rx=%s, tx=%s)", INFRAFI_VERSION, device, ack_device);

    struct sigaction sa = {0};
    sa.sa_handler = signal_handler;
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);

    int lirc_fd = wfr_lirc_open(device);
    if(lirc_fd < 0) {
        syslog(LOG_ERR, "failed to open LIRC device %s", device);
        closelog();
        return 1;
    }

    /* Open LIRC for ACK transmission (non-fatal if it fails) */
    ack_fd = wfr_ack_open(ack_device);
    if(ack_fd < 0) {
        syslog(LOG_WARNING, "infrafid: IR TX not available on %s — ACK disabled", ack_device);
    }

    WfrDecoder decoder;
    wfr_decode_init(&decoder);
    char payload[WFR_MAX_TOTAL_PAYLOAD + 1];

    syslog(LOG_INFO, "infrafid ready, listening for IR transmissions");

    while(running) {
        uint8_t address, command;

        if(wfr_lirc_read_scancode(lirc_fd, &address, &command) < 0) {
            if(!running) break;
            continue;
        }

        int result = wfr_decode_feed_scancode(
            &decoder, address, command, payload, sizeof(payload));
        if(result > 0) {
            handle_credentials(payload);
        }
    }

    syslog(LOG_INFO, "infrafid shutting down");
    wfr_ack_close(ack_fd);
    wfr_lirc_close(lirc_fd);
    closelog();
    return 0;
}
