#include "BluetoothA2DPOutput.h"

#if IS_VALID_PLATFORM

BluetoothA2DPOutputLegacy::BluetoothA2DPOutputLegacy() {
#if A2DP_LEGACY_I2S_SUPPORT
  // default i2s port is 0
  i2s_port = (i2s_port_t)0;

  // setup default i2s config
  i2s_config = {
      .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
      .sample_rate = 44100,
      .bits_per_sample = (i2s_bits_per_sample_t)16,
      .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
      .communication_format = (i2s_comm_format_t)I2S_COMM_FORMAT_STAND_I2S,
      .intr_alloc_flags = 0,  // default interrupt priority
      .dma_buf_count = 8,
      .dma_buf_len = 64,
      .use_apll = false,
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 0, 0)
      .tx_desc_auto_clear =
          true,  // avoiding noise in case of data unavailability
      .fixed_mclk = 0,
      .mclk_multiple = (i2s_mclk_multiple_t) 0, // I2S_MCLK_MULTIPLE_DEFAULT
      .bits_per_chan = I2S_BITS_PER_CHAN_DEFAULT
#else
      .tx_desc_auto_clear =
          true  // avoiding noise in case of data unavailability
#endif
  };

  // setup default pins
  pin_config = {
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 0, 0)
      .mck_io_num = 0,
#endif
      .bck_io_num = 26,
      .ws_io_num = 25,
      .data_out_num = 22,
      .data_in_num = I2S_PIN_NO_CHANGE};

#endif
}

#if A2DP_LEGACY_I2S_SUPPORT && ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 1, 1)

esp_err_t BluetoothA2DPOutputLegacy::i2s_mclk_pin_select(const uint8_t pin) {
  if (pin != 0 && pin != 1 && pin != 3) {
    ESP_LOGE(BT_APP_TAG, "Only support GPIO0/GPIO1/GPIO3, gpio_num:%d", pin);
    return ESP_ERR_INVALID_ARG;
  }
  switch (pin) {
    case 0:
      PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO0_U, FUNC_GPIO0_CLK_OUT1);
      WRITE_PERI_REG(PIN_CTRL, 0xFFF0);
      break;
    case 1:
      PIN_FUNC_SELECT(PERIPHS_IO_MUX_U0TXD_U, FUNC_U0TXD_CLK_OUT3);
      WRITE_PERI_REG(PIN_CTRL, 0xF0F0);
      break;
    case 3:
      PIN_FUNC_SELECT(PERIPHS_IO_MUX_U0RXD_U, FUNC_U0RXD_CLK_OUT2);
      WRITE_PERI_REG(PIN_CTRL, 0xFF00);
      break;
    default:
      break;
  }
  return ESP_OK;
}

#endif

bool BluetoothA2DPOutputLegacy::begin() {
  bool rc = true;
#if A2DP_LEGACY_I2S_SUPPORT
  ESP_LOGI(BT_AV_TAG, "%s", __func__);
  // setup i2s
  if (i2s_driver_install(i2s_port, &i2s_config, 0, nullptr) != ESP_OK) {
    ESP_LOGE(BT_AV_TAG, "i2s_driver_install failed");
    rc = false;
  }

  // pins are only relevant when music is not sent to internal DAC
  if (i2s_config.mode & I2S_MODE_DAC_BUILT_IN) {
    if (i2s_set_pin(i2s_port, nullptr) != ESP_OK) {
      ESP_LOGE(BT_AV_TAG, "i2s_set_pin failed");
      rc = false;
    }
    if (i2s_set_dac_mode(I2S_DAC_CHANNEL_BOTH_EN) != ESP_OK) {
      ESP_LOGE(BT_AV_TAG, "i2s_set_dac_mode failed");
      rc = false;
    }
    ESP_LOGI(BT_AV_TAG, "Output will go to DAC pins");
  } else {
    if (i2s_set_pin(i2s_port, &pin_config) != ESP_OK) {
      ESP_LOGE(BT_AV_TAG, "i2s_set_pin failed");
      rc = false;
    }
  }

#endif
  return rc;
}

size_t BluetoothA2DPOutputLegacy::write(const uint8_t *data, size_t item_size) {
  size_t i2s_bytes_written = 0;

#if A2DP_LEGACY_I2S_SUPPORT
  if (this->i2s_config.mode & I2S_MODE_DAC_BUILT_IN) {
    // special case for internal DAC output, the incomming PCM buffer needs
    // to be converted from signed 16bit to unsigned
    int16_t *data16 = (int16_t *)data;

    // HACK: this is here to remove the const restriction to replace the data
    // in place as per
    // https://github.com/espressif/esp-idf/blob/178b122/components/bt/host/bluedroid/api/include/api/esp_a2dp_api.h
    // the buffer is anyway static block of memory possibly overwritten by
    // next incomming data.

    for (int i = 0; i < item_size / 2; i++) {
      int16_t sample = data[i * 2] | data[i * 2 + 1] << 8;
      data16[i] = sample + 0x8000;
    }
  }

  if (i2s_config.bits_per_sample == I2S_BITS_PER_SAMPLE_16BIT) {
    // standard logic with 16 bits
    if (i2s_write(i2s_port, (void *)data, item_size, &i2s_bytes_written,
                  portMAX_DELAY) != ESP_OK) {
      ESP_LOGE(BT_AV_TAG, "i2s_write has failed");
    }
  } else {
    if (i2s_config.bits_per_sample > 16) {
      // expand e.g to 32 bit for dacs which do not support 16 bits
      if (i2s_write_expand(i2s_port, (void *)data, item_size,
                           I2S_BITS_PER_SAMPLE_16BIT,
                           i2s_config.bits_per_sample, &i2s_bytes_written,
                           portMAX_DELAY) != ESP_OK) {
        ESP_LOGE(BT_AV_TAG, "i2s_write has failed");
      }
    } else {
      ESP_LOGE(BT_AV_TAG, "invalid bits_per_sample: %d",
               i2s_config.bits_per_sample);
    }
  }

  i2s_bytes_written = item_size;
#endif
  return i2s_bytes_written;
}

void BluetoothA2DPOutputLegacy::end() {
#if A2DP_LEGACY_I2S_SUPPORT
  ESP_LOGI(BT_AV_TAG, "%s", __func__);
  if (i2s_driver_uninstall(i2s_port) != ESP_OK) {
    ESP_LOGE(BT_AV_TAG, "Failed to uninstall i2s");
  } 
#endif
}

void BluetoothA2DPOutputLegacy::set_sample_rate(int m_sample_rate) {
#if A2DP_LEGACY_I2S_SUPPORT
  ESP_LOGI(BT_AV_TAG, "%s %d", __func__, m_sample_rate);
  i2s_config.sample_rate = m_sample_rate;
  // setup sample rate and channels
  if (i2s_set_clk(i2s_port, i2s_config.sample_rate, i2s_config.bits_per_sample,
                  i2s_channels) != ESP_OK) {
    ESP_LOGE(BT_AV_TAG, "i2s_set_clk failed with samplerate=%d",
             i2s_config.sample_rate);
  } else {
    ESP_LOGI(BT_AV_TAG, "audio player configured, samplerate=%d",
             i2s_config.sample_rate);
  }
#endif
}

void BluetoothA2DPOutputLegacy::set_output_active(bool active) {
#if A2DP_LEGACY_I2S_SUPPORT
  if (active) {
    ESP_LOGI(BT_AV_TAG, "i2s_start");
    if (i2s_start(i2s_port) != ESP_OK) {
      ESP_LOGE(BT_AV_TAG, "i2s_start");
    }
  } else {
    ESP_LOGW(BT_AV_TAG, "i2s_stop");
    i2s_stop(i2s_port);
    i2s_zero_dma_buffer(i2s_port);
  }
#endif
}

// --------------------------------------------------------------------------

bool BluetoothA2DPOutputAudioTools::begin() {
  bool rc = true;
#if A2DP_I2S_AUDIOTOOLS
  ESP_LOGI(BT_AV_TAG, "%s", __func__);
  if (p_audio_print != nullptr) {
    rc = p_audio_print->begin();
  }
#endif
  return rc;
}


size_t BluetoothA2DPOutputAudioTools::write(const uint8_t *data, size_t item_size) {
  size_t i2s_bytes_written = 0;

#if A2DP_I2S_AUDIOTOOLS || defined(ARDUINO)
  if (p_print != nullptr) {
    i2s_bytes_written = p_print->write(data, item_size);
  }
#endif
  return i2s_bytes_written;
}


void BluetoothA2DPOutputAudioTools::end() {
#if A2DP_I2S_AUDIOTOOLS
  ESP_LOGI(BT_AV_TAG, "%s", __func__);
  if (p_audio_print != nullptr) {
    p_audio_print->end();
  }
#endif
}


void BluetoothA2DPOutputAudioTools::set_sample_rate(int m_sample_rate) {
#if A2DP_I2S_AUDIOTOOLS
  ESP_LOGI(BT_AV_TAG, "%s %d", __func__, m_sample_rate);
  if (p_audio_print != nullptr) {
    audio_tools::AudioInfo info = p_audio_print->audioInfo();
    if (info.sample_rate != m_sample_rate || info.channels != 2 ||
        info.bits_per_sample != 16) {
      info.sample_rate = m_sample_rate;
      info.channels = 2;
      info.bits_per_sample = 16;
      p_audio_print->setAudioInfo(info);
      ESP_LOGI(BT_AV_TAG, "%s sample_rate %d -> %d", __func__, info.sample_rate, p_audio_print->audioInfo().sample_rate);
    } else {
      ESP_LOGI(BT_AV_TAG, "%s sample_rate not changed %d", __func__, info.sample_rate);
    }
  }
#endif
}

void BluetoothA2DPOutputAudioTools::set_output_active(bool active) {
  ESP_LOGI(BT_AV_TAG, "%s %d", __func__, active);
#if A2DP_I2S_AUDIOTOOLS
  if (p_audio_print != nullptr) {
    if (active) {
      // m_pkt_cnt = 0;
      ESP_LOGI(BT_AV_TAG, "i2s_start");
      p_audio_print->begin();
    } else {
      ESP_LOGW(BT_AV_TAG, "i2s_stop");
      p_audio_print->end();
    }
  }
#endif
}

#endif
