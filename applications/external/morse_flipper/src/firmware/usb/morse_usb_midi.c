/*
 * Purpose: Implement the USB MIDI interface used by Morse keying.
 * Owns: USB descriptors, endpoint state, RX callback, and MIDI packet writes.
 * Depends on: morse_usb_midi.h and Flipper USB services.
 * Tests: firmware build; USB behaviour is hardware-only.
 */

#include "morse_usb_midi.h"

#include <furi.h>
#include <usb.h>
#include <usb_std.h>

#define MORSE_USB_MIDI_VID 0x6666
#define MORSE_USB_MIDI_PID 0x4357

#define MORSE_USB_EP0_SIZE     8U
#define MORSE_USB_MIDI_EP_SIZE 64U
#define MORSE_USB_MIDI_EP_IN   0x81U
#define MORSE_USB_MIDI_EP_OUT  0x01U

#define MORSE_USB_CFG_DECONFIGURE 0U
#define MORSE_USB_CFG_CONFIGURE   1U

#define USB_AUDIO_DT_CS_INTERFACE 0x24U
#define USB_AUDIO_DT_CS_ENDPOINT  0x25U

#define USB_AUDIO_SUBCLASS_CONTROL       0x01U
#define USB_AUDIO_SUBCLASS_MIDISTREAMING 0x03U

#define USB_MIDI_SUBTYPE_MS_HEADER     0x01U
#define USB_MIDI_SUBTYPE_MIDI_IN_JACK  0x02U
#define USB_MIDI_SUBTYPE_MIDI_OUT_JACK 0x03U
#define USB_MIDI_SUBTYPE_MS_GENERAL    0x01U

#define USB_MIDI_JACK_TYPE_EMBEDDED 0x01U
#define USB_MIDI_JACK_TYPE_EXTERNAL 0x02U

enum {
    MorseUsbStrZero = 0,
    MorseUsbStrManufacturer,
    MorseUsbStrProduct,
    MorseUsbStrSerial,
};

struct usb_audio_header_descriptor_head {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bDescriptorSubtype;
    uint16_t bcdADC;
    uint16_t wTotalLength;
    uint8_t bInCollection;
} __attribute__((packed));

struct usb_audio_header_descriptor_body {
    uint8_t baInterfaceNr;
} __attribute__((packed));

struct usb_midi_header_descriptor {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bDescriptorSubtype;
    uint16_t bcdMSC;
    uint16_t wTotalLength;
} __attribute__((packed));

struct usb_midi_in_jack_descriptor {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bDescriptorSubtype;
    uint8_t bJackType;
    uint8_t bJackID;
    uint8_t iJack;
} __attribute__((packed));

struct usb_midi_out_jack_descriptor_head {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bDescriptorSubtype;
    uint8_t bJackType;
    uint8_t bJackID;
    uint8_t bNrInputPins;
} __attribute__((packed));

struct usb_midi_out_jack_source_descriptor {
    uint8_t baSourceID;
    uint8_t baSourcePin;
} __attribute__((packed));

struct usb_midi_out_jack_descriptor_tail {
    uint8_t iJack;
} __attribute__((packed));

struct usb_midi_out_jack_descriptor {
    struct usb_midi_out_jack_descriptor_head head;
    struct usb_midi_out_jack_source_descriptor source[1];
    struct usb_midi_out_jack_descriptor_tail tail;
} __attribute__((packed));

struct usb_midi_endpoint_descriptor_head {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bDescriptorSubType;
    uint8_t bNumEmbMIDIJack;
} __attribute__((packed));

struct usb_midi_endpoint_jack_descriptor {
    uint8_t baAssocJackID;
} __attribute__((packed));

struct usb_midi_endpoint_descriptor {
    struct usb_midi_endpoint_descriptor_head head;
    struct usb_midi_endpoint_jack_descriptor jack[1];
} __attribute__((packed));

struct morse_usb_audio_header_descriptor {
    struct usb_audio_header_descriptor_head head;
    struct usb_audio_header_descriptor_body body;
} __attribute__((packed));

struct morse_usb_midi_jacks_descriptor {
    struct usb_midi_header_descriptor header;
    struct usb_midi_in_jack_descriptor in_embedded;
    struct usb_midi_in_jack_descriptor in_external;
    struct usb_midi_out_jack_descriptor out_embedded;
    struct usb_midi_out_jack_descriptor out_external;
} __attribute__((packed));

struct morse_usb_midi_config_descriptor {
    struct usb_config_descriptor config;
    struct usb_interface_descriptor audio_control_iface;
    struct morse_usb_audio_header_descriptor audio_control_header;
    struct usb_interface_descriptor midi_streaming_iface;
    struct morse_usb_midi_jacks_descriptor midi_jacks;
    struct usb_endpoint_descriptor bulk_out;
    struct usb_midi_endpoint_descriptor midi_bulk_out;
    struct usb_endpoint_descriptor bulk_in;
    struct usb_midi_endpoint_descriptor midi_bulk_in;
} __attribute__((packed));

/*
 * USB MIDI is an Audio device with a control interface plus a MIDIStreaming
 * interface. The embedded/external jack pairs make host routing behave.
 */
static struct usb_device_descriptor morse_usb_midi_device_descriptor = {
    .bLength = sizeof(struct usb_device_descriptor),
    .bDescriptorType = USB_DTYPE_DEVICE,
    .bcdUSB = VERSION_BCD(2, 0, 0),
    .bDeviceClass = USB_CLASS_PER_INTERFACE,
    .bDeviceSubClass = USB_SUBCLASS_NONE,
    .bDeviceProtocol = USB_PROTO_NONE,
    .bMaxPacketSize0 = MORSE_USB_EP0_SIZE,
    .idVendor = MORSE_USB_MIDI_VID,
    .idProduct = MORSE_USB_MIDI_PID,
    .bcdDevice = VERSION_BCD(1, 0, 0),
    .iManufacturer = MorseUsbStrManufacturer,
    .iProduct = MorseUsbStrProduct,
    .iSerialNumber = MorseUsbStrSerial,
    .bNumConfigurations = 1,
};

static struct morse_usb_midi_config_descriptor morse_usb_midi_config = {
    .config =
        {
            .bLength = sizeof(struct usb_config_descriptor),
            .bDescriptorType = USB_DTYPE_CONFIGURATION,
            .wTotalLength = sizeof(struct morse_usb_midi_config_descriptor),
            .bNumInterfaces = 2,
            .bConfigurationValue = 1,
            .iConfiguration = 0,
            .bmAttributes = USB_CFG_ATTR_RESERVED,
            .bMaxPower = USB_CFG_POWER_MA(100),
        },
    .audio_control_iface =
        {
            .bLength = sizeof(struct usb_interface_descriptor),
            .bDescriptorType = USB_DTYPE_INTERFACE,
            .bInterfaceNumber = 0,
            .bAlternateSetting = 0,
            .bNumEndpoints = 0,
            .bInterfaceClass = USB_CLASS_AUDIO,
            .bInterfaceSubClass = USB_AUDIO_SUBCLASS_CONTROL,
            .bInterfaceProtocol = USB_PROTO_NONE,
            .iInterface = 0,
        },
    .audio_control_header =
        {
            .head =
                {
                    .bLength = sizeof(struct morse_usb_audio_header_descriptor),
                    .bDescriptorType = USB_AUDIO_DT_CS_INTERFACE,
                    .bDescriptorSubtype = 0x01U,
                    .bcdADC = VERSION_BCD(1, 0, 0),
                    .wTotalLength = sizeof(struct morse_usb_audio_header_descriptor),
                    .bInCollection = 1,
                },
            .body =
                {
                    .baInterfaceNr = 1,
                },
        },
    .midi_streaming_iface =
        {
            .bLength = sizeof(struct usb_interface_descriptor),
            .bDescriptorType = USB_DTYPE_INTERFACE,
            .bInterfaceNumber = 1,
            .bAlternateSetting = 0,
            .bNumEndpoints = 2,
            .bInterfaceClass = USB_CLASS_AUDIO,
            .bInterfaceSubClass = USB_AUDIO_SUBCLASS_MIDISTREAMING,
            .bInterfaceProtocol = USB_PROTO_NONE,
            .iInterface = 0,
        },
    .midi_jacks =
        {
            .header =
                {
                    .bLength = sizeof(struct usb_midi_header_descriptor),
                    .bDescriptorType = USB_AUDIO_DT_CS_INTERFACE,
                    .bDescriptorSubtype = USB_MIDI_SUBTYPE_MS_HEADER,
                    .bcdMSC = VERSION_BCD(1, 0, 0),
                    .wTotalLength = sizeof(struct morse_usb_midi_jacks_descriptor),
                },
            .in_embedded =
                {
                    .bLength = sizeof(struct usb_midi_in_jack_descriptor),
                    .bDescriptorType = USB_AUDIO_DT_CS_INTERFACE,
                    .bDescriptorSubtype = USB_MIDI_SUBTYPE_MIDI_IN_JACK,
                    .bJackType = USB_MIDI_JACK_TYPE_EMBEDDED,
                    .bJackID = 0x01U,
                    .iJack = 0x00U,
                },
            .in_external =
                {
                    .bLength = sizeof(struct usb_midi_in_jack_descriptor),
                    .bDescriptorType = USB_AUDIO_DT_CS_INTERFACE,
                    .bDescriptorSubtype = USB_MIDI_SUBTYPE_MIDI_IN_JACK,
                    .bJackType = USB_MIDI_JACK_TYPE_EXTERNAL,
                    .bJackID = 0x02U,
                    .iJack = 0x00U,
                },
            .out_embedded =
                {
                    .head =
                        {
                            .bLength = sizeof(struct usb_midi_out_jack_descriptor),
                            .bDescriptorType = USB_AUDIO_DT_CS_INTERFACE,
                            .bDescriptorSubtype = USB_MIDI_SUBTYPE_MIDI_OUT_JACK,
                            .bJackType = USB_MIDI_JACK_TYPE_EMBEDDED,
                            .bJackID = 0x03U,
                            .bNrInputPins = 1,
                        },
                    .source[0] =
                        {
                            .baSourceID = 0x02U,
                            .baSourcePin = 0x01U,
                        },
                    .tail =
                        {
                            .iJack = 0x00U,
                        },
                },
            .out_external =
                {
                    .head =
                        {
                            .bLength = sizeof(struct usb_midi_out_jack_descriptor),
                            .bDescriptorType = USB_AUDIO_DT_CS_INTERFACE,
                            .bDescriptorSubtype = USB_MIDI_SUBTYPE_MIDI_OUT_JACK,
                            .bJackType = USB_MIDI_JACK_TYPE_EXTERNAL,
                            .bJackID = 0x04U,
                            .bNrInputPins = 1,
                        },
                    .source[0] =
                        {
                            .baSourceID = 0x01U,
                            .baSourcePin = 0x01U,
                        },
                    .tail =
                        {
                            .iJack = 0x00U,
                        },
                },
        },
    .bulk_out =
        {
            .bLength = sizeof(struct usb_endpoint_descriptor),
            .bDescriptorType = USB_DTYPE_ENDPOINT,
            .bEndpointAddress = MORSE_USB_MIDI_EP_OUT,
            .bmAttributes = USB_EPTYPE_BULK,
            .wMaxPacketSize = MORSE_USB_MIDI_EP_SIZE,
            .bInterval = 0,
        },
    .midi_bulk_out =
        {
            .head =
                {
                    .bLength = sizeof(struct usb_midi_endpoint_descriptor),
                    .bDescriptorType = USB_AUDIO_DT_CS_ENDPOINT,
                    .bDescriptorSubType = USB_MIDI_SUBTYPE_MS_GENERAL,
                    .bNumEmbMIDIJack = 1,
                },
            .jack[0] =
                {
                    .baAssocJackID = 0x01U,
                },
        },
    .bulk_in =
        {
            .bLength = sizeof(struct usb_endpoint_descriptor),
            .bDescriptorType = USB_DTYPE_ENDPOINT,
            .bEndpointAddress = MORSE_USB_MIDI_EP_IN,
            .bmAttributes = USB_EPTYPE_BULK,
            .wMaxPacketSize = MORSE_USB_MIDI_EP_SIZE,
            .bInterval = 0,
        },
    .midi_bulk_in =
        {
            .head =
                {
                    .bLength = sizeof(struct usb_midi_endpoint_descriptor),
                    .bDescriptorType = USB_AUDIO_DT_CS_ENDPOINT,
                    .bDescriptorSubType = USB_MIDI_SUBTYPE_MS_GENERAL,
                    .bNumEmbMIDIJack = 1,
                },
            .jack[0] =
                {
                    .baAssocJackID = 0x03U,
                },
        },
};

static struct usb_string_descriptor morse_usb_midi_manufacturer = USB_STRING_DESC("YO3GND");
static struct usb_string_descriptor morse_usb_midi_product = USB_STRING_DESC("Morse Flipper MIDI");
static struct usb_string_descriptor morse_usb_midi_serial = USB_STRING_DESC("Morse Flipper");

typedef struct {
    usbd_device* dev;
    FuriSemaphore* tx_sem;
    MorseUsbMidiRxCallback rx_callback;
    void* context;
    bool connected;
} MorseUsbMidiState;

static MorseUsbMidiState morse_usb_midi_state;

static void morse_usb_midi_init(usbd_device* dev, FuriHalUsbInterface* intf, void* ctx);
static void morse_usb_midi_deinit(usbd_device* dev);
static void morse_usb_midi_wakeup(usbd_device* dev);
static void morse_usb_midi_suspend(usbd_device* dev);
static usbd_respond morse_usb_midi_ep_config(usbd_device* dev, uint8_t cfg);
static usbd_respond
    morse_usb_midi_control(usbd_device* dev, usbd_ctlreq* req, usbd_rqc_callback* callback);
static void morse_usb_midi_ep_event(usbd_device* dev, uint8_t event, uint8_t ep);
static void morse_usb_midi_reset_tx_sem(void);

FuriHalUsbInterface morse_usb_midi_interface = {
    .init = morse_usb_midi_init,
    .deinit = morse_usb_midi_deinit,
    .wakeup = morse_usb_midi_wakeup,
    .suspend = morse_usb_midi_suspend,
    .dev_descr = (struct usb_device_descriptor*)&morse_usb_midi_device_descriptor,
    .cfg_descr = (void*)&morse_usb_midi_config,
};

bool morse_usb_midi_is_connected(void) {
    return morse_usb_midi_state.connected;
}

void morse_usb_midi_set_context(void* context) {
    morse_usb_midi_state.context = context;
}

void morse_usb_midi_set_rx_callback(MorseUsbMidiRxCallback callback) {
    morse_usb_midi_state.rx_callback = callback;
}

size_t morse_usb_midi_rx(uint8_t* buffer, size_t size) {
    if(buffer == NULL || size == 0U || morse_usb_midi_state.dev == NULL ||
       !morse_usb_midi_state.connected) {
        return 0U;
    }

    return usbd_ep_read(morse_usb_midi_state.dev, MORSE_USB_MIDI_EP_OUT, buffer, size);
}

size_t morse_usb_midi_tx(const uint8_t* buffer, size_t size) {
    if(buffer == NULL || size == 0U || morse_usb_midi_state.dev == NULL ||
       morse_usb_midi_state.tx_sem == NULL || !morse_usb_midi_state.connected) {
        return 0U;
    }

    /* Only one IN packet at a time; endpoint-complete releases the semaphore. */
    if(furi_semaphore_acquire(morse_usb_midi_state.tx_sem, 50U) != FuriStatusOk) {
        return 0U;
    }

    if(!morse_usb_midi_state.connected || morse_usb_midi_state.dev == NULL) {
        furi_semaphore_release(morse_usb_midi_state.tx_sem);
        return 0U;
    }

    int32_t written =
        usbd_ep_write(morse_usb_midi_state.dev, MORSE_USB_MIDI_EP_IN, (uint8_t*)buffer, size);
    if(written <= 0) {
        furi_semaphore_release(morse_usb_midi_state.tx_sem);
        return 0U;
    }

    return (size_t)written;
}

static void morse_usb_midi_init(usbd_device* dev, FuriHalUsbInterface* intf, void* ctx) {
    UNUSED(intf);
    UNUSED(ctx);

    morse_usb_midi_interface.str_manuf_descr = (void*)&morse_usb_midi_manufacturer;
    morse_usb_midi_interface.str_prod_descr = (void*)&morse_usb_midi_product;
    morse_usb_midi_interface.str_serial_descr = (void*)&morse_usb_midi_serial;
    morse_usb_midi_interface.dev_descr->idVendor = MORSE_USB_MIDI_VID;
    morse_usb_midi_interface.dev_descr->idProduct = MORSE_USB_MIDI_PID;
    morse_usb_midi_interface.dev_descr->iManufacturer = MorseUsbStrManufacturer;
    morse_usb_midi_interface.dev_descr->iProduct = MorseUsbStrProduct;
    morse_usb_midi_interface.dev_descr->iSerialNumber = MorseUsbStrSerial;

    morse_usb_midi_state.dev = dev;
    morse_usb_midi_state.connected = false;
    if(morse_usb_midi_state.tx_sem == NULL) {
        morse_usb_midi_state.tx_sem = furi_semaphore_alloc(1U, 1U);
    }
    morse_usb_midi_reset_tx_sem();

    usbd_reg_config(dev, morse_usb_midi_ep_config);
    usbd_reg_control(dev, morse_usb_midi_control);
    usbd_connect(dev, true);
}

static void morse_usb_midi_deinit(usbd_device* dev) {
    morse_usb_midi_state.connected = false;
    morse_usb_midi_state.dev = NULL;

    if(morse_usb_midi_state.tx_sem != NULL) {
        furi_semaphore_free(morse_usb_midi_state.tx_sem);
        morse_usb_midi_state.tx_sem = NULL;
    }

    usbd_reg_config(dev, NULL);
    usbd_reg_control(dev, NULL);
}

static void morse_usb_midi_wakeup(usbd_device* dev) {
    UNUSED(dev);
    morse_usb_midi_state.connected = true;
    morse_usb_midi_reset_tx_sem();
}

static void morse_usb_midi_suspend(usbd_device* dev) {
    UNUSED(dev);
    morse_usb_midi_state.connected = false;
    morse_usb_midi_reset_tx_sem();
}

static usbd_respond morse_usb_midi_ep_config(usbd_device* dev, uint8_t cfg) {
    /* Configure/deconfigure is the host's truth; connected follows that, not hope. */
    switch(cfg) {
    case MORSE_USB_CFG_DECONFIGURE:
        morse_usb_midi_state.connected = false;
        morse_usb_midi_reset_tx_sem();
        usbd_ep_deconfig(dev, MORSE_USB_MIDI_EP_OUT);
        usbd_ep_deconfig(dev, MORSE_USB_MIDI_EP_IN);
        usbd_reg_endpoint(dev, MORSE_USB_MIDI_EP_OUT, NULL);
        usbd_reg_endpoint(dev, MORSE_USB_MIDI_EP_IN, NULL);
        return usbd_ack;
    case MORSE_USB_CFG_CONFIGURE:
        morse_usb_midi_state.connected = true;
        morse_usb_midi_reset_tx_sem();
        usbd_ep_config(dev, MORSE_USB_MIDI_EP_OUT, USB_EPTYPE_BULK, MORSE_USB_MIDI_EP_SIZE);
        usbd_ep_config(dev, MORSE_USB_MIDI_EP_IN, USB_EPTYPE_BULK, MORSE_USB_MIDI_EP_SIZE);
        usbd_reg_endpoint(dev, MORSE_USB_MIDI_EP_OUT, morse_usb_midi_ep_event);
        usbd_reg_endpoint(dev, MORSE_USB_MIDI_EP_IN, morse_usb_midi_ep_event);
        return usbd_ack;
    default:
        return usbd_fail;
    }
}

static usbd_respond
    morse_usb_midi_control(usbd_device* dev, usbd_ctlreq* req, usbd_rqc_callback* callback) {
    UNUSED(dev);
    UNUSED(req);
    UNUSED(callback);
    return usbd_fail;
}

static void morse_usb_midi_ep_event(usbd_device* dev, uint8_t event, uint8_t ep) {
    UNUSED(dev);
    UNUSED(ep);

    if(event == usbd_evt_eptx) {
        furi_semaphore_release(morse_usb_midi_state.tx_sem);
    } else if(event == usbd_evt_eprx) {
        if(morse_usb_midi_state.rx_callback != NULL) {
            morse_usb_midi_state.rx_callback(morse_usb_midi_state.context);
        }
    }
}

static void morse_usb_midi_reset_tx_sem(void) {
    if(morse_usb_midi_state.tx_sem == NULL) {
        return;
    }

    /* Wake a blocked sender after suspend, reset, or unplug. Stale waits are expensive. */
    if(furi_semaphore_get_count(morse_usb_midi_state.tx_sem) == 0U) {
        furi_check(furi_semaphore_release(morse_usb_midi_state.tx_sem) == FuriStatusOk);
    }
}
