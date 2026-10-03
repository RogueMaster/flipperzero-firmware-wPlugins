/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
/* Status word meanings per command and application (SP 800-73-5 Part 1 5.6
 * Table 7, Part 2 3.1.2 and 3.2.1, TWIC Part 2 v5 5.2). */
#include <tiny_crypto/piv_command.h>
#if TC_ENABLE_PIV_COMMAND

static TC_PIV_status verify_status(uint16_t sw, unsigned* retries)
{
  if (sw == 0x6300)
    return TC_PIV_SW_VERIFY_FAILED;
  if ((sw & 0xfff0) == 0x63c0) {
    if (retries)
      *retries = sw & 0x0f;
    return TC_PIV_SW_VERIFY_FAILED;
  }
  return TC_PIV_SW_OTHER;
}

TC_PIV_status TC_PIV_status_classify(uint16_t sw, TC_PIV_command command,
                                     TC_PIV_application_id application, unsigned* retries)
{
  switch (sw) {
  case 0x9000:
    return TC_PIV_SW_SUCCESS;
  case 0x6282:
    /* End of the object before Le bytes (ISO/IEC 7816-4 Table 7). */
    return command == TC_PIV_COMMAND_GET_DATA ? TC_PIV_SW_END_OF_OBJECT : TC_PIV_SW_OTHER;
  case 0x6882:
    return TC_PIV_SW_SM_UNSUPPORTED;
  case 0x6982:
    return TC_PIV_SW_SECURITY_STATUS;
  case 0x6983:
    return TC_PIV_SW_BLOCKED;
  case 0x6987:
    return TC_PIV_SW_SM_MISSING;
  case 0x6988:
    return TC_PIV_SW_SM_INCORRECT;
  case 0x6a80:
    return TC_PIV_SW_WRONG_DATA;
  case 0x6a81:
    return TC_PIV_SW_NOT_SUPPORTED;
  case 0x6a82:
    return TC_PIV_SW_NOT_FOUND;
  case 0x6a84:
    return TC_PIV_SW_NO_MEMORY;
  case 0x6a86:
    return TC_PIV_SW_WRONG_P1P2;
  case 0x6a88:
    /* TWIC GET DATA reports a missing object with 6A88 (TWIC Part 2 v5 5.2).
     * PIV uses it for a missing key or data reference. */
    return command == TC_PIV_COMMAND_GET_DATA && application == TC_PIV_APPLICATION_TWIC
               ? TC_PIV_SW_NOT_FOUND
               : TC_PIV_SW_REFERENCE_NOT_FOUND;
  default:
    return command == TC_PIV_COMMAND_VERIFY ? verify_status(sw, retries) : TC_PIV_SW_OTHER;
  }
}
#endif
