/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include "credential_system.h"
#include <stdio.h>
#include <time.h>

int example_card_now(TC_X509_time* out)
{
  if (!out) return 0;
  const time_t now = time(NULL);
  if (now == (time_t)-1) return 0;
  const struct tm* utc = gmtime(&now);
  if (!utc || utc->tm_year < 70 || utc->tm_year > 8099) return 0;
  const TC_X509_time value = {(unsigned)utc->tm_year + 1900,(uint8_t)(utc->tm_mon + 1),
    (uint8_t)utc->tm_mday,(uint8_t)utc->tm_hour,(uint8_t)utc->tm_min,(uint8_t)utc->tm_sec};
  int64_t seconds;
  if (TC_X509_time_to_unix(&value,&seconds) != TC_TLV_OK) return 0;
  *out = value;
  return 1;
}

TC_status example_card_random(void* context, uint8_t* out, size_t length)
{
  (void)context;
  if (!out || !length) return TC_ERROR;
  FILE* stream = fopen("/dev/urandom","rb");
  if (!stream) return TC_ERROR;
  int complete = fread(out,1,length,stream) == length && !ferror(stream);
  if (fclose(stream)) complete = 0;
  if (!complete) TC_secure_zero(out,length);
  return complete ? TC_OK : TC_ERROR;
}
