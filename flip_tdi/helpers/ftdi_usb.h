#pragma once

typedef struct FtdiUsb FtdiUsb;

FtdiUsb* ftdi_usb_start(void);
void ftdi_usb_stop(FtdiUsb* ftdi);
