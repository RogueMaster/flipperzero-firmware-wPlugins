/*
 * Host-side wrappers for the W5500 register accessors that ioLibrary defines as
 * macros (getSn_IR/getSn_SR/getSn_TxMAX expand to WIZCHIP_READ(...)). A macro
 * cannot be resolved across the plugin boundary, so each is wrapped in a real
 * function here and exported through the app's API table (see lan_tester_ioshim.h,
 * which maps the macro names to these wrappers on the plugin side).
 *
 * This file includes the REAL ioLibrary headers (not lan_tester_ioshim.h) so the
 * macros expand normally — including the shim here would remap the names back to
 * these wrappers and recurse. furi.h must come before socket.h to avoid the
 * STM32 CMSIS "MR" macro clash with the W5500 headers.
 */

#include <furi.h>
#include <socket.h>
#include <w5500.h>

/* Prototypes kept in sync with lan_tester_ioshim.h (cannot include it here). */
uint8_t lt_getSn_IR(uint8_t sn);
uint8_t lt_getSn_SR(uint8_t sn);
uint16_t lt_getSn_TxMAX(uint8_t sn);

uint8_t lt_getSn_IR(uint8_t sn) {
    return getSn_IR(sn);
}

uint8_t lt_getSn_SR(uint8_t sn) {
    return getSn_SR(sn);
}

uint16_t lt_getSn_TxMAX(uint8_t sn) {
    return getSn_TxMAX(sn);
}
