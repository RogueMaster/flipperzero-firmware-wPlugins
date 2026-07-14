// C++ Class Implementation for a A2DP Source to be used as Arduino Library
// The original ESP32 implementation can be found at
// https://github.com/espressif/esp-idf/tree/master/examples/bluetooth/bluedroid/classic_bt/a2dp_source
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Copyright 2020 Phil Schatzmann
// Copyright 2015-2016 Espressif Systems (Shanghai) PTE LTD

#pragma once

#include <vector>

#include "BluetoothA2DPCommon.h"

#if IS_VALID_PLATFORM

#ifdef ARDUINO
#include "Stream.h"
#endif

#if (ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 0, 0))
#define TIMER_ARG_TYPE void *
#else
#define TIMER_ARG_TYPE tmrTimerControl *
#endif

typedef int32_t (*music_data_cb_t)(uint8_t *data, int32_t len);
typedef int32_t (*music_data_frames_cb_t)(Frame *data, int32_t len);
typedef void (*bt_app_copy_cb_t)(bt_app_msg_t *msg, void *p_dest, void *p_src);
typedef void (*bt_app_cb_t)(uint16_t event, void *param);

extern "C" void ccall_a2d_app_heart_beat(TIMER_ARG_TYPE arg);
extern "C" void ccall_bt_app_av_sm_hdlr(uint16_t event, void *param);
extern "C" void ccall_bt_av_hdl_avrc_ct_evt(uint16_t event, void *param);
extern "C" int32_t ccall_bt_app_a2d_data_cb(uint8_t *data, int32_t len);

/**
 * @brief Buetooth A2DP global state
 * @ingroup a2dp
 */
enum APP_AV_STATE {
  APP_AV_STATE_IDLE,
  APP_AV_STATE_DISCOVERING,
  APP_AV_STATE_DISCOVERED,
  APP_AV_STATE_UNCONNECTED,
  APP_AV_STATE_CONNECTING,
  APP_AV_STATE_CONNECTED,
  APP_AV_STATE_DISCONNECTING,
};

static const char *APP_AV_STATE_STR[] = {
    "IDLE",       "DISCOVERING", "DISCOVERED",   "UNCONNECTED",
    "CONNECTING", "CONNECTED",   "DISCONNECTING"};

/**
 * @brief A2DP Bluetooth Source
 * @ingroup a2dp
 * @author Phil Schatzmann
 * @copyright Apache License Version 2
 */

class BluetoothA2DPSource : public BluetoothA2DPCommon {
  friend void ccall_a2d_app_heart_beat(TIMER_ARG_TYPE arg);
  friend void ccall_bt_app_av_sm_hdlr(uint16_t event, void *param);
  friend void ccall_bt_av_hdl_avrc_ct_evt(uint16_t event, void *param);
  friend int32_t ccall_bt_app_a2d_data_cb(uint8_t *data, int32_t len);

 public:
  /// Constructor
  BluetoothA2DPSource();

  /// Destructor
  ~BluetoothA2DPSource();

  /// activate Secure Simple Pairing
  virtual void set_ssp_enabled(bool active) { this->ssp_enabled = active; }

  /// activate / deactivate the automatic reconnection to the last address (per
  /// default this is on)
  virtual void set_auto_reconnect(bool active) {
    this->reconnect_status = active ? AutoReconnect : NoReconnect;
  }

  /// automatically tries to reconnect to the indicated address
  virtual void set_auto_reconnect(esp_bd_addr_t addr) {
    set_auto_reconnect(true);
    memcpy(last_connection, addr, ESP_BD_ADDR_LEN);
  }

  /// Defines the local name
  virtual void set_local_name(const char *name) { dev_name = name; }

  /// Defines the data callback
  virtual void set_data_callback(int32_t(cb)(uint8_t *data, int32_t len)) {
    get_data_cb = cb;
  }

  /// Defines the data callback
  virtual void set_data_callback_in_frames(int32_t(cb)(Frame *data,
                                                       int32_t len)) {
    get_data_in_frames_cb = cb;
  }

#ifdef ARDUINO

  /// Defines a single Arduino Stream (e.g. File) as audio source
  virtual void set_data_source(Stream &data) { p_stream = &data; }
  /// Provide a callback which provides streams
  virtual void set_data_source_callback(Stream &(*next_stream)()) {
    get_next_stream_cb = next_stream;
  }
#endif

  /// Starts the A2DP source w/o indicating any names: use the ssid callback to
  /// select the device
  virtual void start() {
    std::vector<const char *> names;  // empty vector
    start(names);
  }

  /// Starts the A2DP source
  virtual void start(const char *name) {
    std::vector<const char *> names = {name};  // empty vector
    start(names);
  }

  /// Starts the first available A2DP source
  virtual void start(std::vector<const char *> names);

  /// Obsolete: use the start w/o callback and set the callback separately
  virtual void start(const char *name, music_data_frames_cb_t callback) {
    set_data_callback_in_frames(callback);
    std::vector<const char *> names = {name};
    start(names);
  }

  /// Obsolete: use the start w/o callback and set the callback separately
  virtual void start(music_data_frames_cb_t callback) {
    set_data_callback_in_frames(callback);
    start();
  }

  /// Obsolete: use the start w/o callback and set the callback separately
  virtual void start(std::vector<const char *> names,
                     music_data_frames_cb_t callback) {
    set_data_callback_in_frames(callback);
    start(names);
  }

  /// Obsolete: use the start w/o callback and set the callback separately
  virtual void start_raw(const char *name, music_data_cb_t callback = nullptr) {
    set_data_callback(callback);
    start(name);
  }

  /// Obsolete: use the start w/o callback and set the callback separately
  virtual void start_raw(std::vector<const char *> names,
                         music_data_cb_t callback = nullptr) {
    set_data_callback(callback);
    start(names);
  }

  /// Obsolete: use the start w/o callback and set the callback separately
  virtual void start_raw(music_data_cb_t callback = nullptr) {
    set_data_callback(callback);
    start();
  }

  /// Defines the pin code. If nothing is defined we use "1234"
  void set_pin_code(const char *pin_code, esp_bt_pin_type_t pin_type);

  /// Defines if the BLE should be reset on start
  virtual void set_reset_ble(bool doInit);

  /// Define callback to be notified about the found ssids
  virtual void set_ssid_callback(bool (*callback)(const char *ssid,
                                                  esp_bd_addr_t address,
                                                  int rrsi)) {
    ssid_callback = callback;
  }

  /// Define callback to be notified about bt discovery mode state changes
  virtual void set_discovery_mode_callback(
      void (*callback)(esp_bt_gap_discovery_state_t discoveryMode)) {
    discovery_mode_callback = callback;
  }

  /// Provides the current discovery state: returns true when the discovery is
  /// in progress
  virtual bool is_discovery_active() { return discovery_active; }

  /// Defines the valid esp_bt_cod_srvc_t values that are used to identify an
  /// audio service. e.g (ESP_BT_COD_SRVC_RENDERING | ESP_BT_COD_SRVC_AUDIO |
  /// ESP_BT_COD_SRVC_TELEPHONY)
  virtual void set_valid_cod_service(uint16_t filter) {
    this->valid_cod_services = filter;
  }

  /// Define the handler fur button presses on the remote bluetooth speaker
  virtual void set_avrc_passthru_command_callback(void (*cb)(uint8_t key,
                                                             bool isReleased)) {
    passthru_command_callback = cb;
    is_passthru_active = (cb != nullptr);
  }

  /// Ends the processing and releases the resources
  void end(bool releaseMemory=false) override;

 protected:
  /// callback for data
  int32_t (*get_data_cb)(uint8_t *data, int32_t len) = nullptr;
  int32_t (*get_data_in_frames_cb)(Frame *data, int32_t len) = nullptr;
#ifdef ARDUINO
  Stream *p_stream = nullptr;
  Stream &(*get_next_stream_cb)() = nullptr;
#endif
  const char *dev_name = "ESP32_A2DP_SRC";
  bool ssp_enabled = false;
  std::vector<const char *> bt_names;
  esp_bt_pin_type_t pin_type;
  esp_bt_pin_code_t pin_code;
  uint32_t pin_code_len;
  uint8_t s_peer_bdname[ESP_BT_GAP_MAX_BDNAME_LEN + 1];
  APP_AV_STATE s_a2d_state = APP_AV_STATE_IDLE;  // Next Target Connection State
  APP_AV_STATE s_a2d_last_state =
      APP_AV_STATE_IDLE;  // Next Target Connection State
  int s_media_state = 0;
  int s_intv_cnt = 0;
  int s_connecting_heatbeat_count;
  uint32_t s_pkt_cnt;
  TimerHandle_t s_tmr;

  // initialization
  bool reset_ble = false;
  bool discovery_active = false;
  uint16_t valid_cod_services = ESP_BT_COD_SRVC_RENDERING |
                                ESP_BT_COD_SRVC_AUDIO |
                                ESP_BT_COD_SRVC_TELEPHONY;

  bool (*ssid_callback)(const char *ssid, esp_bd_addr_t address,
                        int rrsi) = nullptr;
  void (*discovery_mode_callback)(esp_bt_gap_discovery_state_t discoveryMode) =
      nullptr;
  void (*passthru_command_callback)(uint8_t, bool) = nullptr;
  bool is_passthru_active = false;
  bool is_end = false;

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 0, 0)
  esp_avrc_rn_evt_cap_mask_t s_avrc_peer_rn_cap;
#endif

  void app_gap_callback(esp_bt_gap_cb_event_t event,
                        esp_bt_gap_cb_param_t *param) override;
  void app_rc_ct_callback(esp_avrc_ct_cb_event_t event,
                          esp_avrc_ct_cb_param_t *param) override;
  void app_a2d_callback(esp_a2d_cb_event_t event,
                        esp_a2d_cb_param_t *param) override;
  void av_hdl_stack_evt(uint16_t event, void *p_param) override;

  /// provides the audio data to be sent
  virtual int32_t get_audio_data(uint8_t *data, int32_t len);
  /// provides the audio after applying the volume
  virtual int32_t get_audio_data_volume(uint8_t *data, int32_t len);
  

  virtual void process_user_state_callbacks(uint16_t event, void *param);

  virtual bool bt_app_work_dispatch(bt_app_cb_t p_cback, uint16_t event,
                                    void *p_params, int param_len,
                                    bt_app_copy_cb_t p_copy_cback);
  virtual void bt_app_av_media_proc(uint16_t event, void *param);

  /// A2DP application state machine handler for each state
  virtual void bt_app_av_state_unconnected_hdlr(uint16_t event, void *param);
  virtual void bt_app_av_state_connecting_hdlr(uint16_t event, void *param);
  virtual void bt_app_av_state_connected_hdlr(uint16_t event, void *param);
  virtual void bt_app_av_state_disconnecting_hdlr(uint16_t event, void *param);

  virtual bool get_name_from_eir(uint8_t *eir, uint8_t *bdname,
                                 uint8_t *bdname_len);
  virtual void filter_inquiry_scan_result(esp_bt_gap_cb_param_t *param);

  virtual const char *last_bda_nvs_name() { return "src_bda"; }

  virtual void a2d_app_heart_beat(void *arg);
  /// A2DP application state machine
  virtual void bt_app_av_sm_hdlr(uint16_t event, void *param);
  /// avrc CT event handler
  virtual void bt_av_hdl_avrc_ct_evt(uint16_t event, void *p_param);
  /// resets the last connectioin so that we can reconnect
  virtual void reset_last_connection();
  /// returns true for
  /// ESP_BT_COD_SRVC_RENDERING,ESP_BT_COD_SRVC_AUDIO,ESP_BT_COD_SRVC_TELEPHONY
  virtual bool is_valid_cod_service(uint32_t cod);

  esp_err_t esp_a2d_connect(esp_bd_addr_t peer) override {
    ESP_LOGI(BT_AV_TAG, "==> a2dp connecting to: %s", to_str(peer));
    s_media_state = 0;
    s_a2d_state = APP_AV_STATE_CONNECTING;
    return esp_a2d_source_connect(peer);
  }

  esp_err_t esp_a2d_disconnect(esp_bd_addr_t peer) override {
    ESP_LOGI(BT_AV_TAG, "==> a2dp esp_a2d_source_disconnect from: %s",
             to_str(peer));
    esp_a2d_media_ctrl(ESP_A2D_MEDIA_CTRL_STOP);
    return esp_a2d_source_disconnect(peer);
  }

  /// converts a APP_AV_STATE_ENUM to a string
  const char *to_state_str(int state) { return APP_AV_STATE_STR[state]; }

  void set_scan_mode_connectable_default() override {
    set_scan_mode_connectable(false);
  }

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 0, 0)
  virtual void bt_av_notify_evt_handler(uint8_t event, esp_avrc_rn_param_t *param);
  virtual void bt_av_volume_changed(void);
  virtual void app_rc_tg_callback(esp_avrc_tg_cb_event_t event,
                          esp_avrc_tg_cb_param_t *param) override;
  virtual void av_hdl_avrc_tg_evt(uint16_t event, void *p_param) override;
  virtual bool isSource() { return true;}

#endif
};


#endif // platform