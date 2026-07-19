/* USB-CDC-Modus: Flipper wird virtuelles COM, STK500-Slave laeuft im Worker. */
#pragma once
#include <stdint.h>

typedef struct UsbIsp UsbIsp;

UsbIsp* usb_isp_start(void); /* USB -> CDC, Worker starten */
void usb_isp_stop(UsbIsp* u); /* Worker beenden, USB zuruecksetzen */
uint32_t usb_isp_activity(UsbIsp* u); /* Aktivitaetszaehler (fuer Anzeige) */
