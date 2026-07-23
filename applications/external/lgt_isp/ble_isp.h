/* BLE-Serial-Modus: Flipper wird BLE-UART, STK500-Slave laeuft im Worker.
 * Drop-in-Gegenstueck zu usb_isp.h — dieselbe Stk500Io-Anbindung, nur der
 * Transport ist BLE (ble_profile_serial) statt USB-CDC. avrdude erreicht den
 * Flipper ueber eine PC-seitige BLE<->COM-Bruecke (Nordic-UART-Stil). */
#pragma once
#include <stdint.h>
#include <stdbool.h>

typedef struct BleIsp BleIsp;

BleIsp* ble_isp_start(void); /* BLE-Serial-Profil starten + Worker */
void ble_isp_stop(BleIsp* b); /* Worker beenden, Standard-Profil zurueck */
uint32_t ble_isp_activity(BleIsp* b); /* STK500-Aktivitaetszaehler (Anzeige) */
uint32_t ble_isp_rx_bytes(BleIsp* b); /* ueber BLE empfangene Bytes (Anzeige) */
bool ble_isp_connected(BleIsp* b); /* true, sobald ein Client verbunden ist */
