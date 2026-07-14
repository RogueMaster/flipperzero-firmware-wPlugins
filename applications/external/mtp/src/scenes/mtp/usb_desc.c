#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_usb.h>
#include "usb.h"
#include "usb_desc.h"

const struct usb_string_descriptor dev_manuf_desc = USB_STRING_DESC(USB_MANUFACTURER_STRING);
const struct usb_string_descriptor dev_prod_desc = USB_STRING_DESC(USB_PRODUCT_STRING);

const uint8_t usb_mtp_os_string[] = {
    18,
    USB_DTYPE_STRING,
    /* Signature field: "MSFT100" */
    'M',
    0,
    'S',
    0,
    'F',
    0,
    'T',
    0,
    '1',
    0,
    '0',
    0,
    '0',
    0,
    /* vendor code */
    1,
    /* padding */
    0};
const uint8_t usb_mtp_os_string_len = sizeof(usb_mtp_os_string);

const struct usb_device_descriptor usb_mtp_dev_descr = {
    .bLength = sizeof(struct usb_device_descriptor),
    .bDescriptorType = USB_DTYPE_DEVICE,
    .bcdUSB = VERSION_BCD(2, 0, 0),
    .bDeviceClass = USB_CLASS_STILL_IMAGE, // MTP falls under Still Image class
    .bDeviceSubClass = USB_SUBCLASS_MTP, // Subclass for MTP
    .bDeviceProtocol = USB_PROTO_MTP, // Protocol for MTP
    .bMaxPacketSize0 = USB_EP0_SIZE,
    .idVendor = 0x0483, // STMicroelectronics
    .idProduct = 0x0105, // https://devicehunt.com/view/type/usb/vendor/0483, STM has PID for MTP.
    // .idProduct = 0x5741, // Custom Product ID
    .bcdDevice = BCD_VERSION,
    .iManufacturer = UsbDevManuf, // UsbDevManuf
    .iProduct = UsbDevProduct, // UsbDevProduct
    .iSerialNumber = UsbDevSerial, // UsbDevSerial
    .bNumConfigurations = 1,
};

const struct MtpDescriptor usb_mtp_cfg_descr = {
    .config =
        {
            .bLength = sizeof(struct usb_config_descriptor),
            .bDescriptorType = USB_DTYPE_CONFIGURATION,
            .wTotalLength = sizeof(struct MtpDescriptor),
            .bNumInterfaces = 1,
            .bConfigurationValue = USB_CONF_VAL_MTP,
            .iConfiguration = NO_DESCRIPTOR,
            .bmAttributes = USB_CFG_ATTR_RESERVED | USB_CFG_ATTR_SELFPOWERED,
            .bMaxPower = USB_CFG_POWER_MA(100),
        },
    .intf =
        {
            .bLength = sizeof(struct usb_interface_descriptor),
            .bDescriptorType = USB_DTYPE_INTERFACE,
            .bInterfaceNumber = 0,
            .iInterface = UsbDevManuf,
            .bAlternateSetting = 0,
            .bNumEndpoints = 3,
            .bInterfaceClass = USB_CLASS_STILL_IMAGE,
            .bInterfaceSubClass = USB_SUBCLASS_MTP,
            .bInterfaceProtocol = USB_PROTO_MTP,
        },
    .ep_in =
        {
            .bLength = sizeof(struct usb_endpoint_descriptor),
            .bDescriptorType = USB_DTYPE_ENDPOINT,
            .bEndpointAddress = MTP_EP_IN_ADDR,
            .bmAttributes = USB_EPTYPE_BULK,
            .wMaxPacketSize = MTP_MAX_PACKET_SIZE,
            .bInterval = 0,
        },
    .ep_out =
        {
            .bLength = sizeof(struct usb_endpoint_descriptor),
            .bDescriptorType = USB_DTYPE_ENDPOINT,
            .bEndpointAddress = MTP_EP_OUT_ADDR,
            .bmAttributes = USB_EPTYPE_BULK,
            .wMaxPacketSize = MTP_MAX_PACKET_SIZE,
            .bInterval = 0,
        },
    .ep_int_in =
        {
            .bLength = sizeof(struct usb_endpoint_descriptor),
            .bDescriptorType = USB_DTYPE_ENDPOINT,
            .bEndpointAddress = MTP_EP_INT_IN_ADDR,
            .bmAttributes = USB_EPTYPE_INTERRUPT,
            .wMaxPacketSize = USB_MAX_INTERRUPT_SIZE,
            .bInterval = 6,
        },
};

/* MICROSOFT STUFF */
const struct mtp_ext_config_desc mtp_ext_config_desc = {
    .header =
        {
            .dwLength = sizeof(mtp_ext_config_desc),
            .bcdVersion = BCD_VERSION,
            .wIndex = 0x04,
            .bCount = 1,
        },
    .function =
        {
            .bFirstInterfaceNumber = 0,
            .bInterfaceCount = 1,
            .compatibleID =
                {
                    'M',
                    'T',
                    'P',
                    0,
                    0,
                    0,
                    0,
                    0,
                },
        },
};

FuriHalUsbInterface usb_mtp_interface = {
    .init = usb_mtp_init,
    .deinit = usb_mtp_deinit,
    .wakeup = usb_mtp_wakeup,
    .suspend = usb_mtp_suspend,

    .dev_descr = (struct usb_device_descriptor*)&usb_mtp_dev_descr,

    .str_manuf_descr = (void*)&dev_manuf_desc,
    .str_prod_descr = (void*)&dev_prod_desc,
    .str_serial_descr = NULL,

    .cfg_descr = (void*)&usb_mtp_cfg_descr,
};
