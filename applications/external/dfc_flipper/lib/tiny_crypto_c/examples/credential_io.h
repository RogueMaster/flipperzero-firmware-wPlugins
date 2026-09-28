/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef EXAMPLE_CREDENTIAL_IO_H_
#define EXAMPLE_CREDENTIAL_IO_H_
#include <tiny_crypto/common.h>
#include <tiny_crypto/tlv.h>

#ifdef __cplusplus
extern "C" {
#endif

enum { EXAMPLE_CARD_RESPONSE_BYTES = 256, EXAMPLE_CARD_STATUS_BYTES = 2 };
typedef enum { EXAMPLE_CARD_PIV, EXAMPLE_CARD_TWIC } ExampleCardApplication;
typedef enum { EXAMPLE_CARD_READ_SHORT, EXAMPLE_CARD_READ_EXTENDED } ExampleCardReadMode;
/* GENERAL AUTHENTICATE P1/P2 values from SP 800-78-5 Tables 8 and 9. */
typedef enum {
  EXAMPLE_CARD_ALGORITHM_RSA_3072 = 0x05,
  EXAMPLE_CARD_ALGORITHM_RSA_1024 = 0x06,
  EXAMPLE_CARD_ALGORITHM_RSA_2048 = 0x07,
  EXAMPLE_CARD_ALGORITHM_EC_P256 = 0x11,
  EXAMPLE_CARD_ALGORITHM_EC_P384 = 0x14
} ExampleCardAlgorithm;
typedef enum {
  EXAMPLE_CARD_KEY_PIV_AUTHENTICATION = 0x9a,
  EXAMPLE_CARD_KEY_CARD_AUTHENTICATION = 0x9e
} ExampleCardKeyReference;
typedef enum {
  EXAMPLE_CARD_OK, EXAMPLE_CARD_STATUS, EXAMPLE_CARD_LIMIT,
  EXAMPLE_CARD_TRANSPORT, EXAMPLE_CARD_PROTOCOL, EXAMPLE_CARD_ARGUMENT, EXAMPLE_CARD_REFUSED
} ExampleCardResult;

/* Return nonzero only for a complete response, including its two status bytes.
 * Write at most capacity bytes and set length only after a successful transfer.
 * The callback must finish synchronously and keep command/response bytes private. */
typedef int (*ExampleCardTransmit)(void* context, const uint8_t* command,
    size_t command_length, uint8_t* response, size_t capacity, size_t* length);
typedef struct {
  ExampleCardTransmit transmit;
  void* context;
  size_t exchanges_left;
  int stopped;
} ExampleCardIO;
typedef struct { size_t length; uint16_t status; } ExampleCardResponse;
typedef struct { int used; } ExampleCardPIN;

/* Initialize guard to zero once per utility run, retaining it across reconnects.
 * Requires a validated PIV selection on a contact reader and a held transaction.
 * Supply 6..8 ASCII digits in separate storage. The caller wipes those digits.
 * Queries local PIN retry status, then submits at most once with >=2 retries.
 * An already-verified status succeeds without submitting. Other statuses and
 * transport failures stop the session. No length correction or retry is used.
 * status changes only when a complete card status is received. */
ExampleCardResult example_card_verify_pin(ExampleCardIO* io, ExampleCardPIN* guard,
    const uint8_t* digits, size_t length, uint16_t* status);
typedef enum {
  EXAMPLE_CARD_MODEL_PIV, EXAMPLE_CARD_MODEL_TWIC_LEGACY, EXAMPLE_CARD_MODEL_TWIC_NEXGEN
} ExampleCardModel;

/* Check SELECT response framing and the unique 61/4F application identity.
 * Supports PIV 1.0 and production TWIC 1.1/1.3. Optional properties retain
 * their own schemas. Input and out must be disjoint; out changes only on OK.
 * Limits: 4096 response bytes, 64 elements and four constructed levels. */
TC_TLV_result example_card_identity(TC_bytes response,
    ExampleCardApplication expected, ExampleCardModel* out);

/* Hold the reader transaction for the whole operation. Buffer, io, out and
 * callback state must occupy separate storage. Responses accumulate in buffer;
 * reserve two extra bytes for status and enough space for each requested chunk.
 * out changes only on OK or STATUS. Other processing failures wipe capacity
 * bytes and stop this IO session. Argument errors preserve all storage.
 * Parse and validate the returned object before using its contents. */
ExampleCardResult example_card_select(ExampleCardIO* io, ExampleCardApplication application,
    uint8_t* buffer, size_t capacity, ExampleCardResponse* out);
ExampleCardResult example_card_read(ExampleCardIO* io, const uint8_t* tag,
    size_t tag_length, uint8_t* buffer, size_t capacity, ExampleCardResponse* out);

/* Select explicitly when the card and reader support extended APDUs.
 * Requests min(capacity - 2, 65535) bytes, following 61xx with GET RESPONSE.
 * A 6282 end-of-object warning returns STATUS with the received bytes intact.
 * The caller must check complete object framing before accepting that warning.
 * GET DATA length errors stop the session; GET RESPONSE allows one correction.
 * Requires capacity >= 3. The ownership and failure rules above also apply. */
ExampleCardResult example_card_read_extended(ExampleCardIO* io, const uint8_t* tag,
    size_t tag_length, uint8_t* buffer, size_t capacity, ExampleCardResponse* out);

/* Read one complete 53 object envelope using the selected APDU format.
 * Accepts 9000 or 6282 only after checking exact outer framing. The returned
 * status retains the card's value. Contents still require schema and signature
 * validation. Framing failures wipe buffer and stop the session. */
ExampleCardResult example_card_object_read(ExampleCardIO* io, ExampleCardReadMode mode,
    const uint8_t* tag, size_t tag_length, uint8_t* buffer, size_t capacity,
    ExampleCardResponse* out);

enum { EXAMPLE_TWIC_OBJECTS = 8 };
enum {
  EXAMPLE_TWIC_CHUID = 0x3000, EXAMPLE_TWIC_UNSIGNED_CHUID = 0x3002,
  EXAMPLE_TWIC_FINGERPRINTS = 0x2003, EXAMPLE_TWIC_FACE = 0x6030,
  EXAMPLE_TWIC_PRINTED = 0x3001, EXAMPLE_TWIC_IRIS = 0x1015,
  EXAMPLE_TWIC_PERSONAL = 0x6011, EXAMPLE_TWIC_HANDWRITTEN = 0x6012
};
typedef struct { uint16_t container; TC_bytes contents; } ExampleTWICObject;
typedef struct {
  ExampleTWICObject objects[EXAMPLE_TWIC_OBJECTS];
  size_t count;
  TC_bytes security;
} ExampleTWICInventory;

/* Select and confirm the expected TWIC application, then collect its security
 * object and stored data objects. Legacy requires CHUID, unsigned CHUID and
 * fingerprints. NEXGEN also requires face/printed objects and probes optional
 * iris/personal/handwritten objects; only an empty 6A82 response means absent.
 * Contents borrow the pool and preserve inner TLVs and encrypted bytes. security
 * includes its outer 53. Keep the pool unchanged through signature/hash checks.
 * max_object bounds each response; capacity includes transfer/status headroom.
 * Work bounds parsing bytes; io bounds exchanges. Inputs and writable ranges
 * are disjoint. Only OK writes out. Processing failures wipe the pool and stop
 * the session. This collects data; authenticate it before making decisions. */
ExampleCardResult example_twic_inventory_read(ExampleCardIO* io, ExampleCardModel model, ExampleCardReadMode mode,
    uint8_t* pool, size_t capacity, size_t max_object, size_t* work, ExampleTWICInventory* out);

/* PIV/card-authentication key (9A/9E), SP 800-73-5 Part 2 Appendix A.4.
 * Supply the complete RSA representative or EC digest for the selected algorithm.
 * Algorithm/key policy and fresh challenge generation belong to the caller.
 * Supports RSA 1024/2048/3072 and P-256/P-384. The application must explicitly
 * permit legacy keys before selecting RSA 1024. Buffer retains the complete
 * 7C/82 response for parsing and verification. Reserve at least 514 bytes.
 * Command chaining and GET RESPONSE share io's budget. GENERAL AUTHENTICATE
 * is submitted once per chain; failures stop the session. Input, buffer, io,
 * callback state and out are disjoint. Argument errors preserve caller state. */
ExampleCardResult example_card_authenticate(ExampleCardIO* io, ExampleCardAlgorithm algorithm,
    ExampleCardKeyReference reference, TC_bytes challenge,
    uint8_t* buffer, size_t capacity, ExampleCardResponse* out);
#ifdef __cplusplus
}
#endif
#endif
