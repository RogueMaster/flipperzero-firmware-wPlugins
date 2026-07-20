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

#include "Arduino.h"

#include "AsyncTCP.h"

extern "C" {
#include "lwip/dns.h"
#include "lwip/err.h"
#include "lwip/inet.h"
#include "lwip/opt.h"
#include "lwip/tcp.h"
}

#if CONFIG_ASYNC_TCP_USE_WDT
  #include "esp_task_wdt.h"
#endif

// Required for:
// https://github.com/espressif/arduino-esp32/blob/3.0.3/libraries/Network/src/NetworkInterface.cpp#L37-L47
#if ESP_IDF_VERSION_MAJOR >= 5
  #include <NetworkInterface.h>
#endif

#define TAG "AsyncTCP"

// https://github.com/espressif/arduino-esp32/issues/10526
#ifdef CONFIG_LWIP_TCPIP_CORE_LOCKING
  #define TCP_MUTEX_LOCK()                                \
    if (!sys_thread_tcpip(LWIP_CORE_LOCK_QUERY_HOLDER)) { \
      LOCK_TCPIP_CORE();                                  \
    }

  #define TCP_MUTEX_UNLOCK()                             \
    if (sys_thread_tcpip(LWIP_CORE_LOCK_QUERY_HOLDER)) { \
      UNLOCK_TCPIP_CORE();                               \
    }
#else // CONFIG_LWIP_TCPIP_CORE_LOCKING
  #define TCP_MUTEX_LOCK()
  #define TCP_MUTEX_UNLOCK()
#endif // CONFIG_LWIP_TCPIP_CORE_LOCKING

#define INVALID_CLOSED_SLOT -1

/*
  TCP poll interval is specified in terms of the TCP coarse timer interval, which is called twice a second
  https://github.com/espressif/esp-lwip/blob/2acf959a2bb559313cd2bf9306c24612ba3d0e19/src/core/tcp.c#L1895
*/
#define CONFIG_ASYNC_TCP_POLL_TIMER 1

/*
 * TCP/IP Event Task
 * */

typedef enum {
  LWIP_TCP_SENT,
  LWIP_TCP_RECV,
  LWIP_TCP_FIN,
  LWIP_TCP_ERROR,
  LWIP_TCP_POLL,
  LWIP_TCP_CLEAR,
  LWIP_TCP_ACCEPT,
  LWIP_TCP_CONNECTED,
  LWIP_TCP_DNS
} lwip_event_t;

typedef struct {
    lwip_event_t event;
    void* arg;
    union {
        struct {
            tcp_pcb* pcb;
            int8_t err;
        } connected;
        struct {
            int8_t err;
        } error;
        struct {
            tcp_pcb* pcb;
            uint16_t len;
        } sent;
        struct {
            tcp_pcb* pcb;
            pbuf* pb;
            int8_t err;
        } recv;
        struct {
            tcp_pcb* pcb;
            int8_t err;
        } fin;
        struct {
            tcp_pcb* pcb;
        } poll;
        struct {
            AsyncClient* client;
        } accept;
        struct {
            const char* name;
            ip_addr_t addr;
        } dns;
    };
} lwip_event_packet_t;

static QueueHandle_t _async_queue;
static TaskHandle_t _async_service_task_handle = NULL;

SemaphoreHandle_t _slots_lock;
const int _number_of_closed_slots = CONFIG_LWIP_MAX_ACTIVE_TCP;
static uint32_t _closed_slots[_number_of_closed_slots];
static uint32_t _closed_index = []() {
  _slots_lock = xSemaphoreCreateBinary();
  xSemaphoreGive(_slots_lock);
  for (int i = 0; i < _number_of_closed_slots; ++i) {
    _closed_slots[i] = 1;
  }
  return 1;
}();

static inline bool _init_async_event_queue() {
  if (!_async_queue) {
    _async_queue = xQueueCreate(CONFIG_ASYNC_TCP_QUEUE_SIZE, sizeof(lwip_event_packet_t*));
    if (!_async_queue) {
      return false;
    }
  }
  return true;
}

static inline bool _send_async_event(lwip_event_packet_t** e, TickType_t wait = portMAX_DELAY) {
  return _async_queue && xQueueSend(_async_queue, e, wait) == pdPASS;
}

static inline bool _prepend_async_event(lwip_event_packet_t** e, TickType_t wait = portMAX_DELAY) {
  return _async_queue && xQueueSendToFront(_async_queue, e, wait) == pdPASS;
}

static inline bool _get_async_event(lwip_event_packet_t** e) {
  if (!_async_queue) {
    return false;
  }

#if CONFIG_ASYNC_TCP_USE_WDT
  // need to return periodically to feed the dog
  if (xQueueReceive(_async_queue, e, pdMS_TO_TICKS(1000)) != pdPASS)
    return false;
#else
  if (xQueueReceive(_async_queue, e, portMAX_DELAY) != pdPASS)
    return false;
#endif

  if ((*e)->event != LWIP_TCP_POLL)
    return true;

  /*
    Let's try to coalesce two (or more) consecutive poll events into one
    this usually happens with poor implemented user-callbacks that are runs too long and makes poll events to stack in the queue
    if consecutive user callback for a same connection runs longer that poll time then it will fill the queue with events until it deadlocks.
    This is a workaround to mitigate such poor designs and won't let other events/connections to starve the task time.
    It won't be effective if user would run multiple simultaneous long running callbacks due to message interleaving.
    todo: implement some kind of fair dequeing or (better) simply punish user for a bad designed callbacks by resetting hog connections
  */
  lwip_event_packet_t* next_pkt = NULL;
  while (xQueuePeek(_async_queue, &next_pkt, 0) == pdPASS) {
    if (next_pkt->arg == (*e)->arg && next_pkt->event == LWIP_TCP_POLL) {
      if (xQueueReceive(_async_queue, &next_pkt, 0) == pdPASS) {
        free(next_pkt);
        next_pkt = NULL;
        log_d("coalescing polls, network congestion or async callbacks might be too slow!");
        continue;
      }
    }

    // quit while loop if next event can't be discarded
    break;
  }

  /*
    now we have to decide if to proceed with poll callback handler or discard it?
    poor designed apps using asynctcp without proper dataflow control could flood the queue with interleaved pool/ack events.
    I.e. on each poll app would try to generate more data to send, which in turn results in additional ack event triggering chain effect
    for long connections. Or poll callback could take long time starving other connections. Anyway our goal is to keep the queue length
    grows under control (if possible) and poll events are the safest to discard.
    Let's discard poll events processing using linear-increasing probability curve when queue size grows over 3/4
    Poll events are periodic and connection could get another chance next time
  */
  if (uxQueueMessagesWaiting(_async_queue) > (rand() % CONFIG_ASYNC_TCP_QUEUE_SIZE / 4 + CONFIG_ASYNC_TCP_QUEUE_SIZE * 3 / 4)) {
    free(*e);
    *e = NULL;
    log_d("discarding poll due to queue congestion");
    // evict next event from a queue
    return _get_async_event(e);
  }

  // last resort return
  return true;
}

static bool _remove_events_with_arg(void* arg) {
  if (!_async_queue) {
    return false;
  }

  lwip_event_packet_t* first_packet = NULL;
  lwip_event_packet_t* packet = NULL;

  // figure out which is the first non-matching packet so we can keep the order
  while (!first_packet) {
    if (xQueueReceive(_async_queue, &first_packet, 0) != pdPASS) {
      return false;
    }
    // discard packet if matching
    if ((int)first_packet->arg == (int)arg) {
      free(first_packet);
      first_packet = NULL;
    } else if (xQueueSend(_async_queue, &first_packet, 0) != pdPASS) {
      // try to return first packet to the back of the queue
      // we can't wait here if queue is full, because this call has been done from the only consumer task of this queue
      // otherwise it would deadlock, we have to discard the event
      free(first_packet);
      first_packet = NULL;
      return false;
    }
  }

  while (xQueuePeek(_async_queue, &packet, 0) == pdPASS && packet != first_packet) {
    if (xQueueReceive(_async_queue, &packet, 0) != pdPASS) {
      return false;
    }
    if ((int)packet->arg == (int)arg) {
      // remove matching event
      free(packet);
      packet = NULL;
      // otherwise try to requeue it
    } else if (xQueueSend(_async_queue, &packet, 0) != pdPASS) {
      // we can't wait here if queue is full, because this call has been done from the only consumer task of this queue
      // otherwise it would deadlock, we have to discard the event
      free(packet);
      packet = NULL;
      return false;
    }
  }
  return true;
}

static void _handle_async_event(lwip_event_packet_t* e) {
  if (e->arg == NULL) {
    // do nothing when arg is NULL
    // ets_printf("event arg == NULL: 0x%08x\n", e->recv.pcb);
  } else if (e->event == LWIP_TCP_CLEAR) {
    _remove_events_with_arg(e->arg);
  } else if (e->event == LWIP_TCP_RECV) {
    // ets_printf("-R: 0x%08x\n", e->recv.pcb);
    AsyncClient::_s_recv(e->arg, e->recv.pcb, e->recv.pb, e->recv.err);
  } else if (e->event == LWIP_TCP_FIN) {
    // ets_printf("-F: 0x%08x\n", e->fin.pcb);
    AsyncClient::_s_fin(e->arg, e->fin.pcb, e->fin.err);
  } else if (e->event == LWIP_TCP_SENT) {
    // ets_printf("-S: 0x%08x\n", e->sent.pcb);
    AsyncClient::_s_sent(e->arg, e->sent.pcb, e->sent.len);
  } else if (e->event == LWIP_TCP_POLL) {
    // ets_printf("-P: 0x%08x\n", e->poll.pcb);
    AsyncClient::_s_poll(e->arg, e->poll.pcb);
  } else if (e->event == LWIP_TCP_ERROR) {
    // ets_printf("-E: 0x%08x %d\n", e->arg, e->error.err);
    AsyncClient::_s_error(e->arg, e->error.err);
  } else if (e->event == LWIP_TCP_CONNECTED) {
    // ets_printf("C: 0x%08x 0x%08x %d\n", e->arg, e->connected.pcb, e->connected.err);
    AsyncClient::_s_connected(e->arg, e->connected.pcb, e->connected.err);
  } else if (e->event == LWIP_TCP_ACCEPT) {
    // ets_printf("A: 0x%08x 0x%08x\n", e->arg, e->accept.client);
    AsyncServer::_s_accepted(e->arg, e->accept.client);
  } else if (e->event == LWIP_TCP_DNS) {
    // ets_printf("D: 0x%08x %s = %s\n", e->arg, e->dns.name, ipaddr_ntoa(&e->dns.addr));
    AsyncClient::_s_dns_found(e->dns.name, &e->dns.addr, e->arg);
  }
  free((void*)(e));
}

static void _async_service_task(void* pvParameters) {
#if CONFIG_ASYNC_TCP_USE_WDT
  if (esp_task_wdt_add(NULL) != ESP_OK) {
    log_w("Failed to add async task to WDT");
  }
#endif
  lwip_event_packet_t* packet = NULL;
  for (;;) {
    if (_get_async_event(&packet)) {
      _handle_async_event(packet);
    }
#if CONFIG_ASYNC_TCP_USE_WDT
    esp_task_wdt_reset();
#endif
  }
#if CONFIG_ASYNC_TCP_USE_WDT
  esp_task_wdt_delete(NULL);
#endif
  vTaskDelete(NULL);
  _async_service_task_handle = NULL;
}
/*
static void _stop_async_task(){
    if(_async_service_task_handle){
        vTaskDelete(_async_service_task_handle);
        _async_service_task_handle = NULL;
    }
}
*/

static bool customTaskCreateUniversal(
  TaskFunction_t pxTaskCode,
  const char* const pcName,
  const uint32_t usStackDepth,
  void* const pvParameters,
  UBaseType_t uxPriority,
  TaskHandle_t* const pxCreatedTask,
  const BaseType_t xCoreID) {
#ifndef CONFIG_FREERTOS_UNICORE
  if (xCoreID >= 0 && xCoreID < 2) {
    return xTaskCreatePinnedToCore(pxTaskCode, pcName, usStackDepth, pvParameters, uxPriority, pxCreatedTask, xCoreID);
  } else {
#endif
    return xTaskCreate(pxTaskCode, pcName, usStackDepth, pvParameters, uxPriority, pxCreatedTask);
#ifndef CONFIG_FREERTOS_UNICORE
  }
#endif
}

static bool _start_async_task() {
  if (!_init_async_event_queue()) {
    return false;
  }
  if (!_async_service_task_handle) {
    customTaskCreateUniversal(_async_service_task, "async_tcp", CONFIG_ASYNC_TCP_STACK_SIZE, NULL, CONFIG_ASYNC_TCP_PRIORITY, &_async_service_task_handle, CONFIG_ASYNC_TCP_RUNNING_CORE);
    if (!_async_service_task_handle) {
      return false;
    }
  }
  return true;
}

/*
 * LwIP Callbacks
 * */

static int8_t _tcp_clear_events(void* arg) {
  lwip_event_packet_t* e = (lwip_event_packet_t*)malloc(sizeof(lwip_event_packet_t));
  e->event = LWIP_TCP_CLEAR;
  e->arg = arg;
  if (!_prepend_async_event(&e)) {
    free((void*)(e));
  }
  return ERR_OK;
}

static int8_t _tcp_connected(void* arg, tcp_pcb* pcb, int8_t err) {
  // ets_printf("+C: 0x%08x\n", pcb);
  lwip_event_packet_t* e = (lwip_event_packet_t*)malloc(sizeof(lwip_event_packet_t));
  e->event = LWIP_TCP_CONNECTED;
  e->arg = arg;
  e->connected.pcb = pcb;
  e->connected.err = err;
  if (!_prepend_async_event(&e)) {
    free((void*)(e));
  }
  return ERR_OK;
}

static int8_t _tcp_poll(void* arg, struct tcp_pcb* pcb) {
  // throttle polling events queing when event queue is getting filled up, let it handle _onack's
  // log_d("qs:%u", uxQueueMessagesWaiting(_async_queue));
  if (uxQueueMessagesWaiting(_async_queue) > (rand() % CONFIG_ASYNC_TCP_QUEUE_SIZE / 2 + CONFIG_ASYNC_TCP_QUEUE_SIZE / 4)) {
    log_d("throttling");
    return ERR_OK;
  }

  // ets_printf("+P: 0x%08x\n", pcb);
  lwip_event_packet_t* e = (lwip_event_packet_t*)malloc(sizeof(lwip_event_packet_t));
  e->event = LWIP_TCP_POLL;
  e->arg = arg;
  e->poll.pcb = pcb;
  // poll events are not critical 'cause those are repetitive, so we may not wait the queue in any case
  if (!_send_async_event(&e, 0)) {
    free((void*)(e));
  }
  return ERR_OK;
}

static int8_t _tcp_recv(void* arg, struct tcp_pcb* pcb, struct pbuf* pb, int8_t err) {
  lwip_event_packet_t* e = (lwip_event_packet_t*)malloc(sizeof(lwip_event_packet_t));
  e->arg = arg;
  if (pb) {
    // ets_printf("+R: 0x%08x\n", pcb);
    e->event = LWIP_TCP_RECV;
    e->recv.pcb = pcb;
    e->recv.pb = pb;
    e->recv.err = err;
  } else {
    // ets_printf("+F: 0x%08x\n", pcb);
    e->event = LWIP_TCP_FIN;
    e->fin.pcb = pcb;
    e->fin.err = err;
    // close the PCB in LwIP thread
    AsyncClient::_s_lwip_fin(e->arg, e->fin.pcb, e->fin.err);
  }
  if (!_send_async_event(&e)) {
    free((void*)(e));
  }
  return ERR_OK;
}

static int8_t _tcp_sent(void* arg, struct tcp_pcb* pcb, uint16_t len) {
  // ets_printf("+S: 0x%08x\n", pcb);
  lwip_event_packet_t* e = (lwip_event_packet_t*)malloc(sizeof(lwip_event_packet_t));
  e->event = LWIP_TCP_SENT;
  e->arg = arg;
  e->sent.pcb = pcb;
  e->sent.len = len;
  if (!_send_async_event(&e)) {
    free((void*)(e));
  }
  return ERR_OK;
}

static void _tcp_error(void* arg, int8_t err) {
  // ets_printf("+E: 0x%08x\n", arg);
  lwip_event_packet_t* e = (lwip_event_packet_t*)malloc(sizeof(lwip_event_packet_t));
  e->event = LWIP_TCP_ERROR;
  e->arg = arg;
  e->error.err = err;
  if (!_send_async_event(&e)) {
    free((void*)(e));
  }
}

static void _tcp_dns_found(const char* name, struct ip_addr* ipaddr, void* arg) {
  lwip_event_packet_t* e = (lwip_event_packet_t*)malloc(sizeof(lwip_event_packet_t));
  // ets_printf("+DNS: name=%s ipaddr=0x%08x arg=%x\n", name, ipaddr, arg);
  e->event = LWIP_TCP_DNS;
  e->arg = arg;
  e->dns.name = name;
  if (ipaddr) {
    memcpy(&e->dns.addr, ipaddr, sizeof(struct ip_addr));
  } else {
    memset(&e->dns.addr, 0, sizeof(e->dns.addr));
  }
  if (!_send_async_event(&e)) {
    free((void*)(e));
  }
}

// Used to switch out from LwIP thread
static int8_t _tcp_accept(void* arg, AsyncClient* client) {
  lwip_event_packet_t* e = (lwip_event_packet_t*)malloc(sizeof(lwip_event_packet_t));
  e->event = LWIP_TCP_ACCEPT;
  e->arg = arg;
  e->accept.client = client;
  if (!_prepend_async_event(&e)) {
    free((void*)(e));
  }
  return ERR_OK;
}

/*
 * TCP/IP API Calls
 * */

#include "lwip/priv/tcpip_priv.h"

typedef struct {
    struct tcpip_api_call_data call;
    tcp_pcb* pcb;
    int8_t closed_slot;
    int8_t err;
    union {
        struct {
            const char* data;
            size_t size;
            uint8_t apiflags;
        } write;
        size_t received;
        struct {
            ip_addr_t* addr;
            uint16_t port;
            tcp_connected_fn cb;
        } connect;
        struct {
            ip_addr_t* addr;
            uint16_t port;
        } bind;
        uint8_t backlog;
    };
} tcp_api_call_t;

static err_t _tcp_output_api(struct tcpip_api_call_data* api_call_msg) {
  tcp_api_call_t* msg = (tcp_api_call_t*)api_call_msg;
  msg->err = ERR_CONN;
  if (msg->closed_slot == INVALID_CLOSED_SLOT || !_closed_slots[msg->closed_slot]) {
    msg->err = tcp_output(msg->pcb);
  }
  return msg->err;
}

static esp_err_t _tcp_output(tcp_pcb* pcb, int8_t closed_slot) {
  if (!pcb) {
    return ERR_CONN;
  }
  tcp_api_call_t msg;
  msg.pcb = pcb;
  msg.closed_slot = closed_slot;
  tcpip_api_call(_tcp_output_api, (struct tcpip_api_call_data*)&msg);
  return msg.err;
}

static err_t _tcp_write_api(struct tcpip_api_call_data* api_call_msg) {
  tcp_api_call_t* msg = (tcp_api_call_t*)api_call_msg;
  msg->err = ERR_CONN;
  if (msg->closed_slot == INVALID_CLOSED_SLOT || !_closed_slots[msg->closed_slot]) {
    msg->err = tcp_write(msg->pcb, msg->write.data, msg->write.size, msg->write.apiflags);
  }
  return msg->err;
}

static esp_err_t _tcp_write(tcp_pcb* pcb, int8_t closed_slot, const char* data, size_t size, uint8_t apiflags) {
  if (!pcb) {
    return ERR_CONN;
  }
  tcp_api_call_t msg;
  msg.pcb = pcb;
  msg.closed_slot = closed_slot;
  msg.write.data = data;
  msg.write.size = size;
  msg.write.apiflags = apiflags;
  tcpip_api_call(_tcp_write_api, (struct tcpip_api_call_data*)&msg);
  return msg.err;
}

static err_t _tcp_recved_api(struct tcpip_api_call_data* api_call_msg) {
  tcp_api_call_t* msg = (tcp_api_call_t*)api_call_msg;
  msg->err = ERR_CONN;
  if (msg->closed_slot == INVALID_CLOSED_SLOT || !_closed_slots[msg->closed_slot]) {
    // if(msg->closed_slot != INVALID_CLOSED_SLOT && !_closed_slots[msg->closed_slot]) {
    // if(msg->closed_slot != INVALID_CLOSED_SLOT) {
    msg->err = 0;
    tcp_recved(msg->pcb, msg->received);
  }
  return msg->err;
}

static esp_err_t _tcp_recved(tcp_pcb* pcb, int8_t closed_slot, size_t len) {
  if (!pcb) {
    return ERR_CONN;
  }
  tcp_api_call_t msg;
  msg.pcb = pcb;
  msg.closed_slot = closed_slot;
  msg.received = len;
  tcpip_api_call(_tcp_recved_api, (struct tcpip_api_call_data*)&msg);
  return msg.err;
}

static err_t _tcp_close_api(struct tcpip_api_call_data* api_call_msg) {
  tcp_api_call_t* msg = (tcp_api_call_t*)api_call_msg;
  msg->err = ERR_CONN;
  if (msg->closed_slot == INVALID_CLOSED_SLOT || !_closed_slots[msg->closed_slot]) {
    msg->err = tcp_close(msg->pcb);
  }
  return msg->err;
}

static esp_err_t _tcp_close(tcp_pcb* pcb, int8_t closed_slot) {
  if (!pcb) {
    return ERR_CONN;
  }
  tcp_api_call_t msg;
  msg.pcb = pcb;
  msg.closed_slot = closed_slot;
  tcpip_api_call(_tcp_close_api, (struct tcpip_api_call_data*)&msg);
  return msg.err;
}

static err_t _tcp_abort_api(struct tcpip_api_call_data* api_call_msg) {
  tcp_api_call_t* msg = (tcp_api_call_t*)api_call_msg;
  msg->err = ERR_CONN;
  if (msg->closed_slot == INVALID_CLOSED_SLOT || !_closed_slots[msg->closed_slot]) {
    tcp_abort(msg->pcb);
  }
  return msg->err;
}

static esp_err_t _tcp_abort(tcp_pcb* pcb, int8_t closed_slot) {
  if (!pcb) {
    return ERR_CONN;
  }
  tcp_api_call_t msg;
  msg.pcb = pcb;
  msg.closed_slot = closed_slot;
  tcpip_api_call(_tcp_abort_api, (struct tcpip_api_call_data*)&msg);
  return msg.err;
}

static err_t _tcp_connect_api(struct tcpip_api_call_data* api_call_msg) {
  tcp_api_call_t* msg = (tcp_api_call_t*)api_call_msg;
  msg->err = tcp_connect(msg->pcb, msg->connect.addr, msg->connect.port, msg->connect.cb);
  return msg->err;
}

static esp_err_t _tcp_connect(tcp_pcb* pcb, int8_t closed_slot, ip_addr_t* addr, uint16_t port, tcp_connected_fn cb) {
  if (!pcb) {
    return ESP_FAIL;
  }
  tcp_api_call_t msg;
  msg.pcb = pcb;
  msg.closed_slot = closed_slot;
  msg.connect.addr = addr;
  msg.connect.port = port;
  msg.connect.cb = cb;
  tcpip_api_call(_tcp_connect_api, (struct tcpip_api_call_data*)&msg);
  return msg.err;
}

static err_t _tcp_bind_api(struct tcpip_api_call_data* api_call_msg) {
  tcp_api_call_t* msg = (tcp_api_call_t*)api_call_msg;
  msg->err = tcp_bind(msg->pcb, msg->bind.addr, msg->bind.port);
  return msg->err;
}

static esp_err_t _tcp_bind(tcp_pcb* pcb, ip_addr_t* addr, uint16_t port) {
  if (!pcb) {
    return ESP_FAIL;
  }
  tcp_api_call_t msg;
  msg.pcb = pcb;
  msg.closed_slot = -1;
  msg.bind.addr = addr;
  msg.bind.port = port;
  tcpip_api_call(_tcp_bind_api, (struct tcpip_api_call_data*)&msg);
  return msg.err;
}

static err_t _tcp_listen_api(struct tcpip_api_call_data* api_call_msg) {
  tcp_api_call_t* msg = (tcp_api_call_t*)api_call_msg;
  msg->err = 0;
  msg->pcb = tcp_listen_with_backlog(msg->pcb, msg->backlog);
  return msg->err;
}

static tcp_pcb* _tcp_listen_with_backlog(tcp_pcb* pcb, uint8_t backlog) {
  if (!pcb) {
    return NULL;
  }
  tcp_api_call_t msg;
  msg.pcb = pcb;
  msg.closed_slot = -1;
  msg.backlog = backlog ? backlog : 0xFF;
  tcpip_api_call(_tcp_listen_api, (struct tcpip_api_call_data*)&msg);
  return msg.pcb;
}

/*
  Async TCP Client
 */

AsyncClient::AsyncClient(tcp_pcb* pcb)
    : _connect_cb(0), _connect_cb_arg(0), _discard_cb(0), _discard_cb_arg(0), _sent_cb(0), _sent_cb_arg(0), _error_cb(0), _error_cb_arg(0), _recv_cb(0), _recv_cb_arg(0), _pb_cb(0), _pb_cb_arg(0), _timeout_cb(0), _timeout_cb_arg(0), _ack_pcb(true), _tx_last_packet(0), _rx_timeout(0), _rx_last_ack(0), _ack_timeout(CONFIG_ASYNC_TCP_MAX_ACK_TIME), _connect_port(0), prev(NULL), next(NULL) {
  _pcb = pcb;
  _closed_slot = INVALID_CLOSED_SLOT;
  if (_pcb) {
    _rx_last_packet = millis();
    tcp_arg(_pcb, this);
    tcp_recv(_pcb, &_tcp_recv);
    tcp_sent(_pcb, &_tcp_sent);
    tcp_err(_pcb, &_tcp_error);
    tcp_poll(_pcb, &_tcp_poll, CONFIG_ASYNC_TCP_POLL_TIMER);
    if (!_allocate_closed_slot()) {
      _close();
    }
  }
}

AsyncClient::~AsyncClient() {
  if (_pcb) {
    _close();
  }
  _free_closed_slot();
}

/*
 * Operators
 * */

AsyncClient& AsyncClient::operator=(const AsyncClient& other) {
  if (_pcb) {
    _close();
  }

  _pcb = other._pcb;
  _closed_slot = other._closed_slot;
  if (_pcb) {
    _rx_last_packet = millis();
    tcp_arg(_pcb, this);
    tcp_recv(_pcb, &_tcp_recv);
    tcp_sent(_pcb, &_tcp_sent);
    tcp_err(_pcb, &_tcp_error);
    tcp_poll(_pcb, &_tcp_poll, CONFIG_ASYNC_TCP_POLL_TIMER);
  }
  return *this;
}

bool AsyncClient::operator==(const AsyncClient& other) {
  return _pcb == other._pcb;
}

AsyncClient& AsyncClient::operator+=(const AsyncClient& other) {
  if (next == NULL) {
    next = (AsyncClient*)(&other);
    next->prev = this;
  } else {
    AsyncClient* c = next;
    while (c->next != NULL) {
      c = c->next;
    }
    c->next = (AsyncClient*)(&other);
    c->next->prev = c;
  }
  return *this;
}

/*
 * Callback Setters
 * */

void AsyncClient::onConnect(AcConnectHandler cb, void* arg) {
  _connect_cb = cb;
  _connect_cb_arg = arg;
}

void AsyncClient::onDisconnect(AcConnectHandler cb, void* arg) {
  _discard_cb = cb;
  _discard_cb_arg = arg;
}

void AsyncClient::onAck(AcAckHandler cb, void* arg) {
  _sent_cb = cb;
  _sent_cb_arg = arg;
}

void AsyncClient::onError(AcErrorHandler cb, void* arg) {
  _error_cb = cb;
  _error_cb_arg = arg;
}

void AsyncClient::onData(AcDataHandler cb, void* arg) {
  _recv_cb = cb;
  _recv_cb_arg = arg;
}

void AsyncClient::onPacket(AcPacketHandler cb, void* arg) {
  _pb_cb = cb;
  _pb_cb_arg = arg;
}

void AsyncClient::onTimeout(AcTimeoutHandler cb, void* arg) {
  _timeout_cb = cb;
  _timeout_cb_arg = arg;
}

void AsyncClient::onPoll(AcConnectHandler cb, void* arg) {
  _poll_cb = cb;
  _poll_cb_arg = arg;
}

/*
 * Main Public Methods
 * */

bool AsyncClient::_connect(ip_addr_t addr, uint16_t port) {
  if (_pcb) {
    log_d("already connected, state %d", _pcb->state);
    return false;
  }
  if (!_start_async_task()) {
    log_e("failed to start task");
    return false;
  }

  if (!_allocate_closed_slot()) {
    log_e("failed to allocate: closed slot full");
    return false;
  }

  TCP_MUTEX_LOCK();
  tcp_pcb* pcb = tcp_new_ip_type(addr.type);
  if (!pcb) {
    TCP_MUTEX_UNLOCK();
    log_e("pcb == NULL");
    return false;
  }
  tcp_arg(pcb, this);
  tcp_err(pcb, &_tcp_error);
  tcp_recv(pcb, &_tcp_recv);
  tcp_sent(pcb, &_tcp_sent);
  tcp_poll(pcb, &_tcp_poll, CONFIG_ASYNC_TCP_POLL_TIMER);
  TCP_MUTEX_UNLOCK();

  esp_err_t err = _tcp_connect(pcb, _closed_slot, &addr, port, (tcp_connected_fn)&_tcp_connected);
  return err == ESP_OK;
}

bool AsyncClient::connect(const IPAddress& ip, uint16_t port) {
  ip_addr_t addr;
#if ESP_IDF_VERSION_MAJOR < 5
  addr.u_addr.ip4.addr = ip;
  addr.type = IPADDR_TYPE_V4;
#else
  ip.to_ip_addr_t(&addr);
#endif

  return _connect(addr, port);
}

#if LWIP_IPV6 && ESP_IDF_VERSION_MAJOR < 5
bool AsyncClient::connect(const IPv6Address& ip, uint16_t port) {
  auto ipaddr = static_cast<const uint32_t*>(ip);
  ip_addr_t addr = IPADDR6_INIT(ipaddr[0], ipaddr[1], ipaddr[2], ipaddr[3]);

  return _connect(addr, port);
}
#endif

bool AsyncClient::connect(const char* host, uint16_t port) {
  ip_addr_t addr;

  if (!_start_async_task()) {
    log_e("failed to start task");
    return false;
  }

  TCP_MUTEX_LOCK();
  err_t err = dns_gethostbyname(host, &addr, (dns_found_callback)&_tcp_dns_found, this);
  TCP_MUTEX_UNLOCK();
  if (err == ERR_OK) {
#if ESP_IDF_VERSION_MAJOR < 5
  #if LWIP_IPV6
    if (addr.type == IPADDR_TYPE_V6) {
      return connect(IPv6Address(addr.u_addr.ip6.addr), port);
    }
    return connect(IPAddress(addr.u_addr.ip4.addr), port);
  #else
    return connect(IPAddress(addr.addr), port);
  #endif
#else
    return _connect(addr, port);
#endif
  } else if (err == ERR_INPROGRESS) {
    _connect_port = port;
    return true;
  }
  log_d("error: %d", err);
  return false;
}

void AsyncClient::close(bool now) {
  if (_pcb) {
    _tcp_recved(_pcb, _closed_slot, _rx_ack_len);
  }
  _close();
}

int8_t AsyncClient::abort() {
  if (_pcb) {
    _tcp_abort(_pcb, _closed_slot);
    _pcb = NULL;
  }
  return ERR_ABRT;
}

size_t AsyncClient::space() {
  if ((_pcb != NULL) && (_pcb->state == ESTABLISHED)) {
    return tcp_sndbuf(_pcb);
  }
  return 0;
}

size_t AsyncClient::add(const char* data, size_t size, uint8_t apiflags) {
  if (!_pcb || size == 0 || data == NULL) {
    return 0;
  }
  size_t room = space();
  if (!room) {
    return 0;
  }
  size_t will_send = (room < size) ? room : size;
  int8_t err = ERR_OK;
  err = _tcp_write(_pcb, _closed_slot, data, will_send, apiflags);
  if (err != ERR_OK) {
    return 0;
  }
  return will_send;
}

bool AsyncClient::send() {
  auto backup = _tx_last_packet;
  _tx_last_packet = millis();
  if (_tcp_output(_pcb, _closed_slot) == ERR_OK) {
    return true;
  }
  _tx_last_packet = backup;
  return false;
}

size_t AsyncClient::ack(size_t len) {
  if (len > _rx_ack_len)
    len = _rx_ack_len;
  if (len) {
    _tcp_recved(_pcb, _closed_slot, len);
  }
  _rx_ack_len -= len;
  return len;
}

void AsyncClient::ackPacket(struct pbuf* pb) {
  if (!pb) {
    return;
  }
  _tcp_recved(_pcb, _closed_slot, pb->len);
  pbuf_free(pb);
}

/*
 * Main Private Methods
 * */

int8_t AsyncClient::_close() {
  // ets_printf("X: 0x%08x\n", (uint32_t)this);
  int8_t err = ERR_OK;
  if (_pcb) {
    TCP_MUTEX_LOCK();
    tcp_arg(_pcb, NULL);
    tcp_sent(_pcb, NULL);
    tcp_recv(_pcb, NULL);
    tcp_err(_pcb, NULL);
    tcp_poll(_pcb, NULL, 0);
    TCP_MUTEX_UNLOCK();
    _tcp_clear_events(this);
    err = _tcp_close(_pcb, _closed_slot);
    if (err != ERR_OK) {
      err = abort();
    }
    _free_closed_slot();
    _pcb = NULL;
    if (_discard_cb) {
      _discard_cb(_discard_cb_arg, this);
    }
  }
  return err;
}

bool AsyncClient::_allocate_closed_slot() {
  if (_closed_slot != INVALID_CLOSED_SLOT) {
    return true;
  }
  xSemaphoreTake(_slots_lock, portMAX_DELAY);
  uint32_t closed_slot_min_index = 0;
  for (int i = 0; i < _number_of_closed_slots; ++i) {
    if ((_closed_slot == INVALID_CLOSED_SLOT || _closed_slots[i] <= closed_slot_min_index) && _closed_slots[i] != 0) {
      closed_slot_min_index = _closed_slots[i];
      _closed_slot = i;
    }
  }
  if (_closed_slot != INVALID_CLOSED_SLOT) {
    _closed_slots[_closed_slot] = 0;
  }
  xSemaphoreGive(_slots_lock);
  return (_closed_slot != INVALID_CLOSED_SLOT);
}

void AsyncClient::_free_closed_slot() {
  xSemaphoreTake(_slots_lock, portMAX_DELAY);
  if (_closed_slot != INVALID_CLOSED_SLOT) {
    _closed_slots[_closed_slot] = _closed_index;
    _closed_slot = INVALID_CLOSED_SLOT;
    ++_closed_index;
  }
  xSemaphoreGive(_slots_lock);
}

/*
 * Private Callbacks
 * */

int8_t AsyncClient::_connected(tcp_pcb* pcb, int8_t err) {
  _pcb = reinterpret_cast<tcp_pcb*>(pcb);
  if (_pcb) {
    _rx_last_packet = millis();
  }
  if (_connect_cb) {
    _connect_cb(_connect_cb_arg, this);
  }
  return ERR_OK;
}

void AsyncClient::_error(int8_t err) {
  if (_pcb) {
    TCP_MUTEX_LOCK();
    tcp_arg(_pcb, NULL);
    if (_pcb->state == LISTEN) {
      tcp_sent(_pcb, NULL);
      tcp_recv(_pcb, NULL);
      tcp_err(_pcb, NULL);
      tcp_poll(_pcb, NULL, 0);
    }
    TCP_MUTEX_UNLOCK();
    _free_closed_slot();
    _pcb = NULL;
  }
  if (_error_cb) {
    _error_cb(_error_cb_arg, this, err);
  }
  if (_discard_cb) {
    _discard_cb(_discard_cb_arg, this);
  }
}

// In LwIP Thread
int8_t AsyncClient::_lwip_fin(tcp_pcb* pcb, int8_t err) {
  if (!_pcb || pcb != _pcb) {
    log_d("0x%08x != 0x%08x", (uint32_t)pcb, (uint32_t)_pcb);
    return ERR_OK;
  }
  tcp_arg(_pcb, NULL);
  if (_pcb->state == LISTEN) {
    tcp_sent(_pcb, NULL);
    tcp_recv(_pcb, NULL);
    tcp_err(_pcb, NULL);
    tcp_poll(_pcb, NULL, 0);
  }
  if (tcp_close(_pcb) != ERR_OK) {
    tcp_abort(_pcb);
  }
  _free_closed_slot();
  _pcb = NULL;
  return ERR_OK;
}

// In Async Thread
int8_t AsyncClient::_fin(tcp_pcb* pcb, int8_t err) {
  _tcp_clear_events(this);
  if (_discard_cb) {
    _discard_cb(_discard_cb_arg, this);
  }
  return ERR_OK;
}

int8_t AsyncClient::_sent(tcp_pcb* pcb, uint16_t len) {
  _rx_last_ack = _rx_last_packet = millis();
  if (_sent_cb) {
    _sent_cb(_sent_cb_arg, this, len, (_rx_last_packet - _tx_last_packet));
  }
  return ERR_OK;
}

int8_t AsyncClient::_recv(tcp_pcb* pcb, pbuf* pb, int8_t err) {
  while (pb != NULL) {
    _rx_last_packet = millis();
    // we should not ack before we assimilate the data
    _ack_pcb = true;
    pbuf* b = pb;
    pb = b->next;
    b->next = NULL;
    if (_pb_cb) {
      _pb_cb(_pb_cb_arg, this, b);
    } else {
      if (_recv_cb) {
        _recv_cb(_recv_cb_arg, this, b->payload, b->len);
      }
      if (!_ack_pcb) {
        _rx_ack_len += b->len;
      } else if (_pcb) {
        _tcp_recved(_pcb, _closed_slot, b->len);
      }
    }
    pbuf_free(b);
  }
  return ERR_OK;
}

int8_t AsyncClient::_poll(tcp_pcb* pcb) {
  if (!_pcb) {
    // log_d("pcb is NULL");
    return ERR_OK;
  }
  if (pcb != _pcb) {
    log_d("0x%08x != 0x%08x", (uint32_t)pcb, (uint32_t)_pcb);
    return ERR_OK;
  }

  uint32_t now = millis();

  // ACK Timeout
  if (_ack_timeout) {
    const uint32_t one_day = 86400000;
    bool last_tx_is_after_last_ack = (_rx_last_ack - _tx_last_packet + one_day) < one_day;
    if (last_tx_is_after_last_ack && (now - _tx_last_packet) >= _ack_timeout) {
      log_d("ack timeout %d", pcb->state);
      if (_timeout_cb)
        _timeout_cb(_timeout_cb_arg, this, (now - _tx_last_packet));
      return ERR_OK;
    }
  }
  // RX Timeout
  if (_rx_timeout && (now - _rx_last_packet) >= (_rx_timeout * 1000)) {
    log_d("rx timeout %d", pcb->state);
    _close();
    return ERR_OK;
  }
  // Everything is fine
  if (_poll_cb) {
    _poll_cb(_poll_cb_arg, this);
  }
  return ERR_OK;
}

void AsyncClient::_dns_found(struct ip_addr* ipaddr) {
#if ESP_IDF_VERSION_MAJOR < 5
  if (ipaddr && IP_IS_V4(ipaddr)) {
    connect(IPAddress(ip_addr_get_ip4_u32(ipaddr)), _connect_port);
  #if LWIP_IPV6
  } else if (ipaddr && ipaddr->u_addr.ip6.addr) {
    connect(IPv6Address(ipaddr->u_addr.ip6.addr), _connect_port);
  #endif
#else
  if (ipaddr) {
    IPAddress ip;
    ip.from_ip_addr_t(ipaddr);
    connect(ip, _connect_port);
#endif
  } else {
    if (_error_cb) {
      _error_cb(_error_cb_arg, this, -55);
    }
    if (_discard_cb) {
      _discard_cb(_discard_cb_arg, this);
    }
  }
}

/*
 * Public Helper Methods
 * */

bool AsyncClient::free() {
  if (!_pcb) {
    return true;
  }
  if (_pcb->state == CLOSED || _pcb->state > ESTABLISHED) {
    return true;
  }
  return false;
}

size_t AsyncClient::write(const char* data, size_t size, uint8_t apiflags) {
  size_t will_send = add(data, size, apiflags);
  if (!will_send || !send()) {
    return 0;
  }
  return will_send;
}

void AsyncClient::setRxTimeout(uint32_t timeout) {
  _rx_timeout = timeout;
}

uint32_t AsyncClient::getRxTimeout() {
  return _rx_timeout;
}

uint32_t AsyncClient::getAckTimeout() {
  return _ack_timeout;
}

void AsyncClient::setAckTimeout(uint32_t timeout) {
  _ack_timeout = timeout;
}

void AsyncClient::setNoDelay(bool nodelay) {
  if (!_pcb) {
    return;
  }
  if (nodelay) {
    tcp_nagle_disable(_pcb);
  } else {
    tcp_nagle_enable(_pcb);
  }
}

bool AsyncClient::getNoDelay() {
  if (!_pcb) {
    return false;
  }
  return tcp_nagle_disabled(_pcb);
}

void AsyncClient::setKeepAlive(uint32_t ms, uint8_t cnt) {
  if (ms != 0) {
    _pcb->so_options |= SOF_KEEPALIVE; // Turn on TCP Keepalive for the given pcb
    // Set the time between keepalive messages in milli-seconds
    _pcb->keep_idle = ms;
    _pcb->keep_intvl = ms;
    _pcb->keep_cnt = cnt; // The number of unanswered probes required to force closure of the socket
  } else {
    _pcb->so_options &= ~SOF_KEEPALIVE; // Turn off TCP Keepalive for the given pcb
  }
}

uint16_t AsyncClient::getMss() {
  if (!_pcb) {
    return 0;
  }
  return tcp_mss(_pcb);
}

uint32_t AsyncClient::getRemoteAddress() {
  if (!_pcb) {
    return 0;
  }
#if LWIP_IPV4 && LWIP_IPV6
  return _pcb->remote_ip.u_addr.ip4.addr;
#else
  return _pcb->remote_ip.addr;
#endif
}

#if LWIP_IPV6
ip6_addr_t AsyncClient::getRemoteAddress6() {
  if (!_pcb) {
    ip6_addr_t nulladdr;
    ip6_addr_set_zero(&nulladdr);
    return nulladdr;
  }
  return _pcb->remote_ip.u_addr.ip6;
}

ip6_addr_t AsyncClient::getLocalAddress6() {
  if (!_pcb) {
    ip6_addr_t nulladdr;
    ip6_addr_set_zero(&nulladdr);
    return nulladdr;
  }
  return _pcb->local_ip.u_addr.ip6;
}
  #if ESP_IDF_VERSION_MAJOR < 5
IPv6Address AsyncClient::remoteIP6() {
  return IPv6Address(getRemoteAddress6().addr);
}

IPv6Address AsyncClient::localIP6() {
  return IPv6Address(getLocalAddress6().addr);
}
  #else
IPAddress AsyncClient::remoteIP6() {
  if (!_pcb) {
    return IPAddress(IPType::IPv6);
  }
  IPAddress ip;
  ip.from_ip_addr_t(&(_pcb->remote_ip));
  return ip;
}

IPAddress AsyncClient::localIP6() {
  if (!_pcb) {
    return IPAddress(IPType::IPv6);
  }
  IPAddress ip;
  ip.from_ip_addr_t(&(_pcb->local_ip));
  return ip;
}
  #endif
#endif

uint16_t AsyncClient::getRemotePort() {
  if (!_pcb) {
    return 0;
  }
  return _pcb->remote_port;
}

uint32_t AsyncClient::getLocalAddress() {
  if (!_pcb) {
    return 0;
  }
#if LWIP_IPV4 && LWIP_IPV6
  return _pcb->local_ip.u_addr.ip4.addr;
#else
  return _pcb->local_ip.addr;
#endif
}

uint16_t AsyncClient::getLocalPort() {
  if (!_pcb) {
    return 0;
  }
  return _pcb->local_port;
}

IPAddress AsyncClient::remoteIP() {
#if ESP_IDF_VERSION_MAJOR < 5
  return IPAddress(getRemoteAddress());
#else
  if (!_pcb) {
    return IPAddress();
  }
  IPAddress ip;
  ip.from_ip_addr_t(&(_pcb->remote_ip));
  return ip;
#endif
}

uint16_t AsyncClient::remotePort() {
  return getRemotePort();
}

IPAddress AsyncClient::localIP() {
#if ESP_IDF_VERSION_MAJOR < 5
  return IPAddress(getLocalAddress());
#else
  if (!_pcb) {
    return IPAddress();
  }
  IPAddress ip;
  ip.from_ip_addr_t(&(_pcb->local_ip));
  return ip;
#endif
}

uint16_t AsyncClient::localPort() {
  return getLocalPort();
}

uint8_t AsyncClient::state() {
  if (!_pcb) {
    return 0;
  }
  return _pcb->state;
}

bool AsyncClient::connected() {
  if (!_pcb) {
    return false;
  }
  return _pcb->state == ESTABLISHED;
}

bool AsyncClient::connecting() {
  if (!_pcb) {
    return false;
  }
  return _pcb->state > CLOSED && _pcb->state < ESTABLISHED;
}

bool AsyncClient::disconnecting() {
  if (!_pcb) {
    return false;
  }
  return _pcb->state > ESTABLISHED && _pcb->state < TIME_WAIT;
}

bool AsyncClient::disconnected() {
  if (!_pcb) {
    return true;
  }
  return _pcb->state == CLOSED || _pcb->state == TIME_WAIT;
}

bool AsyncClient::freeable() {
  if (!_pcb) {
    return true;
  }
  return _pcb->state == CLOSED || _pcb->state > ESTABLISHED;
}

bool AsyncClient::canSend() {
  return space() > 0;
}

const char* AsyncClient::errorToString(int8_t error) {
  switch (error) {
    case ERR_OK:
      return "OK";
    case ERR_MEM:
      return "Out of memory error";
    case ERR_BUF:
      return "Buffer error";
    case ERR_TIMEOUT:
      return "Timeout";
    case ERR_RTE:
      return "Routing problem";
    case ERR_INPROGRESS:
      return "Operation in progress";
    case ERR_VAL:
      return "Illegal value";
    case ERR_WOULDBLOCK:
      return "Operation would block";
    case ERR_USE:
      return "Address in use";
    case ERR_ALREADY:
      return "Already connected";
    case ERR_CONN:
      return "Not connected";
    case ERR_IF:
      return "Low-level netif error";
    case ERR_ABRT:
      return "Connection aborted";
    case ERR_RST:
      return "Connection reset";
    case ERR_CLSD:
      return "Connection closed";
    case ERR_ARG:
      return "Illegal argument";
    case -55:
      return "DNS failed";
    default:
      return "UNKNOWN";
  }
}

const char* AsyncClient::stateToString() {
  switch (state()) {
    case 0:
      return "Closed";
    case 1:
      return "Listen";
    case 2:
      return "SYN Sent";
    case 3:
      return "SYN Received";
    case 4:
      return "Established";
    case 5:
      return "FIN Wait 1";
    case 6:
      return "FIN Wait 2";
    case 7:
      return "Close Wait";
    case 8:
      return "Closing";
    case 9:
      return "Last ACK";
    case 10:
      return "Time Wait";
    default:
      return "UNKNOWN";
  }
}

/*
 * Static Callbacks (LwIP C2C++ interconnect)
 * */

void AsyncClient::_s_dns_found(const char* name, struct ip_addr* ipaddr, void* arg) {
  reinterpret_cast<AsyncClient*>(arg)->_dns_found(ipaddr);
}

int8_t AsyncClient::_s_poll(void* arg, struct tcp_pcb* pcb) {
  return reinterpret_cast<AsyncClient*>(arg)->_poll(pcb);
}

int8_t AsyncClient::_s_recv(void* arg, struct tcp_pcb* pcb, struct pbuf* pb, int8_t err) {
  return reinterpret_cast<AsyncClient*>(arg)->_recv(pcb, pb, err);
}

int8_t AsyncClient::_s_fin(void* arg, struct tcp_pcb* pcb, int8_t err) {
  return reinterpret_cast<AsyncClient*>(arg)->_fin(pcb, err);
}

int8_t AsyncClient::_s_lwip_fin(void* arg, struct tcp_pcb* pcb, int8_t err) {
  return reinterpret_cast<AsyncClient*>(arg)->_lwip_fin(pcb, err);
}

int8_t AsyncClient::_s_sent(void* arg, struct tcp_pcb* pcb, uint16_t len) {
  return reinterpret_cast<AsyncClient*>(arg)->_sent(pcb, len);
}

void AsyncClient::_s_error(void* arg, int8_t err) {
  reinterpret_cast<AsyncClient*>(arg)->_error(err);
}

int8_t AsyncClient::_s_connected(void* arg, struct tcp_pcb* pcb, int8_t err) {
  return reinterpret_cast<AsyncClient*>(arg)->_connected(pcb, err);
}

/*
  Async TCP Server
 */

AsyncServer::AsyncServer(IPAddress addr, uint16_t port)
    : _port(port)
#if ESP_IDF_VERSION_MAJOR < 5
      ,
      _bind4(true), _bind6(false)
#else
      ,
      _bind4(addr.type() != IPType::IPv6), _bind6(addr.type() == IPType::IPv6)
#endif
      ,
      _addr(addr), _noDelay(false), _pcb(0), _connect_cb(0), _connect_cb_arg(0) {
}

#if ESP_IDF_VERSION_MAJOR < 5
AsyncServer::AsyncServer(IPv6Address addr, uint16_t port)
    : _port(port), _bind4(false), _bind6(true), _addr6(addr), _noDelay(false), _pcb(0), _connect_cb(0), _connect_cb_arg(0) {}
#endif

AsyncServer::AsyncServer(uint16_t port)
    : _port(port), _bind4(true), _bind6(false), _addr((uint32_t)IPADDR_ANY)
#if ESP_IDF_VERSION_MAJOR < 5
      ,
      _addr6()
#endif
      ,
      _noDelay(false), _pcb(0), _connect_cb(0), _connect_cb_arg(0) {
}

AsyncServer::~AsyncServer() {
  end();
}

void AsyncServer::onClient(AcConnectHandler cb, void* arg) {
  _connect_cb = cb;
  _connect_cb_arg = arg;
}

void AsyncServer::begin() {
  if (_pcb) {
    return;
  }

  if (!_start_async_task()) {
    log_e("failed to start task");
    return;
  }
  int8_t err;
  TCP_MUTEX_LOCK();
  _pcb = tcp_new_ip_type(_bind4 && _bind6 ? IPADDR_TYPE_ANY : (_bind6 ? IPADDR_TYPE_V6 : IPADDR_TYPE_V4));
  TCP_MUTEX_UNLOCK();
  if (!_pcb) {
    log_e("_pcb == NULL");
    return;
  }

  ip_addr_t local_addr;
#if ESP_IDF_VERSION_MAJOR < 5
  if (_bind6) { // _bind6 && _bind4 both at the same time is not supported on Arduino 2 in this lib API
    local_addr.type = IPADDR_TYPE_V6;
    memcpy(local_addr.u_addr.ip6.addr, static_cast<const uint32_t*>(_addr6), sizeof(uint32_t) * 4);
  } else {
    local_addr.type = IPADDR_TYPE_V4;
    local_addr.u_addr.ip4.addr = _addr;
  }
#else
  _addr.to_ip_addr_t(&local_addr);
#endif
  err = _tcp_bind(_pcb, &local_addr, _port);

  if (err != ERR_OK) {
    _tcp_close(_pcb, -1);
    log_e("bind error: %d", err);
    return;
  }

  static uint8_t backlog = 5;
  _pcb = _tcp_listen_with_backlog(_pcb, backlog);
  if (!_pcb) {
    log_e("listen_pcb == NULL");
    return;
  }
  TCP_MUTEX_LOCK();
  tcp_arg(_pcb, (void*)this);
  tcp_accept(_pcb, &_s_accept);
  TCP_MUTEX_UNLOCK();
}

void AsyncServer::end() {
  if (_pcb) {
    TCP_MUTEX_LOCK();
    tcp_arg(_pcb, NULL);
    tcp_accept(_pcb, NULL);
    if (tcp_close(_pcb) != ERR_OK) {
      TCP_MUTEX_UNLOCK();
      _tcp_abort(_pcb, -1);
    } else {
      TCP_MUTEX_UNLOCK();
    }
    _pcb = NULL;
  }
}

// runs on LwIP thread
int8_t AsyncServer::_accept(tcp_pcb* pcb, int8_t err) {
  // ets_printf("+A: 0x%08x\n", pcb);
  if (_connect_cb) {
    AsyncClient* c = new AsyncClient(pcb);
    if (c) {
      c->setNoDelay(_noDelay);
      return _tcp_accept(this, c);
    }
  }
  if (tcp_close(pcb) != ERR_OK) {
    tcp_abort(pcb);
  }
  log_d("FAIL");
  return ERR_OK;
}

int8_t AsyncServer::_accepted(AsyncClient* client) {
  if (_connect_cb) {
    _connect_cb(_connect_cb_arg, client);
  }
  return ERR_OK;
}

void AsyncServer::setNoDelay(bool nodelay) {
  _noDelay = nodelay;
}

bool AsyncServer::getNoDelay() {
  return _noDelay;
}

uint8_t AsyncServer::status() {
  if (!_pcb) {
    return 0;
  }
  return _pcb->state;
}

int8_t AsyncServer::_s_accept(void* arg, tcp_pcb* pcb, int8_t err) {
  return reinterpret_cast<AsyncServer*>(arg)->_accept(pcb, err);
}

int8_t AsyncServer::_s_accepted(void* arg, AsyncClient* client) {
  return reinterpret_cast<AsyncServer*>(arg)->_accepted(client);
}
