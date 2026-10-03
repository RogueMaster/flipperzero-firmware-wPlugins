/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef TC_PKI_BUDGET_INTERNAL_H_
#define TC_PKI_BUDGET_INTERNAL_H_

#include <tiny_crypto/tlv.h>

/* The PKI layer meters work in size_t. Callers holding a 32-bit
 * TC_work_budget lend it to a PKI operation and settle what was used. On
 * targets where size_t is narrower, the loan is capped at SIZE_MAX and the
 * rest of the budget is kept. */
static inline size_t tc_pki_work_lend(const TC_work_budget* budget)
{
#if SIZE_MAX < UINT32_MAX
  return budget->remaining > SIZE_MAX ? SIZE_MAX : (size_t)budget->remaining;
#else
  return budget->remaining;
#endif
}

/* Charge the budget for a loan of lent units that left units unused. */
static inline void tc_pki_work_settle(TC_work_budget* budget, size_t lent, size_t left)
{
  budget->remaining -= (uint32_t)(lent - left);
}

/* Charge amount against work. Exhaustion sets work to zero and returns LIMIT. */
static inline TC_TLV_result tc_pki_work_charge(size_t* work, size_t amount)
{
  if (amount > *work) {
    *work = 0;
    return TC_TLV_LIMIT;
  }
  *work -= amount;
  return TC_TLV_OK;
}

#endif
