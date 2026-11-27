/*
 * page_send.h — write a response body to a non-blocking socket, giving up on
 * a client that stops taking data (no Arduino dependencies)
 *
 * The dashboard page goes out on the loop task, which also drains CAN.
 * WiFiClient::write() waits in select() for a client that takes nothing, up to
 * WIFI_CLIENT_MAX_WRITE_RETRY (10) times WIFI_CLIENT_SELECT_TIMEOUT_US (1 s)
 * per call, both fixed in the Arduino core: a phone that drops off the Wi-Fi
 * mid-page held the loop for about 10 s. page_send() drops a client once no
 * byte has gone out for a short while: a client that has gone stalls with no
 * progress at all, while a slow one (in-car Wi-Fi, a phone in power save)
 * still takes a buffer's worth every round trip and gets the whole page; a
 * cut page is a dashboard that does not load. A cap on the whole body is the
 * backstop for a client that only ever takes a trickle. The socket calls and
 * the clock are passed in, so both limits are unit tested on the host
 * (test/test_page_send.cpp); web_dashboard.cpp passes send(MSG_DONTWAIT),
 * select() and millis().
 */
#ifndef PAGE_SEND_H
#define PAGE_SEND_H

#include <stddef.h>
#include <stdint.h>

// send(p, n): bytes the socket took (> 0), 0 when its buffer is full, < 0 when
//             the connection failed.
// wait(ms):   return once the socket can take more or ms have passed; false
//             when the socket failed.
// now():      milliseconds.
// Returns the bytes sent: all of len, or fewer when the connection failed,
// stall_ms passed without a byte going out, or the body took max_ms. A wait
// never runs past either limit.
template <typename Send, typename Wait, typename Now>
size_t page_send(const uint8_t *p, size_t len, uint32_t stall_ms, uint32_t max_ms,
                 Send send, Wait wait, Now now) {
    uint32_t start = now();
    uint32_t last = start;   // last time a byte went out
    size_t done = 0;
    while (done < len) {
        long n = send(p + done, len - done);
        if (n > 0) {
            done += (size_t)n;
            last = now();
            continue;
        }
        if (n < 0) break;
        uint32_t t = now();
        uint32_t idle = t - last, spent = t - start;
        if (idle >= stall_ms || spent >= max_ms) break;
        uint32_t ms = stall_ms - idle;
        if (max_ms - spent < ms) ms = max_ms - spent;
        if (!wait(ms)) break;
    }
    return done;
}

#endif // PAGE_SEND_H
