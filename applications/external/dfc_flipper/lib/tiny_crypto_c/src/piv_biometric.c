/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include <tiny_crypto/common.h>
#if TC_ENABLE_PIV_OBJECTS
#include "internal.h"
#include <string.h>
#include <tiny_crypto/piv_biometric.h>

enum {
  FINGERPRINT_HEADER_BYTES = 26,
  FINGERPRINT_VIEW_HEADER_BYTES = 4,
  FINGERPRINT_MINUTIA_BYTES = 6,
  FINGERPRINT_EXTENSION_LENGTH_BYTES = 2,
  FINGERPRINT_RECORD_MAX = 1574,
  FINGERPRINT_PIV_VIEWS = 2,
  FINGERPRINT_PIV_RESOLUTION = 197,
  FINGERPRINT_MINUTIA_MAX = 128,
  FINGERPRINT_CAPTURE_COMPLIANCE = 8,
  FINGERPRINT_CAPTURE_ID_MASK = 0x0fff
};

static uint16_t read_u16(const uint8_t *value) {
  return (uint16_t)((uint16_t)value[0] << 8) | value[1];
}

static int finger_quality(uint8_t quality) {
  return quality == 20 || quality == 40 || quality == 60 || quality == 80 ||
         quality == 100 || quality == 254 || quality == 255;
}

static uint32_t read_u32(const uint8_t *value) {
  return ((uint32_t)value[0] << 24) | ((uint32_t)value[1] << 16) |
         ((uint32_t)value[2] << 8) | value[3];
}

TC_TLV_result TC_PIV_fingerprint_read(TC_bytes input,
                                      TC_PIV_fingerprint_record *out) {
  static const uint8_t format[] = {'F', 'M', 'R', 0};
  static const uint8_t version[] = {' ', '2', '0', 0};
  TC_PIV_fingerprint_record parsed;
  if (!out || !input.data ||
      !tc_internal_ranges_disjoint(input.data, input.length, out, sizeof *out))
    return TC_TLV_ARGUMENT;
  if (input.length < FINGERPRINT_HEADER_BYTES ||
      input.length > FINGERPRINT_RECORD_MAX)
    return TC_TLV_INVALID;
  const uint8_t *bytes = input.data;
  if (memcmp(bytes, format, sizeof format) ||
      memcmp(bytes + 4, version, sizeof version) ||
      read_u16(bytes + 8) != input.length)
    return TC_TLV_INVALID;

  parsed.encoded = input;
  parsed.product_owner = read_u16(bytes + 10);
  parsed.product_type = read_u16(bytes + 12);
  parsed.capture_equipment = read_u16(bytes + 14);
  parsed.width = read_u16(bytes + 16);
  parsed.height = read_u16(bytes + 18);
  parsed.view_count = bytes[24];
  if (!parsed.product_owner || !parsed.product_type ||
      (parsed.capture_equipment >> 12) != FINGERPRINT_CAPTURE_COMPLIANCE ||
      !(parsed.capture_equipment & FINGERPRINT_CAPTURE_ID_MASK) ||
      !parsed.width || !parsed.height ||
      read_u16(bytes + 20) != FINGERPRINT_PIV_RESOLUTION ||
      read_u16(bytes + 22) != FINGERPRINT_PIV_RESOLUTION ||
      parsed.view_count != FINGERPRINT_PIV_VIEWS || bytes[25])
    return TC_TLV_INVALID;

  size_t offset = FINGERPRINT_HEADER_BYTES;
  uint8_t positions[FINGERPRINT_PIV_VIEWS];
  for (size_t view = 0; view < FINGERPRINT_PIV_VIEWS; ++view) {
    if (input.length - offset < FINGERPRINT_VIEW_HEADER_BYTES)
      return TC_TLV_INVALID;
    positions[view] = bytes[offset];
    const uint8_t view_and_impression = bytes[offset + 1];
    const uint8_t impression = view_and_impression & 0x0f;
    const uint8_t minutiae = bytes[offset + 3];
    if (positions[view] > 10 || (view && positions[view] == positions[0]) ||
        (view_and_impression >> 4) != 0 ||
        (impression != 0 && impression != 2) ||
        !finger_quality(bytes[offset + 2]) ||
        minutiae > FINGERPRINT_MINUTIA_MAX)
      return TC_TLV_INVALID;
    offset += FINGERPRINT_VIEW_HEADER_BYTES;
    if (minutiae > (input.length - offset) / FINGERPRINT_MINUTIA_BYTES)
      return TC_TLV_INVALID;
    for (size_t i = 0; i < minutiae; ++i) {
      const uint8_t *minutia = bytes + offset;
      const unsigned x = read_u16(minutia) & 0x3fff;
      const unsigned y = read_u16(minutia + 2) & 0x3fff;
      if ((minutia[0] >> 6) == 3 || (minutia[2] & 0xc0) || x >= parsed.width ||
          y >= parsed.height || minutia[4] > 179 || minutia[5] > 100)
        return TC_TLV_INVALID;
      offset += FINGERPRINT_MINUTIA_BYTES;
    }
    if (input.length - offset < FINGERPRINT_EXTENSION_LENGTH_BYTES ||
        read_u16(bytes + offset))
      return TC_TLV_INVALID;
    offset += FINGERPRINT_EXTENSION_LENGTH_BYTES;
  }
  if (offset != input.length)
    return TC_TLV_INVALID;
  *out = parsed;
  return TC_TLV_OK;
}

enum {
  FACE_HEADER_BYTES = 14,
  FACE_INFORMATION_BYTES = 20,
  FACE_FEATURE_BYTES = 8,
  FACE_IMAGE_INFORMATION_BYTES = 12,
  FACE_IMAGE_MIN_BYTES = FACE_INFORMATION_BYTES + FACE_IMAGE_INFORMATION_BYTES,
  FACE_FULL_FRONTAL = 1,
  FACE_JPEG = 0,
  FACE_JPEG2000 = 1,
  FACE_SRGB = 1,
  FACE_DIGITAL_STILL = 2,
  FACE_DIGITAL_VIDEO = 6,
  FACE_MIN_WIDTH = 421
};

static TC_TLV_result face_image(TC_bytes input, TC_PIV_face_profile profile,
                                size_t offset, TC_PIV_face_image *out,
                                size_t *next) {
  if (input.length - offset < FACE_INFORMATION_BYTES)
    return TC_TLV_INVALID;
  const uint8_t *bytes = input.data + offset;
  const uint32_t block_length = read_u32(bytes);
  const uint16_t features = read_u16(bytes + 4);
  if (block_length < FACE_IMAGE_MIN_BYTES ||
      block_length > input.length - offset ||
      features > (block_length - FACE_IMAGE_MIN_BYTES) / FACE_FEATURE_BYTES)
    return TC_TLV_INVALID;
  const size_t image_info_offset =
      FACE_INFORMATION_BYTES + (size_t)features * FACE_FEATURE_BYTES;
  if (block_length <= image_info_offset + FACE_IMAGE_INFORMATION_BYTES)
    return TC_TLV_INVALID;
  const uint8_t *information = bytes + image_info_offset;
  const uint16_t width = read_u16(information + 2);
  const uint16_t height = read_u16(information + 4);
  const uint16_t expression = read_u16(bytes + 12);
  if ((profile == TC_PIV_FACE_PROFILE_PIV ? expression != 1 : expression > 1) ||
      bytes[14] || bytes[15] || bytes[16] || bytes[17] || bytes[18] ||
      bytes[19] ||
      (profile == TC_PIV_FACE_PROFILE_PIV
           ? information[0] != FACE_FULL_FRONTAL || width < FACE_MIN_WIDTH
           : information[0] > FACE_FULL_FRONTAL || !width) ||
      information[1] > FACE_JPEG2000 || !height ||
      information[6] != FACE_SRGB ||
      (information[7] != FACE_DIGITAL_STILL &&
       information[7] != FACE_DIGITAL_VIDEO) ||
      read_u16(information + 10) > 100)
    return TC_TLV_INVALID;
  for (size_t i = 0; i < features; ++i) {
    const uint8_t *feature =
        bytes + FACE_INFORMATION_BYTES + i * FACE_FEATURE_BYTES;
    if (feature[0] != 1 || !feature[1] || !read_u16(feature + 2) ||
        read_u16(feature + 2) > width || !read_u16(feature + 4) ||
        read_u16(feature + 4) > height || read_u16(feature + 6))
      return TC_TLV_INVALID;
  }
  const uint8_t *image = information + FACE_IMAGE_INFORMATION_BYTES;
  const size_t image_length =
      block_length - image_info_offset - FACE_IMAGE_INFORMATION_BYTES;
  if (information[1] == FACE_JPEG) {
    if (image_length < 4 || image[0] != 0xff || image[1] != 0xd8 ||
        image[image_length - 2] != 0xff || image[image_length - 1] != 0xd9)
      return TC_TLV_INVALID;
  } else {
    static const uint8_t signature[] = {0,   0,   0,  12, 'j',  'P',
                                        ' ', ' ', 13, 10, 0x87, 10};
    if (image_length < sizeof signature ||
        memcmp(image, signature, sizeof signature))
      return TC_TLV_INVALID;
  }
  if (out) {
    *out = (TC_PIV_face_image){{image, image_length},
                               features,
                               width,
                               height,
                               read_u16(information + 8),
                               read_u16(information + 10),
                               information[0],
                               information[1],
                               information[6],
                               information[7]};
  }
  *next = offset + block_length;
  return TC_TLV_OK;
}

TC_TLV_result TC_PIV_face_read(TC_bytes input, TC_PIV_face_profile profile,
                               TC_PIV_face_record *out) {
  static const uint8_t format[] = {'F', 'A', 'C', 0};
  static const uint8_t version[] = {'0', '1', '0', 0};
  if (!out || !input.data ||
      (profile != TC_PIV_FACE_PROFILE_PIV &&
       profile != TC_PIV_FACE_PROFILE_TWIC) ||
      !tc_internal_ranges_disjoint(input.data, input.length, out, sizeof *out))
    return TC_TLV_ARGUMENT;
  if (input.length < FACE_HEADER_BYTES ||
      memcmp(input.data, format, sizeof format) ||
      memcmp(input.data + 4, version, sizeof version) ||
      read_u32(input.data + 8) != input.length)
    return TC_TLV_INVALID;
  const uint16_t count = read_u16(input.data + 12);
  if (!count)
    return TC_TLV_INVALID;
  size_t offset = FACE_HEADER_BYTES;
  for (size_t i = 0; i < count; ++i) {
    TC_TLV_result result = face_image(input, profile, offset, NULL, &offset);
    if (result != TC_TLV_OK)
      return result;
  }
  if (offset != input.length)
    return TC_TLV_INVALID;
  *out = (TC_PIV_face_record){input, count, profile};
  return TC_TLV_OK;
}

TC_TLV_result TC_PIV_face_image_read(const TC_PIV_face_record *record,
                                     size_t index, TC_PIV_face_image *out) {
  if (!record || !out || index >= record->image_count ||
      !record->encoded.data ||
      (record->profile != TC_PIV_FACE_PROFILE_PIV &&
       record->profile != TC_PIV_FACE_PROFILE_TWIC) ||
      !tc_internal_ranges_disjoint(record->encoded.data, record->encoded.length,
                                   out, sizeof *out))
    return TC_TLV_ARGUMENT;
  size_t offset = FACE_HEADER_BYTES;
  TC_PIV_face_image image = {0};
  for (size_t i = 0; i <= index; ++i) {
    TC_TLV_result result =
        face_image(record->encoded, (TC_PIV_face_profile)record->profile,
                   offset, &image, &offset);
    if (result != TC_TLV_OK)
      return result;
  }
  *out = image;
  return TC_TLV_OK;
}
#endif
