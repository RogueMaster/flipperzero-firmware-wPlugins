/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
/* ISO/IEC 7816-4 command and response APDUs: encoding, response reading,
 * status classes and a bounded exchange channel over a caller transport. The
 * channel sends command chains, follows 61XX with GET RESPONSE and applies one
 * 6CXX correction per step.
 * Standards: ISO/IEC 7816-4:2020 sections 5.2 to 5.6 and 12.8.1.
 * Configuration: TC_ENABLE_APDU.
 * Limitations: interindustry CLA values 00 to 1F only (first interindustry
 * values, logical channels 0 to 3). T=0 TPDU handling, secure messaging and
 * card-application status meanings belong to the transport and the
 * application layers.
 * Contracts: docs/api.md.
 * Guide: docs/apdu.md. */
#ifndef TINY_CRYPTO_APDU_H_
#define TINY_CRYPTO_APDU_H_
#include <tiny_crypto/common.h>
#ifdef __cplusplus
extern "C" {
#endif

/* Results follow the TC_RSA_result order. INVALID is a malformed card
 * answer. LIMIT is a short buffer, scratch, card size limit or exhausted
 * exchange budget. ARGUMENT is a NULL pointer, a bad span, a forbidden overlap
 * or a value outside the function's domain. UNSUPPORTED is a CLA outside the
 * first interindustry values. ERROR is a transport failure or a stopped
 * channel. */
typedef TC_result TC_APDU_result;
#define TC_APDU_OK TC_RESULT_OK
#define TC_APDU_INVALID TC_RESULT_INVALID
#define TC_APDU_LIMIT TC_RESULT_LIMIT
#define TC_APDU_ARGUMENT TC_RESULT_ARGUMENT
#define TC_APDU_UNSUPPORTED TC_RESULT_UNSUPPORTED
#define TC_APDU_ERROR TC_RESULT_ERROR

/* Length-field format (ISO/IEC 7816-4 5.2). SHORT uses one-byte Lc and Le
 * and chains longer command data. EXTENDED uses the short form whenever
 * Nc <= 255 and Ne <= 256, and the extended form otherwise. Select EXTENDED
 * only when the card states the capability (5.1, 12.8.1). */
typedef enum { TC_APDU_SHORT, TC_APDU_EXTENDED } TC_APDU_length_format;

#define TC_APDU_HEADER_BYTES 4u
#define TC_APDU_STATUS_BYTES 2u
#define TC_APDU_SHORT_MAX_NC 255u
#define TC_APDU_SHORT_MAX_NE 256u
#define TC_APDU_MAX_NC 65535u
#define TC_APDU_MAX_NE 65536ul
/* Largest SHORT command: header, Lc, 255 data bytes and Le. */
#define TC_APDU_SHORT_COMMAND_MAX_BYTES 261u
/* Upper bound for an EXTENDED command with nc data bytes: header, 3-byte Lc
 * and 2-byte Le. Valid for nc <= SIZE_MAX - 9. TC_APDU_command_size reports
 * LIMIT when the size is not representable. */
#define TC_APDU_EXTENDED_COMMAND_BYTES(nc) (9u + (size_t)(nc))
/* Response buffer bytes for nr data bytes and SW1 SW2. */
#define TC_APDU_RESPONSE_BYTES(nr) ((size_t)(nr) + TC_APDU_STATUS_BYTES)

/* One command APDU. data is borrowed and Nc = data.length, 0 to 65535. ne is
 * Ne: 0 omits the Le field, 1 to 65536 request that many bytes (256 encodes
 * short 00 and 65536 encodes extended 0000). ne is uint32_t so 65536 fits a
 * 16-bit size_t. CLA b5 (0x10) is reserved for the chaining the channel
 * performs. */
typedef struct {
  TC_bytes data;
  uint32_t ne;
  uint8_t cla, ins, p1, p2;
} TC_APDU_command;

/* One response APDU. data borrows the response bytes before SW1 SW2. sw is
 * SW1 << 8 | SW2. */
typedef struct {
  TC_bytes data;
  uint16_t sw;
} TC_APDU_response;

/* Status classes of ISO/IEC 7816-4 5.6 Table 6. Classes are informational.
 * Application layers map specific values to their own meanings. */
typedef enum {
  TC_APDU_SW_SUCCESS,     /* 9000 */
  TC_APDU_SW_MORE_DATA,   /* 61XX: SW2 bytes still available */
  TC_APDU_SW_WARNING,     /* 62XX, 63XX */
  TC_APDU_SW_EXECUTION,   /* 64XX, 65XX, 66XX */
  TC_APDU_SW_WRONG_LE,    /* 6CXX: SW2 is the exact available length */
  TC_APDU_SW_CHECKING,    /* 67XX to 6FXX except 6CXX */
  TC_APDU_SW_PROPRIETARY, /* 9XXX except 9000 */
  TC_APDU_SW_INVALID      /* 60XX and every value outside 6XXX and 9XXX */
} TC_APDU_status_class;

#if TC_ENABLE_APDU
/* Encoded size of command in format (ISO/IEC 7816-4 5.2, cases 1 to 4E).
 *
 * TC_APDU_ARGUMENT     NULL command or size, a span with NULL data and a
 *                      nonzero length, an unknown format, Nc above 65535, Ne
 *                      above 65536, Nc above 255 or Ne above 256 in SHORT, or
 *                      CLA b5 set.
 * TC_APDU_UNSUPPORTED  CLA above 1F: proprietary (b8 set), further
 *                      interindustry (40 to 7F) or RFU (20 to 3F) values
 *                      (5.4.1 Tables 2 and 3).
 * TC_APDU_LIMIT        the size is not representable in size_t.
 *
 * *size changes only on TC_APDU_OK. */
TC_APDU_result TC_APDU_command_size(const TC_APDU_command* command, TC_APDU_length_format format,
                                    size_t* size);

/* Encode command into out and set *written. out must be disjoint from the
 * command data, *command and *written. Results as TC_APDU_command_size, plus
 * ARGUMENT for NULL written, out with NULL data and a nonzero capacity, or an
 * overlap, and LIMIT when out.capacity is below the size. Every failure
 * leaves out and *written unchanged. */
TC_APDU_result TC_APDU_command_encode(const TC_APDU_command* command, TC_APDU_length_format format,
                                      TC_buffer out, size_t* written);

/* Read one response APDU. out->data borrows encoded, which stays alive and
 * unchanged while out is used.
 *
 * TC_APDU_ARGUMENT  NULL out, a span with NULL data and a nonzero length, or
 *                   out overlapping encoded.
 * TC_APDU_INVALID   fewer than 2 bytes, an invalid SW (60XX, or outside 6XXX
 *                   and 9XXX), or response data with SW1 64 to 6F
 *                   (ISO/IEC 7816-4 5.6).
 *
 * out changes only on TC_APDU_OK. */
TC_APDU_result TC_APDU_response_read(TC_bytes encoded, TC_APDU_response* out);

/* Classify sw by ISO/IEC 7816-4 5.6 Table 6. */
TC_APDU_status_class TC_APDU_status_classify(uint16_t sw);
#endif

/* Send one command APDU and receive one complete response APDU (data, SW1,
 * SW2) into response. Write at most response.capacity bytes and set *length
 * only on TC_OK. Return TC_ERROR when delivery failed or is uncertain. The
 * call is synchronous, and command stays valid only during the call. */
typedef TC_status (*TC_APDU_transmit)(void* context, TC_bytes command, TC_buffer response,
                                      size_t* length);

/* A transmit callback and its context. The caller owns context and keeps it
 * alive while a channel uses the transport. */
typedef struct {
  TC_APDU_transmit transmit;
  void* context;
} TC_APDU_transport;

enum {
  /* GET RESPONSE uses CLA & 0x03 (SM and chaining bits cleared) in place of
   * the command CLA. ISO/IEC 7816-4 5.6 permits the same CLA. SP 800-73-5
   * Part 2 4.2.6 and A.4.1 use 00. */
  TC_APDU_GET_RESPONSE_PLAIN_CLA = 1u << 0
};

/* Channel configuration.
 * format             length-field format of every command.
 * flags              TC_APDU_GET_RESPONSE_* bits.
 * exchanges          C-RP budget for the channel lifetime, at least 1.
 * max_command_bytes  0, or the card's largest command APDU (DO 7F66 first
 *                    integer, ISO/IEC 7816-4 12.8.1), at least 4.
 * max_response_bytes 0, or the card's largest response APDU (DO 7F66 second
 *                    integer), at least 3. Every step stays within it: the
 *                    command's Ne and each GET RESPONSE Le are lowered to
 *                    this size less SW1 SW2. */
typedef struct {
  TC_APDU_length_format format;
  unsigned flags;
  size_t exchanges;
  size_t max_command_bytes;
  size_t max_response_bytes;
} TC_APDU_channel_options;

/* Exchange state over one transport. Members are private. Do not copy an
 * initialized channel. The channel borrows the scratch buffer and the
 * transport context until TC_APDU_channel_clear. */
typedef struct {
  TC_APDU_transport transport;
  uint8_t* scratch;
  size_t scratch_capacity, exchanges_left, max_command_bytes, max_response_bytes;
  unsigned flags;
  uint8_t format, stopped;
} TC_APDU_channel;

#if TC_ENABLE_APDU
/* Start a channel. scratch holds one encoded command fragment and is wiped
 * after every transmit. TC_APDU_SHORT_COMMAND_MAX_BYTES covers every SHORT
 * command, and TC_APDU_EXTENDED_COMMAND_BYTES(nc) an EXTENDED command with nc
 * data bytes. A command that needs more scratch returns LIMIT before transmit.
 *
 * TC_APDU_ARGUMENT  NULL channel, options or transport.transmit, scratch with
 *                   NULL data or fewer than 4 bytes, scratch overlapping
 *                   *channel or *options, an unknown format or flag, zero
 *                   exchanges, or a card limit below its minimum.
 *
 * *channel changes only on TC_APDU_OK. */
TC_APDU_result TC_APDU_channel_init(TC_APDU_channel* channel, TC_APDU_transport transport,
                                    const TC_APDU_channel_options* options, TC_buffer scratch);

/* Tighten the card limits (ISO/IEC 7816-4 12.8.1) and replace the
 * GET RESPONSE flags. A zero limit keeps the current one. A nonzero limit
 * replaces it only when smaller or when none was set.
 *
 * TC_APDU_ARGUMENT  NULL or cleared channel, an unknown flag, or a nonzero
 *                   limit below its minimum, with the channel unchanged.
 * TC_APDU_ERROR     the channel is stopped. */
TC_APDU_result TC_APDU_channel_restrict(TC_APDU_channel* channel, size_t max_command_bytes,
                                        size_t max_response_bytes, unsigned flags);

/* Send command and collect the complete response in response.
 *
 * SHORT commands with Nc above 255 are sent as 255-byte fragments with
 * CLA | 0x10 and no Le, then the last fragment with the command CLA and Le
 * (ISO/IEC 7816-4 5.3.3). Each intermediate answer must be 9000 without data
 * to continue. Any other valid status without data ends the exchange with
 * TC_APDU_OK and that status, such as 6883, 6884 or 6982. Data or a 62XX or
 * 63XX warning on an intermediate answer is INVALID (5.6).
 *
 * A final 61XX is followed by GET RESPONSE (INS C0, P1 P2 00 00, Le = SW2,
 * where 00 requests 256, lowered to the card's response buffer), and each data
 * chunk is appended (5.3.4). GET RESPONSE uses the command CLA, or CLA & 0x03
 * with TC_APDU_GET_RESPONSE_PLAIN_CLA. A 6CXX answer without data to an
 * unchained command with Le, or to a GET RESPONSE step, re-issues that step
 * once with Le = SW2 (5.6), when the command CLA has no SM bits. A second 6CXX
 * on the same step, and 6CXX on a chained command, on a command without Le or
 * under SM, end the exchange with TC_APDU_OK, that status and the data
 * collected before it. A command without Le, such as VERIFY, is sent once.
 *
 * A card may return more bytes than Ne up to the offered capacity (TWIC Part 2
 * v5 5.2 note 2). Each transmit offers the remaining response capacity. A
 * GET RESPONSE or corrected step needs room for its Le and SW1 SW2. When the
 * channel has max_response_bytes, Ne above max_response_bytes - 2 is lowered
 * to that value. An encoded fragment above max_command_bytes or the scratch
 * capacity returns LIMIT before transmit. Each transmit consumes one exchange
 * from the budget. A SHORT chain is checked whole before its first fragment:
 * every fragment must fit both bounds, and the budget must cover every
 * fragment.
 *
 * On TC_APDU_OK, out->data borrows response.data with the collected data and
 * out->sw is the final status of any class. Bytes after the data that held
 * status bytes are wiped. The transport must report 2 <= *length <= offered
 * capacity.
 *
 * TC_APDU_ARGUMENT     NULL or cleared channel, NULL command or out,
 *                      response with NULL data or below 2 bytes, command
 *                      values rejected by TC_APDU_command_size (SHORT allows
 *                      Nc above 255), or response overlapping scratch, the
 *                      command data, *channel, *command or *out, or scratch
 *                      overlapping the command data, *command or *out.
 * TC_APDU_UNSUPPORTED  CLA above 1F.
 * TC_APDU_ERROR        the channel is stopped. These three results and
 *                      ARGUMENT leave response and out unchanged.
 * TC_APDU_ERROR        the transport returned TC_ERROR or broke its length
 *                      contract. The channel stops until the next
 *                      TC_APDU_channel_init.
 * TC_APDU_INVALID      a malformed card answer (5.6, 5.3.3).
 * TC_APDU_LIMIT        the response capacity, scratch, max_command_bytes or
 *                      the exchange budget ran out. The channel stays usable
 *                      and a new command ends the interrupted chain.
 *
 * ERROR after processing, INVALID and LIMIT wipe the whole response buffer.
 * out changes only on TC_APDU_OK. */
TC_APDU_result TC_APDU_transceive(TC_APDU_channel* channel, const TC_APDU_command* command,
                                  TC_buffer response, TC_APDU_response* out);

/* Remaining exchange budget. A NULL channel has none. */
size_t TC_APDU_channel_exchanges_left(const TC_APDU_channel* channel);

/* Wipe the scratch buffer and the channel state. Accepts NULL. */
void TC_APDU_channel_clear(TC_APDU_channel* channel);
#endif

#ifdef __cplusplus
}
#endif
#endif
