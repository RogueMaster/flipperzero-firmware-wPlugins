#pragma once
#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_usb.h>

#define USB_MANUFACTURER_STRING "Flipper Devices Inc."
#define USB_PRODUCT_STRING      "Flipper Zero Virtual MTP Device"
#define USB_DEVICE_MODEL        "Flipper Zero"

#define USB_SUBCLASS_MTP 0x01
#define USB_PROTO_MTP    0x01
#define USB_CONF_VAL_MTP 1

// Endpoint Addresses
#define MTP_EP_IN_ADDR     0x81
#define MTP_EP_OUT_ADDR    0x02
#define MTP_EP_INT_IN_ADDR 0x03

#define MTP_MAX_PACKET_SIZE    64
#define USB_MAX_INTERRUPT_SIZE 28
#define BCD_VERSION            VERSION_BCD(1, 0, 0)

struct MtpDescriptor {
    struct usb_config_descriptor config;
    struct usb_interface_descriptor intf;

    // I/O endpoints
    struct usb_endpoint_descriptor ep_in;
    struct usb_endpoint_descriptor ep_out;

    // Interrupt endpoint
    struct usb_endpoint_descriptor ep_int_in;
} __attribute__((packed));

extern const struct usb_string_descriptor dev_manuf_desc;
extern const struct usb_string_descriptor dev_prod_desc;
extern const struct usb_device_descriptor usb_mtp_dev_descr;
extern const struct MtpDescriptor usb_mtp_cfg_descr;
extern FuriHalUsbInterface usb_mtp_interface;
extern const uint8_t usb_mtp_os_string[];
extern const uint8_t usb_mtp_os_string_len;

/* Microsoft Extended Configuration Descriptor Header Section */
struct mtp_ext_config_desc_header {
    uint32_t dwLength;
    uint16_t bcdVersion;
    uint16_t wIndex;
    uint8_t bCount;
    uint8_t reserved[7];
};
/* Microsoft Extended Configuration Descriptor Function Section */
struct mtp_ext_config_desc_function {
    uint8_t bFirstInterfaceNumber;
    uint8_t bInterfaceCount;
    uint8_t compatibleID[8];
    uint8_t subCompatibleID[8];
    uint8_t reserved[6];
};

struct mtp_ext_config_desc {
    struct mtp_ext_config_desc_header header;
    struct mtp_ext_config_desc_function function;
};

extern const struct mtp_ext_config_desc mtp_ext_config_desc;
