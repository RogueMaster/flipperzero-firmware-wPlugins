/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
/* PIV biometric record readers: INCITS 378 fingerprint minutiae and
 * INCITS 385 facial images under the PIV and TWIC profiles.
 * Standards: SP 800-76-2, INCITS 378-2004, INCITS 385-2004.
 * Configuration: TC_ENABLE_PIV_OBJECTS.
 * Limitations: structural validation only. Matching belongs to the
 * application. Iris records are unsupported.
 * Contracts: docs/api.md. */
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

#if TC_ENABLE_PIV_OBJECTS
/* Validate an INCITS 378-2004 minutiae record against the PIV card profile
 * (SP 800-76-2 section 4.4). Checks the FMR header, the record length, a
 * nonzero product and CBEFF-registered capture equipment, 197 pixels/cm
 * resolution, two distinct finger views, quality values, minutiae within the
 * image and empty extension areas. The record length is at most 1574 bytes.
 * encoded borrows input. input and out must be disjoint. Charges no work.
 * Returns OK with out written. ARGUMENT for NULL out or input data, or
 * overlap. INVALID for any structural or profile failure. out changes only on
 * OK. Matching belongs to the application. */
TC_TLV_result TC_PIV_fingerprint_read(TC_bytes input, TC_PIV_fingerprint_record* out);
#endif

typedef enum { TC_PIV_FACE_PROFILE_PIV, TC_PIV_FACE_PROFILE_TWIC } TC_PIV_face_profile;

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

#if TC_ENABLE_PIV_OBJECTS
/* Validate the framing and every image block of an INCITS 385-2004 facial
 * record (SP 800-76-2 section 7.2). TC_PIV_FACE_PROFILE_PIV requires Full
 * Frontal images at least 421 pixels wide. TC_PIV_FACE_PROFILE_TWIC accepts
 * the Basic records found on TWIC credentials. Both check lengths, feature
 * points, pose fields, sRGB color space, the source type and the JPEG or JPEG
 * 2000 signature. encoded borrows input. input and out must be disjoint.
 * Charges no work.
 * Returns OK with out written. ARGUMENT for NULL out or input data, an
 * unknown profile or overlap. INVALID for any structural or profile failure,
 * including a record without images. out changes only on OK. */
TC_TLV_result TC_PIV_face_read(TC_bytes input, TC_PIV_face_profile profile,
                               TC_PIV_face_record* out);

/* Return image index of a record accepted by TC_PIV_face_read. The call
 * rescans the image blocks before index, so its cost grows with index. The
 * image span borrows the record input. record->encoded and out must be
 * disjoint. Charges no work.
 * Returns OK with out written. ARGUMENT for NULL arguments, an index at or
 * beyond image_count, an unknown record profile or overlap. INVALID when the
 * record bytes changed after TC_PIV_face_read. out changes only on OK. */
TC_TLV_result TC_PIV_face_image_read(const TC_PIV_face_record* record, size_t index,
                                     TC_PIV_face_image* out);
#endif

#ifdef __cplusplus
}
#endif
#endif
