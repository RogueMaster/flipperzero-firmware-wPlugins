// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Copyright 2020 Phil Schatzmann

/**
 * @defgroup a2dp ESP32 A2DP
 * @brief A Simple ESP32 Bluetooth A2DP Library (to implement a Music Receiver
 * or Sender) that supports Arduino, PlatformIO and Espressif IDF
 * @file BluetoothA2DPCommon.h
 * @author Phil Schatzmann
 * @copyright GPLv3
 */

#pragma once

// Compile only for ESP32
#ifdef ARDUINO_ARCH_ESP32
#  include "sdkconfig.h"
#endif
#if defined(CONFIG_IDF_TARGET_ESP32) 
#  define IS_VALID_PLATFORM true
#else
#  error "Please read the ESP32-A2DP Wiki about the supported Platforms!"
#  define IS_VALID_PLATFORM false
#endif

// Continue only for ESP32
#if IS_VALID_PLATFORM

#include "config.h"
// If you use #include "I2S.h" the i2s functionality is hidden in a namespace
// this hack prevents any error messages
#ifdef _I2S_H_INCLUDED
using namespace esp_i2s;
#endif

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <vector>

#include "esp_idf_version.h"
#include "freertos/FreeRTOS.h"  // needed for ESP Arduino < 2.0
#include "freertos/FreeRTOSConfig.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 2, 0)
#include "freertos/xtensa_api.h"
#else
#include "xtensa_api.h"
#endif
#include "A2DPVolumeControl.h"
#include "esp_a2dp_api.h"
#include "esp_avrc_api.h"
#include "esp_bt.h"
#include "esp_bt_device.h"
#include "esp_bt_main.h"
#include "esp_gap_bt_api.h"
#include "esp_spp_api.h"
#include "esp_task_wdt.h"
#include "esp_timer.h"
#include "nvs.h"
#include "nvs_flash.h"

#ifdef ARDUINO_ARCH_ESP32
#include "esp32-hal-bt.h"
#include "esp32-hal-log.h"
#else
#include "esp_log.h"
#endif

#if !defined(ESP_IDF_VERSION)
#error Unsupported ESP32 Version: Upgrade the ESP32 version in the Board Manager
#endif

// Support for old and new IDF version
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(4, 0, 0) && \
    !defined(I2S_COMM_FORMAT_STAND_I2S)
// support for old idf releases
#define I2S_COMM_FORMAT_STAND_I2S \
  (I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB)
#define I2S_COMM_FORMAT_STAND_MSB \
  (I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_LSB)
#define I2S_COMM_FORMAT_STAND_PCM_LONG \
  (I2S_COMM_FORMAT_PCM | I2S_COMM_FORMAT_PCM_LONG)
#define I2S_COMM_FORMAT_STAND_PCM_SHORT \
  (I2S_COMM_FORMAT_PCM | I2S_COMM_FORMAT_PCM_SHORT)
#endif

// Prior IDF 5 support
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 0, 0)
#define TaskHandle_t xTaskHandle
#define QueueHandle_t xQueueHandle
#define TickType_t portTickType
#endif

#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 3, 0)
#define esp_bt_gap_set_device_name esp_bt_dev_set_device_name
#endif

#define A2DP_DEPRECATED __attribute__((deprecated))

#define BT_APP_SIG_WORK_DISPATCH (0x01)

/**
 * @brief     handler for the dispatched work
 */
typedef void (*app_callback_t)(uint16_t event, void *param);

/** @brief Internal message to be sent for BluetoothA2DPSink and
 * BluetoothA2DPSource */
typedef struct {
  uint16_t sig;      /*!< signal to app_task */
  uint16_t event;    /*!< message event id */
  app_callback_t cb; /*!< context switch callback */
  void *param;       /*!< parameter area needs to be last */
} bt_app_msg_t;

#define BT_AV_TAG "BT_AV"
#define BT_RC_CT_TAG "RCCT"
#define BT_APP_TAG "BT_API"

// AVRCP used transaction labels 
#define APP_RC_CT_TL_GET_CAPS (0)
#define APP_RC_CT_TL_GET_META_DATA (1)
#define APP_RC_CT_TL_RN_TRACK_CHANGE (2)
#define APP_RC_CT_TL_RN_PLAYBACK_CHANGE (3)
#define APP_RC_CT_TL_RN_PLAY_POS_CHANGE (4)

// common a2dp callbacks
extern "C" void ccall_bt_app_task_handler(void *arg);
extern "C" void ccall_app_gap_callback(esp_bt_gap_cb_event_t event,
                                       esp_bt_gap_cb_param_t *param);
extern "C" void ccall_app_rc_ct_callback(esp_avrc_ct_cb_event_t event,
                                         esp_avrc_ct_cb_param_t *param);
extern "C" void ccall_app_a2d_callback(esp_a2d_cb_event_t event,
                                       esp_a2d_cb_param_t *param);
extern "C" void ccall_av_hdl_stack_evt(uint16_t event, void *p_param);

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 0, 0)
extern "C" void ccall_app_rc_tg_callback(esp_avrc_tg_cb_event_t event,
                                         esp_avrc_tg_cb_param_t *param);
extern "C" void ccall_av_hdl_avrc_tg_evt(uint16_t event, void *p_param);
#endif

/**
 * @brief Buetooth A2DP Reconnect Status
 * @ingroup a2dp
 */
enum ReconnectStatus { NoReconnect, AutoReconnect, IsReconnecting };

/**
 * @brief Common Bluetooth A2DP functions
 * @author Phil Schatzmann
 * @copyright Apache License Version 2
 */
class BluetoothA2DPCommon {
  /// task handler
  friend void ccall_bt_app_task_handler(void *arg);
  friend void ccall_app_gap_callback(esp_bt_gap_cb_event_t event,
                                     esp_bt_gap_cb_param_t *param);
  friend void ccall_app_rc_ct_callback(esp_avrc_ct_cb_event_t event,
                                       esp_avrc_ct_cb_param_t *param);
  friend void ccall_app_a2d_callback(esp_a2d_cb_event_t event,
                                     esp_a2d_cb_param_t *param);
  friend void ccall_av_hdl_stack_evt(uint16_t event, void *p_param);

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 0, 0)
  /// handle esp_avrc_tg_cb_event_t
  friend void ccall_app_rc_tg_callback(esp_avrc_tg_cb_event_t event,
                                       esp_avrc_tg_cb_param_t *param);
  /* avrc TG event handler */
  friend void ccall_av_hdl_avrc_tg_evt(uint16_t event, void *p_param);
#endif

 public:
  /// Default constructor
  BluetoothA2DPCommon();
  /// Destructor
  virtual ~BluetoothA2DPCommon() = default;

  /// activate / deactivate the automatic reconnection to the last address (per
  /// default this is on)
  void set_auto_reconnect(bool active);

  /// Closes the connection
  virtual void disconnect();

  /// Reconnects to the last device
  virtual bool reconnect();

  /// Connnects to the indicated address
  virtual bool connect_to(esp_bd_addr_t peer);

  /// Calls disconnect or reconnect
  virtual void set_connected(bool active);

  /// Closes the connection and stops A2DP
  virtual void end(bool releaseMemory = false);

  /// Checks if A2DP is connected
  virtual bool is_connected() {
    return connection_state == ESP_A2D_CONNECTION_STATE_CONNECTED;
  }

  /// Sets the volume (range 0 - 127)
  virtual void set_volume(uint8_t volume) {
    volume_value = std::min((int)volume, 0x7F);
    ESP_LOGI(BT_AV_TAG, "set_volume: %d", volume_value);
    volume_control()->set_volume(volume_value);
    volume_control()->set_enabled(true);
    is_volume_used = true;
  }

  /// Determines the actual volume
  virtual int get_volume() { return is_volume_used ? volume_value : 0; }

  /// you can define a custom VolumeControl implementation
  virtual void set_volume_control(A2DPVolumeControl *ptr) {
    volume_control_ptr = ptr;
  }

  /// Determine the actual audio state
  virtual esp_a2d_audio_state_t get_audio_state();

  /// Determine the connection state
  virtual esp_a2d_connection_state_t get_connection_state();

  /// Set the callback that is called when the connection state is changed
  /// This callback is called before the I2S bus is changed.
  virtual void set_on_connection_state_changed(
      void (*callBack)(esp_a2d_connection_state_t state, void *),
      void *obj = nullptr);

  /// Set the callback that is called after the audio state has changed.
  /// This callback is called after the I2S bus has changed.
  virtual void set_on_audio_state_changed_post(
      void (*callBack)(esp_a2d_audio_state_t state, void *),
      void *obj = nullptr);

  /// Set the callback that is called when the audio state is changed
  virtual void set_on_audio_state_changed(
      void (*callBack)(esp_a2d_audio_state_t state, void *),
      void *obj = nullptr);

  /// Prevents that the same method is executed multiple times within the
  /// indicated time limit
  virtual void debounce(void (*cb)(void), int ms);

  /// Logs the free heap
  void log_free_heap();

  /// converts esp_a2d_connection_state_t to a string
  const char *to_str(esp_a2d_connection_state_t state);

  /// converts a esp_a2d_audio_state_t to a string
  const char *to_str(esp_a2d_audio_state_t state);

  /// converts a esp_bd_addr_t to a string - the string is 18 characters long!
  const char *to_str(esp_bd_addr_t bda);

  /// converts esp_bt_gap_discovery_state_t to a string
  const char *to_str(esp_bt_gap_discovery_state_t state);

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 0, 0)
  /// converts esp_avrc_playback_stat_t to a string
  const char *to_str(esp_avrc_playback_stat_t state);
#endif

  /// defines the task priority (the default value is configMAX_PRIORITIES - 10)
  void set_task_priority(UBaseType_t priority) { task_priority = priority; }

  /// Defines the core which is used to start the tasks (to process the events
  /// and audio queue): default is 1
  void set_task_core(BaseType_t core) { task_core = core; }

  /// Defines the queue size of the event task
  void set_event_queue_size(int size) { event_queue_size = size; }

  /// Defines the stack size of the event task (in bytes)
  void set_event_stack_size(int size) { event_stack_size = size; }

  /// Provides the address of the last device
  virtual esp_bd_addr_t *get_last_peer_address() { return &last_connection; }

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 0, 0)
  /// Bluetooth discoverability
  virtual void set_discoverability(esp_bt_discovery_mode_t d);
#endif

  /// Bluetooth connectable
  virtual void set_connectable(bool connectable) {
    set_scan_mode_connectable(connectable);
  }

  /// Provides the actual SSID name
  virtual const char *get_name() { return bt_name; }

  /// clean last connection (delete)
  virtual void clean_last_connection();

  /// Defines the default bt mode. The default is ESP_BT_MODE_CLASSIC_BT: use
  /// this e.g. to set to ESP_BT_MODE_BTDM
  virtual void set_default_bt_mode(esp_bt_mode_t mode) { bt_mode = mode; }

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 2, 1)
  /// Defines the esp_bluedroid_config_t: Available from IDF 5.2.1
  void set_bluedroid_config_t(esp_bluedroid_config_t cfg) {
    bluedroid_config = cfg;
  }
#endif
  /// calls vTaskDelay to pause for the indicated number of milliseconds
  void delay_ms(uint32_t millis);
  /// Provides the time in milliseconds since the last system boot
  unsigned long get_millis();

  /// Define the vector of esp_avrc_rn_event_ids_t with e.g.
  /// ESP_AVRC_RN_PLAY_STATUS_CHANGE | ESP_AVRC_RN_TRACK_CHANGE |
  /// ESP_AVRC_RN_TRACK_REACHED_END | ESP_AVRC_RN_TRACK_REACHED_START |
  /// ESP_AVRC_RN_PLAY_POS_CHANGED | ESP_AVRC_RN_BATTERY_STATUS_CHANGE |
  /// ESP_AVRC_RN_SYSTEM_STATUS_CHANGE | ESP_AVRC_RN_APP_SETTING_CHANGE |
  /// ESP_AVRC_RN_NOW_PLAYING_CHANGE | ESP_AVRC_RN_AVAILABLE_PLAYERS_CHANGE |
  /// ESP_AVRC_RN_ADDRESSED_PLAYER_CHANGE |
  /// ESP_AVRC_RN_UIDS_CHANGE|ESP_AVRC_RN_VOLUME_CHANGE
  virtual void set_avrc_rn_events(std::vector<esp_avrc_rn_event_ids_t> events) {
    avrc_rn_events = events;
  }

 protected:
  const char *bt_name = {0};
  esp_bd_addr_t peer_bd_addr;
  ReconnectStatus reconnect_status = NoReconnect;
  unsigned long reconnect_timout = 0;
  unsigned int default_reconnect_timout = 10000;
  bool is_autoreconnect_allowed = false;
  uint32_t debounce_ms = 0;
  A2DPDefaultVolumeControl default_volume_control;
  A2DPVolumeControl *volume_control_ptr = nullptr;
  esp_bd_addr_t last_connection = {0, 0, 0, 0, 0, 0};
  bool is_start_disabled = false;
  bool is_target_status_active = true;
  void (*connection_state_callback)(esp_a2d_connection_state_t state,
                                    void *obj) = nullptr;
  void (*audio_state_callback)(esp_a2d_audio_state_t state,
                               void *obj) = nullptr;
  void (*audio_state_callback_post)(esp_a2d_audio_state_t state,
                                    void *obj) = nullptr;
  void *connection_state_obj = nullptr;
  void *audio_state_obj = nullptr;
  void *audio_state_obj_post = nullptr;
  const char *m_a2d_conn_state_str[4] = {"Disconnected", "Connecting",
                                         "Connected", "Disconnecting"};
  const char *m_a2d_audio_state_str[4] = {"Suspended", "Started",  "Suspended", "Suspended"};
  const char *m_avrc_playback_state_str[5] = {"stopped", "playing", "paused",
                                              "forward seek", "reverse seek"};
  const char *m_esp_bt_gap_discovery_state_t[2] = {"Stopped", "Started"};
  esp_a2d_audio_state_t audio_state = ESP_A2D_AUDIO_STATE_STOPPED;
  esp_a2d_connection_state_t connection_state =
      ESP_A2D_CONNECTION_STATE_DISCONNECTED;
  UBaseType_t task_priority = configMAX_PRIORITIES - 10;
  // volume
  uint8_t volume_value = 0;
  bool is_volume_used = false;
  bool is_bluedroid_initialized = false;
  BaseType_t task_core = 1;

  int event_queue_size = 20;
  int event_stack_size = 3072;
  esp_bt_mode_t bt_mode = ESP_BT_MODE_CLASSIC_BT;
  std::vector<esp_avrc_rn_event_ids_t> avrc_rn_events = {
      ESP_AVRC_RN_VOLUME_CHANGE};

  QueueHandle_t app_task_queue = nullptr;
  TaskHandle_t app_task_handle = nullptr;

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 2, 1)
  esp_bluedroid_config_t bluedroid_config{.ssp_en = true};
#endif

  virtual void init_nvs();
  virtual esp_err_t esp_a2d_connect(esp_bd_addr_t peer) = 0;
  virtual const char *last_bda_nvs_name() = 0;
  virtual void get_last_connection();
  virtual void set_last_connection(esp_bd_addr_t bda);
  virtual bool has_last_connection();
  virtual bool read_address(const char *name, esp_bd_addr_t &bda);
  virtual bool write_address(const char *name, esp_bd_addr_t bda);

  // change the scan mode
  virtual void set_scan_mode_connectable(bool connectable);
  virtual void set_scan_mode_connectable_default() = 0;

  /// provides access to the VolumeControl object
  virtual A2DPVolumeControl *volume_control() {
    return volume_control_ptr != nullptr ? volume_control_ptr
                                         : &default_volume_control;
  }

  virtual bool bt_start();
  virtual esp_err_t bluedroid_init();
  virtual esp_err_t esp_a2d_disconnect(esp_bd_addr_t remote_bda) = 0;
  virtual void app_task_start_up();
  virtual void app_task_shut_down();
  virtual bool app_send_msg(bt_app_msg_t *msg);
  virtual void app_task_handler(void *arg);
  virtual void app_work_dispatched(bt_app_msg_t *msg);
  virtual bool isSource() = 0;
  // GAP callback
  virtual void app_gap_callback(esp_bt_gap_cb_event_t event,
                                esp_bt_gap_cb_param_t *param) = 0;
  /// callback function for AVRCP controller
  virtual void app_rc_ct_callback(esp_avrc_ct_cb_event_t event,
                                  esp_avrc_ct_cb_param_t *param) = 0;
  /// callback function for A2DP source
  virtual void app_a2d_callback(esp_a2d_cb_event_t event,
                                esp_a2d_cb_param_t *param) = 0;
  // handler for bluetooth stack enabled events
  virtual void av_hdl_stack_evt(uint16_t event, void *p_param) = 0;

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 0, 0)
  esp_bt_discovery_mode_t discoverability = ESP_BT_GENERAL_DISCOVERABLE;
  virtual void app_rc_tg_callback(esp_avrc_tg_cb_event_t event,
                                  esp_avrc_tg_cb_param_t *param) = 0;
  virtual void av_hdl_avrc_tg_evt(uint16_t event, void *p_param) = 0;
#endif

};

extern BluetoothA2DPCommon *actual_bluetooth_a2dp_common;

#endif // platform