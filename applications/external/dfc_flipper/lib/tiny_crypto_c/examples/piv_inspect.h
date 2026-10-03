/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
/* Inspect a PIV card over any TC_APDU_transport: select, secure messaging,
 * the virtual contact interface, the PIN, the inventory, the composed card
 * check and key proofs, in protocol order, with a printed report. The
 * library does the card work. This example owns the order, the text and its
 * acceptance requirements. Guide: docs/piv-card-check.md. */
#ifndef EXAMPLE_PIV_INSPECT_H_
#define EXAMPLE_PIV_INSPECT_H_
#include <tiny_crypto/piv_card_check.h>
#include <tiny_crypto/source.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

enum { EXAMPLE_PIV_INSPECT_ANCHORS = 4, EXAMPLE_PIV_INSPECT_CRLS = 4 };

/* Called with each present object that holds no secret material, for a
 * dump. The object borrows storage that the run wipes before it returns. */
typedef void (*ExamplePIVInspectDump)(void* context, const TC_PIV_object* object);

/* Inputs of one run. Every span stays borrowed and unchanged for the run.
 * interface        the interface of the transport. Contactless applies the
 *                  PIN and VCI rules of SP 800-73-5 Part 1 Table 4.
 * format           SHORT, or EXTENDED when the reader and card support it.
 * pin              6 to 8 ASCII digits, or empty for no PIN.
 * pairing_code     8 ASCII digits for the VCI, or empty.
 * minimum_retries  PIN tries the card must report before a submission, at
 *                  least 2.
 * anchors          DER trust anchors for the card certificates and the
 *                  content signers, 1 to EXAMPLE_PIV_INSPECT_ANCHORS.
 * crls             DER CRL byte sources, up to EXAMPLE_PIV_INSPECT_CRLS, of any
 *                  size. Each is prepared once for the certificates the
 *                  check queries (examples/piv_inspect_crl.h).
 * ocsp             one DER OCSP response per TC_PIV_CARD_SLOT_*, or empty.
 * revocation       the revocation evidence policy of both contexts.
 * at               the evaluation time.
 * random           secure messaging scalars and key proof challenges.
 * host_id          the host identifier of key establishment (Part 2 4.1.1).
 * dump             NULL, or a callback for present objects. */
typedef struct {
  TC_PIV_interface interface;
  TC_APDU_length_format format;
  TC_bytes pin, pairing_code;
  unsigned minimum_retries;
  const TC_bytes* anchors;
  size_t anchor_count;
  const TC_source* crls;
  size_t crl_count;
  TC_bytes ocsp[TC_PIV_CARD_CERTIFICATES];
  TC_validation_revocation revocation;
  TC_X509_time at;
  TC_random_source random;
  uint8_t host_id[8];
  ExamplePIVInspectDump dump;
  void* dump_context;
} ExamplePIVInspectOptions;

/* Run the inspection on transport and print it to out. The run uses static
 * storage, so one run at a time. It clears the inventory, the link and the
 * secure messaging session and wipes its storage on every return.
 * report receives the check entries of the PIV application. Its views
 * borrow the wiped storage, so read only checks and count after return.
 * Returns 0 when the report meets the example's requirements, 1 when a
 * check FAILED, a requirement did not pass, or the card refused a supplied
 * PIN or pairing code, and 2 for invalid options. */
int example_piv_inspect_run(const ExamplePIVInspectOptions* options, TC_APDU_transport transport,
                            FILE* out, TC_PIV_card_report* report);

#ifdef __cplusplus
}
#endif
#endif
