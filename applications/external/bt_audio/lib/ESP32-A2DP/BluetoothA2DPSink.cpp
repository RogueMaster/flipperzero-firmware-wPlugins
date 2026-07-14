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
// Copyright 2015-2016 Espressif Systems (Shanghai) PTE LTD

#include "BluetoothA2DPSink.h"

#if IS_VALID_PLATFORM

// to support static callback functions
BluetoothA2DPSink *actual_bluetooth_a2dp_sink;

extern "C" void ccall_i2s_task_handler(void *arg) {
  ESP_LOGD(BT_AV_TAG, "%s", __func__);
  if (actual_bluetooth_a2dp_sink)
    actual_bluetooth_a2dp_sink->i2s_task_handler(arg);
}

extern "C" void ccall_audio_data_callback(const uint8_t *data, uint32_t len) {
  // ESP_LOGD(BT_AV_TAG, "%s", __func__);
  if (actual_bluetooth_a2dp_sink)
    actual_bluetooth_a2dp_sink->audio_data_callback(data, len);
}

extern "C" void ccall_av_hdl_avrc_evt(uint16_t event, void *param) {
  ESP_LOGD(BT_AV_TAG, "%s", __func__);
  if (actual_bluetooth_a2dp_sink) {
    actual_bluetooth_a2dp_sink->av_hdl_avrc_evt(event, param);
  }
}

extern "C" void ccall_av_hdl_a2d_evt(uint16_t event, void *param) {
  ESP_LOGD(BT_AV_TAG, "%s", __func__);
  if (actual_bluetooth_a2dp_sink) {
    actual_bluetooth_a2dp_sink->av_hdl_a2d_evt(event, param);
  }
}

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 5, 0)
extern "C" void ccall_audio_encoded_callback(esp_a2d_conn_hdl_t conn_hdl,
                                             esp_a2d_audio_buff_t *audio_buf) {
  ESP_LOGI(BT_AV_TAG, "ccall_audio_encoded_callback");
  if (actual_bluetooth_a2dp_sink &&
      actual_bluetooth_a2dp_sink->encoded_stream_reader && audio_buf) {
    // pass raw encoded bytes
    ESP_LOGI(BT_AV_TAG, "encoded_stream_reader=%d", (int)audio_buf->data_len);
    actual_bluetooth_a2dp_sink->encoded_stream_reader(audio_buf->data,
                                                      audio_buf->data_len);
  }
  if (audio_buf) {
    esp_a2d_audio_buff_free(audio_buf);
  }
}
#endif

/**
 * Constructor
 */
BluetoothA2DPSink::BluetoothA2DPSink() {
  actual_bluetooth_a2dp_sink = this;
  actual_bluetooth_a2dp_common = this;

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 0, 0)
  s_avrc_peer_rn_cap.bits = 0;
  _lock_init(&s_volume_lock);
#endif
}

BluetoothA2DPSink::~BluetoothA2DPSink() {
  if (app_task_queue != nullptr) {
    end();
  }
}

void BluetoothA2DPSink::end(bool release_memory) {
  ESP_LOGI(BT_AV_TAG, "%s", __func__);
  // reconnect should not work after end
  if (is_autoreconnect_allowed) {
     is_autoreconnect_allowed = false;
     // https://github.com/pschatzmann/ESP32-A2DP/issues/750
     if (release_memory && !avrc_connection_state) delay(2100); // give it some time to end
  }


  BluetoothA2DPCommon::end(release_memory);

  if (is_output) {
    out->end();
  }
}

void BluetoothA2DPSink::set_stream_reader(void (*callBack)(const uint8_t *,
                                                           uint32_t),
                                          bool is_i2s) {
  this->stream_reader = callBack;
  this->is_output = is_i2s;
}

void BluetoothA2DPSink::set_raw_stream_reader(void (*callBack)(const uint8_t *,
                                                               uint32_t)) {
  this->raw_stream_reader = callBack;
}

void BluetoothA2DPSink::set_on_data_received(void (*callBack)()) {
  this->data_received = callBack;
}

// kept for backwards compatibility
void BluetoothA2DPSink::set_on_volumechange(void (*callBack)(int)) {
  this->bt_volumechange = callBack;
}

void BluetoothA2DPSink::set_avrc_rn_volumechange(void (*callBack)(int)) {
  this->bt_volumechange = callBack;
}

void BluetoothA2DPSink::set_avrc_rn_volumechange_completed(
    void (*callBack)(int)) {
  this->avrc_rn_volchg_complete_callback = callBack;
}

void BluetoothA2DPSink::start(const char *name, bool auto_reconnect) {
  set_auto_reconnect(auto_reconnect, AUTOCONNECT_TRY_NUM);
  start(name);
}
/**
 * Main function to start the Bluetooth Processing
 */
void BluetoothA2DPSink::start(const char *name) {
  ESP_LOGD(BT_AV_TAG, "%s", __func__);
  log_free_heap();

  is_autoreconnect_allowed = (reconnect_status == AutoReconnect);

  if (is_start_disabled) {
    ESP_LOGE(BT_AV_TAG, "re-start not supported after end(true)");
    return;
  }

  // store parameters
  if (name) {
    this->bt_name = name;
  }
  ESP_LOGI(BT_AV_TAG, "Device name will be set to '%s'", this->bt_name);

  // Initialize NVS
  init_nvs();
  if (is_autoreconnect_allowed) {
    // reconnect management
    // grab last connnectiom, even if we dont use it now for auto reconnect
    get_last_connection();

    memcpy(peer_bd_addr, last_connection, ESP_BD_ADDR_LEN);

    // trigger timeout
    delay_ms(reconnect_delay);
  }

  // setup i2s
  init_i2s();

  // setup bluetooth
  init_bluetooth();

  // create application task
  app_task_start_up();

  // Bluetooth device name, connection mode and profile set up
  app_work_dispatch(ccall_av_hdl_stack_evt, BT_APP_EVT_STACK_UP, nullptr, 0);

  // handle security pin
  if (is_pin_code_active) {
    // Set default parameters for Secure Simple Pairing
    esp_bt_sp_param_t param_type = ESP_BT_SP_IOCAP_MODE;
    esp_bt_io_cap_t iocap = ESP_BT_IO_CAP_IO;
    esp_bt_gap_set_security_param(param_type, &iocap, sizeof(uint8_t));
    // invokes callbacks
    esp_bt_pin_type_t pin_type = ESP_BT_PIN_TYPE_VARIABLE;
    esp_bt_pin_code_t pin_code;
    esp_bt_gap_set_pin(pin_type, 0, pin_code);

  } else {
    // Set default parameters for Secure Simple Pairing
    esp_bt_sp_param_t param_type = ESP_BT_SP_IOCAP_MODE;
    esp_bt_io_cap_t iocap = ESP_BT_IO_CAP_NONE;
    esp_bt_gap_set_security_param(param_type, &iocap, sizeof(uint8_t));
    // no callbacks
    esp_bt_pin_type_t pin_type = ESP_BT_PIN_TYPE_FIXED;
    esp_bt_pin_code_t pin_code;
    esp_bt_gap_set_pin(pin_type, 0, pin_code);
  }

  ESP_LOGI(BT_AV_TAG, "IDF Version %d.%d", ESP_IDF_VERSION_MAJOR,
           ESP_IDF_VERSION_MINOR);
  log_free_heap();
}

void BluetoothA2DPSink::init_i2s() {
  ESP_LOGI(BT_AV_TAG, "init_i2s");
  if (is_output) {
    out->begin();
    is_i2s_active = true;
  }
}

esp_a2d_mct_t BluetoothA2DPSink::get_audio_type() { return audio_type; }

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 5, 0)

bool BluetoothA2DPSink::set_codec(A2DPCodec codec,
                                  void (*encoded_cb)(const uint8_t *data,
                                                     size_t len)) {
  encoded_stream_reader = encoded_cb;
  ESP_LOGI(BT_AV_TAG, "set_codec() called with codec=%d", codec);
  is_output = false;
  desired_codec = codec;
  ESP_LOGD(BT_AV_TAG, "set_codec: is_bluedroid_initialized=%d",
           is_bluedroid_initialized);
  if (!is_bluedroid_initialized) {
    ESP_LOGD(BT_AV_TAG,
             "set_codec: will register later (bluetooth not initialized)");
    return true;  // will register later
  }
  ESP_LOGD(BT_AV_TAG, "set_codec: codec_sep_registered=%d",
           codec_sep_registered);
  if (codec_sep_registered) {
    ESP_LOGD(BT_AV_TAG, "set_codec: SEP already registered");
    return true;
  }
  esp_a2d_mcc_t mcc{};
  ESP_LOGD(BT_AV_TAG, "set_codec: preparing mcc struct");
  switch (codec) {
    case A2DP_CODEC_SBC:
      mcc.type = ESP_A2D_MCT_SBC;
      mcc.cie.sbc_info.samp_freq =
          ESP_A2D_SBC_CIE_SF_44K | ESP_A2D_SBC_CIE_SF_48K;
      mcc.cie.sbc_info.ch_mode =
          ESP_A2D_SBC_CIE_CH_MODE_STEREO | ESP_A2D_SBC_CIE_CH_MODE_JOINT_STEREO;
      mcc.cie.sbc_info.block_len = ESP_A2D_SBC_CIE_BLOCK_LEN_16;
      mcc.cie.sbc_info.alloc_mthd = ESP_A2D_SBC_CIE_ALLOC_MTHD_LOUDNESS;
      mcc.cie.sbc_info.num_subbands = ESP_A2D_SBC_CIE_NUM_SUBBANDS_8;
      mcc.cie.sbc_info.min_bitpool = 2;
      mcc.cie.sbc_info.max_bitpool = 250;
      break;
    case A2DP_CODEC_M12:
      mcc.type = ESP_A2D_MCT_M12;  // may not be supported
      break;
    case A2DP_CODEC_AAC:
      mcc.type = ESP_A2D_MCT_M24;  // AAC (MPEG-2/4) - may not be supported
      break;
    case A2DP_CODEC_ATRAC:
      mcc.type = ESP_A2D_MCT_ATRAC;  // may not be supported
      break;
  }
  ESP_LOGI(BT_AV_TAG,
           "set_codec: calling esp_a2d_sink_register_stream_endpoint");
  esp_err_t err = esp_a2d_sink_register_stream_endpoint(0, &mcc);
  ESP_LOGD(BT_AV_TAG,
           "set_codec: esp_a2d_sink_register_stream_endpoint returned %d", err);
  if (err == ESP_OK) {
    codec_sep_registered = true;
    ESP_LOGI(BT_AV_TAG, "Registered SEP for codec type %d", mcc.type);
    if (encoded_cb) {
      ESP_LOGD(BT_AV_TAG, "set_codec: encoded_cb provided");
      esp_err_t cb_err = esp_a2d_sink_register_audio_data_callback(
          ccall_audio_encoded_callback);
      ESP_LOGI(
          BT_AV_TAG,
          "set_codec: esp_a2d_sink_register_audio_data_callback returned %d",
          cb_err);
      if (cb_err != ESP_OK) {
        ESP_LOGW(BT_AV_TAG, "Failed to register encoded audio callback");
      }
    }

    return true;
  } else {
    ESP_LOGW(BT_AV_TAG, "Failed to register SEP codec %d err=%d", mcc.type,
             err);
    return false;
  }
}
#endif

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 0, 0)
const char *BluetoothA2DPSink::get_connected_source_name() {
  if (is_connected()) {
    return (remote_name);
  } else {
    return ("unknown");
  }
}
#endif

int BluetoothA2DPSink::init_bluetooth() {
  ESP_LOGD(BT_AV_TAG, "%s", __func__);
  if (!bt_start()) {
    ESP_LOGE(BT_AV_TAG, "Failed to initialize controller");
    return false;
  }
  ESP_LOGI(BT_AV_TAG, "controller initialized");

  esp_bluedroid_status_t bt_stack_status = esp_bluedroid_get_status();

  if (bt_stack_status == ESP_BLUEDROID_STATUS_UNINITIALIZED) {
    if (bluedroid_init() != ESP_OK) {
      ESP_LOGE(BT_AV_TAG, "Failed to initialize bluedroid");
      return false;
    }
    is_bluedroid_initialized = true;
    ESP_LOGI(BT_AV_TAG, "bluedroid initialized");
  }

  while (bt_stack_status != ESP_BLUEDROID_STATUS_ENABLED) {
    if (esp_bluedroid_enable() != ESP_OK) {
      ESP_LOGE(BT_AV_TAG, "Failed to enable bluedroid");
      delay_ms(100);
      // return false;
    } else {
      ESP_LOGI(BT_AV_TAG, "bluedroid enabled");
    }
    bt_stack_status = esp_bluedroid_get_status();
  }

  if (esp_bt_gap_register_callback(ccall_app_gap_callback) != ESP_OK) {
    ESP_LOGE(BT_AV_TAG, "gap register failed");
    return false;
  }

#if A2DP_SPP_SUPPORT  
# if (ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 0, 0))
  if (spp_active) {
    if ((esp_spp_init(esp_spp_mode)) != ESP_OK) {
      ESP_LOGE(BT_AV_TAG, "esp_spp_init failed");
      return false;
    }
  }
# else
  if (spp_active) {
    if ((esp_spp_enhanced_init(&spp_cfg)) != ESP_OK) {
      ESP_LOGE(BT_AV_TAG, "esp_spp_init failed");
      return false;
    }
  }
# endif
#endif


  return true;
}

bool BluetoothA2DPSink::app_work_dispatch(app_callback_t p_cback,
                                          uint16_t event, void *p_params,
                                          int param_len) {
  ESP_LOGD(BT_APP_TAG, "%s event 0x%x, param len %d", __func__, event,
           param_len);

  bt_app_msg_t msg;
  memset(&msg, 0, sizeof(bt_app_msg_t));

  msg.sig = APP_SIG_WORK_DISPATCH;
  msg.event = event;
  msg.cb = p_cback;

  if (param_len == 0) {
    return app_send_msg(&msg);
  } else if (p_params && param_len > 0) {
    if ((msg.param = malloc(param_len)) != nullptr) {
      memcpy(msg.param, p_params, param_len);
      return app_send_msg(&msg);
    }
  }

  return false;
}

void BluetoothA2DPSink::app_alloc_meta_buffer(esp_avrc_ct_cb_param_t *param) {
  ESP_LOGD(BT_AV_TAG, "%s", __func__);
  esp_avrc_ct_cb_param_t *rc = (esp_avrc_ct_cb_param_t *)(param);
  uint8_t *attr_text = (uint8_t *)malloc(rc->meta_rsp.attr_length + 1);
  memcpy(attr_text, rc->meta_rsp.attr_text, rc->meta_rsp.attr_length);
  attr_text[rc->meta_rsp.attr_length] = 0;

  rc->meta_rsp.attr_text = attr_text;
}

void BluetoothA2DPSink::app_gap_callback(esp_bt_gap_cb_event_t event,
                                         esp_bt_gap_cb_param_t *param) {
  switch (event) {
    case ESP_BT_GAP_AUTH_CMPL_EVT: {
      if (param->auth_cmpl.stat == ESP_BT_STATUS_SUCCESS) {
        ESP_LOGI(BT_AV_TAG, "authentication success: %s",
                 param->auth_cmpl.device_name);
        //  esp_log_buffer_hex(BT_AV_TAG, param->auth_cmpl.bda,
        //  ESP_BD_ADDR_LEN);

      } else {
        ESP_LOGE(BT_AV_TAG, "authentication failed, status:%d",
                 param->auth_cmpl.stat);
        // reset pin_code data to "undefined" after authentication failure
        // just like when in disconnected state
        pin_code_int = 0;
        pin_code_request = Undefined;
      }
      break;
    }

    case ESP_BT_GAP_PIN_REQ_EVT: {
      memcpy(peer_bd_addr, param->pin_req.bda, ESP_BD_ADDR_LEN);
      ESP_LOGI(BT_AV_TAG, "partner address: %s", to_str(peer_bd_addr));
    } break;

    case ESP_BT_GAP_CFM_REQ_EVT: {
      memcpy(peer_bd_addr, param->cfm_req.bda, ESP_BD_ADDR_LEN);
      ESP_LOGI(BT_AV_TAG, "partner address: %s", to_str(peer_bd_addr));

      ESP_LOGI(BT_AV_TAG,
               "ESP_BT_GAP_CFM_REQ_EVT Please confirm the passkey: %d",
               param->cfm_req.num_val);
      pin_code_int = param->key_notif.passkey;
      pin_code_request = Confirm;
    } break;

    case ESP_BT_GAP_KEY_NOTIF_EVT: {
      ESP_LOGI(BT_AV_TAG, "ESP_BT_GAP_KEY_NOTIF_EVT passkey:%d",
               param->key_notif.passkey);
      pin_code_int = param->key_notif.passkey;
      pin_code_request = Reply;
    } break;

    case ESP_BT_GAP_KEY_REQ_EVT: {
      ESP_LOGI(BT_AV_TAG, "ESP_BT_GAP_KEY_REQ_EVT Please enter passkey!");
      pin_code_request = Reply;
    } break;

    case ESP_BT_GAP_READ_RSSI_DELTA_EVT: {
      last_rssi_delta = param->read_rssi_delta;
      if (rssi_callback != nullptr) {
        rssi_callback(last_rssi_delta);
      }
      break;
    }

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 0, 0)
    case ESP_BT_GAP_READ_REMOTE_NAME_EVT: {
      ESP_LOGI(BT_AV_TAG, "ESP_BT_GAP_READ_REMOTE_NAME_EVT stat:%d",
               param->read_rmt_name.stat);
      if (param->read_rmt_name.stat == ESP_BT_STATUS_SUCCESS) {
        ESP_LOGI(BT_AV_TAG, "ESP_BT_GAP_READ_REMOTE_NAME_EVT remote name:%s",
                 param->read_rmt_name.rmt_name);
        memcpy(remote_name, param->read_rmt_name.rmt_name,
               ESP_BT_GAP_MAX_BDNAME_LEN);
        if (peer_name_callback != nullptr) {
          peer_name_callback(remote_name);
        }
      }
    } break;

    case ESP_BT_GAP_MODE_CHG_EVT: {
      ESP_LOGI(BT_AV_TAG, "ESP_BT_GAP_MODE_CHG_EVT");
      log_free_heap();
    } break;
#endif

    default: {
      ESP_LOGI(BT_AV_TAG, "event: %d", event);
      break;
    }
  }
  return;
}

void BluetoothA2DPSink::app_rc_ct_callback(esp_avrc_ct_cb_event_t event,
                                           esp_avrc_ct_cb_param_t *param) {
  ESP_LOGD(BT_AV_TAG, "%s", __func__);

  switch (event) {
    case ESP_AVRC_CT_METADATA_RSP_EVT:
      ESP_LOGD(BT_AV_TAG, "%s ESP_AVRC_CT_METADATA_RSP_EVT", __func__);
      app_alloc_meta_buffer(param);
      app_work_dispatch(ccall_av_hdl_avrc_evt, event, param,
                        sizeof(esp_avrc_ct_cb_param_t));
      break;
    case ESP_AVRC_CT_CONNECTION_STATE_EVT:
      ESP_LOGD(BT_AV_TAG, "%s ESP_AVRC_CT_CONNECTION_STATE_EVT", __func__);
      app_work_dispatch(ccall_av_hdl_avrc_evt, event, param,
                        sizeof(esp_avrc_ct_cb_param_t));
      break;
    case ESP_AVRC_CT_PASSTHROUGH_RSP_EVT:
      ESP_LOGD(BT_AV_TAG, "%s ESP_AVRC_CT_PASSTHROUGH_RSP_EVT", __func__);
      app_work_dispatch(ccall_av_hdl_avrc_evt, event, param,
                        sizeof(esp_avrc_ct_cb_param_t));
      break;
    case ESP_AVRC_CT_CHANGE_NOTIFY_EVT:
      ESP_LOGD(BT_AV_TAG, "%s ESP_AVRC_CT_CHANGE_NOTIFY_EVT", __func__);
      app_work_dispatch(ccall_av_hdl_avrc_evt, event, param,
                        sizeof(esp_avrc_ct_cb_param_t));
      break;
    case ESP_AVRC_CT_REMOTE_FEATURES_EVT: {
      ESP_LOGD(BT_AV_TAG, "%s ESP_AVRC_CT_REMOTE_FEATURES_EVT", __func__);
      app_work_dispatch(ccall_av_hdl_avrc_evt, event, param,
                        sizeof(esp_avrc_ct_cb_param_t));
      break;
    }

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 0, 0)

    case ESP_AVRC_CT_GET_RN_CAPABILITIES_RSP_EVT: {
      ESP_LOGD(BT_AV_TAG, "%s ESP_AVRC_CT_GET_RN_CAPABILITIES_RSP_EVT",
               __func__);
      app_work_dispatch(ccall_av_hdl_avrc_evt, event, param,
                        sizeof(esp_avrc_ct_cb_param_t));
      break;
    }
#endif

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 5, 0)

    case ESP_AVRC_CT_PROF_STATE_EVT: {
      ESP_LOGD(BT_AV_TAG, "%s ESP_AVRC_CT_PROF_STATE_EVT",
               __func__);
      app_work_dispatch(ccall_av_hdl_avrc_evt, event, param,
                        sizeof(esp_avrc_ct_cb_param_t));
      break;
    }
#endif


    default:
      ESP_LOGE(BT_AV_TAG, "Invalid AVRC event: %d", event);
      break;
  }
}

void BluetoothA2DPSink::av_hdl_a2d_evt(uint16_t event, void *p_param) {
  ESP_LOGD(BT_AV_TAG, "%s evt %d", __func__, event);
  esp_a2d_cb_param_t *a2d = nullptr;
  switch (event) {
    case ESP_A2D_CONNECTION_STATE_EVT: {
      ESP_LOGD(BT_AV_TAG, "%s ESP_A2D_CONNECTION_STATE_EVT", __func__);
      handle_connection_state(event, p_param);
    } break;

    case ESP_A2D_AUDIO_STATE_EVT: {
      ESP_LOGD(BT_AV_TAG, "%s ESP_A2D_AUDIO_STATE_EVT", __func__);
      handle_audio_state(event, p_param);

    } break;
    case ESP_A2D_AUDIO_CFG_EVT: {
      ESP_LOGD(BT_AV_TAG, "%s ESP_A2D_AUDIO_CFG_EVT", __func__);
      handle_audio_cfg(event, p_param);

    } break;

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 0, 0)

    case ESP_A2D_PROF_STATE_EVT: {
      a2d = (esp_a2d_cb_param_t *)(p_param);
      if (ESP_A2D_INIT_SUCCESS == a2d->a2d_prof_stat.init_state) {
        ESP_LOGI(BT_AV_TAG, "A2DP PROF STATE: Init Compl\n");
      } else {
        ESP_LOGI(BT_AV_TAG, "A2DP PROF STATE: Deinit Compl\n");
      }
    } break;

#endif

    default:
      ESP_LOGE(BT_AV_TAG, "%s unhandled evt %d", __func__, event);
      break;
  }
}

void BluetoothA2DPSink::handle_audio_cfg(uint16_t event, void *p_param) {
  ESP_LOGD(BT_AV_TAG, "%s evt %d", __func__, event);
  esp_a2d_cb_param_t *a2d = (esp_a2d_cb_param_t *)(p_param);
  audio_type = a2d->audio_cfg.mcc.type;
  ESP_LOGI(BT_AV_TAG, "a2dp audio_cfg_cb , codec type %d",
           a2d->audio_cfg.mcc.type);

  // determine sample rate
  int sample_rate = 0;
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 5, 0)
  // Use new sbc_info (avoid deprecated sbc[] array)
  uint8_t sf = a2d->audio_cfg.mcc.cie.sbc_info.samp_freq;
#ifdef ESP_A2D_SBC_CIE_SF_32K
  if (sf & ESP_A2D_SBC_CIE_SF_32K) {
    sample_rate = 32000;
  } else
#endif
      if (sf & ESP_A2D_SBC_CIE_SF_44K) {
    sample_rate = 44100;
  } else if (sf & ESP_A2D_SBC_CIE_SF_48K) {
    sample_rate = 48000;
  }
#else
  // Legacy field parsing (older IDF)
  char oct0 = a2d->audio_cfg.mcc.cie.sbc[0];
  if (oct0 & (0x01 << 6)) {
    sample_rate = 32000;
  } else if (oct0 & (0x01 << 5)) {
    sample_rate = 44100;
  } else if (oct0 & (0x01 << 4)) {
    sample_rate = 48000;
  }
#endif

  // for now only SBC stream is supported
  if (a2d->audio_cfg.mcc.type == ESP_A2D_MCT_SBC) {
    // determine channel count from negotiated SBC channel mode
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 5, 0)
    uint8_t ch_mode = a2d->audio_cfg.mcc.cie.sbc_info.ch_mode;
    if (ch_mode == ESP_A2D_SBC_CIE_CH_MODE_MONO) {
      m_channels = 1;
    } else {
      m_channels = 2;  // dual/stereo/joint are all rendered as 2 channels
    }
#else
    // legacy parsing: octet 1 bits 0..3 represent channel mode; only one is set
    uint8_t oct1 = a2d->audio_cfg.mcc.cie.sbc[1];
    // Bit masks per A2DP spec for channel mode (mono bit3, dual bit2, stereo
    // bit1, joint bit0)
    if (oct1 & 0x08) {
      m_channels = 1;  // mono
    } else {
      m_channels = 2;  // any other mode
    }
#endif
    ESP_LOGI(BT_AV_TAG, "a2dp audio_cfg_cb , channels %d", m_channels);
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 5, 0)
    ESP_LOGI(BT_AV_TAG, "configure audio player %x-%x-%x-%x\n",
             (int)a2d->audio_cfg.mcc.cie.sbc_info.samp_freq,
             (int)a2d->audio_cfg.mcc.cie.sbc_info.ch_mode,
             (int)a2d->audio_cfg.mcc.cie.sbc_info.block_len,
             (int)a2d->audio_cfg.mcc.cie.sbc_info.alloc_mthd);
#else
    ESP_LOGI(
        BT_AV_TAG, "configure audio player %x-%x-%x-%x\n",
        (int)a2d->audio_cfg.mcc.cie.sbc[0], (int)a2d->audio_cfg.mcc.cie.sbc[1],
        (int)a2d->audio_cfg.mcc.cie.sbc[2], (int)a2d->audio_cfg.mcc.cie.sbc[3]);
#endif

    ESP_LOGI(BT_AV_TAG, "a2dp audio_cfg_cb , sample_rate %u", m_sample_rate);
  }

  // inform caller about new values
  if (sample_rate != 0) {
    m_sample_rate = sample_rate;
    // act on determined data
    if (sample_rate_callback != nullptr) {
      sample_rate_callback(m_sample_rate);
    }
    out->set_sample_rate(m_sample_rate);
  }
}

void BluetoothA2DPSink::handle_avrc_connection_state(bool connected) {
  ESP_LOGD(BT_AV_TAG, "%s state %d", __func__, connected);
  avrc_connection_state = connected;
  if (avrc_connection_state_callback != nullptr) {
    avrc_connection_state_callback(connected);
  }
}

void BluetoothA2DPSink::handle_audio_state(uint16_t event, void *p_param) {
  ESP_LOGD(BT_AV_TAG, "%s evt %d", __func__, event);
  esp_a2d_cb_param_t *a2d = (esp_a2d_cb_param_t *)(p_param);
  ESP_LOGI(BT_AV_TAG, "A2DP audio state: %s", to_str(a2d->audio_stat.state));

  // callback on state change
  audio_state = a2d->audio_stat.state;
  if (audio_state_callback != nullptr) {
    audio_state_callback(a2d->audio_stat.state, audio_state_obj);
  }

  if (ESP_A2D_AUDIO_STATE_STARTED == a2d->audio_stat.state) {
    set_i2s_active(true);
  } else if (ESP_A2D_AUDIO_STATE_REMOTE_SUSPEND == a2d->audio_stat.state ||
             ESP_A2D_AUDIO_STATE_STOPPED == a2d->audio_stat.state) {
    set_i2s_active(false);
  }

  if (audio_state_callback_post != nullptr) {
    audio_state_callback_post(a2d->audio_stat.state, audio_state_obj_post);
  }
}

void BluetoothA2DPSink::set_i2s_active(bool active) {
  ESP_LOGI(BT_AV_TAG, "%s %d", __func__, active);
  if (active) m_pkt_cnt = 0;
  if (is_output) {
    if (is_i2s_active != active) {
      // mark deactive before deactivating i2s
      if (!active) is_i2s_active = false;
      out->set_output_active(active);
      // active flag after i2s is active
      if (active) is_i2s_active = true;
    }
  } else {
    // just update the actual status
    is_i2s_active = active;
  }
}

void BluetoothA2DPSink::handle_connection_state(uint16_t event, void *p_param) {
  ESP_LOGD(BT_AV_TAG, "%s evt %d", __func__, event);
  esp_a2d_cb_param_t *a2d = (esp_a2d_cb_param_t *)(p_param);

  // determine remote BDA
  memcpy(peer_bd_addr, a2d->conn_stat.remote_bda, ESP_BD_ADDR_LEN);
  ESP_LOGI(BT_AV_TAG, "partner address: %s", to_str(peer_bd_addr));

  ESP_LOGI(BT_AV_TAG, "A2DP connection state: %s, [%s]",
           to_str(a2d->conn_stat.state), to_str(a2d->conn_stat.remote_bda));
  switch (a2d->conn_stat.state) {
    case ESP_A2D_CONNECTION_STATE_DISCONNECTING:
      ESP_LOGI(BT_AV_TAG, "ESP_A2D_CONNECTION_STATE_DISCONNECTING");
      if (a2d->conn_stat.disc_rsn == ESP_A2D_DISC_RSN_NORMAL) {
        is_autoreconnect_allowed = false;
      }
      break;

    case ESP_A2D_CONNECTION_STATE_DISCONNECTED:
      ESP_LOGI(BT_AV_TAG, "ESP_A2D_CONNECTION_STATE_DISCONNECTED");
      if (a2d->conn_stat.disc_rsn == ESP_A2D_DISC_RSN_NORMAL) {
        is_autoreconnect_allowed = false;
      }

      // reset pin code
      pin_code_int = 0;
      pin_code_request = Undefined;

      // call callback
      if (bt_dis_connected != nullptr) {
        (*bt_dis_connected)();
      }

      bt_i2s_task_shut_down();

      // RECONNECTION MGMT
      // do not auto reconnect when disconnect was requested from device
      if (is_autoreconnect_allowed) {
        if (is_reconnect(a2d->conn_stat.disc_rsn)) {
          if (connection_rety_count < try_reconnect_max_count) {
            ESP_LOGI(BT_AV_TAG, "Connection try number: %d",
                     connection_rety_count);
            // make sure that any open connection is timing out on the target
            memcpy(peer_bd_addr, last_connection, ESP_BD_ADDR_LEN);
            reconnect();
            // when we lost the connection we do allow any others to connect
            // after 2 trials
            if (connection_rety_count == 2) set_scan_mode_connectable(true);

          } else {
            ESP_LOGI(BT_AV_TAG, "Reconect retry limit reached");
            if (has_last_connection() &&
                a2d->conn_stat.disc_rsn == ESP_A2D_DISC_RSN_NORMAL) {
              clean_last_connection();
            }
            set_scan_mode_connectable(true);
          }
        } else {
          set_scan_mode_connectable(true);
        }
      } else {
        set_scan_mode_connectable(true);
      }
      break;

    case ESP_A2D_CONNECTION_STATE_CONNECTING:
      ESP_LOGI(BT_AV_TAG, "ESP_A2D_CONNECTION_STATE_CONNECTING");
      connection_rety_count++;
      break;

    case ESP_A2D_CONNECTION_STATE_CONNECTED:
      ESP_LOGI(BT_AV_TAG, "ESP_A2D_CONNECTION_STATE_CONNECTED");

      // stop reconnect retries in event loop
      if (reconnect_status == IsReconnecting) {
        reconnect_status = AutoReconnect;
      }

      // checks if the address is valid
      bool is_valid = true;
      if (address_validator != nullptr) {
        uint8_t *bda = a2d->conn_stat.remote_bda;
        if (!address_validator(bda)) {
          ESP_LOGI(BT_AV_TAG, "esp_a2d_sink_disconnect: %s", (char *)bda);
          esp_a2d_sink_disconnect(bda);
          is_valid = false;
        }
      }

      if (is_valid) {
        if (bt_connected != nullptr) {
          (*bt_connected)();
        }

        set_scan_mode_connectable(false);
        connection_rety_count = 0;

        bt_i2s_task_start_up();

        // record current connection
        if (reconnect_status == AutoReconnect && is_valid) {
          set_last_connection(a2d->conn_stat.remote_bda);
        }
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 0, 0)
        // ask for the remote name
        esp_err_t esp_err =
            esp_bt_gap_read_remote_name(a2d->conn_stat.remote_bda);
        if (esp_err != ESP_OK) {
          ESP_LOGE(BT_AV_TAG, "esp_bt_gap_read_remote_name");
        }
#endif

        // Get RSSI
        if (rssi_active) {
          esp_bt_gap_read_rssi_delta(a2d->conn_stat.remote_bda);
        }
      }
      break;
  }

  // callback
  connection_state = a2d->conn_stat.state;
  if (connection_state_callback != nullptr) {
    connection_state_callback(connection_state, connection_state_obj);
  }
}

void BluetoothA2DPSink::av_new_track() {
  ESP_LOGD(BT_AV_TAG, "%s", __func__);
  // Register notifications and request metadata
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 0, 0)
  esp_avrc_ct_send_metadata_cmd(APP_RC_CT_TL_GET_META_DATA,
                                avrc_metadata_flags);
  if (esp_avrc_rn_evt_bit_mask_operation(ESP_AVRC_BIT_MASK_OP_TEST,
                                         &s_avrc_peer_rn_cap,
                                         ESP_AVRC_RN_TRACK_CHANGE)) {
    esp_avrc_ct_send_register_notification_cmd(APP_RC_CT_TL_RN_TRACK_CHANGE,
                                               ESP_AVRC_RN_TRACK_CHANGE, 0);
  }
#endif
}

void BluetoothA2DPSink::av_playback_changed() {
  ESP_LOGD(BT_AV_TAG, "%s", __func__);
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 0, 0)
  if (esp_avrc_rn_evt_bit_mask_operation(ESP_AVRC_BIT_MASK_OP_TEST,
                                         &s_avrc_peer_rn_cap,
                                         ESP_AVRC_RN_PLAY_STATUS_CHANGE)) {
    esp_avrc_ct_send_register_notification_cmd(
        APP_RC_CT_TL_RN_PLAYBACK_CHANGE, ESP_AVRC_RN_PLAY_STATUS_CHANGE, 0);
  }
#endif
}

void BluetoothA2DPSink::av_play_pos_changed(void) {
  ESP_LOGD(BT_AV_TAG, "%s", __func__);
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 0, 0)
  if (esp_avrc_rn_evt_bit_mask_operation(ESP_AVRC_BIT_MASK_OP_TEST,
                                         &s_avrc_peer_rn_cap,
                                         ESP_AVRC_RN_PLAY_POS_CHANGED)) {
    esp_avrc_ct_send_register_notification_cmd(APP_RC_CT_TL_RN_PLAY_POS_CHANGE,
                                               ESP_AVRC_RN_PLAY_POS_CHANGED,
                                               notif_interval_s);
  }
#endif
}

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 0, 0)
void BluetoothA2DPSink::av_notify_evt_handler(
    uint8_t event_id, esp_avrc_rn_param_t *event_parameter)
#else
void BluetoothA2DPSink::av_notify_evt_handler(uint8_t event_id,
                                              uint32_t event_parameter)
#endif
{
  ESP_LOGD(BT_AV_TAG, "%s", __func__);
  switch (event_id) {
    case ESP_AVRC_RN_TRACK_CHANGE:
      ESP_LOGD(BT_AV_TAG, "%s ESP_AVRC_RN_TRACK_CHANGE %d", __func__, event_id);
      av_new_track();
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 0, 0)
      // call avrc track change notification callback if available
      if (avrc_rn_track_change_callback != nullptr) {
        avrc_rn_track_change_callback(event_parameter->elm_id);
      }
#endif
      break;
    case ESP_AVRC_RN_PLAY_STATUS_CHANGE:
      ESP_LOGD(BT_AV_TAG, "%s ESP_AVRC_RN_PLAY_STATUS_CHANGE %d, to %d",
               __func__, event_id, event_parameter->playback);
      av_playback_changed();
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 0, 0)
      // call avrc play status notification callback if available
      if (avrc_rn_playstatus_callback != nullptr) {
        avrc_rn_playstatus_callback(event_parameter->playback);
      }
#endif
      break;

    case ESP_AVRC_RN_PLAY_POS_CHANGED:
      ESP_LOGI(BT_AV_TAG, "Play position changed: %d-ms",
               event_parameter->play_pos);
      av_play_pos_changed();
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 0, 0)
      // call avrc play status notification callback if available
      if (avrc_rn_play_pos_callback != nullptr) {
        avrc_rn_play_pos_callback(event_parameter->play_pos);
      }
#endif
      break;

    default:
      ESP_LOGE(BT_AV_TAG, "%s unhandled evt %d", __func__, event_id);
      break;
  }
}

void BluetoothA2DPSink::av_hdl_avrc_evt(uint16_t event, void *p_param) {
  ESP_LOGD(BT_AV_TAG, "%s evt %d", __func__, event);
  esp_avrc_ct_cb_param_t *rc = (esp_avrc_ct_cb_param_t *)(p_param);
  switch (event) {
    case ESP_AVRC_CT_CONNECTION_STATE_EVT: {
      ESP_LOGI(BT_AV_TAG, "AVRC conn_state evt: state %d, [%s]",
               rc->conn_stat.connected, to_str(rc->conn_stat.remote_bda));
      avrc_connection_state = rc->conn_stat.connected;

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 0, 0)
      if (avrc_connection_state) {
        // get remote supported event_ids of peer AVRCP Target
        esp_avrc_ct_send_get_rn_capabilities_cmd(APP_RC_CT_TL_GET_CAPS);
      } else {
        // clear peer notification capability record
        s_avrc_peer_rn_cap.bits = 0;
        handle_avrc_connection_state(avrc_connection_state);
      }
#else
      if (avrc_connection_state) {
        av_new_track();
      }
      handle_avrc_connection_state(avrc_connection_state);
#endif
      break;
    }
    case ESP_AVRC_CT_PASSTHROUGH_RSP_EVT: {
      ESP_LOGI(BT_AV_TAG, "AVRC passthrough rsp: key_code 0x%x, key_state %d",
               rc->psth_rsp.key_code, rc->psth_rsp.key_state);
      break;
    }
    case ESP_AVRC_CT_METADATA_RSP_EVT: {
      ESP_LOGI(BT_AV_TAG, "AVRC metadata rsp: attribute id 0x%x, %s",
               rc->meta_rsp.attr_id, rc->meta_rsp.attr_text);
      // call metadata callback if available
      if (avrc_metadata_callback != nullptr) {
        avrc_metadata_callback(rc->meta_rsp.attr_id, rc->meta_rsp.attr_text);
      }

      free(rc->meta_rsp.attr_text);
      break;
    }
    case ESP_AVRC_CT_CHANGE_NOTIFY_EVT: {
      // ESP_LOGI(BT_AV_TAG, "AVRC event notification: %d, param: %d",
      // (int)rc->change_ntf.event_id, (int)rc->change_ntf.event_parameter);
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 0, 0)
      av_notify_evt_handler(rc->change_ntf.event_id,
                            &rc->change_ntf.event_parameter);
#else
      av_notify_evt_handler(rc->change_ntf.event_id,
                            rc->change_ntf.event_parameter);
#endif
      break;
    }
    case ESP_AVRC_CT_REMOTE_FEATURES_EVT: {
      ESP_LOGI(BT_AV_TAG, "AVRC remote features %x", rc->rmt_feats.feat_mask);
      break;
    }

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 0, 0)

    case ESP_AVRC_CT_GET_RN_CAPABILITIES_RSP_EVT: {
      ESP_LOGI(BT_AV_TAG, "remote rn_cap: count %d, bitmask 0x%x",
               rc->get_rn_caps_rsp.cap_count, rc->get_rn_caps_rsp.evt_set.bits);
      s_avrc_peer_rn_cap.bits = rc->get_rn_caps_rsp.evt_set.bits;
      av_new_track();
      av_playback_changed();
      av_play_pos_changed();

      // now we ready to callback
      handle_avrc_connection_state(avrc_connection_state);

      break;
    }

#endif

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 5, 0)

    /* when avrcp controller init or deinit completed, this event comes */
    case ESP_AVRC_CT_PROF_STATE_EVT: {
        if (ESP_AVRC_INIT_SUCCESS == rc->avrc_ct_init_stat.state) {
            ESP_LOGI(BT_RC_CT_TAG, "AVRCP CT STATE: Init Complete");
        } else if (ESP_AVRC_DEINIT_SUCCESS == rc->avrc_ct_init_stat.state) {
            ESP_LOGI(BT_RC_CT_TAG, "AVRCP CT STATE: Deinit Complete");
        } else {
            ESP_LOGE(BT_RC_CT_TAG, "AVRCP CT STATE error: %d", rc->avrc_ct_init_stat.state);
        }
        break;
    }
#endif


    default:
      ESP_LOGE(BT_AV_TAG, "%s unhandled evt %d", __func__, event);
      break;
  }
}

void BluetoothA2DPSink::av_hdl_stack_evt(uint16_t event, void *p_param) {
  ESP_LOGD(BT_AV_TAG, "%s evt %d", __func__, event);
  esp_err_t result;

  switch (event) {
    case BT_APP_EVT_STACK_UP: {
      ESP_LOGD(BT_AV_TAG, "%s av_hdl_stack_evt %s", __func__,
               "BT_APP_EVT_STACK_UP");
      /* set up device name */
      esp_bt_gap_set_device_name(bt_name);

      // initialize AVRCP controller
      result = esp_avrc_ct_init();
      if (result == ESP_OK) {
        result = esp_avrc_ct_register_callback(ccall_app_rc_ct_callback);
        if (result == ESP_OK) {
          ESP_LOGD(BT_AV_TAG, "AVRCP controller initialized!");
        } else {
          ESP_LOGE(BT_AV_TAG, "esp_avrc_ct_register_callback: %d", result);
        }
      } else {
        ESP_LOGE(BT_AV_TAG, "esp_avrc_ct_init: %d", result);
      }

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 0, 0)

      /* initialize AVRCP target */
      if (esp_avrc_tg_init() == ESP_OK) {
        esp_avrc_tg_register_callback(ccall_app_rc_tg_callback);
        // add request to ESP_AVRC_RN_VOLUME_CHANGE
        esp_avrc_rn_evt_cap_mask_t evt_set = {0};
        for (auto event : avrc_rn_events) {
          esp_avrc_rn_evt_bit_mask_operation(ESP_AVRC_BIT_MASK_OP_SET, &evt_set,
                                             event);
        }
        if (esp_avrc_tg_set_rn_evt_cap(&evt_set) != ESP_OK) {
          ESP_LOGE(BT_AV_TAG, "esp_avrc_tg_set_rn_evt_cap failed");
        }
      } else {
        ESP_LOGE(BT_AV_TAG, "esp_avrc_tg_init failed");
      }

#endif

      /* initialize A2DP sink */
      if (esp_a2d_register_callback(ccall_app_a2d_callback) != ESP_OK) {
        ESP_LOGE(BT_AV_TAG, "esp_a2d_register_callback");
      }
      if (esp_a2d_sink_register_data_callback(ccall_audio_data_callback) !=
          ESP_OK) {
        ESP_LOGE(BT_AV_TAG, "esp_a2d_sink_register_data_callback");
      }
      if (esp_a2d_sink_init() != ESP_OK) {
        ESP_LOGE(BT_AV_TAG, "esp_a2d_sink_init");
      }

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 5, 0)
      // Register only later when user sets a callback
      if (!codec_sep_registered && encoded_stream_reader != nullptr) {
        set_codec(desired_codec, encoded_stream_reader);
      }
#endif

      // start automatic reconnect if relevant and stack is up
      if (reconnect_status == AutoReconnect && has_last_connection()) {
        ESP_LOGD(BT_AV_TAG, "reconnect");
        memcpy(peer_bd_addr, last_connection, ESP_BD_ADDR_LEN);
        reconnect();
      }

      /* set discoverable and connectable mode, wait to be connected */
      ESP_LOGD(BT_AV_TAG, "set_scan_mode_connectable(true)");
      set_scan_mode_connectable(true);
      break;
    }

    default:
      ESP_LOGE(BT_AV_TAG, "%s unhandled evt %d", __func__, event);
      break;
  }
}

/* callback for A2DP sink */
void BluetoothA2DPSink::app_a2d_callback(esp_a2d_cb_event_t event,
                                         esp_a2d_cb_param_t *param) {
  ESP_LOGD(BT_AV_TAG, "%s", __func__);

  switch (event) {
    case ESP_A2D_CONNECTION_STATE_EVT:
      ESP_LOGD(BT_AV_TAG, "%s ESP_A2D_CONNECTION_STATE_EVT", __func__);
      app_work_dispatch(ccall_av_hdl_a2d_evt, event, param,
                        sizeof(esp_a2d_cb_param_t));
      break;
    case ESP_A2D_AUDIO_STATE_EVT:
      ESP_LOGD(BT_AV_TAG, "%s ESP_A2D_AUDIO_STATE_EVT", __func__);
      audio_state = param->audio_stat.state;
      app_work_dispatch(ccall_av_hdl_a2d_evt, event, param,
                        sizeof(esp_a2d_cb_param_t));
      break;
    case ESP_A2D_AUDIO_CFG_EVT: {
      ESP_LOGD(BT_AV_TAG, "%s ESP_A2D_AUDIO_CFG_EVT", __func__);
      app_work_dispatch(ccall_av_hdl_a2d_evt, event, param,
                        sizeof(esp_a2d_cb_param_t));
      break;
    }

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 0, 0)
    case ESP_A2D_PROF_STATE_EVT: {
      ESP_LOGD(BT_AV_TAG, "%s ESP_A2D_PROF_STATE_EVT", __func__);
      app_work_dispatch(ccall_av_hdl_a2d_evt, event, param,
                        sizeof(esp_a2d_cb_param_t));
      break;
    }
#endif

    default:
      ESP_LOGI(BT_AV_TAG, "Unhandled A2DP event: %d", event);
      break;
  }
}

void BluetoothA2DPSink::audio_data_callback(const uint8_t *data, uint32_t len) {
  ESP_LOGD(BT_AV_TAG, "%s", __func__);

  // swap left and right channels
  if (swap_left_right) {
    Frame *frame = (Frame *)data;
    for (int i = 0; i < len / 4; i++) {
      int16_t temp = frame[i].channel1;
      frame[i].channel1 = frame[i].channel2;
      frame[i].channel2 = temp;
    }
  }

  // make data available via callback, before volume control
  if (raw_stream_reader != nullptr) {
    ESP_LOGD(BT_AV_TAG, "raw_stream_reader");
    (*raw_stream_reader)(data, len);
  }

  // adjust the volume
  volume_control()->update_audio_data((Frame *)data, len / 4);

  // make data available via callback
  if (stream_reader != nullptr) {
    ESP_LOGD(BT_AV_TAG, "stream_reader");
    (*stream_reader)(data, len);
  }

  // put data into ringbuffer
  if (is_output) {
    write_audio(data, len);
  }

  // data_received callback
  if (data_received != nullptr) {
    ESP_LOGD(BT_AV_TAG, "data_received");
    (*data_received)();
  }
}

bool BluetoothA2DPSink::is_avrc_connected() { return avrc_connection_state; }

void BluetoothA2DPSink::execute_avrc_command(int cmd) {
  ESP_LOGD(BT_AV_TAG, "execute_avrc_command: %d", cmd);
  esp_err_t ok =
      esp_avrc_ct_send_passthrough_cmd(0, cmd, ESP_AVRC_PT_CMD_STATE_PRESSED);
  if (ok == ESP_OK) {
    delay_ms(100);
    ok = esp_avrc_ct_send_passthrough_cmd(0, cmd,
                                          ESP_AVRC_PT_CMD_STATE_RELEASED);
    if (ok == ESP_OK) {
      ESP_LOGD(BT_AV_TAG, "execute_avrc_command: %d -> OK", cmd);
    } else {
      ESP_LOGE(BT_AV_TAG,
               "execute_avrc_command ESP_AVRC_PT_CMD_STATE_RELEASED FAILED: %d",
               ok);
    }
  } else {
    ESP_LOGE(BT_AV_TAG,
             "execute_avrc_command ESP_AVRC_PT_CMD_STATE_PRESSED FAILED: %d",
             ok);
  }
}

void BluetoothA2DPSink::play() { execute_avrc_command(ESP_AVRC_PT_CMD_PLAY); }

void BluetoothA2DPSink::pause() { execute_avrc_command(ESP_AVRC_PT_CMD_PAUSE); }

void BluetoothA2DPSink::stop() { execute_avrc_command(ESP_AVRC_PT_CMD_STOP); }

void BluetoothA2DPSink::next() {
  execute_avrc_command(ESP_AVRC_PT_CMD_FORWARD);
}
void BluetoothA2DPSink::previous() {
  execute_avrc_command(ESP_AVRC_PT_CMD_BACKWARD);
}
void BluetoothA2DPSink::fast_forward() {
  execute_avrc_command(ESP_AVRC_PT_CMD_FAST_FORWARD);
}
void BluetoothA2DPSink::rewind() {
  execute_avrc_command(ESP_AVRC_PT_CMD_REWIND);
}

void BluetoothA2DPSink::volume_up() {
  execute_avrc_command(ESP_AVRC_PT_CMD_VOL_UP);
}

void BluetoothA2DPSink::volume_down() {
  execute_avrc_command(ESP_AVRC_PT_CMD_VOL_DOWN);
}

void BluetoothA2DPSink::set_volume(uint8_t volume) {
  // limit the volume to 127
  s_volume = std::min((int)volume, 0x7f);
  ESP_LOGI(BT_AV_TAG, "set_volume %d -> %d", volume, s_volume);
  volume_control()->set_volume(s_volume);
  volume_control()->set_enabled(true);

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 0, 0)
  volume_set_by_local_host(s_volume);
#endif
}

int BluetoothA2DPSink::get_volume() {
  // ESP_LOGI(BT_AV_TAG, "get_volume %d", s_volume);
  return s_volume;
}

void BluetoothA2DPSink::activate_pin_code(bool active) {
  is_pin_code_active = active;
}

void BluetoothA2DPSink::confirm_pin_code() {
  if (pin_code_int != 0) {
    confirm_pin_code(pin_code_int);
  } else {
    ESP_LOGI(BT_AV_TAG, "pincode not available (yet)");
  }
}

void BluetoothA2DPSink::confirm_pin_code(int code) {
  switch (pin_code_request) {
    case Confirm:
      ESP_LOGI(BT_AV_TAG, "-> %s", to_str(peer_bd_addr));
      if (esp_bt_gap_ssp_confirm_reply(peer_bd_addr, true) != ESP_OK) {
        ESP_LOGE(BT_AV_TAG, "esp_bt_gap_ssp_passkey_reply");
      }
      break;
    case Reply:
      ESP_LOGI(BT_AV_TAG, "confirm_pin_code %d -> %s", code,
               to_str(peer_bd_addr));
      if (esp_bt_gap_ssp_passkey_reply(peer_bd_addr, true, code) != ESP_OK) {
        ESP_LOGE(BT_AV_TAG, "esp_bt_gap_ssp_passkey_reply");
      }
      break;
    default:
      ESP_LOGE(BT_AV_TAG, "No open request -> %s", to_str(peer_bd_addr));
      break;
  }
}

size_t BluetoothA2DPSink::i2s_write_data(const uint8_t *data,
                                         size_t item_size) {
  if (!is_output) return item_size;

  if (!is_i2s_active) {
    ESP_LOGW(BT_AV_TAG, "%s failed - inactive", __func__);
    return 0;
  }

  // split up outout to max size
  int open = item_size;
  int processed = 0;
  while (open > 0) {
    int written = out->write(data + processed, std::min(open, max_write_size));
    open -= written;
    processed += written;
    // add some delay between the writes
    delay_ms(max_write_delay_ms);
  }
  return processed;
}

//------------------------------------------------------------
// ==> Methods which are only supported in new ESP Release 4

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 0, 0)

void BluetoothA2DPSink::app_rc_tg_callback(esp_avrc_tg_cb_event_t event,
                                           esp_avrc_tg_cb_param_t *param) {
  ESP_LOGD(BT_AV_TAG, "%s", __func__);
  switch (event) {
    case ESP_AVRC_TG_CONNECTION_STATE_EVT:
    case ESP_AVRC_TG_REMOTE_FEATURES_EVT:
    case ESP_AVRC_TG_PASSTHROUGH_CMD_EVT:
    case ESP_AVRC_TG_SET_ABSOLUTE_VOLUME_CMD_EVT:
    case ESP_AVRC_TG_REGISTER_NOTIFICATION_EVT:
    case ESP_AVRC_TG_SET_PLAYER_APP_VALUE_EVT: 
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 5, 0)
    case ESP_AVRC_TG_PROF_STATE_EVT: 
#endif
    {
      app_work_dispatch(ccall_av_hdl_avrc_tg_evt, event, param,
                        sizeof(esp_avrc_tg_cb_param_t));
      break;
    }
    default:
      ESP_LOGE(BT_AV_TAG, "Unsupported AVRC event: %d", event);
      break;
  }
}

void BluetoothA2DPSink::volume_set_by_controller(uint8_t volume) {
  ESP_LOGI(BT_AV_TAG, "Volume is set by remote controller to %d",
           (uint32_t)volume * 100 / 0x7f);

  _lock_acquire(&s_volume_lock);
  s_volume = volume;
  _lock_release(&s_volume_lock);

  volume_control()->set_volume(s_volume);
  volume_control()->set_enabled(true);

  if (bt_volumechange != nullptr) {
    (*bt_volumechange)(s_volume);
  }
}

void BluetoothA2DPSink::volume_set_by_local_host(uint8_t volume) {
  ESP_LOGI(BT_AV_TAG, "Volume is set locally to: %d%%",
           (uint32_t)volume * 100 / 0x7f);

  _lock_acquire(&s_volume_lock);
  s_volume = volume;
  _lock_release(&s_volume_lock);

  if (s_volume_notify) {
    esp_avrc_rn_param_t rn_param;
    rn_param.volume = s_volume;
    esp_avrc_tg_send_rn_rsp(ESP_AVRC_RN_VOLUME_CHANGE, ESP_AVRC_RN_RSP_CHANGED,
                            &rn_param);
  }
}

void BluetoothA2DPSink::av_hdl_avrc_tg_evt(uint16_t event, void *p_param) {
  ESP_LOGD(BT_AV_TAG, "%s evt %d", __func__, event);
  esp_avrc_tg_cb_param_t *rc = (esp_avrc_tg_cb_param_t *)(p_param);

  switch (event) {
    case ESP_AVRC_TG_CONNECTION_STATE_EVT: {
      ESP_LOGI(BT_AV_TAG, "AVRC conn_state evt: state %d, [%s]",
               rc->conn_stat.connected, to_str(rc->conn_stat.remote_bda));
      break;
    }

    case ESP_AVRC_TG_PASSTHROUGH_CMD_EVT: {
      ESP_LOGI(BT_AV_TAG, "AVRC passthrough cmd: key_code 0x%x, key_state %d",
               rc->psth_cmd.key_code, rc->psth_cmd.key_state);
      break;
    }

    case ESP_AVRC_TG_SET_ABSOLUTE_VOLUME_CMD_EVT: {
      ESP_LOGI(BT_AV_TAG, "AVRC remote set absolute volume: %d%%",
               (int)rc->set_abs_vol.volume * 100 / 0x7f);
      volume_set_by_controller(rc->set_abs_vol.volume);
      break;
    }

    case ESP_AVRC_TG_REGISTER_NOTIFICATION_EVT: {
      ESP_LOGI(BT_AV_TAG, "AVRC register event notification: %d, param: 0x%x",
               rc->reg_ntf.event_id, rc->reg_ntf.event_parameter);
      if (rc->reg_ntf.event_id == ESP_AVRC_RN_VOLUME_CHANGE) {
        s_volume_notify = true;
        esp_avrc_rn_param_t rn_param;
        rn_param.volume = s_volume;
        esp_avrc_tg_send_rn_rsp(ESP_AVRC_RN_VOLUME_CHANGE,
                                ESP_AVRC_RN_RSP_INTERIM, &rn_param);
        // notify user aplication volume change by local is completed
        if (avrc_rn_volchg_complete_callback != nullptr) {
          (*avrc_rn_volchg_complete_callback)(s_volume);
        }
      }
      break;
    }

    case ESP_AVRC_TG_REMOTE_FEATURES_EVT: {
      ESP_LOGI(BT_AV_TAG, "AVRC remote features %x, CT features %x",
               rc->rmt_feats.feat_mask, rc->rmt_feats.ct_feat_flag);
      break;
    }

 #if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 5, 0)
   
    /* when avrcp target init or deinit completed, this event comes */
    case ESP_AVRC_TG_PROF_STATE_EVT: {
        if (ESP_AVRC_INIT_SUCCESS == rc->avrc_tg_init_stat.state) {
            ESP_LOGI(BT_RC_CT_TAG, "AVRCP TG STATE: Init Complete");
        } else if (ESP_AVRC_DEINIT_SUCCESS == rc->avrc_tg_init_stat.state) {
            ESP_LOGI(BT_RC_CT_TAG, "AVRCP TG STATE: Deinit Complete");
        } else {
            ESP_LOGE(BT_RC_CT_TAG, "AVRCP TG STATE error: %d", rc->avrc_tg_init_stat.state);
        }
        break;
    }
#endif

    default:
      ESP_LOGE(BT_AV_TAG, "%s unhandled evt %d", __func__, event);
      break;
  }
}

#endif

#endif // platform
