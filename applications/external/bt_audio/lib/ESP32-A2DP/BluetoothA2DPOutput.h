#pragma once
#include "BluetoothA2DPCommon.h"

#ifdef ARDUINO
#include "Print.h"
#endif

#if A2DP_LEGACY_I2S_SUPPORT
#include "driver/i2s.h"
#endif

#if A2DP_I2S_AUDIOTOOLS
#include "AudioTools.h"
#endif

/**
 * @brief Abstract Output Class
 * @author Phil Schatzmann
 * @ingroup a2dp
 * @copyright Apache License Version 2
 */
class BluetoothA2DPOutput {
 public:
  virtual bool begin() = 0;
  virtual size_t write(const uint8_t *data, size_t len) = 0;
  virtual void end() = 0;
  virtual void set_sample_rate(int rate) = 0;
  virtual void set_output_active(bool active) = 0;

#if A2DP_I2S_AUDIOTOOLS
  /// Not implemented
  virtual void set_output(audio_tools::AudioOutput &output) {}
  /// Not implemented
  virtual void set_output(audio_tools::AudioStream &output) {}
#endif

#ifdef ARDUINO
  /// Not implemented
  virtual void set_output(Print &output) {}
#endif

#if A2DP_LEGACY_I2S_SUPPORT
  /// Define the pins (Legacy I2S: OBSOLETE!)
  virtual void set_pin_config(i2s_pin_config_t pin_config) {}
  /// Define an alternative i2s port other then 0 (Legacy I2S: OBSOLETE!)
  virtual void set_i2s_port(i2s_port_t i2s_num) {}

  /// Define the i2s configuration (Legacy I2S: OBSOLETE!)
  virtual void set_i2s_config(i2s_config_t i2s_config) {}

  /// Defines the bits per sample for output (if > 16 output will be expanded)
  /// (Legacy I2S: OBSOLETE!)
  virtual void set_bits_per_sample(int bps) {}

#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 1, 1)
  /// Obsolete: define the master clock pin via the pins
  virtual esp_err_t i2s_mclk_pin_select(const uint8_t pin) { return ESP_FAIL; };
#endif

#endif
};

/**
 * @brief Output Class using AudioTools library:
 * https://github.com/pschatzmann/arduino-audio-tools
 * @author Phil Schatzmann
 * @copyright Apache License Version 2
 */
class BluetoothA2DPOutputAudioTools : public BluetoothA2DPOutput {
 public:
  BluetoothA2DPOutputAudioTools() = default;
  bool begin() override;
  size_t write(const uint8_t *data, size_t len) override;
  void end() override;
  void set_sample_rate(int rate) override;
  void set_output_active(bool active) override;

  operator bool() { 
#if A2DP_I2S_AUDIOTOOLS || defined(ARDUINO)
    return p_print != nullptr; 
#else
    return false;
#endif
  }


#if A2DP_I2S_AUDIOTOOLS
  /// Output AudioStream using AudioTools library
  void set_output(audio_tools::AudioOutput &output) {
    p_print = &output;
    p_audio_print = &output;
  }

  /// Output AudioStream using AudioTools library
  void set_output(audio_tools::AudioStream &output) {
    static audio_tools::AdapterAudioStreamToAudioOutput adapter(output);
    adapter.setStream(output);
    p_print = &output;
    p_audio_print = &adapter;
  }
#endif

#ifdef ARDUINO
  /// Output to Arduino Print
  void set_output(Print &output) { p_print = &output; }
#endif

 protected:
#if defined(ARDUINO) || A2DP_I2S_AUDIOTOOLS
  Print *p_print = nullptr;
#endif

#if A2DP_I2S_AUDIOTOOLS
  audio_tools::AudioOutput *p_audio_print = nullptr;
#endif
};

#ifdef ARDUINO

/**
 * @brief Output Class using Print API:
 * @author Phil Schatzmann
 * @copyright Apache License Version 2
 */

class BluetoothA2DPOutputPrint : public BluetoothA2DPOutput {
 public:
  BluetoothA2DPOutputPrint() = default;
  bool begin() { return true;};
  size_t write(const uint8_t *data, size_t len) override { 
    if (p_print==nullptr) return 0;
    return p_print->write(data, len);
  }
  void end() override {}
  void set_sample_rate(int rate) override {};
  void set_output_active(bool active) override {};

  operator bool() { 
    return p_print != nullptr; 
  }

  /// Output to Arduino Print
  void set_output(Print &output) { p_print = &output; }

 protected:
  Print *p_print = nullptr;

};
#endif


/**
 * @brief Legacy I2S Output Class
 * @author Phil Schatzmann
 * @copyright Apache License Version 2
 */
class BluetoothA2DPOutputLegacy : public BluetoothA2DPOutput {
 public:
  BluetoothA2DPOutputLegacy();
  bool begin() override;
  size_t write(const uint8_t *data, size_t len) override;
  void end() override;
  void set_sample_rate(int rate) override;
  void set_output_active(bool active) override;

#if A2DP_LEGACY_I2S_SUPPORT
  /// Define the pins (Legacy I2S: OBSOLETE!)
  virtual void set_pin_config(i2s_pin_config_t pin_config) {
    this->pin_config = pin_config;
  }
  /// Define an alternative i2s port other then 0 (Legacy I2S: OBSOLETE!)
  virtual void set_i2s_port(i2s_port_t i2s_num) { i2s_port = i2s_num; }

  /// Define the i2s configuration (Legacy I2S: OBSOLETE!)
  virtual void set_i2s_config(i2s_config_t i2s_config) {
    this->i2s_config = i2s_config;
  }

  /// Defines the bits per sample for output (if > 16 output will be expanded)
  /// (Legacy I2S: OBSOLETE!)
  virtual void set_bits_per_sample(int bps) {
    i2s_config.bits_per_sample = (i2s_bits_per_sample_t)bps;
  }

#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 1, 1)
  virtual esp_err_t i2s_mclk_pin_select(const uint8_t pin);
#endif

 protected:
  i2s_config_t i2s_config;
  i2s_pin_config_t pin_config;
  i2s_channel_t i2s_channels = I2S_CHANNEL_STEREO;
  i2s_port_t i2s_port = I2S_NUM_0;
#endif
};

/**
 * @brief Default Output Class providing both the Legacy I2S and the AudioTools
 * I2S functionality
 * @author Phil Schatzmann
 * @copyright Apache License Version 2
 */
class BluetoothA2DPOutputDefault : public BluetoothA2DPOutput {
 public:
  BluetoothA2DPOutputDefault() = default;
  bool begin() {
    bool rc = false;
    if (out_tools)
      rc = out_tools.begin();
    else
      rc = out_legacy.begin();
    return rc;
  }
  
  size_t write(const uint8_t *data, size_t len) {
    size_t result = 0;
    if (out_tools)
      result = out_tools.write(data, len);
    else
      result = out_legacy.write(data, len);
    return result;
  }
  
  void end() override {
    if (out_tools)
      out_tools.end();
    else
      out_legacy.end();
  }
  
  void set_sample_rate(int rate) override {
    if (out_tools)
      out_tools.set_sample_rate(rate);
    else
      out_legacy.set_sample_rate(rate);
  }

  void set_output_active(bool active) override {
    if (out_tools)
      out_tools.set_output_active(active);
    else
      out_legacy.set_output_active(active);
  }

#if A2DP_I2S_AUDIOTOOLS
  /// Output AudioStream using AudioTools library
  void set_output(audio_tools::AudioOutput &output) override  { out_tools.set_output(output); }
  /// Output AudioStream using AudioTools library
  void set_output(audio_tools::AudioStream &output) override { out_tools.set_output(output); }
#endif

#ifdef ARDUINO
  /// Output to Arduino Print
  void set_output(Print &output) override { out_tools.set_output(output); }
#endif

#if A2DP_LEGACY_I2S_SUPPORT
  /// Define the pins (Legacy I2S: OBSOLETE!)
  virtual void set_pin_config(i2s_pin_config_t pin_config) override {
    out_legacy.set_pin_config(pin_config);
  }
  /// Define an alternative i2s port other then 0 (Legacy I2S: OBSOLETE!)
  virtual void set_i2s_port(i2s_port_t i2s_num) override {
    out_legacy.set_i2s_port(i2s_num);
  }

  /// Define the i2s configuration (Legacy I2S: OBSOLETE!)
  virtual void set_i2s_config(i2s_config_t i2s_config) override {
    out_legacy.set_i2s_config(i2s_config);
  }

  /// Defines the bits per sample for output (if > 16 output will be expanded)
  /// (Legacy I2S: OBSOLETE!)
  virtual void set_bits_per_sample(int bps) override {
    out_legacy.set_bits_per_sample(bps);
  }

#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 1, 1)
  virtual esp_err_t i2s_mclk_pin_select(const uint8_t pin) override {
    return out_legacy.i2s_mclk_pin_select(pin);
  }
#endif
#endif  // A2DP_LEGACY_I2S_SUPPORT
 protected:
  BluetoothA2DPOutputAudioTools out_tools;
  BluetoothA2DPOutputLegacy out_legacy;
};