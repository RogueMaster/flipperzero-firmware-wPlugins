/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Storage preflight shared by operations that write caller-owned buffers. */
#include "pki_storage_internal.h"

void tc_pki_storage_plan_begin(tc_pki_storage_plan* plan, TC_bytes* writes, size_t capacity,
                               size_t budget)
{
  plan->writes = writes;
  plan->count = 0;
  plan->capacity = capacity;
  plan->budget = budget;
  plan->initial_budget = budget;
  plan->sealed = 0;
  plan->status = writes || !capacity ? TC_TLV_OK : TC_TLV_ARGUMENT;
}

void tc_pki_storage_plan_write(tc_pki_storage_plan* plan, const void* data, size_t count,
                               size_t width)
{
  if (plan->status != TC_TLV_OK)
    return;
  if (plan->sealed || plan->count == plan->capacity) {
    plan->status = TC_TLV_ARGUMENT;
    return;
  }
  plan->status = tc_pki_storage_span(data, count, width, &plan->writes[plan->count]);
  if (plan->status == TC_TLV_OK)
    ++plan->count;
}

void tc_pki_storage_plan_write_span(tc_pki_storage_plan* plan, TC_bytes write)
{
  tc_pki_storage_plan_write(plan, write.data, write.length, 1);
}

void tc_pki_storage_plan_fail(tc_pki_storage_plan* plan, TC_TLV_result status)
{
  if (plan->status == TC_TLV_OK)
    plan->status = status;
}

void tc_pki_storage_plan_seal(tc_pki_storage_plan* plan)
{
  size_t i;
  if (plan->status != TC_TLV_OK)
    return;
  for (i = 0; i < plan->count && plan->status == TC_TLV_OK; ++i)
    plan->status = tc_pki_storage_input(plan->writes, i, plan->writes[i], &plan->budget);
  plan->sealed = 1;
}

void tc_pki_storage_plan_input_span(tc_pki_storage_plan* plan, TC_bytes input)
{
  if (plan->status != TC_TLV_OK)
    return;
  if (!plan->sealed) {
    plan->status = TC_TLV_ARGUMENT;
    return;
  }
  plan->status = tc_pki_storage_input(plan->writes, plan->count, input, &plan->budget);
}

void tc_pki_storage_plan_input(tc_pki_storage_plan* plan, const void* data, size_t count,
                               size_t width)
{
  TC_bytes input;
  if (plan->status != TC_TLV_OK)
    return;
  plan->status = tc_pki_storage_span(data, count, width, &input);
  tc_pki_storage_plan_input_span(plan, input);
}

void tc_pki_storage_plan_input_spans(tc_pki_storage_plan* plan, const TC_bytes* inputs,
                                     size_t count)
{
  size_t i;
  for (i = 0; i < count && plan->status == TC_TLV_OK; ++i)
    tc_pki_storage_plan_input_span(plan, inputs[i]);
}

TC_TLV_result tc_pki_storage_plan_finish(const tc_pki_storage_plan* plan, size_t* work)
{
  if (plan->status == TC_TLV_OK && work)
    *work = plan->budget;
  return plan->status;
}

size_t tc_pki_storage_plan_used(const tc_pki_storage_plan* plan)
{
  return plan->initial_budget - plan->budget;
}
