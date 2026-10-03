/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "pki_input.h"
#include <limits.h>
#include <string.h>
#include <sys/stat.h>
#if defined(__APPLE__) || defined(__linux__)
#include <fcntl.h>
#include <unistd.h>
#endif

int example_read_created_file(const char* path, uint8_t* buffer, size_t capacity, TC_bytes* out,
                              uint64_t* created)
{
  if (!path || !buffer || !capacity || !out || !created)
    return 0;
  FILE* stream = fopen(path, "rb");
  if (!stream)
    return 0;
  int valid = 0;
  int64_t seconds = 0;
  /* The open descriptor keeps metadata and contents tied to the same file. */
#if defined(__APPLE__)
  struct stat info;
  if (!fstat(fileno(stream), &info) && S_ISREG(info.st_mode)) {
    seconds = info.st_birthtimespec.tv_sec;
    valid = seconds > 0;
  }
#elif defined(__linux__) && defined(STATX_BTIME)
  struct statx info = {0};
  /* Some Linux filesystems omit birth time even when statx succeeds. */
  if (!statx(fileno(stream), "", AT_EMPTY_PATH, STATX_BTIME | STATX_TYPE, &info) &&
      (info.stx_mask & STATX_BTIME) && (info.stx_mask & STATX_TYPE) && S_ISREG(info.stx_mode)) {
    seconds = info.stx_btime.tv_sec;
    valid = seconds > 0;
  }
#endif
  TC_bytes bytes = {0};
  if (valid)
    valid = example_read_stream(stream, buffer, capacity, &bytes);
  if (fclose(stream))
    valid = 0;
  if (!valid) {
    TC_secure_zero(buffer, capacity);
    return 0;
  }
  *out = bytes;
  *created = (uint64_t)seconds;
  return 1;
}

static TC_status stream_read(void* context, uint64_t offset, uint8_t* output, size_t length)
{
  FILE* stream = context;
  if (!stream || (!output && length) || offset > LONG_MAX ||
      (uint64_t)length > (uint64_t)LONG_MAX - offset)
    return TC_ERROR;
  if (fseek(stream, (long)offset, SEEK_SET))
    return TC_ERROR;
  if (!length)
    return TC_OK;
  if (fread(output, 1, length, stream) == length && !ferror(stream))
    return TC_OK;
  TC_secure_zero(output, length);
  return TC_ERROR;
}

int example_stream_source(FILE* stream, uint64_t max_bytes, TC_source* out)
{
  if (!stream || !out)
    return 0;
  const long position = ftell(stream);
  if (position < 0 || fseek(stream, 0, SEEK_END))
    return 0;
  const long length = ftell(stream);
  if (fseek(stream, position, SEEK_SET) || length < 0 || (uint64_t)length > max_bytes)
    return 0;
  const TC_source source = {stream_read, stream, (uint64_t)length};
  *out = source;
  return 1;
}

int example_read_stream(FILE* stream, uint8_t* buffer, size_t capacity, TC_bytes* out)
{
  if (!stream || !buffer || !capacity || !out)
    return 0;
  const size_t length = fread(buffer, 1, capacity, stream);
  /* Probe once beyond capacity so a full buffer cannot hide a truncated file. */
  const int complete = fgetc(stream) == EOF && !ferror(stream);
  if (!complete || !length) {
    TC_secure_zero(buffer, capacity);
    return 0;
  }
  out->data = buffer;
  out->length = length;
  return 1;
}

int example_read_file(const char* path, uint8_t* buffer, size_t capacity, TC_bytes* out)
{
  if (!path || !buffer || !capacity || !out)
    return 0;
  FILE* stream = fopen(path, "rb");
  if (!stream) {
    TC_secure_zero(buffer, capacity);
    return 0;
  }
  TC_bytes bytes;
  int complete = example_read_stream(stream, buffer, capacity, &bytes);
  if (fclose(stream))
    complete = 0;
  if (!complete) {
    TC_secure_zero(buffer, capacity);
    return 0;
  }
  *out = bytes;
  return 1;
}

static TC_TLV_result candidate(void* context, size_t index, size_t* work, TC_bytes* out)
{
  const ExampleX509Source* source = context;
  if (!source || !source->candidates || index >= source->candidate_count || !work || !out)
    return TC_TLV_ARGUMENT;
  if (!*work)
    return TC_TLV_LIMIT;
  --*work;
  *out = source->candidates[index];
  return TC_TLV_OK;
}

static TC_TLV_result anchor(void* context, size_t index, size_t* work, TC_X509_store_anchor* out)
{
  const ExampleX509Source* source = context;
  if (!source || !source->anchors || index >= source->anchor_count || !work || !out)
    return TC_TLV_ARGUMENT;
  if (!*work)
    return TC_TLV_LIMIT;
  --*work;
  *out = source->anchors[index];
  return TC_TLV_OK;
}

TC_X509_store_source example_x509_source(ExampleX509Source* source)
{
  const TC_X509_store_source view = {source, source ? source->candidate_count : 0,
                                     source ? source->anchor_count : 0, candidate, anchor};
  return view;
}

/* The value of count decimal digits at text. The caller checked the digits. */
static unsigned decimal(const char* text, size_t count)
{
  unsigned value = 0;
  for (size_t i = 0; i < count; ++i)
    value = value * 10u + (unsigned)(text[i] - '0');
  return value;
}

int example_dump_path(char* out, size_t capacity, const char* directory, TC_bytes tag)
{
  /* Three-byte PIV tags and one-byte ISO tags, as hex. */
  enum { TAG_MAX = 8 };
  char hex[2 * TAG_MAX + 1];
  if (!out || !directory || !tag.data || !tag.length || tag.length > TAG_MAX)
    return 0;
  for (size_t i = 0; i < tag.length; ++i)
    snprintf(hex + 2 * i, 3, "%02x", tag.data[i]);
  /* One bounded format of the whole name. The result decides truncation
   * before any use of the buffer. */
  char name[4096];
  const int length = snprintf(name, sizeof name, "%s/%s.bin", directory, hex);
  if (length < 0 || (size_t)length >= sizeof name || (size_t)length >= capacity)
    return 0;
  memcpy(out, name, (size_t)length + 1);
  return 1;
}

int example_time_parse(const char* text, TC_X509_time* out)
{
  /* 0 marks a required digit. Every other byte must match exactly. */
  static const char form[] = "0000-00-00T00:00:00Z";
  if (!text || !out || strlen(text) != sizeof form - 1)
    return 0;
  for (size_t i = 0; i < sizeof form - 1; ++i) {
    const int digit = text[i] >= '0' && text[i] <= '9';
    if (form[i] == '0' ? !digit : text[i] != form[i])
      return 0;
  }
  /* Two digits fit the uint8_t fields. TC_X509_time_check rejects the
   * out-of-range values. */
  const TC_X509_time value = {decimal(text, 4),
                              (uint8_t)decimal(text + 5, 2),
                              (uint8_t)decimal(text + 8, 2),
                              (uint8_t)decimal(text + 11, 2),
                              (uint8_t)decimal(text + 14, 2),
                              (uint8_t)decimal(text + 17, 2)};
  if (TC_X509_time_check(&value) != TC_TLV_OK)
    return 0;
  *out = value;
  return 1;
}
