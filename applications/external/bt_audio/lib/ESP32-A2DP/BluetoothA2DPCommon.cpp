
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

#include "BluetoothA2DPCommon.h"

#if IS_VALID_PLATFORM


BluetoothA2DPCommon* actual_bluetooth_a2dp_common = nullptr;

extern "C" void ccall_bt_app_task_handler(void* arg) {
  if (actual_bluetooth_a2dp_common)
    actual_bluetooth_a2dp_common->app_task_handler(arg);
}

extern "C" void ccall_app_gap_callback(esp_bt_gap_cb_event_t event,
                                       esp_bt_gap_cb_param_t* param) {
  ESP_LOGD(BT_AV_TAG, "%s", __func__);
  if (actual_bluetooth_a2dp_common)
    actual_bluetooth_a2dp_common->app_gap_callback(event, param);
}

extern "C" void ccall_app_rc_ct_callback(esp_avrc_ct_cb_event_t event,
                                         esp_avrc_ct_cb_param_t* param) {
  if (actual_bluetooth_a2dp_common)
    actual_bluetooth_a2dp_common->app_rc_ct_callback(event, param);
}

extern "C" void ccall_app_a2d_callback(esp_a2d_cb_event_t event,
                                       esp_a2d_cb_param_t* param) {
  if (actual_bluetooth_a2dp_common)
    actual_bluetooth_a2dp_common->app_a2d_callback(event, param);
}

extern "C" void ccall_av_hdl_stack_evt(uint16_t event, void* p_param) {
  if (actual_bluetooth_a2dp_common)
    actual_bluetooth_a2dp_common->av_hdl_stack_evt(event, p_param);
}

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 0, 0)

void ccall_app_rc_tg_callback(esp_avrc_tg_cb_event_t event,
                              esp_avrc_tg_cb_param_t* param) {
  ESP_LOGD(BT_AV_TAG, "%s", __func__);
  if (actual_bluetooth_a2dp_common)
    actual_bluetooth_a2dp_common->app_rc_tg_callback(event, param);
}

void ccall_av_hdl_avrc_tg_evt(uint16_t event, void* param) {
  ESP_LOGD(BT_AV_TAG, "%s", __func__);
  if (actual_bluetooth_a2dp_common)
    actual_bluetooth_a2dp_common->av_hdl_avrc_tg_evt(event, param);
}
#endif

BluetoothA2DPCommon::BluetoothA2DPCommon() {
  actual_bluetooth_a2dp_common = this;
}

esp_a2d_audio_state_t BluetoothA2DPCommon::get_audio_state() {
  return audio_state;
}

esp_a2d_connection_state_t BluetoothA2DPCommon::get_connection_state() {
  return connection_state;
}

/// activate / deactivate the automatic reconnection to the last address (per
/// default this is on)
void BluetoothA2DPCommon::set_auto_reconnect(bool active) {
  this->reconnect_status = active ? AutoReconnect : NoReconnect;
}

/// Reconnects to the last device
bool BluetoothA2DPCommon::reconnect() {
  if (has_last_connection()) {
    is_target_status_active = true;
    is_autoreconnect_allowed = true;
    reconnect_status = IsReconnecting;
    reconnect_timout = get_millis() + default_reconnect_timout;
    return connect_to(last_connection);
  }

  return false;
}

bool BluetoothA2DPCommon::connect_to(esp_bd_addr_t peer) {
  ESP_LOGW(BT_AV_TAG, "connect_to to %s", to_str(peer));
  set_scan_mode_connectable_default();
  esp_err_t err = esp_a2d_connect(peer);
  if (err != ESP_OK) {
    ESP_LOGE(BT_AV_TAG, "esp_a2d_source_connect:%d", err);
  }
  return err == ESP_OK;
}

/// Calls disconnect or reconnect
void BluetoothA2DPCommon::set_connected(bool active) {
  if (active) {
    reconnect();
  } else {
    disconnect();
  }
}

/// Closes the connection
void BluetoothA2DPCommon::disconnect() {
  ESP_LOGI(BT_AV_TAG, "disconect a2d: %s", to_str(last_connection));

  is_target_status_active = false;
  // Prevent automatic reconnect
  is_autoreconnect_allowed = false;

  esp_err_t status = esp_a2d_disconnect(last_connection);
  if (status == ESP_FAIL) {
    ESP_LOGE(BT_AV_TAG, "Failed disconnecting to device!");
  }
}

void BluetoothA2DPCommon::end(bool release_memory) {
  ESP_LOGI(BT_AV_TAG, "%s", __func__);
  // delay which prevents a crash when log level Warning or Error
  int wait_ms = 50;
  // reconnect should not work after end
  is_start_disabled = false;
  clean_last_connection();
  log_free_heap();

  // Disconnect and wait
  int limit = A2DP_DISCONNECT_LIMIT;
  if (is_connected()){
    disconnect();
    while (get_connection_state() != ESP_A2D_CONNECTION_STATE_DISCONNECTED) {
      delay_ms(100);
      if (limit-- < 0) {
        ESP_LOGW(BT_AV_TAG, "Waiting for Disconnect has timed out");
        break;
      };
    }
  }

  delay_ms(wait_ms);

  if (esp_avrc_ct_deinit() != ESP_OK) {
    ESP_LOGE(BT_AV_TAG, "Failed to deinit avrc ct");
  }

  delay_ms(wait_ms);

  if (esp_avrc_tg_deinit() != ESP_OK) {
    ESP_LOGE(BT_AV_TAG, "Failed to deinit avrc tg");
  }

  delay_ms(wait_ms);

  if (isSource()) {
    if (esp_a2d_source_deinit() != ESP_OK) {
      ESP_LOGE(BT_AV_TAG, "Failed to deinit source");
    }
  } else {
    if (esp_a2d_sink_deinit() != ESP_OK) {
      ESP_LOGE(BT_AV_TAG, "Failed to deinit source");
    }
  }

  delay_ms(wait_ms);
  log_free_heap();

  if (release_memory) {
    ESP_LOGI(BT_AV_TAG, "disable bluetooth");
    if (esp_bluedroid_disable() != ESP_OK) {
      ESP_LOGE(BT_AV_TAG, "Failed to disable bluetooth");
    }
    log_free_heap();

    ESP_LOGI(BT_AV_TAG, "deinit bluetooth");
    if (esp_bluedroid_deinit() != ESP_OK) {
      ESP_LOGE(BT_AV_TAG, "Failed to deinit bluetooth");
    }
    log_free_heap();

    ESP_LOGI(BT_AV_TAG, "esp_bt_controller_disable");
    if (esp_bt_controller_disable() != ESP_OK) {
      ESP_LOGE(BT_AV_TAG, "esp_bt_controller_disable failed");
    }
    log_free_heap();

    // waiting for status change
    limit = A2DP_DISCONNECT_LIMIT;
    while (esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_ENABLED) {
      delay_ms(100);
      if (limit-- < 0) {
        ESP_LOGW(BT_AV_TAG, "Waiting for Enabled has timed out");
        break;
      };
    }

    if (esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_INITED) {
      ESP_LOGI(BT_AV_TAG, "esp_bt_controller_deinit");
      if (esp_bt_controller_deinit() != ESP_OK) {
        ESP_LOGE(BT_AV_TAG, "esp_bt_controller_deinit failed");
      }
      log_free_heap();
    }

    // after a release memory - a restart will not be possible
    ESP_LOGI(BT_AV_TAG, "esp_bt_controller_mem_release");
    if (esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT) != ESP_OK) {
      ESP_LOGE(BT_AV_TAG, "esp_bt_controller_mem_release failed");
    }

    esp_bluedroid_deinit();
    is_bluedroid_initialized = false;
    
    log_free_heap();
    is_start_disabled = true;
  }

  delay_ms(wait_ms);

  app_task_shut_down();

  delay_ms(wait_ms);
  log_free_heap();
}

void BluetoothA2DPCommon::init_nvs() {
  ESP_LOGD(BT_AV_TAG, "%s", __func__);
  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES ||
      err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    err = nvs_flash_init();
  }
  ESP_ERROR_CHECK(err);
}

bool BluetoothA2DPCommon::has_last_connection() {
  esp_bd_addr_t empty_connection = {0, 0, 0, 0, 0, 0};
  int result = memcmp(last_connection, empty_connection, ESP_BD_ADDR_LEN);
  return result != 0;
}

void BluetoothA2DPCommon::get_last_connection() {
  ESP_LOGD(BT_AV_TAG, "%s", __func__);

  if (is_autoreconnect_allowed){
    esp_bd_addr_t bda;
    if (read_address(last_bda_nvs_name(), bda)) {
      memcpy(last_connection, bda, ESP_BD_ADDR_LEN);
    }
  }
  ESP_LOGD(BT_AV_TAG, "=> %s", to_str(last_connection));
}

void BluetoothA2DPCommon::set_last_connection(esp_bd_addr_t bda) {
  ESP_LOGD(BT_AV_TAG, "set_last_connection: %s", to_str(bda));

  // same value: nothing to store
  if (memcmp(bda, last_connection, ESP_BD_ADDR_LEN) == 0) {
    ESP_LOGD(BT_AV_TAG, "no change!");
    return;
  }
  // update nvs only when autoreconnect is enabled
  if (reconnect_status != NoReconnect) {
    write_address(last_bda_nvs_name(), bda);
  }
  // update last_connection variable
  memcpy(last_connection, bda, ESP_BD_ADDR_LEN);
}

void BluetoothA2DPCommon::clean_last_connection() {
  ESP_LOGD(BT_AV_TAG, "%s", __func__);
  esp_bd_addr_t cleanBda = {0};
  set_last_connection(cleanBda);
}

bool BluetoothA2DPCommon::read_address(const char* name, esp_bd_addr_t& bda) {
  nvs_handle my_handle;
  esp_err_t err = ESP_OK;

  err = nvs_open("connected_bda", NVS_READONLY, &my_handle);
  if (err != ESP_OK) {
    ESP_LOGE(BT_AV_TAG, "NVS OPEN ERROR");
    return false;
  }

  size_t size = ESP_BD_ADDR_LEN;
  err = nvs_get_blob(my_handle, name, bda, &size);
  if (err != ESP_OK) {
    if (err == ESP_ERR_NVS_NOT_FOUND) {
      ESP_LOGI(BT_AV_TAG, "nvs_blob does not exist");
    } else {
      ESP_LOGE(BT_AV_TAG, "nvs_get_blob failed");
    }
  }
  nvs_close(my_handle);
  return err == ESP_OK;
}

bool BluetoothA2DPCommon::write_address(const char* name, esp_bd_addr_t bda) {
  nvs_handle my_handle;
  esp_err_t err;

  err = nvs_open("connected_bda", NVS_READWRITE, &my_handle);
  if (err != ESP_OK) {
    ESP_LOGE(BT_AV_TAG, "NVS OPEN ERROR");
    return false;
  }
  err = nvs_set_blob(my_handle, last_bda_nvs_name(), bda, ESP_BD_ADDR_LEN);
  if (err != ESP_OK) {
    ESP_LOGE(BT_AV_TAG, "NVS WRITE ERROR");
  }
  err = nvs_commit(my_handle);
  if (err != ESP_OK) {
    ESP_LOGE(BT_AV_TAG, "NVS COMMIT ERROR");
  }
  nvs_close(my_handle);
  return err != ESP_OK;
}

/// Set the callback that is called when the connection state is changed
void BluetoothA2DPCommon::set_on_connection_state_changed(
    void (*callBack)(esp_a2d_connection_state_t state, void*), void* obj) {
  connection_state_callback = callBack;
  connection_state_obj = obj;
}

/// Set the callback that is called when the audio state is changed
/// This callback is called before the I2S bus is changed.
void BluetoothA2DPCommon::set_on_audio_state_changed(
    void (*callBack)(esp_a2d_audio_state_t state, void*), void* obj) {
  audio_state_callback = callBack;
  audio_state_obj = obj;
}

/// Set the callback that is called after the audio state has changed.
/// This callback is called after the I2S bus has changed.
void BluetoothA2DPCommon::set_on_audio_state_changed_post(
    void (*callBack)(esp_a2d_audio_state_t state, void*), void* obj) {
  audio_state_callback_post = callBack;
  audio_state_obj_post = obj;
}

/// Prevents that the same method is executed multiple times within the
/// indicated time limit
void BluetoothA2DPCommon::debounce(void (*cb)(void), int ms) {
  if (debounce_ms < get_millis()) {
    cb();
    // new time limit
    debounce_ms = get_millis() + ms;
  }
}

/// Logs the free heap
void BluetoothA2DPCommon::log_free_heap() {
  ESP_LOGI(BT_AV_TAG, "Available Heap: %zu", esp_get_free_heap_size());
}

/// converts esp_a2d_connection_state_t to a string
const char* BluetoothA2DPCommon::to_str(esp_a2d_connection_state_t state) {
  return m_a2d_conn_state_str[state];
}

/// converts a esp_a2d_audio_state_t to a string
const char* BluetoothA2DPCommon::to_str(esp_a2d_audio_state_t state) {
  return m_a2d_audio_state_str[state];
}

/// converts a esp_bd_addr_t to a string - the string is 18 characters long!
const char* BluetoothA2DPCommon::to_str(esp_bd_addr_t bda) {
  static char bda_str[18];
  sprintf(bda_str, "%02x:%02x:%02x:%02x:%02x:%02x", bda[0], bda[1], bda[2],
          bda[3], bda[4], bda[5]);
  return (const char*)bda_str;
}

  /// converts esp_bt_gap_discovery_state_t to a string
  const char* BluetoothA2DPCommon::to_str(esp_bt_gap_discovery_state_t state) {
  return m_esp_bt_gap_discovery_state_t[state];
}

/**
 * @brief Startup logic as implemented by Arduino
 *
 * @return true
 * @return false
 */
bool BluetoothA2DPCommon::bt_start() {
#ifdef ARDUINO
  return btStart();
#else
  esp_bt_controller_config_t cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
  // esp_bt_controller_enable(MODE) This mode must be equal as the mode in “cfg”
  // of esp_bt_controller_init().
  cfg.mode = bt_mode;
  if (cfg.mode == ESP_BT_MODE_CLASSIC_BT) {
    ESP_LOGI(BT_APP_TAG, "mode is ESP_BT_MODE_CLASSIC_BT");
    esp_bt_controller_mem_release(ESP_BT_MODE_BLE);
  }

  if (esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_ENABLED) {
    return true;
  }
  esp_err_t ret;
  if (esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_IDLE) {
    if ((ret = esp_bt_controller_init(&cfg)) != ESP_OK) {
      ESP_LOGE(BT_APP_TAG, "esp_bt_controller_init failed: %d", ret);
      return false;
    }
    while (esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_IDLE) {
      delay_ms(100);
    }
  }
  if (esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_INITED) {
    if ((ret = esp_bt_controller_enable(bt_mode)) != ESP_OK) {
      ESP_LOGE(BT_APP_TAG, "BT Enable failed: %d", ret);
      return false;
    }
  }
  if (esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_ENABLED) {
    ESP_LOGI(BT_APP_TAG, "BT enabled");
    return true;
  }
  ESP_LOGE(BT_APP_TAG, "BT Start failed");
  return false;
#endif
}

esp_err_t BluetoothA2DPCommon::bluedroid_init() {
  if (is_bluedroid_initialized) return ESP_OK;
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 2, 1)
  return esp_bluedroid_init_with_cfg(&bluedroid_config);
#else
  return esp_bluedroid_init();
#endif
}

void BluetoothA2DPCommon::app_task_start_up() {
  ESP_LOGD(BT_AV_TAG, "%s", __func__);
  if (app_task_queue == nullptr){
    app_task_queue = xQueueCreate(event_queue_size, sizeof(bt_app_msg_t));
  }

  if (app_task_handle == nullptr) {
    if (xTaskCreatePinnedToCore(ccall_bt_app_task_handler, "BtAppT",
                                event_stack_size, nullptr, task_priority,
                                &app_task_handle, task_core) != pdPASS) {
      ESP_LOGE(BT_APP_TAG, "%s failed", __func__);
    }
  }
}

void BluetoothA2DPCommon::app_task_shut_down(void) {
  ESP_LOGD(BT_AV_TAG, "%s", __func__);
  if (app_task_handle != nullptr) {
    vTaskDelete(app_task_handle);
    app_task_handle = nullptr;
  }

  if (app_task_queue != nullptr) {
    QueueHandle_t queue = app_task_queue;
    app_task_queue = nullptr;
    vQueueDelete(queue);
  }
}

void BluetoothA2DPCommon::app_task_handler(void* arg) {
  ESP_LOGI(BT_AV_TAG, "%s", __func__);
  bt_app_msg_t msg;

  for (;;) {
    while (app_task_queue == nullptr) {
      ESP_LOGW(BT_APP_TAG, "app_task_queue is null");
      delay_ms(1000);
    }

    /* receive message from work queue and handle it */
    if (pdTRUE ==
        xQueueReceive(app_task_queue, &msg, (TickType_t)portMAX_DELAY)) {
      ESP_LOGD(BT_APP_TAG, "%s, signal: 0x%x, event: 0x%x", __func__, msg.sig,
               msg.event);

      switch (msg.sig) {
        case BT_APP_SIG_WORK_DISPATCH:
          app_work_dispatched(&msg);
          break;
        default:
          ESP_LOGW(BT_APP_TAG, "%s, unhandled signal: %d", __func__, msg.sig);
          break;
      }

      if (msg.param) {
        free(msg.param);
      }
    }
  }
}

void BluetoothA2DPCommon::app_work_dispatched(bt_app_msg_t* msg) {
  ESP_LOGD(BT_AV_TAG, "%s", __func__);
  if (msg->cb) {
    msg->cb(msg->event, msg->param);
  }
}

bool BluetoothA2DPCommon::app_send_msg(bt_app_msg_t* msg) {
  if (msg == nullptr || app_task_queue == nullptr) {
    return false;
  }

  if (xQueueSend(app_task_queue, msg, 10 / portTICK_PERIOD_MS) != pdTRUE) {
    ESP_LOGE(BT_APP_TAG, "%s xQueue send failed", __func__);
    return false;
  }
  return true;
}

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 0, 0)

/// converts a esp_a2d_audio_state_t to a string
const char* BluetoothA2DPCommon::to_str(esp_avrc_playback_stat_t state) {
  if (state == esp_avrc_playback_stat_t::ESP_AVRC_PLAYBACK_ERROR)
    return "error";
  else {
    return m_avrc_playback_state_str[state];
  }
}

/// Defines if the bluetooth is discoverable
void BluetoothA2DPCommon::set_discoverability(esp_bt_discovery_mode_t d) {
  discoverability = d;
  if (get_connection_state() == ESP_A2D_CONNECTION_STATE_DISCONNECTED ||
      d != ESP_BT_NON_DISCOVERABLE) {
    esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, discoverability);
  }
}

/// Defines if the bluetooth is connectable
void BluetoothA2DPCommon::set_scan_mode_connectable(bool connectable) {
  ESP_LOGI(BT_AV_TAG, "set_scan_mode_connectable %s",
           connectable ? "true" : "false");
  if (connectable) {
    if (esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, discoverability) !=
        ESP_OK) {
      ESP_LOGE(BT_AV_TAG, "esp_bt_gap_set_scan_mode");
    }
  } else {
    if (esp_bt_gap_set_scan_mode(ESP_BT_NON_CONNECTABLE,
                                 ESP_BT_NON_DISCOVERABLE)) {
      ESP_LOGE(BT_AV_TAG, "esp_bt_gap_set_scan_mode");
    }
  }
}
#else

void BluetoothA2DPCommon::set_scan_mode_connectable(bool connectable) {
  ESP_LOGI(BT_AV_TAG, "set_scan_mode_connectable %s",
           connectable ? "true" : "false");
  if (connectable) {
    if (esp_bt_gap_set_scan_mode(ESP_BT_SCAN_MODE_CONNECTABLE_DISCOVERABLE) !=
        ESP_OK) {
      ESP_LOGE(BT_AV_TAG, "esp_bt_gap_set_scan_mode");
    }
  } else {
    if (esp_bt_gap_set_scan_mode(ESP_BT_SCAN_MODE_NONE) != ESP_OK) {
      ESP_LOGE(BT_AV_TAG, "esp_bt_gap_set_scan_mode");
    }
  }
}

#endif

void BluetoothA2DPCommon::delay_ms(uint32_t millis) {
#ifdef ARDUINO
  delay(millis);
#else
  const TickType_t xDelay = millis / portTICK_PERIOD_MS;
  vTaskDelay(xDelay);
#endif
}

unsigned long BluetoothA2DPCommon::get_millis() {
#ifdef ARDUINO
  return millis();
#else
  return (unsigned long)(esp_timer_get_time() / 1000ULL);
#endif
}

#endif // platform