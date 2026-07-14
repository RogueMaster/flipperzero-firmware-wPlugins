#pragma once

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

#include "esp_log.h"

    /**
     * @brief Utility structure that can be used to split a int32_t up into 2
     * separate channels with int16_t data.
     * @author Phil Schatzmann
     * @copyright Apache License Version 2
     */
    struct __attribute__((packed)) Frame {
  int16_t channel1;  ///< Left audio channel data
  int16_t channel2;  ///< Right audio channel data

  /**
   * @brief Default constructor - sets both channels to the same value
   * @param v Value to set for both channels (default: 0)
   */
  Frame(int v = 0) { channel1 = channel2 = v; }

  /**
   * @brief Constructor with separate channel values
   * @param ch1 Value for channel 1 (left)
   * @param ch2 Value for channel 2 (right)
   */
  Frame(int ch1, int ch2) {
    channel1 = ch1;
    channel2 = ch2;
  }
};

/**
 * @brief Abstract class for handling of the volume of the audio data
 * @ingroup a2dp
 * @author Phil Schatzmann
 * @copyright Apache License Version 2
 */

class A2DPVolumeControl {
 public:
  /**
   * @brief Default constructor
   */
  A2DPVolumeControl() = default;

  /**
   * @brief Updates audio data with volume control and optional mono downmix
   * @param data Pointer to raw audio data (uint8_t)
   * @param byteCount Number of bytes to process
   */
  virtual void update_audio_data(uint8_t* data, uint16_t byteCount) {
    update_audio_data((Frame*)data, byteCount / 4);
  }

  /**
   * @brief Updates audio data with volume control and optional mono downmix
   * @param data Pointer to audio frame data
   * @param frameCount Number of frames to process
   */
  virtual void update_audio_data(Frame* data, uint16_t frameCount) {
    if (data != nullptr && frameCount > 0 && (mono_downmix || is_volume_used)) {
      ESP_LOGD("VolumeControl", "update_audio_data");
      for (int i = 0; i < frameCount; i++) {
        int32_t pcmLeft = data[i].channel1;
        int32_t pcmRight = data[i].channel2;
        // if mono -> we provide the same output on both channels
        if (mono_downmix) {
          pcmRight = pcmLeft = (pcmLeft + pcmRight) / 2;
        }
        // adjust the volume
        if (is_volume_used) {
          pcmLeft = clip(pcmLeft * volumeFactor / volumeFactorMax);
          pcmRight = clip(pcmRight * volumeFactor / volumeFactorMax);
        }
        data[i].channel1 = pcmLeft;
        data[i].channel2 = pcmRight;
      }
    }
  }

  /**
   * @brief Gets the current volume factor
   * @return Volume factor in the range of 0 to 4096
   */
  int32_t get_volume_factor() { return volumeFactor; }

  /**
   * @brief Gets the maximum volume factor value
   * @return Maximum factor value (4096)
   */
  int32_t get_volume_factor_max() { return volumeFactorMax; }

  /**
   * @brief Enables or disables volume control
   * @param enabled True to enable volume control, false to disable
   */
  void set_enabled(bool enabled) { is_volume_used = enabled; }

  /**
   * @brief Enables or disables mono downmix
   * @param enabled True to enable mono downmix, false to disable
   */
  void set_mono_downmix(bool enabled) { mono_downmix = enabled; }

  /**
   * @brief Sets the volume level (pure virtual function)
   * @param volume Volume level (0-127)
   */
  virtual void set_volume(uint8_t volume) = 0;

 protected:
  bool is_volume_used = false;  ///< Flag indicating if volume control is enabled
  bool mono_downmix = false;    ///< Flag indicating if mono downmix is enabled
  int32_t volumeFactor = 1;     ///< Current volume factor
  int32_t volumeFactorMax = 0x1000;     ///< Maximum volume factor (4096)
  int32_t volumeFactorClippingLimit = 0xfff;  ///< Volume factor clipping limit (4095)

  /**
   * @brief Clips audio sample value to prevent overflow
   * @param value Input audio sample value
   * @return Clipped value within valid 16-bit range (-32768 to 32767)
   */
  int32_t clip(int32_t value) {
    int32_t result = value;
    if (value < -32768) result = -32768;
    if (value > 32767) result = 32767;
    return result;
  }
};

/**
 * @brief Default implementation for handling of the volume of the audio data
 * @author elehobica
 * @copyright Apache License Version 2
 */
class A2DPDefaultVolumeControl : public A2DPVolumeControl {
 public:
  /**
   * @brief Default constructor
   */
  A2DPDefaultVolumeControl() = default;

  /**
   * @brief Constructor with custom volume factor clipping limit
   * @param limit Maximum volume factor limit (must be less than
   * 4096)
   */
  A2DPDefaultVolumeControl(int32_t limit) {
    assert(limit < volumeFactorMax);
    volumeFactorClippingLimit = limit;
  };

 protected:
  /**
   * @brief Sets the volume using exponential curve calculation
   * @param volume Volume level (0-127)
   */
  void set_volume(uint8_t volume) override {
    constexpr float base = 1.4f;
    constexpr float bits = 12.0f;
    constexpr float zero_ofs = pow(base, -bits);
    constexpr float scale = pow(2.0f, bits);
    float volumeFactorFloat =
        (pow(base, volume * bits / 127.0f - bits) - zero_ofs) * scale /
        (1.0f - zero_ofs);
    volumeFactor = volumeFactorFloat;
    if (volumeFactor > volumeFactorClippingLimit) {
      volumeFactor = volumeFactorClippingLimit;
    }
  }
};

/**
 * @brief Exponential volume control
 * @author rbruelma
 * @copyright Apache License Version 2
 */
class A2DPSimpleExponentialVolumeControl : public A2DPVolumeControl {
 public:
  /**
   * @brief Default constructor
   */
  A2DPSimpleExponentialVolumeControl() = default;

  /**
   * @brief Constructor with custom volume factor clipping limit
   * @param limit Maximum volume factor limit (must be less than
   * 4096)
   */
  A2DPSimpleExponentialVolumeControl(int32_t limit) {
    assert(limit < volumeFactorMax);
    volumeFactorClippingLimit = limit;
  };

 protected:
  /**
   * @brief Sets the volume using simple exponential calculation
   * @param volume Volume level (0-127)
   */
  void set_volume(uint8_t volume) override {
    float volumeFactorFloat = volume;
    volumeFactorFloat = pow(2.0f, volumeFactorFloat * 12.0f / 127.0f);
    volumeFactor = volumeFactorFloat - 1.0f;
    if (volumeFactor > volumeFactorClippingLimit) {
      volumeFactor = volumeFactorClippingLimit;
    }
  }
};

/**
 * @brief The simplest possible implementation of a VolumeControl
 * @author pschatzmann
 * @copyright Apache License Version 2
 */
class A2DPLinearVolumeControl : public A2DPVolumeControl {
 public:
  /**
   * @brief Constructor that sets volumeFactorMax to 128 for linear scaling
   */
  A2DPLinearVolumeControl() { volumeFactorMax = 128; }

 protected:
  /**
   * @brief Sets the volume using direct linear mapping
   * @param volume Volume level (0-127) - directly used as volume factor
   */
  void set_volume(uint8_t volume) override { volumeFactor = volume; }
};

/**
 * @brief Keeps the audio data as is -> no volume control!
 * @author pschatzmann
 * @copyright Apache License Version 2
 */
class A2DPNoVolumeControl : public A2DPVolumeControl {
 public:
  /**
   * @brief Constructor with optional fixed volume setting
   * @param fixedVolume Fixed volume factor (default: 0x1000/4096 for no change)
   *                    If different from 0x1000, volume control will be enabled with this fixed value
   */
  A2DPNoVolumeControl(int32_t fixedVolume = 0x1000) {
    is_volume_used = fixedVolume != 0x1000;
    mono_downmix = false;
    volumeFactor = fixedVolume;  // fixed volume
    volumeFactorMax = 0x1000;  // no change
  }
  
  /**
   * @brief Override that does nothing - no audio data modification
   * @param data Pointer to audio frame data (unused)
   * @param frameCount Number of frames (unused)
   */
  void update_audio_data(Frame* data, uint16_t frameCount) override {}

  /**
   * @brief Override that does nothing - no volume setting
   * @param volume Volume level (unused)
   */
  void set_volume(uint8_t volume) override {}
};