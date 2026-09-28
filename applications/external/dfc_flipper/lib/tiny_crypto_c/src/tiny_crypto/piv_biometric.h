/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef TINY_CRYPTO_PIV_BIOMETRIC_H_
#define TINY_CRYPTO_PIV_BIOMETRIC_H_
#include <tiny_crypto/tlv.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  TC_bytes encoded;
  uint16_t product_owner;
  uint16_t product_type;
  uint16_t capture_equipment;
  uint16_t width;
  uint16_t height;
  uint8_t view_count;
} TC_PIV_fingerprint_record;

/* Validate an INCITS 378-2004 record against the PIV card profile.
 * The encoded span borrows input. out changes only on success. */
TC_TLV_result TC_PIV_fingerprint_read(TC_bytes input,
                                      TC_PIV_fingerprint_record *out);

typedef enum {
  TC_PIV_FACE_PROFILE_PIV,
  TC_PIV_FACE_PROFILE_TWIC
} TC_PIV_face_profile;

typedef struct {
  TC_bytes encoded;
  uint16_t image_count;
  TC_PIV_face_profile profile;
} TC_PIV_face_record;

typedef struct {
  TC_bytes image;
  uint16_t feature_count;
  uint16_t width;
  uint16_t height;
  uint16_t device_type;
  uint16_t quality;
  uint8_t image_type;
  uint8_t encoding;
  uint8_t color_space;
  uint8_t source_type;
} TC_PIV_face_image;

/* Validate an INCITS 385-2004 record against the PIV card profile.
 * The encoded span borrows input. out changes only on success. */
TC_TLV_result TC_PIV_face_read(TC_bytes input, TC_PIV_face_profile profile,
                               TC_PIV_face_record *out);

/* Return one validated image from a record accepted by TC_PIV_face_read.
 * The image span borrows the record. out changes only on success. */
TC_TLV_result TC_PIV_face_image_read(const TC_PIV_face_record *record,
                                     size_t index, TC_PIV_face_image *out);

#ifdef __cplusplus
}
#endif
#endif
