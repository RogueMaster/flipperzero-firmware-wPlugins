/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
/* piv_inspect: inspect a PIV card on a PC/SC reader with examples/piv_inspect.c.
 * This file owns the arguments, the PIN prompt, the reader and the dumps. */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#include "credential_pcsc.h"
#include "credential_system.h"
#include "pki_input.h"
#include "piv_inspect.h"
#include <tiny_crypto/hash.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>

enum { FILE_BYTES = 16384, SECRET_BYTES = 16 };

#if defined(EXAMPLE_PIV_INSPECT_GUARD)
/* Test builds link a transmit guard provider, such as
 * tests/piv/hardware/inspect_live.c. It returns NULL when it has none. */
const ExampleCardPCSCGuard* example_piv_inspect_guard(void);
#else
static const ExampleCardPCSCGuard* example_piv_inspect_guard(void)
{
  return NULL;
}
#endif

static const char usage[] =
    "Usage: piv_inspect --reader NAME --anchor CA.der [--anchor-sha256 HEX]\n"
    "  [--anchor CA.der [--anchor-sha256 HEX]]... [--crl ISSUER.crl]...\n"
    "  [--ocsp 9a|9c|9d|9e RESPONSE.der]... [--interface contact|contactless]\n"
    "  [--extended] [--revocation required|when-available]\n"
    "  [--at YYYY-MM-DDTHH:MM:SSZ] [--min-retries N] [--pin-prompt] [--dump-dir DIR]\n"
    "Reads the card, runs the library card check and prints the report.\n"
    "--reader defaults to TC_PIV_READER, a substring of one reader name, for\n"
    "example 'ACR1552 1S CL Reader PICC'. Yubico readers are refused.\n"
    "The PIN and pairing code come from TC_PIV_PIN and TC_PIV_PAIRING_CODE, or\n"
    "from a prompt without echo. An empty answer sends none.\n"
    "TC_PIV_HARDWARE_GUARD=1 installs the hardware test transmit guard, which\n"
    "only test builds provide.\n"
    "Exit status: 0 accepted, 1 rejected or failed, 2 invalid arguments.\n";

static struct {
  uint8_t anchors[EXAMPLE_PIV_INSPECT_ANCHORS][FILE_BYTES];
  uint8_t ocsp[TC_PIV_CARD_CERTIFICATES][FILE_BYTES];
  /* CRLs stay on disk and are read as byte sources during the run. */
  FILE* crls[EXAMPLE_PIV_INSPECT_CRLS];
} inputs;

/* PIN and pairing code, locked in memory and wiped on exit. */
static struct {
  char pin[SECRET_BYTES], pairing[SECRET_BYTES];
} secrets;

typedef struct {
  const char* reader;
  const char* anchors[EXAMPLE_PIV_INSPECT_ANCHORS];
  const char* anchor_digests[EXAMPLE_PIV_INSPECT_ANCHORS];
  const char* crls[EXAMPLE_PIV_INSPECT_CRLS];
  const char* ocsp[TC_PIV_CARD_CERTIFICATES];
  const char* at;
  const char* dump_dir;
  size_t anchor_count, crl_count;
  ExampleCardPCSCInterface interface;
  TC_APDU_length_format format;
  TC_validation_revocation revocation;
  unsigned minimum_retries;
  int pin_prompt;
} Arguments;

static int slot_parse(const char* text, size_t* out)
{
  static const char* const keys[] = {"9a", "9c", "9d", "9e"};
  for (size_t i = 0; i < TC_PIV_CARD_CERTIFICATES; ++i)
    if (!strcmp(text, keys[i])) {
      *out = i;
      return 1;
    }
  return 0;
}

static int arguments_parse(int argc, char** argv, Arguments* out)
{
  memset(out, 0, sizeof *out);
  out->reader = getenv("TC_PIV_READER");
  out->minimum_retries = 3;
  for (int i = 1; i < argc; ++i) {
    const char* option = argv[i];
    if (!strcmp(option, "--extended")) {
      out->format = TC_APDU_EXTENDED;
      continue;
    }
    if (!strcmp(option, "--pin-prompt")) {
      out->pin_prompt = 1;
      continue;
    }
    if (i + 1 >= argc)
      return 0;
    const char* value = argv[++i];
    size_t slot;
    if (!strcmp(option, "--reader")) {
      out->reader = value;
    } else if (!strcmp(option, "--anchor")) {
      if (out->anchor_count == EXAMPLE_PIV_INSPECT_ANCHORS)
        return 0;
      out->anchors[out->anchor_count++] = value;
    } else if (!strcmp(option, "--anchor-sha256")) {
      if (!out->anchor_count || out->anchor_digests[out->anchor_count - 1])
        return 0;
      out->anchor_digests[out->anchor_count - 1] = value;
    } else if (!strcmp(option, "--crl")) {
      if (out->crl_count == EXAMPLE_PIV_INSPECT_CRLS)
        return 0;
      out->crls[out->crl_count++] = value;
    } else if (!strcmp(option, "--ocsp")) {
      if (i + 1 >= argc || !slot_parse(value, &slot) || out->ocsp[slot])
        return 0;
      out->ocsp[slot] = argv[++i];
    } else if (!strcmp(option, "--interface")) {
      if (!strcmp(value, "contact"))
        out->interface = EXAMPLE_PCSC_CONTACT;
      else if (!strcmp(value, "contactless"))
        out->interface = EXAMPLE_PCSC_CONTACTLESS;
      else
        return 0;
    } else if (!strcmp(option, "--revocation")) {
      if (!strcmp(value, "required"))
        out->revocation = TC_VALIDATION_REVOCATION_REQUIRED;
      else if (!strcmp(value, "when-available"))
        out->revocation = TC_VALIDATION_REVOCATION_WHEN_AVAILABLE;
      else
        return 0;
    } else if (!strcmp(option, "--at")) {
      out->at = value;
    } else if (!strcmp(option, "--min-retries")) {
      char* end;
      const unsigned long retries = strtoul(value, &end, 10);
      if (*end || retries < 2 || retries > 15)
        return 0;
      out->minimum_retries = (unsigned)retries;
    } else if (!strcmp(option, "--dump-dir")) {
      out->dump_dir = value;
    } else {
      return 0;
    }
  }
  return out->reader && *out->reader && out->anchor_count;
}

static int hex_nibble(char value, uint8_t* out)
{
  if (value >= '0' && value <= '9')
    *out = (uint8_t)(value - '0');
  else if (value >= 'a' && value <= 'f')
    *out = (uint8_t)(value - 'a' + 10);
  else if (value >= 'A' && value <= 'F')
    *out = (uint8_t)(value - 'A' + 10);
  else
    return 0;
  return 1;
}

/* 1 when the SHA-256 of encoded equals the 64 hex digits of expected. */
static int digest_match(TC_bytes encoded, const char* expected)
{
  uint8_t supplied[TC_SHA256_DIGESTLEN], actual[TC_SHA256_DIGESTLEN];
  if (strlen(expected) != sizeof supplied * 2)
    return 0;
  for (size_t i = 0; i < sizeof supplied; ++i) {
    uint8_t high, low;
    if (!hex_nibble(expected[i * 2], &high) || !hex_nibble(expected[i * 2 + 1], &low))
      return 0;
    supplied[i] = (uint8_t)((high << 4) | low);
  }
  struct TC_SHA256_ctx hash;
  return TC_SHA256_init(&hash) == TC_OK && TC_SHA256_update(&hash, encoded) == TC_OK &&
         TC_SHA256_final(&hash, actual) == TC_OK && !memcmp(actual, supplied, sizeof actual);
}

/* Read a line without echo when input is a terminal. The line ending is
 * removed. */
static int secret_read(const char* prompt, char* out, size_t capacity)
{
  struct termios saved = {0}, quiet;
  const int terminal = isatty(STDIN_FILENO) && !tcgetattr(STDIN_FILENO, &saved);
  fputs(prompt, stderr);
  if (terminal) {
    quiet = saved;
    quiet.c_lflag &= ~(tcflag_t)ECHO;
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &quiet))
      return 0;
  }
  const int read = fgets(out, (int)capacity, stdin) != NULL;
  if (terminal) {
    (void)tcsetattr(STDIN_FILENO, TCSAFLUSH, &saved);
    fputc('\n', stderr);
  }
  if (!read)
    out[0] = 0;
  const size_t length = strcspn(out, "\r\n");
  if (out[length] == 0 && length + 1 == capacity)
    return 0; /* longer than any PIN */
  out[length] = 0;
  return 1;
}

/* A secret from the environment, or from the prompt. */
static int secret_get(const char* name, const char* prompt, int ask, char* out, size_t capacity)
{
  const char* value = getenv(name);
  if (value) {
    if (strlen(value) >= capacity)
      return 0;
    memcpy(out, value, strlen(value) + 1);
    return 1;
  }
  return ask ? secret_read(prompt, out, capacity) : 1;
}

static TC_bytes secret_bytes(const char* text)
{
  return (TC_bytes){(const uint8_t*)text, strlen(text)};
}

/* Write each dumped object to DIR/<tag>.bin. */
static void dump(void* context, const TC_PIV_object* object)
{
  const char* directory = context;
  char path[1024];
  if (!example_dump_path(path, sizeof path, directory,
                         (TC_bytes){object->info->tag, object->info->tag_length})) {
    fputs("Dump path is too long\n", stderr);
    return;
  }
  const int file = open(path, O_WRONLY | O_CREAT | O_EXCL, 0600);
  if (file < 0) {
    fprintf(stderr, "Unable to create %s: %s\n", path, strerror(errno));
    return;
  }
  const ssize_t written = write(file, object->encoded.data, object->encoded.length);
  if (close(file) || written != (ssize_t)object->encoded.length)
    fprintf(stderr, "Unable to write %s\n", path);
}

/* The dump directory must be private to its owner. */
static int dump_dir_private(const char* path)
{
  struct stat info;
  return !stat(path, &info) && S_ISDIR(info.st_mode) && !(info.st_mode & 077) &&
         info.st_uid == geteuid();
}

/* Load the trust and evidence files named by arguments into options. Each
 * CRL file stays open as a byte source of any size until inputs_close. */
static int inputs_load(const Arguments* arguments, TC_bytes* anchors, TC_source* crls,
                       ExamplePIVInspectOptions* options)
{
  for (size_t i = 0; i < arguments->anchor_count; ++i) {
    if (!example_read_file(arguments->anchors[i], inputs.anchors[i], FILE_BYTES, &anchors[i]) ||
        (arguments->anchor_digests[i] && !digest_match(anchors[i], arguments->anchor_digests[i]))) {
      fprintf(stderr, "Unable to load or match anchor %s\n", arguments->anchors[i]);
      return 0;
    }
  }
  for (size_t i = 0; i < arguments->crl_count; ++i) {
    inputs.crls[i] = fopen(arguments->crls[i], "rb");
    if (!inputs.crls[i] || !example_stream_source(inputs.crls[i], UINT64_MAX, &crls[i])) {
      fprintf(stderr, "Unable to open CRL %s\n", arguments->crls[i]);
      return 0;
    }
  }
  for (size_t i = 0; i < TC_PIV_CARD_CERTIFICATES; ++i)
    if (arguments->ocsp[i] &&
        !example_read_file(arguments->ocsp[i], inputs.ocsp[i], FILE_BYTES, &options->ocsp[i])) {
      fprintf(stderr, "Unable to load OCSP response %s\n", arguments->ocsp[i]);
      return 0;
    }
  options->anchors = anchors;
  options->anchor_count = arguments->anchor_count;
  options->crls = crls;
  options->crl_count = arguments->crl_count;
  return 1;
}

static int inputs_close(void)
{
  int closed = 1;
  for (size_t i = 0; i < EXAMPLE_PIV_INSPECT_CRLS; ++i)
    if (inputs.crls[i] && fclose(inputs.crls[i]))
      closed = 0;
  TC_secure_zero(&inputs, sizeof inputs);
  return closed;
}

static const char* open_failure(ExampleCardPCSCResult result)
{
  switch (result) {
  case EXAMPLE_PCSC_NO_READER:
    return "No reader name contains the filter";
  case EXAMPLE_PCSC_AMBIGUOUS:
    return "Several reader names contain the filter";
  case EXAMPLE_PCSC_REFUSED:
    return "Refused a Yubico reader or YubiKey";
  case EXAMPLE_PCSC_NO_CARD:
    return "The reader holds no card";
  case EXAMPLE_PCSC_INTERFACE:
    return "The ATR shows a contactless card";
  default:
    return "Unable to acquire the reader transaction";
  }
}

static int inspect(const Arguments* arguments)
{
  static TC_bytes anchors[EXAMPLE_PIV_INSPECT_ANCHORS];
  static TC_source crls[EXAMPLE_PIV_INSPECT_CRLS];
  ExamplePIVInspectOptions options;
  memset(&options, 0, sizeof options);
  options.format = arguments->format;
  options.minimum_retries = arguments->minimum_retries;
  options.revocation = arguments->revocation;
  options.random = (TC_random_source){example_card_random, NULL};
  const int ask = arguments->pin_prompt || (!getenv("TC_PIV_PIN") && isatty(STDIN_FILENO));
  if (!inputs_load(arguments, anchors, crls, &options) ||
      (arguments->at ? !example_time_parse(arguments->at, &options.at)
                     : !example_card_now(&options.at)) ||
      (arguments->dump_dir && !dump_dir_private(arguments->dump_dir)) ||
      !secret_get("TC_PIV_PIN", "PIN (empty for none): ", ask, secrets.pin, sizeof secrets.pin) ||
      !secret_get("TC_PIV_PAIRING_CODE", "Pairing code (empty for none): ", ask, secrets.pairing,
                  sizeof secrets.pairing) ||
      example_card_random(NULL, options.host_id, sizeof options.host_id) != TC_OK) {
    fputs("Invalid inputs, time, dump directory or secrets\n", stderr);
    return 2;
  }
  options.pin = secret_bytes(secrets.pin);
  options.pairing_code = secret_bytes(secrets.pairing);
  if (arguments->dump_dir) {
    options.dump = dump;
    options.dump_context = (void*)arguments->dump_dir;
  }
  /* TC_PIV_HARDWARE_GUARD=1 requires the guard, so a build without one
   * refuses to run. */
  const char* guarded = getenv("TC_PIV_HARDWARE_GUARD");
  const ExampleCardPCSCGuard* guard = NULL;
  if (guarded && strcmp(guarded, "0")) {
    guard = example_piv_inspect_guard();
    if (!guard) {
      fputs("TC_PIV_HARDWARE_GUARD is set, and this build has no transmit guard\n", stderr);
      return 2;
    }
  }
  ExampleCardPCSC connection = {0};
  const ExampleCardPCSCOptions reader = {arguments->reader, arguments->interface, guard};
  const ExampleCardPCSCResult opened = example_card_pcsc_open(&connection, &reader);
  if (opened != EXAMPLE_PCSC_OPENED) {
    fprintf(stderr, "%s\n", open_failure(opened));
    return 1;
  }
  options.interface = example_card_pcsc_interface(&connection);
  /* A reset on close clears the PIN status the run may leave. */
  if (options.pin.length)
    example_card_pcsc_reset_on_close(&connection);
  int status = example_piv_inspect_run(
      &options, (TC_APDU_transport){example_card_pcsc_transmit, &connection}, stdout, NULL);
  if (!example_card_pcsc_close(&connection)) {
    fputs("Reader cleanup failed\n", stderr);
    status = status ? status : 1;
  }
  return status;
}

int main(int argc, char** argv)
{
  if (argc == 2 && !strcmp(argv[1], "--help")) {
    fputs(usage, stdout);
    return 0;
  }
  Arguments arguments;
  if (!arguments_parse(argc, argv, &arguments)) {
    fputs(usage, stderr);
    return 2;
  }
  /* Disable core files, lock the secrets before reading them and keep a
   * prompted PIN out of the stdio input buffer. */
  const struct rlimit core_limit = {0, 0};
  if (setrlimit(RLIMIT_CORE, &core_limit) || mlock(&secrets, sizeof secrets) ||
      setvbuf(stdin, NULL, _IONBF, 0)) {
    fputs("Unable to protect credential memory\n", stderr);
    return 1;
  }
  int status = inspect(&arguments);
  TC_secure_zero(&secrets, sizeof secrets);
  if (!inputs_close() && !status)
    status = 1;
  if (munlock(&secrets, sizeof secrets))
    return 1;
  return status;
}
