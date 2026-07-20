/*
  Asynchronous TCP library for Espressif MCUs

  Copyright (c) 2016 Hristo Gochkov. All rights reserved.
  This file is part of the esp8266 core for Arduino environment.

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef ASYNCTCP_H_
#define ASYNCTCP_H_

#define ASYNCTCP_VERSION          "3.3.2"
#define ASYNCTCP_VERSION_MAJOR    3
#define ASYNCTCP_VERSION_MINOR    3
#define ASYNCTCP_VERSION_REVISION 2
#define ASYNCTCP_FORK_ESP32Async

#include "IPAddress.h"
#if ESP_IDF_VERSION_MAJOR < 5
  #include "IPv6Address.h"
#endif
#include "lwip/ip6_addr.h"
#include "lwip/ip_addr.h"
#include <functional>

#ifndef LIBRETINY
  #include "sdkconfig.h"
extern "C" {
  #include "freertos/semphr.h"
  #include "lwip/pbuf.h"
}
#else
extern "C" {
  #include <lwip/pbuf.h>
  #include <semphr.h>
}
  #define CONFIG_ASYNC_TCP_RUNNING_CORE -1 // any available core
#endif

// If core is not defined, then we are running in Arduino or PIO
#ifndef CONFIG_ASYNC_TCP_RUNNING_CORE
  #define CONFIG_ASYNC_TCP_RUNNING_CORE -1 // any available core
#endif

// guard AsyncTCP task with watchdog
#ifndef CONFIG_ASYNC_TCP_USE_WDT
  #define CONFIG_ASYNC_TCP_USE_WDT 1
#endif

#ifndef CONFIG_ASYNC_TCP_STACK_SIZE
  #define CONFIG_ASYNC_TCP_STACK_SIZE 8192 * 2
#endif

#ifndef CONFIG_ASYNC_TCP_PRIORITY
  #define CONFIG_ASYNC_TCP_PRIORITY 10
#endif

#ifndef CONFIG_ASYNC_TCP_QUEUE_SIZE
  #define CONFIG_ASYNC_TCP_QUEUE_SIZE 64
#endif

#ifndef CONFIG_ASYNC_TCP_MAX_ACK_TIME
  #define CONFIG_ASYNC_TCP_MAX_ACK_TIME 5000
#endif

class AsyncClient;

#define ASYNC_WRITE_FLAG_COPY 0x01 // will allocate new buffer to hold the data while sending (else will hold reference to the data given)
#define ASYNC_WRITE_FLAG_MORE 0x02 // will not send PSH flag, meaning that there should be more data to be sent before the application should react.

typedef std::function<void(void*, AsyncClient*)> AcConnectHandler;
typedef std::function<void(void*, AsyncClient*, size_t len, uint32_t time)> AcAckHandler;
typedef std::function<void(void*, AsyncClient*, int8_t error)> AcErrorHandler;
typedef std::function<void(void*, AsyncClient*, void* data, size_t len)> AcDataHandler;
typedef std::function<void(void*, AsyncClient*, struct pbuf* pb)> AcPacketHandler;
typedef std::function<void(void*, AsyncClient*, uint32_t time)> AcTimeoutHandler;

struct tcp_pcb;
struct ip_addr;

class AsyncClient {
  public:
    AsyncClient(tcp_pcb* pcb = 0);
    ~AsyncClient();

    AsyncClient& operator=(const AsyncClient& other);
    AsyncClient& operator+=(const AsyncClient& other);

    bool operator==(const AsyncClient& other);

    bool operator!=(const AsyncClient& other) {
      return !(*this == other);
    }
    bool connect(const IPAddress& ip, uint16_t port);
#if ESP_IDF_VERSION_MAJOR < 5
    bool connect(const IPv6Address& ip, uint16_t port);
#endif
    bool connect(const char* host, uint16_t port);
    /**
     * @brief close connection
     *
     * @param now - ignored
     */
    void close(bool now = false);
    // same as close()
    void stop() { close(false); };
    int8_t abort();
    bool free();

    // ack is not pending
    bool canSend();
    // TCP buffer space available
    size_t space();

    /**
     * @brief add data to be send (but do not send yet)
     * @note add() would call lwip's tcp_write()
        By default apiflags=ASYNC_WRITE_FLAG_COPY
        You could try to use apiflags with this flag unset to pass data by reference and avoid copy to socket buffer,
        but looks like it does not work for Arduino's lwip in ESP32/IDF at least
        it is enforced in https://github.com/espressif/esp-lwip/blob/0606eed9d8b98a797514fdf6eabb4daf1c8c8cd9/src/core/tcp_out.c#L422C5-L422C30
        if LWIP_NETIF_TX_SINGLE_PBUF is set, and it is set indeed in IDF
        https://github.com/espressif/esp-idf/blob/a0f798cfc4bbd624aab52b2c194d219e242d80c1/components/lwip/port/include/lwipopts.h#L744
     *
     * @param data
     * @param size
     * @param apiflags
     * @return size_t amount of data that has been copied
     */
    size_t add(const char* data, size_t size, uint8_t apiflags = ASYNC_WRITE_FLAG_COPY);

    /**
     * @brief send data previously add()'ed
     *
     * @return true on success
     * @return false on error
     */
    bool send();

    /**
     * @brief add and enqueue data for sending
     * @note it is same as add() + send()
     * @note only make sense when canSend() == true
     *
     * @param data
     * @param size
     * @param apiflags
     * @return size_t
     */
    size_t write(const char* data, size_t size, uint8_t apiflags = ASYNC_WRITE_FLAG_COPY);

    /**
     * @brief add and enque data for sending
     * @note treats data as null-terminated string
     *
     * @param data
     * @return size_t
     */
    size_t write(const char* data) { return data == NULL ? 0 : write(data, strlen(data)); };

    uint8_t state();
    bool connecting();
    bool connected();
    bool disconnecting();
    bool disconnected();

    // disconnected or disconnecting
    bool freeable();

    uint16_t getMss();

    uint32_t getRxTimeout();
    // no RX data timeout for the connection in seconds
    void setRxTimeout(uint32_t timeout);

    uint32_t getAckTimeout();
    // no ACK timeout for the last sent packet in milliseconds
    void setAckTimeout(uint32_t timeout);

    void setNoDelay(bool nodelay);
    bool getNoDelay();

    void setKeepAlive(uint32_t ms, uint8_t cnt);

    uint32_t getRemoteAddress();
    uint16_t getRemotePort();
    uint32_t getLocalAddress();
    uint16_t getLocalPort();
#if LWIP_IPV6
    ip6_addr_t getRemoteAddress6();
    ip6_addr_t getLocalAddress6();
  #if ESP_IDF_VERSION_MAJOR < 5
    IPv6Address remoteIP6();
    IPv6Address localIP6();
  #else
    IPAddress remoteIP6();
    IPAddress localIP6();
  #endif
#endif

    // compatibility
    IPAddress remoteIP();
    uint16_t remotePort();
    IPAddress localIP();
    uint16_t localPort();

    // set callback - on successful connect
    void onConnect(AcConnectHandler cb, void* arg = 0);
    // set callback - disconnected
    void onDisconnect(AcConnectHandler cb, void* arg = 0);
    // set callback - ack received
    void onAck(AcAckHandler cb, void* arg = 0);
    // set callback - unsuccessful connect or error
    void onError(AcErrorHandler cb, void* arg = 0);
    // set callback - data received (called if onPacket is not used)
    void onData(AcDataHandler cb, void* arg = 0);
    // set callback - data received
    void onPacket(AcPacketHandler cb, void* arg = 0);
    // set callback - ack timeout
    void onTimeout(AcTimeoutHandler cb, void* arg = 0);
    // set callback - every 125ms when connected
    void onPoll(AcConnectHandler cb, void* arg = 0);

    // ack pbuf from onPacket
    void ackPacket(struct pbuf* pb);
    // ack data that you have not acked using the method below
    size_t ack(size_t len);
    // will not ack the current packet. Call from onData
    void ackLater() { _ack_pcb = false; }

    static const char* errorToString(int8_t error);
    const char* stateToString();

    // internal callbacks - Do NOT call any of the functions below in user code!
    static int8_t _s_poll(void* arg, struct tcp_pcb* tpcb);
    static int8_t _s_recv(void* arg, struct tcp_pcb* tpcb, struct pbuf* pb, int8_t err);
    static int8_t _s_fin(void* arg, struct tcp_pcb* tpcb, int8_t err);
    static int8_t _s_lwip_fin(void* arg, struct tcp_pcb* tpcb, int8_t err);
    static void _s_error(void* arg, int8_t err);
    static int8_t _s_sent(void* arg, struct tcp_pcb* tpcb, uint16_t len);
    static int8_t _s_connected(void* arg, struct tcp_pcb* tpcb, int8_t err);
    static void _s_dns_found(const char* name, struct ip_addr* ipaddr, void* arg);

    int8_t _recv(tcp_pcb* pcb, pbuf* pb, int8_t err);
    tcp_pcb* pcb() { return _pcb; }

  protected:
    bool _connect(ip_addr_t addr, uint16_t port);

    tcp_pcb* _pcb;
    int8_t _closed_slot;

    AcConnectHandler _connect_cb;
    void* _connect_cb_arg;
    AcConnectHandler _discard_cb;
    void* _discard_cb_arg;
    AcAckHandler _sent_cb;
    void* _sent_cb_arg;
    AcErrorHandler _error_cb;
    void* _error_cb_arg;
    AcDataHandler _recv_cb;
    void* _recv_cb_arg;
    AcPacketHandler _pb_cb;
    void* _pb_cb_arg;
    AcTimeoutHandler _timeout_cb;
    void* _timeout_cb_arg;
    AcConnectHandler _poll_cb;
    void* _poll_cb_arg;

    bool _ack_pcb;
    uint32_t _tx_last_packet;
    uint32_t _rx_ack_len;
    uint32_t _rx_last_packet;
    uint32_t _rx_timeout;
    uint32_t _rx_last_ack;
    uint32_t _ack_timeout;
    uint16_t _connect_port;

    int8_t _close();
    void _free_closed_slot();
    bool _allocate_closed_slot();
    int8_t _connected(tcp_pcb* pcb, int8_t err);
    void _error(int8_t err);
    int8_t _poll(tcp_pcb* pcb);
    int8_t _sent(tcp_pcb* pcb, uint16_t len);
    int8_t _fin(tcp_pcb* pcb, int8_t err);
    int8_t _lwip_fin(tcp_pcb* pcb, int8_t err);
    void _dns_found(struct ip_addr* ipaddr);

  public:
    AsyncClient* prev;
    AsyncClient* next;
};

class AsyncServer {
  public:
    AsyncServer(IPAddress addr, uint16_t port);
#if ESP_IDF_VERSION_MAJOR < 5
    AsyncServer(IPv6Address addr, uint16_t port);
#endif
    AsyncServer(uint16_t port);
    ~AsyncServer();
    void onClient(AcConnectHandler cb, void* arg);
    void begin();
    void end();
    void setNoDelay(bool nodelay);
    bool getNoDelay();
    uint8_t status();

    // Do not use any of the functions below!
    static int8_t _s_accept(void* arg, tcp_pcb* newpcb, int8_t err);
    static int8_t _s_accepted(void* arg, AsyncClient* client);

  protected:
    uint16_t _port;
    bool _bind4 = false;
    bool _bind6 = false;
    IPAddress _addr;
#if ESP_IDF_VERSION_MAJOR < 5
    IPv6Address _addr6;
#endif
    bool _noDelay;
    tcp_pcb* _pcb;
    AcConnectHandler _connect_cb;
    void* _connect_cb_arg;

    int8_t _accept(tcp_pcb* newpcb, int8_t err);
    int8_t _accepted(AsyncClient* client);
};

#endif /* ASYNCTCP_H_ */
