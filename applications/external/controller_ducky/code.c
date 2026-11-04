/*
 * Controller Ducky: game controller emulator for Flipper Zero (USB, wired).
 *
 * Emulates one of three controllers, picked when the app starts (and again
 * from the main menu):
 *
 *   Switch Pro Controller  VID 057E / PID 2009. Answers the Switch's USB
 *                          handshake (0x80 xx) and subcommands (0x01 ...),
 *                          then streams standard 0x30 input reports.
 *                          On the Switch, enable "Pro Controller Wired
 *                          Communication" in System Settings > Controllers
 *                          and Sensors, or the console will ignore it.
 *   DualSense (PS5)        VID 054C / PID 0CE6. Plain HID, input report 0x01.
 *   Xbox          VID 045E / PID 028E. Uses the XInput (Xbox 360
 *                          wired) protocol, which Windows, Steam and most
 *                          emulators accept as an Xbox controller.
 *
 * Compatibility notes
 *   - DualSense and Xbox modes are meant for PCs (Windows, Linux, Steam,
 *     emulators). PlayStation and Xbox consoles authenticate their
 *     controllers with keys that cannot be emulated, so they will not
 *     accept these modes.
 *   - The Xbox mode is NOT the Xbox One / Series "GIP" protocol, it is the
 *     older XInput protocol (see above).
 *
 * App flow
 *   Select screen: pick Switch Pro / DualSense / Xbox (Back: quit the app)
 *   Wait screen  : "plug into a host", until the host enumerates the device
 *                  (Back: return to the select screen)
 *   Menu         : Controller Mode / Ducky Mode / Switch Controller
 *                  (Switch Controller and Back: return to the select screen
 *                  and stop the USB connection, the Flipper's normal USB
 *                  mode comes back until a controller is picked again)
 *   If the USB connection to the host is lost while the menu, controller
 *   mode or a script is active, the app drops back to the select screen
 *   and shows "Connection lost". Just like going back by hand, this stops
 *   emulating the controller completely: the USB connection is released
 *   and nothing is sent to the host until a controller is picked again.
 *   Picking a controller then goes to the wait screen, exactly like the
 *   first connection, until the host is back.
 *   Controller   : picture of the controller that lights up with your input
 *                    Arrow keys / OK / Back : mapped according to the layout
 *                                             (layouts: D-Pad, L-Stick, R-Stick,
 *                                             Face, Shoulder, Home)
 *                    Hold OK                : next layout
 *                    Hold Back              : back to the menu
 *   Ducky Mode   : pick a .txt script from apps_data/<app>/scripts and run it
 *                    The screen shows the script name, the line number and
 *                    state, and the action being executed (DELAY, PRESS,
 *                    HOLD, PUSH, RELEASE; never REM, CONTROLLER or REPEAT)
 *                    Back while running     : stop the script
 *                    When stopped / done    : OK runs it again,
 *                                             Back returns to the script list
 *
 * OK and Back are tap-only in controller mode: the mapped button is sent as a
 * short pulse when the key is released, so that holding OK / Back for the
 * layout switch or menu does not also send a stray button press to the host.
 * In the D-Pad and L-Stick layouts OK / Back are the "confirm" / "cancel"
 * buttons of the emulated controller: A / B on the Switch, Cross / Circle on
 * the DualSense, A / B on the Xbox.
 *
 * Ducky script commands (one per line, case-insensitive)
 *   CONTROLLER name     header: the controller this script is written for
 *                       (SWITCH, DUALSENSE or XBOX; PS5 and PRO also work).
 *                       Must come before any other command (REM lines and
 *                       blank lines may precede it). If it names a different
 *                       controller than the one currently selected, the
 *                       script does not run and the screen says which
 *                       controller it needs. Scripts without a header run
 *                       on whatever controller is selected.
 *   REM text            comment
 *   DELAY ms            wait
 *   PRESS A [B ...]     tap one or more buttons
 *   HOLD A [B ...] ms   hold buttons for ms
 *   PUSH A [B ...]      press and keep held
 *   RELEASE A | ALL     release held buttons
 *   REPEAT n            run the previous command n more times
 *   REPEAT START [n]    begin a loop: run the block n times, or forever if
 *                       n is omitted (until Back is pressed). Loops can be
 *                       nested up to 8 deep.
 *   REPEAT END          close the innermost open REPEAT START: jump back to
 *                       its start, or carry on once its runs are done
 *
 * A script whose REPEAT START / REPEAT END lines do not pair up, or whose
 * loops nest deeper than 8 levels, does not run: the screen shows the line
 * with the problem.
 *
 * Button names depend on the controller selected in the app. Names that do
 * not exist on the selected controller are ignored.
 *   Switch    : A B X Y L R ZL ZR PLUS MINUS HOME CAPTURE LSTICK RSTICK
 *               UP DOWN LEFT RIGHT (d-pad)  LS_UP LS_DOWN LS_LEFT LS_RIGHT
 *               RS_UP RS_DOWN RS_LEFT RS_RIGHT
 *   DualSense : CROSS CIRCLE SQUARE TRIANGLE L1 R1 L2 R2 CREATE OPTIONS PS
 *               TOUCHPAD MUTE L3 R3
 *               UP DOWN LEFT RIGHT (d-pad)  LS_UP LS_DOWN LS_LEFT LS_RIGHT
 *               RS_UP RS_DOWN RS_LEFT RS_RIGHT
 *   Xbox      : A B X Y LB RB LT RT MENU VIEW XBOX LS RS
 *               UP DOWN LEFT RIGHT (d-pad)  LS_UP LS_DOWN LS_LEFT LS_RIGHT
 *               RS_UP RS_DOWN RS_LEFT RS_RIGHT
 * (LS_* move the left stick, RS_* move the right stick; LS / RS / L3 / R3 /
 * LSTICK / RSTICK click the sticks.)
 *
 * Example scripts
 *   CONTROLLER SWITCH
 *   REM mash A forever
 *   REPEAT START
 *   PRESS A
 *   DELAY 200
 *   REPEAT END
 *
 *   CONTROLLER SWITCH
 *   REM nested: press A once, then B five times, and do that three times
 *   REPEAT START 3
 *   PRESS A
 *   REPEAT START 5
 *   PRESS B
 *   DELAY 100
 *   REPEAT END
 *   REPEAT END
 *
 * Protocol notes are based on the public Switch controller reverse
 * engineering docs (dekuNukem/Nintendo_Switch_Reverse_Engineering), on a
 * USB capture of a real Pro Controller in wired mode (GBAtemp thread
 * "Reverse engineering the Switch Pro controller wired mode"), and on the
 * Linux hid-playstation driver (DualSense report layout).
 */

#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_usb.h>
#include <gui/gui.h>
#include <gui/elements.h>
#include <input/input.h>
#include <storage/storage.h>
#include <toolbox/stream/stream.h>
#include <toolbox/stream/file_stream.h>
#include <usb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Generated by ufbt from images/icon.png (fap_icon_assets="images").
 * The header is named <appid>_icons.h: change it if your appid differs. */
#include "controller_ducky_icons.h"

#define TAG "ControllerDucky"

/* Uncomment to skip the "waiting for host" screen (UI testing without a host) */
/* #define SP_SKIP_USB_WAIT */

/* ------------------------------------------------------------------ */
/* Constants                                                          */
/* ------------------------------------------------------------------ */

/* Switch Pro Controller */
#define SW_VID 0x057E
#define SW_PID 0x2009

/* DualSense */
#define DS_VID 0x054C
#define DS_PID 0x0CE6

/* Xbox 360 wired controller (XInput) */
#define XB_VID 0x045E
#define XB_PID 0x028E

#define SW_EP_IN       0x81
#define SW_EP_OUT      0x01
#define SW_EP_SIZE     64 /* Switch Pro and DualSense */
#define SW_EP_INTERVAL 8
#define DS_EP_INTERVAL 4
#define XB_EP_SIZE     32

#define XB_CLASS_VENDOR 0xFF

#ifndef USB_EP0_SIZE
#define USB_EP0_SIZE 8 /* must match what the Flipper USB HAL configured */
#endif

#define HID_REQ_GET_REPORT      0x01
#define HID_REQ_SET_REPORT      0x09
#define HID_REQ_SET_IDLE        0x0A
#define HID_REQ_SET_PROTOCOL    0x0B
#define HID_DTYPE_HID           0x21
#define HID_DTYPE_REPORT        0x22
#define HID_REPORT_TYPE_FEATURE 0x03

#ifndef USB_CLASS_HID
#define USB_CLASS_HID 0x03
#endif

#define REPORT_INTERVAL_MS 8
#define CONN_LOST_MS       1000 /* host must be gone this long before we return to the select screen */
#define TAP_MS             105 /* how long a tap on OK / Back (or a script PRESS) is held down */

/* Internal button bitmask. This is the Switch layout (byte3 | byte4 << 8 |
 * byte5 << 16 of the 0x30 report); the other controllers translate it to
 * their own reports. Buttons are named by their Switch position:
 *   X = north, A = east, B = south, Y = west. */
#define BTN_Y       0x000001u
#define BTN_X       0x000002u
#define BTN_B       0x000004u
#define BTN_A       0x000008u
#define BTN_R       0x000040u
#define BTN_ZR      0x000080u
#define BTN_MINUS   0x000100u
#define BTN_PLUS    0x000200u
#define BTN_RSTICK  0x000400u
#define BTN_LSTICK  0x000800u
#define BTN_HOME    0x001000u
#define BTN_CAPTURE 0x002000u
#define BTN_MUTE    0x004000u /* internal only (DualSense mic mute), never sent to a Switch */
#define BTN_DOWN    0x010000u
#define BTN_UP      0x020000u
#define BTN_RIGHT   0x040000u
#define BTN_LEFT    0x080000u
#define BTN_L       0x400000u
#define BTN_ZL      0x800000u

/* Extra bits (not part of the report) used by scripts for left stick directions */
#define SP_LS_UP       0x01000000u
#define SP_LS_DOWN     0x02000000u
#define SP_LS_LEFT     0x04000000u
#define SP_LS_RIGHT    0x08000000u
#define SP_RS_UP       0x10000000u
#define SP_RS_DOWN     0x20000000u
#define SP_RS_LEFT     0x40000000u
#define SP_RS_RIGHT    0x80000000u
#define SP_REPORT_MASK 0x00FFFFFFu

#define STICK_CENTER 2048
#define STICK_RANGE  1500 /* must match the calibration data below */
#define STICK_DIAG   1060 /* ~ 1500 * 0.707 */

/* Fake, locally administered MAC address */
static const uint8_t SW_MAC[6] = {0x02, 0xF1, 0x95, 0x00, 0x00, 0x01};

/* Script button names */
typedef struct {
    const char* name;
    uint32_t mask;
} ButtonName;

static const ButtonName names_switch[] = {
    {"A", BTN_A},
    {"B", BTN_B},
    {"X", BTN_X},
    {"Y", BTN_Y},
    {"L", BTN_L},
    {"R", BTN_R},
    {"ZL", BTN_ZL},
    {"ZR", BTN_ZR},
    {"PLUS", BTN_PLUS},
    {"MINUS", BTN_MINUS},
    {"HOME", BTN_HOME},
    {"CAPTURE", BTN_CAPTURE},
    {"LSTICK", BTN_LSTICK},
    {"RSTICK", BTN_RSTICK},
    {"UP", BTN_UP},
    {"DOWN", BTN_DOWN},
    {"LEFT", BTN_LEFT},
    {"RIGHT", BTN_RIGHT},
    {"LS_UP", SP_LS_UP},
    {"LS_DOWN", SP_LS_DOWN},
    {"LS_LEFT", SP_LS_LEFT},
    {"LS_RIGHT", SP_LS_RIGHT},
    {"RS_UP", SP_RS_UP},
    {"RS_DOWN", SP_RS_DOWN},
    {"RS_LEFT", SP_RS_LEFT},
    {"RS_RIGHT", SP_RS_RIGHT},
};

static const ButtonName names_dualsense[] = {
    {"CROSS", BTN_B},
    {"CIRCLE", BTN_A},
    {"SQUARE", BTN_Y},
    {"TRIANGLE", BTN_X},
    {"L1", BTN_L},
    {"R1", BTN_R},
    {"L2", BTN_ZL},
    {"R2", BTN_ZR},
    {"CREATE", BTN_MINUS},
    {"OPTIONS", BTN_PLUS},
    {"PS", BTN_HOME},
    {"TOUCHPAD", BTN_CAPTURE},
    {"MUTE", BTN_MUTE},
    {"L3", BTN_LSTICK},
    {"R3", BTN_RSTICK},
    {"UP", BTN_UP},
    {"DOWN", BTN_DOWN},
    {"LEFT", BTN_LEFT},
    {"RIGHT", BTN_RIGHT},
    {"LS_UP", SP_LS_UP},
    {"LS_DOWN", SP_LS_DOWN},
    {"LS_LEFT", SP_LS_LEFT},
    {"LS_RIGHT", SP_LS_RIGHT},
    {"RS_UP", SP_RS_UP},
    {"RS_DOWN", SP_RS_DOWN},
    {"RS_LEFT", SP_RS_LEFT},
    {"RS_RIGHT", SP_RS_RIGHT},
};

static const ButtonName names_xbox[] = {
    {"A", BTN_B},
    {"B", BTN_A},
    {"X", BTN_Y},
    {"Y", BTN_X},
    {"LB", BTN_L},
    {"RB", BTN_R},
    {"LT", BTN_ZL},
    {"RT", BTN_ZR},
    {"MENU", BTN_PLUS},
    {"VIEW", BTN_MINUS},
    {"XBOX", BTN_HOME},
    {"LS", BTN_LSTICK},
    {"RS", BTN_RSTICK},
    {"UP", BTN_UP},
    {"DOWN", BTN_DOWN},
    {"LEFT", BTN_LEFT},
    {"RIGHT", BTN_RIGHT},
    {"LS_UP", SP_LS_UP},
    {"LS_DOWN", SP_LS_DOWN},
    {"LS_LEFT", SP_LS_LEFT},
    {"LS_RIGHT", SP_LS_RIGHT},
    {"RS_UP", SP_RS_UP},
    {"RS_DOWN", SP_RS_DOWN},
    {"RS_LEFT", SP_RS_LEFT},
    {"RS_RIGHT", SP_RS_RIGHT},
};

/* ------------------------------------------------------------------ */
/* USB descriptors                                                    */
/* ------------------------------------------------------------------ */

/* ---- Switch Pro Controller ---- */

static const uint8_t sw_report_desc[] = {
    0x05, 0x01, 0x15, 0x00, 0x09, 0x04, 0xA1, 0x01, 0x85, 0x30, 0x05, 0x01, 0x05, 0x09, 0x19, 0x01,
    0x29, 0x0A, 0x15, 0x00, 0x25, 0x01, 0x75, 0x01, 0x95, 0x0A, 0x55, 0x00, 0x65, 0x00, 0x81, 0x02,
    0x05, 0x09, 0x19, 0x0B, 0x29, 0x0E, 0x15, 0x00, 0x25, 0x01, 0x75, 0x01, 0x95, 0x04, 0x81, 0x02,
    0x75, 0x01, 0x95, 0x02, 0x81, 0x03, 0x0B, 0x01, 0x00, 0x01, 0x00, 0xA1, 0x00, 0x0B, 0x30, 0x00,
    0x01, 0x00, 0x0B, 0x31, 0x00, 0x01, 0x00, 0x0B, 0x32, 0x00, 0x01, 0x00, 0x0B, 0x35, 0x00, 0x01,
    0x00, 0x15, 0x00, 0x27, 0xFF, 0xFF, 0x00, 0x00, 0x75, 0x10, 0x95, 0x04, 0x81, 0x02, 0xC0, 0x0B,
    0x39, 0x00, 0x01, 0x00, 0x15, 0x00, 0x25, 0x07, 0x35, 0x00, 0x46, 0x3B, 0x01, 0x65, 0x14, 0x75,
    0x04, 0x95, 0x01, 0x81, 0x02, 0x05, 0x09, 0x19, 0x0F, 0x29, 0x12, 0x15, 0x00, 0x25, 0x01, 0x75,
    0x01, 0x95, 0x04, 0x81, 0x02, 0x75, 0x08, 0x95, 0x34, 0x81, 0x03, 0x06, 0x00, 0xFF, 0x85, 0x21,
    0x09, 0x01, 0x75, 0x08, 0x95, 0x3F, 0x81, 0x03, 0x85, 0x81, 0x09, 0x02, 0x75, 0x08, 0x95, 0x3F,
    0x81, 0x03, 0x85, 0x01, 0x09, 0x03, 0x75, 0x08, 0x95, 0x3F, 0x91, 0x83, 0x85, 0x10, 0x09, 0x04,
    0x75, 0x08, 0x95, 0x3F, 0x91, 0x83, 0x85, 0x80, 0x09, 0x05, 0x75, 0x08, 0x95, 0x3F, 0x91, 0x83,
    0x85, 0x82, 0x09, 0x06, 0x75, 0x08, 0x95, 0x3F, 0x91, 0x83, 0xC0,
};

/* ---- DualSense ----
 * Simplified version of the real descriptor: input report 0x01 (63 bytes +
 * ID), output report 0x02, and the feature reports hosts query at start-up. */
static const uint8_t ds_report_desc[] = {
    0x05, 0x01, /* Usage Page (Generic Desktop) */
    0x09, 0x05, /* Usage (Game Pad) */
    0xA1, 0x01, /* Collection (Application) */
    0x85, 0x01, /*   Report ID (1) */
    0x09, 0x30, 0x09, 0x31, 0x09, 0x32, 0x09, 0x35, 0x09, 0x33, 0x09, 0x34,
    0x15, 0x00, 0x26, 0xFF, 0x00, 0x75, 0x08, 0x95, 0x06, 0x81, 0x02, /* 6 axes */
    0x06, 0x00, 0xFF, 0x09, 0x20, 0x95, 0x01, 0x81, 0x02, /* counter */
    0x05, 0x01, 0x09, 0x39, 0x15, 0x00, 0x25, 0x07, 0x35, 0x00, 0x46, 0x3B,
    0x01, 0x65, 0x14, 0x75, 0x04, 0x95, 0x01, 0x81, 0x42, /* hat switch */
    0x65, 0x00, 0x35, 0x00, 0x45, 0x00, 0x05, 0x09, 0x19, 0x01, 0x29, 0x0F,
    0x15, 0x00, 0x25, 0x01, 0x75, 0x01, 0x95, 0x0F, 0x81, 0x02, /* 15 buttons */
    0x06, 0x00, 0xFF, 0x09, 0x21, 0x95, 0x0D, 0x81, 0x02, /* padding bits */
    0x06, 0x00, 0xFF, 0x09, 0x22, 0x15, 0x00, 0x26, 0xFF, 0x00, 0x75, 0x08,
    0x95, 0x34, 0x81, 0x02, /* IMU, touchpad, status */
    0x85, 0x02, 0x09, 0x23, 0x95, 0x2F, 0x91, 0x02, /* output report 2 */
    0x85, 0x05, 0x09, 0x33, 0x95, 0x28, 0xB1, 0x02, /* feature 5: calibration */
    0x85, 0x09, 0x09, 0x24, 0x95, 0x13, 0xB1, 0x02, /* feature 9: pairing info */
    0x85, 0x20, 0x09, 0x26, 0x95, 0x3F, 0xB1, 0x02, /* feature 0x20: firmware info */
    0xC0,
};

/* Feature report 0x05: IMU calibration (neutral values, 41 bytes with ID) */
static const uint8_t ds_feat_cal[41] = {
    0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* gyro bias pitch / yaw / roll */
    0x00, 0x20, 0x00, 0xE0, /* gyro pitch +/- */
    0x00, 0x20, 0x00, 0xE0, /* gyro yaw +/- */
    0x00, 0x20, 0x00, 0xE0, /* gyro roll +/- */
    0x1C, 0x02, 0x1C, 0x02, /* gyro speed +/- (540) */
    0x00, 0x20, 0x00, 0xE0, /* accel x +/- */
    0x00, 0x20, 0x00, 0xE0, /* accel y +/- */
    0x00, 0x20, 0x00, 0xE0, /* accel z +/- */
};

/* Feature report 0x09: pairing info (MAC, 20 bytes with ID) */
static const uint8_t ds_feat_pair[20] = {
    0x09,
    0x02,
    0xF1,
    0x95,
    0x00,
    0x00,
    0x02, /* MAC */
    0x08,
    0x25,
    0x00,
};

/* Feature report 0x20: firmware info (64 bytes with ID) */
static const uint8_t ds_feat_fw[64] = {
    0x20,        'S',         'e',         'p',         ' ', '2', '9', ' ',
    '2',         '0',         '2',         '0', /* build date */
    '1',         '2',         ':',         '0',         '0', ':', '0', '0', /* build time */
    [24] = 0x20, [25] = 0x0B, /* hardware version (LE32) */
    [28] = 0x00, [29] = 0x01, [30] = 0x00, [31] = 0x01, /* firmware version (LE32) */
    [44] = 0x24, [45] = 0x02,
};

static const uint8_t* ds_feature(uint8_t id, size_t* len) {
    switch(id) {
    case 0x05:
        *len = sizeof(ds_feat_cal);
        return ds_feat_cal;
    case 0x09:
        *len = sizeof(ds_feat_pair);
        return ds_feat_pair;
    case 0x20:
        *len = sizeof(ds_feat_fw);
        return ds_feat_fw;
    default:
        return NULL;
    }
}

/* ---- Shared HID config layout (Switch Pro and DualSense) ---- */

typedef struct {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint16_t bcdHID;
    uint8_t bCountryCode;
    uint8_t bNumDescriptors;
    uint8_t bDescriptorType0;
    uint16_t wDescriptorLength0;
} __attribute__((packed)) SwHidDescriptor;

typedef struct {
    struct usb_config_descriptor config;
    struct usb_interface_descriptor intf;
    SwHidDescriptor hid;
    struct usb_endpoint_descriptor ep_in;
    struct usb_endpoint_descriptor ep_out;
} __attribute__((packed)) SwConfigDescriptor;

#define HID_CONFIG_DESC(report_len, interval)                           \
    {                                                                   \
        .config =                                                       \
            {                                                           \
                .bLength = sizeof(struct usb_config_descriptor),        \
                .bDescriptorType = USB_DTYPE_CONFIGURATION,             \
                .wTotalLength = sizeof(SwConfigDescriptor),             \
                .bNumInterfaces = 1,                                    \
                .bConfigurationValue = 1,                               \
                .iConfiguration = 0,                                    \
                .bmAttributes = 0xA0, /* bus powered + remote wakeup */ \
                .bMaxPower = 250, /* 500 mA */                          \
            },                                                          \
        .intf =                                                         \
            {                                                           \
                .bLength = sizeof(struct usb_interface_descriptor),     \
                .bDescriptorType = USB_DTYPE_INTERFACE,                 \
                .bInterfaceNumber = 0,                                  \
                .bAlternateSetting = 0,                                 \
                .bNumEndpoints = 2,                                     \
                .bInterfaceClass = USB_CLASS_HID,                       \
                .bInterfaceSubClass = 0,                                \
                .bInterfaceProtocol = 0,                                \
                .iInterface = 0,                                        \
            },                                                          \
        .hid =                                                          \
            {                                                           \
                .bLength = sizeof(SwHidDescriptor),                     \
                .bDescriptorType = HID_DTYPE_HID,                       \
                .bcdHID = 0x0111,                                       \
                .bCountryCode = 0,                                      \
                .bNumDescriptors = 1,                                   \
                .bDescriptorType0 = HID_DTYPE_REPORT,                   \
                .wDescriptorLength0 = (report_len),                     \
            },                                                          \
        .ep_in =                                                        \
            {                                                           \
                .bLength = sizeof(struct usb_endpoint_descriptor),      \
                .bDescriptorType = USB_DTYPE_ENDPOINT,                  \
                .bEndpointAddress = SW_EP_IN,                           \
                .bmAttributes = USB_EPTYPE_INTERRUPT,                   \
                .wMaxPacketSize = SW_EP_SIZE,                           \
                .bInterval = (interval),                                \
            },                                                          \
        .ep_out =                                                       \
            {                                                           \
                .bLength = sizeof(struct usb_endpoint_descriptor),      \
                .bDescriptorType = USB_DTYPE_ENDPOINT,                  \
                .bEndpointAddress = SW_EP_OUT,                          \
                .bmAttributes = USB_EPTYPE_INTERRUPT,                   \
                .wMaxPacketSize = SW_EP_SIZE,                           \
                .bInterval = (interval),                                \
            },                                                          \
    }

static const struct usb_device_descriptor sw_dev_desc = {
    .bLength = sizeof(struct usb_device_descriptor),
    .bDescriptorType = USB_DTYPE_DEVICE,
    .bcdUSB = VERSION_BCD(2, 0, 0),
    .bDeviceClass = USB_CLASS_PER_INTERFACE,
    .bDeviceSubClass = USB_SUBCLASS_NONE,
    .bDeviceProtocol = USB_PROTO_NONE,
    .bMaxPacketSize0 = USB_EP0_SIZE,
    .idVendor = SW_VID,
    .idProduct = SW_PID,
    .bcdDevice = VERSION_BCD(2, 0, 0),
    .iManufacturer = 1,
    .iProduct = 2,
    .iSerialNumber = 3,
    .bNumConfigurations = 1,
};

static const SwConfigDescriptor sw_cfg_desc =
    HID_CONFIG_DESC(sizeof(sw_report_desc), SW_EP_INTERVAL);

static const struct usb_string_descriptor sw_str_manuf = USB_STRING_DESC("Nintendo Co., Ltd.");
static const struct usb_string_descriptor sw_str_prod = USB_STRING_DESC("Pro Controller");
static const struct usb_string_descriptor sw_str_serial = USB_STRING_DESC("000000000001");

/* DualSense */

static const struct usb_device_descriptor ds_dev_desc = {
    .bLength = sizeof(struct usb_device_descriptor),
    .bDescriptorType = USB_DTYPE_DEVICE,
    .bcdUSB = VERSION_BCD(2, 0, 0),
    .bDeviceClass = USB_CLASS_PER_INTERFACE,
    .bDeviceSubClass = USB_SUBCLASS_NONE,
    .bDeviceProtocol = USB_PROTO_NONE,
    .bMaxPacketSize0 = USB_EP0_SIZE,
    .idVendor = DS_VID,
    .idProduct = DS_PID,
    .bcdDevice = VERSION_BCD(1, 0, 0),
    .iManufacturer = 1,
    .iProduct = 2,
    .iSerialNumber = 3,
    .bNumConfigurations = 1,
};

static const SwConfigDescriptor ds_cfg_desc =
    HID_CONFIG_DESC(sizeof(ds_report_desc), DS_EP_INTERVAL);

static const struct usb_string_descriptor ds_str_manuf =
    USB_STRING_DESC("Sony Interactive Entertainment");
static const struct usb_string_descriptor ds_str_prod =
    USB_STRING_DESC("DualSense Wireless Controller");
static const struct usb_string_descriptor ds_str_serial = USB_STRING_DESC("000000000002");

/* Xbox (XInput): one vendor-specific interface, no HID */

typedef struct {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t data[15];
} __attribute__((packed)) XbClassDescriptor;

typedef struct {
    struct usb_config_descriptor config;
    struct usb_interface_descriptor intf;
    XbClassDescriptor xb;
    struct usb_endpoint_descriptor ep_in;
    struct usb_endpoint_descriptor ep_out;
} __attribute__((packed)) XbConfigDescriptor;

static const struct usb_device_descriptor xb_dev_desc = {
    .bLength = sizeof(struct usb_device_descriptor),
    .bDescriptorType = USB_DTYPE_DEVICE,
    .bcdUSB = VERSION_BCD(2, 0, 0),
    .bDeviceClass = XB_CLASS_VENDOR,
    .bDeviceSubClass = 0xFF,
    .bDeviceProtocol = 0xFF,
    .bMaxPacketSize0 = USB_EP0_SIZE,
    .idVendor = XB_VID,
    .idProduct = XB_PID,
    .bcdDevice = VERSION_BCD(1, 1, 4),
    .iManufacturer = 1,
    .iProduct = 2,
    .iSerialNumber = 3,
    .bNumConfigurations = 1,
};

static const XbConfigDescriptor xb_cfg_desc = {
    .config =
        {
            .bLength = sizeof(struct usb_config_descriptor),
            .bDescriptorType = USB_DTYPE_CONFIGURATION,
            .wTotalLength = sizeof(XbConfigDescriptor),
            .bNumInterfaces = 1,
            .bConfigurationValue = 1,
            .iConfiguration = 0,
            .bmAttributes = 0xA0,
            .bMaxPower = 250,
        },
    .intf =
        {
            .bLength = sizeof(struct usb_interface_descriptor),
            .bDescriptorType = USB_DTYPE_INTERFACE,
            .bInterfaceNumber = 0,
            .bAlternateSetting = 0,
            .bNumEndpoints = 2,
            .bInterfaceClass = XB_CLASS_VENDOR,
            .bInterfaceSubClass = 0x5D, /* Xbox 360 wired controller */
            .bInterfaceProtocol = 0x01,
            .iInterface = 0,
        },
    .xb =
        {
            .bLength = sizeof(XbClassDescriptor),
            .bDescriptorType = 0x21,
            .data =
                {0x10,
                 0x01,
                 0x01,
                 0x25,
                 0x81,
                 0x14,
                 0x00,
                 0x00,
                 0x00,
                 0x00,
                 0x13,
                 0x01,
                 0x08,
                 0x00,
                 0x00},
        },
    .ep_in =
        {
            .bLength = sizeof(struct usb_endpoint_descriptor),
            .bDescriptorType = USB_DTYPE_ENDPOINT,
            .bEndpointAddress = SW_EP_IN,
            .bmAttributes = USB_EPTYPE_INTERRUPT,
            .wMaxPacketSize = XB_EP_SIZE,
            .bInterval = 4,
        },
    .ep_out =
        {
            .bLength = sizeof(struct usb_endpoint_descriptor),
            .bDescriptorType = USB_DTYPE_ENDPOINT,
            .bEndpointAddress = SW_EP_OUT,
            .bmAttributes = USB_EPTYPE_INTERRUPT,
            .wMaxPacketSize = XB_EP_SIZE,
            .bInterval = 8,
        },
};

static const struct usb_string_descriptor xb_str_manuf = USB_STRING_DESC("Microsoft");
static const struct usb_string_descriptor xb_str_prod = USB_STRING_DESC("Controller");
static const struct usb_string_descriptor xb_str_serial = USB_STRING_DESC("000000000003");

/* ------------------------------------------------------------------ */
/* App types                                                          */
/* ------------------------------------------------------------------ */

typedef enum {
    EventInput,
    EventUsbRx,
} EventType;

typedef struct {
    EventType type;
    union {
        InputEvent input;
        struct {
            uint8_t len;
            uint8_t data[64];
        } rx;
    };
} AppEvent;

typedef struct {
    uint32_t buttons;
    int8_t lx; /* -1, 0, +1  (+1 = right) */
    int8_t ly; /* -1, 0, +1  (+1 = up)    */
    int8_t rx; /* right stick, same convention */
    int8_t ry;
} KeyAction;

typedef struct {
    const char* name;
    const char* hint1;
    const char* hint2;
    bool confirm_ab; /* OK / Back are the controller's confirm / cancel buttons */
    KeyAction map[InputKeyMAX];
} Layout;

typedef enum {
    CtrlSwitch,
    CtrlDualSense,
    CtrlXbox,
    CtrlCount,
} ControllerType;

/* Everything that differs between the emulated controllers */
typedef struct {
    ControllerType type;
    const char* title;
    const char* wait1; /* "plug in" hint, two lines */
    const char* wait2;
    FuriHalUsbInterface* usb_if;
    const SwHidDescriptor* hid_desc; /* NULL for non-HID (Xbox) */
    const uint8_t* report_desc;
    uint16_t report_desc_len;
    uint8_t ep_size;
    bool needs_handshake; /* host must run the Switch 0x80 handshake first */
    bool confirm_south; /* "confirm" is the south face button (PS / Xbox), not east */
    const ButtonName* buttons;
    size_t button_count;
    const char* face[4]; /* picture labels: north, east, south, west */
    bool face_symbols; /* draw face labels T / O / X / S as PlayStation shapes */
} Profile;

typedef enum {
    ScreenSelect,
    ScreenWait,
    ScreenMenu,
    ScreenController,
    ScreenPicker,
    ScreenDucky,
} Screen;

typedef enum {
    MenuController,
    MenuDucky,
    MenuSwitch,
    MenuCount,
} MenuItem;

typedef enum {
    ScriptRunning,
    ScriptWaitHost,
    ScriptDone,
    ScriptStopped,
    ScriptFailed,
    ScriptWrongCtrl,
    ScriptError,
} ScriptState;

static const char* const script_state_names[] = {
    "Running",
    "Waiting for host",
    "Done",
    "Stopped",
    "Open failed",
    "Wrong controller",
    "Script error",
};

#define LOOP_MAX_DEPTH 8 /* how deeply REPEAT START blocks can nest */
#define PICKER_MAX     24
#define PICKER_NAME    48
#define SCRIPT_DIR     APP_DATA_PATH("scripts")

/* One open REPEAT START block */
typedef struct {
    size_t pos; /* stream offset just after the REPEAT START line */
    uint32_t line; /* line number of the REPEAT START line */
    uint32_t left; /* runs remaining when a count was given */
    bool forever; /* no count given: loop until stopped */
} LoopFrame;

typedef struct {
    Gui* gui;
    ViewPort* view_port;
    FuriMessageQueue* queue;

    bool running;
    Screen screen;
    uint8_t menu_sel;

    /* Controller selection */
    const Profile* profile; /* active profile, NULL until one has been picked */
    uint8_t select_sel;
    bool conn_lost; /* the select screen was reached because the host went away */
    bool lost_pending; /* the host seems gone, waiting out CONN_LOST_MS */
    uint32_t lost_since;
    bool usb_changed; /* we replaced the USB configuration (restore it on exit) */
    FuriHalUsbInterface* usb_prev; /* the USB configuration to restore */
    uint8_t report_seq;

    uint8_t layout;
    bool key_down[InputKeyMAX];

    /* OK / Back are tap-only in controller mode (see header comment) */
    bool seen_press[InputKeyMAX]; /* the press for this key happened on this screen */
    bool consumed[InputKeyMAX]; /* long press already handled for this hold */
    bool tapping[InputKeyMAX]; /* a tap pulse is currently being sent */
    uint32_t tap_until[InputKeyMAX];

    uint32_t buttons;
    int8_t lx;
    int8_t ly;
    int8_t rx; /* right stick */
    int8_t ry;

    bool streaming;
    uint32_t last_tx;

    /* Script picker */
    char picker_names[PICKER_MAX][PICKER_NAME];
    uint8_t picker_count;
    uint8_t picker_sel;

    /* Ducky script runner */
    Stream* script;
    FuriString* script_line;
    FuriString* script_last;
    char script_name[PICKER_NAME];
    char script_note[32]; /* shown on the Ducky screen, e.g. which controller is needed */
    char script_action[48]; /* the command being executed, shown on the Ducky screen */
    ScriptState script_state;
    uint32_t script_line_no;
    uint32_t script_mask; /* buttons currently held by the script */
    uint32_t script_until;
    uint32_t script_clear; /* buttons to release when the current wait ends */
    uint32_t script_tail; /* extra wait (ms) after releasing */
    uint16_t script_repeat_left;
    bool script_busy;

    /* REPEAT START / REPEAT END (a stack, so loops can nest) */
    LoopFrame loops[LOOP_MAX_DEPTH];
    uint8_t loop_depth;
} ControllerDucky;

/* State shared with the USB callbacks (they have no user pointer) */
static struct {
    usbd_device* dev;
    FuriSemaphore* tx_sem;
    FuriMessageQueue* queue;
    const Profile* profile;
    volatile bool configured;
    volatile bool suspended;
    volatile bool reconfigured;
    volatile bool link_fresh; /* a (re)connection happened since the last "connection lost" */
} sw_usb;

/* ------------------------------------------------------------------ */
/* Layouts                                                            */
/* ------------------------------------------------------------------ */

static const Layout layouts[] = {
    {
        .name = "D-Pad",
        .hint1 = "Arrows: D-Pad",
        .hint2 = "OK: A   Back: B",
        .confirm_ab = true,
        .map =
            {
                [InputKeyUp] = {.buttons = BTN_UP},
                [InputKeyDown] = {.buttons = BTN_DOWN},
                [InputKeyLeft] = {.buttons = BTN_LEFT},
                [InputKeyRight] = {.buttons = BTN_RIGHT},
                [InputKeyOk] = {.buttons = BTN_A},
                [InputKeyBack] = {.buttons = BTN_B},
            },
    },
    {
        .name = "L-Stick",
        .hint1 = "Arrows: Left stick",
        .hint2 = "OK: A   Back: B",
        .confirm_ab = true,
        .map =
            {
                [InputKeyUp] = {.ly = 1},
                [InputKeyDown] = {.ly = -1},
                [InputKeyLeft] = {.lx = -1},
                [InputKeyRight] = {.lx = 1},
                [InputKeyOk] = {.buttons = BTN_A},
                [InputKeyBack] = {.buttons = BTN_B},
            },
    },
    {
        .name = "R-Stick",
        .hint1 = "Arrows: Right stick",
        .hint2 = "OK: A   Back: B",
        .confirm_ab = true,
        .map =
            {
                [InputKeyUp] = {.ry = 1},
                [InputKeyDown] = {.ry = -1},
                [InputKeyLeft] = {.rx = -1},
                [InputKeyRight] = {.rx = 1},
                [InputKeyOk] = {.buttons = BTN_A},
                [InputKeyBack] = {.buttons = BTN_B},
            },
    },
    {
        .name = "Face",
        .hint1 = "U:X D:B L:Y R:A",
        .hint2 = "OK: +   Back: -",
        .map =
            {
                [InputKeyUp] = {.buttons = BTN_X},
                [InputKeyDown] = {.buttons = BTN_B},
                [InputKeyLeft] = {.buttons = BTN_Y},
                [InputKeyRight] = {.buttons = BTN_A},
                [InputKeyOk] = {.buttons = BTN_PLUS},
                [InputKeyBack] = {.buttons = BTN_MINUS},
            },
    },
    {
        .name = "Shoulder",
        .hint1 = "U:ZL D:ZR L:L R:R",
        .hint2 = "OK:Home Back:Capture",
        .map =
            {
                [InputKeyUp] = {.buttons = BTN_ZL},
                [InputKeyDown] = {.buttons = BTN_ZR},
                [InputKeyLeft] = {.buttons = BTN_L},
                [InputKeyRight] = {.buttons = BTN_R},
                [InputKeyOk] = {.buttons = BTN_HOME},
                [InputKeyBack] = {.buttons = BTN_CAPTURE},
            },
    },
    {
        /* The three buttons in a row at the top centre of the controller:
         * Switch: - Home +   DualSense: Create / PS / Options   Xbox: View / Xbox / Menu */
        .name = "Home",
        .hint1 = "L:Minus R:Plus",
        .hint2 = "OK: Home",
        .map =
            {
                [InputKeyLeft] = {.buttons = BTN_MINUS},
                [InputKeyRight] = {.buttons = BTN_PLUS},
                [InputKeyOk] = {.buttons = BTN_HOME},
            },
    },
};
#define LAYOUT_COUNT (sizeof(layouts) / sizeof(layouts[0]))

/* ------------------------------------------------------------------ */
/* Fake SPI flash contents (Switch calibration etc.)                  */
/* ------------------------------------------------------------------ */

static const uint8_t spi_imu_cal[24] = {
    0xD3, 0xFF, 0xD5, 0xFF, 0x55, 0x01, 0x00, 0x40, 0x00, 0x40, 0x00, 0x40,
    0x19, 0x00, 0xDD, 0xFF, 0xDC, 0xFF, 0x3B, 0x34, 0x3B, 0x34, 0x3B, 0x34,
};

/* Center 0x800, +/-1500 (0x5DC) on both axes, 12-bit packed.
 * Left : max-above, center, min-below.  Right: center, min-below, max-above. */
static const uint8_t spi_stick_cal[18] = {
    0xDC,
    0xC5,
    0x5D,
    0x00,
    0x08,
    0x80,
    0xDC,
    0xC5,
    0x5D,
    0x00,
    0x08,
    0x80,
    0xDC,
    0xC5,
    0x5D,
    0xDC,
    0xC5,
    0x5D,
};

/* body, buttons, left grip, right grip */
static const uint8_t spi_colors[12] = {
    0x32,
    0x32,
    0x32,
    0xFF,
    0xFF,
    0xFF,
    0x32,
    0x32,
    0x32,
    0x32,
    0x32,
    0x32,
};

/* Sensor horizontal offsets (a real controller returns these 6 bytes at
 * 0x6080, right before the stick parameters; the host reads 0x18 bytes
 * from 0x6080 in one go) */
static const uint8_t spi_sensor_off[6] = {0x50, 0xFD, 0x00, 0x00, 0xC6, 0x0F};

static const uint8_t spi_stick_params[18] = {
    0x0F,
    0x30,
    0x61,
    0x96,
    0x30,
    0xF3,
    0xD4,
    0x14,
    0x54,
    0x41,
    0x15,
    0x54,
    0xC7,
    0x79,
    0x9C,
    0x33,
    0x36,
    0x63,
};

typedef struct {
    uint32_t addr;
    const uint8_t* data;
    uint8_t len;
} SpiRegion;

static const SpiRegion spi_regions[] = {
    {0x6020, spi_imu_cal, sizeof(spi_imu_cal)},
    {0x603D, spi_stick_cal, sizeof(spi_stick_cal)},
    {0x6050, spi_colors, sizeof(spi_colors)},
    {0x6080, spi_sensor_off, sizeof(spi_sensor_off)},
    {0x6086, spi_stick_params, sizeof(spi_stick_params)},
    {0x6098, spi_stick_params, sizeof(spi_stick_params)},
};

/* Anything not listed reads back as 0xFF (= "not set / no user calibration") */
static void spi_read(uint32_t addr, uint8_t len, uint8_t* out) {
    memset(out, 0xFF, len);
    for(size_t i = 0; i < sizeof(spi_regions) / sizeof(spi_regions[0]); i++) {
        const SpiRegion* r = &spi_regions[i];
        uint32_t start = addr > r->addr ? addr : r->addr;
        uint32_t a_end = addr + len;
        uint32_t r_end = r->addr + r->len;
        uint32_t end = a_end < r_end ? a_end : r_end;
        if(start < end) {
            memcpy(&out[start - addr], &r->data[start - r->addr], end - start);
        }
    }
}

/* ------------------------------------------------------------------ */
/* USB device side                                                    */
/* ------------------------------------------------------------------ */

static bool usb_ready(void) {
    return sw_usb.dev && sw_usb.configured && !sw_usb.suspended;
}

/* Puts the TX semaphore back to "free". Releasing an already-free binary
 * semaphore just returns an error, which is fine to ignore. */
static void usb_tx_reset(void) {
    if(sw_usb.tx_sem) furi_semaphore_release(sw_usb.tx_sem);
}

/* Sends one input report of len bytes. */
static bool usb_send(const uint8_t* data, uint8_t len) {
    if(!usb_ready()) return false;
    if(furi_semaphore_acquire(sw_usb.tx_sem, furi_ms_to_ticks(20)) != FuriStatusOk) return false;
    /* If the write is rejected there will be no TX-complete event, so give
     * the semaphore back ourselves or every later send would time out. */
    if(usbd_ep_write(sw_usb.dev, SW_EP_IN, data, len) < 0) {
        furi_semaphore_release(sw_usb.tx_sem);
        return false;
    }
    return true;
}

static void sw_ep_callback(usbd_device* dev, uint8_t event, uint8_t ep) {
    if(event == usbd_evt_eptx) {
        furi_semaphore_release(sw_usb.tx_sem);
    } else if(event == usbd_evt_eprx) {
        AppEvent ev;
        ev.type = EventUsbRx;
        int32_t n = usbd_ep_read(dev, ep, ev.rx.data, sizeof(ev.rx.data));
        if(n > 0 && sw_usb.queue) {
            ev.rx.len = (uint8_t)n;
            furi_message_queue_put(sw_usb.queue, &ev, 0);
        }
    }
}

static usbd_respond sw_ep_config(usbd_device* dev, uint8_t cfg) {
    uint8_t ep_size = sw_usb.profile ? sw_usb.profile->ep_size : SW_EP_SIZE;

    switch(cfg) {
    case 0:
        sw_usb.configured = false;
        usbd_ep_deconfig(dev, SW_EP_IN);
        usbd_ep_deconfig(dev, SW_EP_OUT);
        usbd_reg_endpoint(dev, SW_EP_IN, NULL);
        usbd_reg_endpoint(dev, SW_EP_OUT, NULL);
        usb_tx_reset(); /* any in-flight transfer is gone */
        return usbd_ack;
    case 1:
        usbd_ep_config(dev, SW_EP_IN, USB_EPTYPE_INTERRUPT, ep_size);
        usbd_ep_config(dev, SW_EP_OUT, USB_EPTYPE_INTERRUPT, ep_size);
        usbd_reg_endpoint(dev, SW_EP_IN, sw_ep_callback);
        usbd_reg_endpoint(dev, SW_EP_OUT, sw_ep_callback);
        usb_tx_reset(); /* start every configuration with a free TX slot */
        sw_usb.suspended = false;
        sw_usb.link_fresh = true;
        sw_usb.reconfigured = true;
        sw_usb.configured = true;
        return usbd_ack;
    default:
        return usbd_fail;
    }
}

static usbd_respond sw_control(usbd_device* dev, usbd_ctlreq* req, usbd_rqc_callback* callback) {
    UNUSED(callback);

    const Profile* pf = sw_usb.profile;
    if(!pf || !pf->report_desc) return usbd_fail; /* Xbox: no HID requests */

    /* HID class requests */
    if(((USB_REQ_RECIPIENT | USB_REQ_TYPE) & req->bmRequestType) ==
           (USB_REQ_INTERFACE | USB_REQ_CLASS) &&
       req->wIndex == 0) {
        switch(req->bRequest) {
        case HID_REQ_GET_REPORT: {
            /* DualSense hosts read a few feature reports at start-up */
            if(pf->type == CtrlDualSense && (req->wValue >> 8) == HID_REPORT_TYPE_FEATURE) {
                size_t len = 0;
                const uint8_t* d = ds_feature((uint8_t)(req->wValue & 0xFF), &len);
                if(d) {
                    dev->status.data_ptr = (void*)d;
                    dev->status.data_count = (uint16_t)(len < req->wLength ? len : req->wLength);
                    return usbd_ack;
                }
            }
            return usbd_fail;
        }
        case HID_REQ_SET_REPORT:
        case HID_REQ_SET_IDLE:
        case HID_REQ_SET_PROTOCOL:
            return usbd_ack;
        default:
            return usbd_fail;
        }
    }

    /* HID + report descriptors */
    if(((USB_REQ_RECIPIENT | USB_REQ_TYPE) & req->bmRequestType) ==
           (USB_REQ_INTERFACE | USB_REQ_STANDARD) &&
       req->wIndex == 0 && req->bRequest == USB_STD_GET_DESCRIPTOR) {
        switch(req->wValue >> 8) {
        case HID_DTYPE_HID:
            dev->status.data_ptr = (void*)pf->hid_desc;
            dev->status.data_count = sizeof(SwHidDescriptor);
            return usbd_ack;
        case HID_DTYPE_REPORT:
            dev->status.data_ptr = (void*)pf->report_desc;
            dev->status.data_count = pf->report_desc_len;
            return usbd_ack;
        default:
            return usbd_fail;
        }
    }
    return usbd_fail;
}

static void sw_usb_init(usbd_device* dev, FuriHalUsbInterface* intf, void* ctx) {
    UNUSED(intf);
    UNUSED(ctx);
    sw_usb.dev = dev;
    sw_usb.configured = false;
    sw_usb.suspended = false;
    usbd_reg_config(dev, sw_ep_config);
    usbd_reg_control(dev, sw_control);
    usbd_connect(dev, true);
}

static void sw_usb_deinit(usbd_device* dev) {
    sw_usb.configured = false;
    usbd_reg_config(dev, NULL);
    usbd_reg_control(dev, NULL);
    sw_usb.dev = NULL;
}

static void sw_usb_wakeup(usbd_device* dev) {
    UNUSED(dev);
    sw_usb.suspended = false;
    sw_usb.link_fresh = true;
}

static void sw_usb_suspend(usbd_device* dev) {
    UNUSED(dev);
    sw_usb.suspended = true;
}

static FuriHalUsbInterface sw_usb_if = {
    .init = sw_usb_init,
    .deinit = sw_usb_deinit,
    .wakeup = sw_usb_wakeup,
    .suspend = sw_usb_suspend,
    .dev_descr = (struct usb_device_descriptor*)&sw_dev_desc,
    .str_manuf_descr = (void*)&sw_str_manuf,
    .str_prod_descr = (void*)&sw_str_prod,
    .str_serial_descr = (void*)&sw_str_serial,
    .cfg_descr = (void*)&sw_cfg_desc,
};

static FuriHalUsbInterface ds_usb_if = {
    .init = sw_usb_init,
    .deinit = sw_usb_deinit,
    .wakeup = sw_usb_wakeup,
    .suspend = sw_usb_suspend,
    .dev_descr = (struct usb_device_descriptor*)&ds_dev_desc,
    .str_manuf_descr = (void*)&ds_str_manuf,
    .str_prod_descr = (void*)&ds_str_prod,
    .str_serial_descr = (void*)&ds_str_serial,
    .cfg_descr = (void*)&ds_cfg_desc,
};

static FuriHalUsbInterface xb_usb_if = {
    .init = sw_usb_init,
    .deinit = sw_usb_deinit,
    .wakeup = sw_usb_wakeup,
    .suspend = sw_usb_suspend,
    .dev_descr = (struct usb_device_descriptor*)&xb_dev_desc,
    .str_manuf_descr = (void*)&xb_str_manuf,
    .str_prod_descr = (void*)&xb_str_prod,
    .str_serial_descr = (void*)&xb_str_serial,
    .cfg_descr = (void*)&xb_cfg_desc,
};

/* ------------------------------------------------------------------ */
/* Controller profiles                                                */
/* ------------------------------------------------------------------ */

static const Profile profiles[CtrlCount] = {
    [CtrlSwitch] =
        {
            .type = CtrlSwitch,
            .title = "Switch Pro",
            .wait1 = "Plug into a Nintendo Switch",
            .wait2 = "or Computer to begin.",
            .usb_if = &sw_usb_if,
            .hid_desc = &sw_cfg_desc.hid,
            .report_desc = sw_report_desc,
            .report_desc_len = sizeof(sw_report_desc),
            .ep_size = SW_EP_SIZE,
            .needs_handshake = true,
            .confirm_south = false,
            .buttons = names_switch,
            .button_count = COUNT_OF(names_switch),
            .face = {"X", "A", "B", "Y"},
        },
    [CtrlDualSense] =
        {
            .type = CtrlDualSense,
            .title = "DualSense (PS5)",
            .wait1 = "Plug into a Computer",
            .wait2 = "to begin.",
            .usb_if = &ds_usb_if,
            .hid_desc = &ds_cfg_desc.hid,
            .report_desc = ds_report_desc,
            .report_desc_len = sizeof(ds_report_desc),
            .ep_size = SW_EP_SIZE,
            .needs_handshake = false,
            .confirm_south = true,
            .buttons = names_dualsense,
            .button_count = COUNT_OF(names_dualsense),
            .face = {"T", "O", "X", "S"}, /* triangle, circle, cross, square */
            .face_symbols = true,
        },
    [CtrlXbox] =
        {
            .type = CtrlXbox,
            .title = "Xbox",
            .wait1 = "Plug into a Computer",
            .wait2 = "to begin.",
            .usb_if = &xb_usb_if,
            .hid_desc = NULL,
            .report_desc = NULL,
            .report_desc_len = 0,
            .ep_size = XB_EP_SIZE,
            .needs_handshake = false,
            .confirm_south = true,
            .buttons = names_xbox,
            .button_count = COUNT_OF(names_xbox),
            .face = {"Y", "B", "A", "X"},
        },
};

/* ------------------------------------------------------------------ */
/* Controller state -> reports                                        */
/* ------------------------------------------------------------------ */

static void pack12(uint8_t* o, uint16_t a, uint16_t b) {
    o[0] = a & 0xFF;
    o[1] = ((a >> 8) & 0x0F) | ((b & 0x0F) << 4);
    o[2] = (b >> 4) & 0xFF;
}

/* Recomputes what is sent to the host from the current screen's input:
 * controller mode -> physical keys through the layout,
 * Ducky mode      -> the buttons held by the running script,
 * anywhere else   -> idle controller. */
static void pad_update(ControllerDucky* app) {
    uint32_t buttons = 0;
    int lx = 0, ly = 0, rx = 0, ry = 0;

    if(app->screen == ScreenController) {
        const Layout* l = &layouts[app->layout];
        for(size_t k = 0; k < InputKeyMAX; k++) {
            if(!app->key_down[k]) continue;
            buttons |= l->map[k].buttons;
            lx += l->map[k].lx;
            ly += l->map[k].ly;
            rx += l->map[k].rx;
            ry += l->map[k].ry;
        }
        /* PlayStation / Xbox "confirm" is the south button, not the east one */
        if(l->confirm_ab && app->profile && app->profile->confirm_south) {
            bool a = buttons & BTN_A;
            bool b = buttons & BTN_B;
            buttons &= ~(BTN_A | BTN_B);
            if(a) buttons |= BTN_B;
            if(b) buttons |= BTN_A;
        }
    } else if(app->screen == ScreenDucky) {
        uint32_t m = app->script_mask;
        buttons = m & SP_REPORT_MASK;
        if(m & SP_LS_LEFT) lx -= 1;
        if(m & SP_LS_RIGHT) lx += 1;
        if(m & SP_LS_UP) ly += 1;
        if(m & SP_LS_DOWN) ly -= 1;
        if(m & SP_RS_LEFT) rx -= 1;
        if(m & SP_RS_RIGHT) rx += 1;
        if(m & SP_RS_UP) ry += 1;
        if(m & SP_RS_DOWN) ry -= 1;
    }

    app->buttons = buttons;
    app->lx = lx > 1 ? 1 : (lx < -1 ? -1 : lx);
    app->ly = ly > 1 ? 1 : (ly < -1 ? -1 : ly);
    app->rx = rx > 1 ? 1 : (rx < -1 ? -1 : rx);
    app->ry = ry > 1 ? 1 : (ry < -1 ? -1 : ry);
}

/* One resting IMU sample as captured from a real controller: accel
 * (x, y, z) = (-1024, 0, 4096) i.e. ~1 g on Z, gyro (0, 0, 0). A 0x30
 * report carries three of these (bytes 13..48). */
static const uint8_t sw_imu_sample[12] = {
    0x00,
    0xFC,
    0x00,
    0x00,
    0x00,
    0x10, /* accel x / y / z (LE16) */
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00, /* gyro x / y / z (LE16) */
};

/* Fills the common header of the Switch 0x21 and 0x30 reports. The values
 * follow a capture of a real Pro Controller in wired mode. */
static void fill_common(ControllerDucky* app, uint8_t* r, uint8_t id) {
    uint32_t b = app->buttons & ~BTN_MUTE;

    memset(r, 0, 64);
    r[0] = id;
    r[1] = (uint8_t)(furi_get_tick() / 5);
    r[2] = 0x91; /* battery full + charging / powered via USB */
    r[3] = b & 0xFF;
    r[4] = ((b >> 8) & 0xFF) | 0x80; /* 0x80 = charging grip / wired, always set when wired */
    r[5] = (b >> 16) & 0xFF;

    int mag = (app->lx != 0 && app->ly != 0) ? STICK_DIAG : STICK_RANGE;
    pack12(&r[6], STICK_CENTER + app->lx * mag, STICK_CENTER + app->ly * mag);
    int rmag = (app->rx != 0 && app->ry != 0) ? STICK_DIAG : STICK_RANGE;
    pack12(&r[9], STICK_CENTER + app->rx * rmag, STICK_CENTER + app->ry * rmag);
    r[12] = 0x0A; /* vibrator input report (a real controller shows 8..12) */

    if(id == 0x30) {
        for(int i = 0; i < 3; i++)
            memcpy(&r[13 + i * 12], sw_imu_sample, sizeof(sw_imu_sample));
    }
}

/* D-pad bits -> HID hat value (0 = up, clockwise, 8 = neutral) */
static uint8_t dpad_hat(uint32_t b) {
    bool u = b & BTN_UP, d = b & BTN_DOWN, l = b & BTN_LEFT, rt = b & BTN_RIGHT;
    if(u && d) u = d = false;
    if(l && rt) l = rt = false;
    if(u) return rt ? 1 : (l ? 7 : 0);
    if(d) return rt ? 3 : (l ? 5 : 4);
    if(rt) return 2;
    if(l) return 6;
    return 8;
}

static uint8_t ds_axis(int dir, bool diag) {
    int mag = diag ? 90 : 127;
    return (uint8_t)(128 + dir * mag);
}

/* DualSense USB input report 0x01 (64 bytes) */
static void build_dualsense(ControllerDucky* app, uint8_t* r) {
    uint32_t b = app->buttons;
    bool diag = app->lx != 0 && app->ly != 0;
    bool rdiag = app->rx != 0 && app->ry != 0;

    memset(r, 0, 64);
    r[0] = 0x01;
    r[1] = ds_axis(app->lx, diag); /* left stick X */
    r[2] = ds_axis(-app->ly, diag); /* left stick Y (HID: down is larger) */
    r[3] = ds_axis(app->rx, rdiag); /* right stick X */
    r[4] = ds_axis(-app->ry, rdiag); /* right stick Y (HID: down is larger) */
    r[5] = (b & BTN_ZL) ? 0xFF : 0x00; /* L2 analog */
    r[6] = (b & BTN_ZR) ? 0xFF : 0x00; /* R2 analog */
    r[7] = app->report_seq++;

    uint8_t b0 = dpad_hat(b);
    if(b & BTN_Y) b0 |= 0x10; /* square */
    if(b & BTN_B) b0 |= 0x20; /* cross */
    if(b & BTN_A) b0 |= 0x40; /* circle */
    if(b & BTN_X) b0 |= 0x80; /* triangle */
    r[8] = b0;

    uint8_t b1 = 0;
    if(b & BTN_L) b1 |= 0x01; /* L1 */
    if(b & BTN_R) b1 |= 0x02; /* R1 */
    if(b & BTN_ZL) b1 |= 0x04; /* L2 */
    if(b & BTN_ZR) b1 |= 0x08; /* R2 */
    if(b & BTN_MINUS) b1 |= 0x10; /* create */
    if(b & BTN_PLUS) b1 |= 0x20; /* options */
    if(b & BTN_LSTICK) b1 |= 0x40; /* L3 */
    if(b & BTN_RSTICK) b1 |= 0x80; /* R3 */
    r[9] = b1;

    uint8_t b2 = 0;
    if(b & BTN_HOME) b2 |= 0x01; /* PS */
    if(b & BTN_CAPTURE) b2 |= 0x02; /* touchpad click */
    if(b & BTN_MUTE) b2 |= 0x04; /* mic mute */
    r[10] = b2;

    /* sensor timestamp (units of 1/3 us) */
    uint32_t ts = furi_get_tick() * 3000u;
    r[28] = ts & 0xFF;
    r[29] = (ts >> 8) & 0xFF;
    r[30] = (ts >> 16) & 0xFF;
    r[31] = (ts >> 24) & 0xFF;

    r[33] = 0x80; /* touch point 1: not touching */
    r[37] = 0x80; /* touch point 2: not touching */
    r[53] = 0x28; /* battery full */
}

/* Xbox 360 / XInput input report (20 bytes) */
static void build_xbox(ControllerDucky* app, uint8_t* r) {
    uint32_t b = app->buttons;

    memset(r, 0, 20);
    r[0] = 0x00; /* message type: input */
    r[1] = 0x14; /* length */

    uint8_t lo = 0;
    if(b & BTN_UP) lo |= 0x01;
    if(b & BTN_DOWN) lo |= 0x02;
    if(b & BTN_LEFT) lo |= 0x04;
    if(b & BTN_RIGHT) lo |= 0x08;
    if(b & BTN_PLUS) lo |= 0x10; /* menu (start) */
    if(b & BTN_MINUS) lo |= 0x20; /* view (back) */
    if(b & BTN_LSTICK) lo |= 0x40;
    if(b & BTN_RSTICK) lo |= 0x80;
    r[2] = lo;

    uint8_t hi = 0;
    if(b & BTN_L) hi |= 0x01; /* LB */
    if(b & BTN_R) hi |= 0x02; /* RB */
    if(b & BTN_HOME) hi |= 0x04; /* Xbox button */
    if(b & BTN_B) hi |= 0x10; /* A (south) */
    if(b & BTN_A) hi |= 0x20; /* B (east) */
    if(b & BTN_Y) hi |= 0x40; /* X (west) */
    if(b & BTN_X) hi |= 0x80; /* Y (north) */
    r[3] = hi;

    r[4] = (b & BTN_ZL) ? 0xFF : 0x00; /* LT */
    r[5] = (b & BTN_ZR) ? 0xFF : 0x00; /* RT */

    int mag = (app->lx != 0 && app->ly != 0) ? 23170 : 32767;
    int16_t x = (int16_t)(app->lx * mag);
    int16_t y = (int16_t)(app->ly * mag); /* XInput: up is positive */
    r[6] = x & 0xFF;
    r[7] = ((uint16_t)x >> 8) & 0xFF;
    r[8] = y & 0xFF;
    r[9] = ((uint16_t)y >> 8) & 0xFF;

    int rmag = (app->rx != 0 && app->ry != 0) ? 23170 : 32767;
    int16_t rx = (int16_t)(app->rx * rmag);
    int16_t ry = (int16_t)(app->ry * rmag);
    r[10] = rx & 0xFF;
    r[11] = ((uint16_t)rx >> 8) & 0xFF;
    r[12] = ry & 0xFF;
    r[13] = ((uint16_t)ry >> 8) & 0xFF;
    /* r[14..19] reserved */
}

/* Builds the next input report for the active controller, returns its length */
static uint8_t build_input_report(ControllerDucky* app, uint8_t* r) {
    switch(app->profile->type) {
    case CtrlDualSense:
        build_dualsense(app, r);
        return 64;
    case CtrlXbox:
        build_xbox(app, r);
        return 20;
    default:
        fill_common(app, r, 0x30);
        return 64;
    }
}

/* ---- Switch handshake and subcommands ---- */

static void send_subcmd_reply(
    ControllerDucky* app,
    uint8_t ack,
    uint8_t subcmd,
    const uint8_t* data,
    size_t len) {
    uint8_t r[64];
    fill_common(app, r, 0x21);
    r[13] = ack;
    r[14] = subcmd;
    if(len > sizeof(r) - 15) len = sizeof(r) - 15;
    if(data && len) memcpy(&r[15], data, len);
    usb_send(r, 64);
}

static void handle_subcommand(ControllerDucky* app, const uint8_t* rx) {
    uint8_t sub = rx[10];
    const uint8_t* arg = &rx[11];

    switch(sub) {
    case 0x01: { /* bluetooth manual pairing: the real controller acks with 0x81
                  * and a single 0x03 data byte */
        uint8_t d[1] = {0x03};
        send_subcmd_reply(app, 0x81, sub, d, sizeof(d));
        break;
    }
    case 0x02: { /* request device info */
        uint8_t d[12] = {0x03, 0x48, 0x03, 0x02};
        memcpy(&d[4], SW_MAC, 6); /* normal order here (reversed in the 0x80 0x01 reply) */
        d[10] = 0x01;
        d[11] = 0x01; /* use colors from SPI */
        send_subcmd_reply(app, 0x82, sub, d, sizeof(d));
        break;
    }
    case 0x04: /* trigger buttons elapsed time */
        send_subcmd_reply(app, 0x83, sub, NULL, 0);
        break;
    case 0x10: { /* SPI flash read */
        uint32_t addr = arg[0] | ((uint32_t)arg[1] << 8) | ((uint32_t)arg[2] << 16) |
                        ((uint32_t)arg[3] << 24);
        uint8_t len = arg[4];
        if(len > 0x1D) len = 0x1D;
        uint8_t d[5 + 0x1D];
        memcpy(d, arg, 4);
        d[4] = len;
        spi_read(addr, len, &d[5]);
        send_subcmd_reply(app, 0x90, sub, d, 5 + len);
        break;
    }
    case 0x21: { /* NFC/IR MCU config */
        static const uint8_t d[] = {0x01, 0x00, 0xFF, 0x00, 0x03, 0x00, 0x05, 0x01};
        send_subcmd_reply(app, 0xA0, sub, d, sizeof(d));
        break;
    }
    default:
        /* 0x03 report mode, 0x08, 0x22, 0x30 player lights, 0x38, 0x40 IMU,
         * 0x41, 0x48 vibration, ... : plain ACK */
        send_subcmd_reply(app, 0x80, sub, NULL, 0);
        break;
    }
}

static void handle_usb_rx(ControllerDucky* app, const uint8_t* rx, uint8_t len) {
    /* DualSense output reports and Xbox rumble packets are simply ignored */
    if(!app->profile || !app->profile->needs_handshake) return;
    if(len < 2) return;
    switch(rx[0]) {
    case 0x80: {
        uint8_t r[64] = {0};
        switch(rx[1]) {
        case 0x01: /* connection status + MAC */
            r[0] = 0x81;
            r[1] = 0x01;
            r[2] = 0x00;
            r[3] = 0x03; /* Pro Controller */
            /* a real controller sends its MAC reversed here */
            for(int i = 0; i < 6; i++)
                r[4 + i] = SW_MAC[5 - i];
            usb_send(r, 64);
            break;
        case 0x02: /* handshake */
        case 0x03: /* 3 Mbaud request (meaningless on USB) */
            r[0] = 0x81;
            r[1] = rx[1];
            usb_send(r, 64);
            break;
        case 0x04: /* HID-only mode, no timeout: start streaming */
            app->streaming = true;
            break;
        default:
            break;
        }
        break;
    }
    case 0x01: /* rumble + subcommand */
        if(len >= 11) handle_subcommand(app, rx);
        break;
    default: /* 0x10 = rumble only, ignored */
        break;
    }
}

static void pump_reports(ControllerDucky* app) {
    if(!app->profile) return;

    if(sw_usb.reconfigured) {
        sw_usb.reconfigured = false;
        app->streaming = false;
    }
    /* DualSense / Xbox hosts just poll: stream as soon as we are configured */
    if(!app->profile->needs_handshake) app->streaming = sw_usb.configured;
    if(!app->streaming || !usb_ready()) return;

    uint32_t now = furi_get_tick();
    if(now - app->last_tx < REPORT_INTERVAL_MS) return;

    uint8_t r[64];
    uint8_t len = build_input_report(app, r);
    if(usb_send(r, len)) app->last_tx = now;
}

/* ------------------------------------------------------------------ */
/* Screen switching                                                   */
/* ------------------------------------------------------------------ */

static void set_screen(ControllerDucky* app, Screen s) {
    app->screen = s;
    memset(app->key_down, 0, sizeof(app->key_down));
    memset(app->seen_press, 0, sizeof(app->seen_press));
    memset(app->consumed, 0, sizeof(app->consumed));
    memset(app->tapping, 0, sizeof(app->tapping));
    app->script_mask = 0;
    pad_update(app);
    view_port_update(app->view_port);
}

/* The host currently has us enumerated and is not suspended */
static bool host_recognized(void) {
#ifdef SP_SKIP_USB_WAIT
    return true;
#else
    return sw_usb.configured && !sw_usb.suspended;
#endif
}

/* True only once the host has (re)connected since the connection was last
 * declared lost. A stale "configured" flag left over from before the cable
 * was pulled does not count, so the wait screen really waits for the host. */
static bool usb_link_up(void) {
#ifdef SP_SKIP_USB_WAIT
    return true;
#else
    return sw_usb.configured && !sw_usb.suspended && sw_usb.link_fresh;
#endif
}

static const char* usb_status(const ControllerDucky* app) {
    return !host_recognized() ? "USB: not connected" :
           app->streaming     ? "USB: active" :
                                "USB: handshaking";
}

/* Stops emulating the controller: the Flipper's original USB configuration is
 * restored, so the host sees the controller disappear. No controller is
 * active afterwards (profile == NULL), so nothing is sent or answered until a
 * new one is picked. */
static void usb_release(ControllerDucky* app) {
    app->streaming = false;
    app->profile = NULL;
    if(app->usb_changed) {
        furi_hal_usb_set_config(app->usb_prev, NULL);
        app->usb_changed = false;
    }
    sw_usb.profile = NULL;
    sw_usb.configured = false;
    sw_usb.suspended = false;
    sw_usb.reconfigured = false;
    sw_usb.link_fresh = false;
}

/* Back out to the controller select screen and drop the USB connection.
 * Only for screens where a controller is active. lost = the host went away
 * (the select screen then says "Connection lost"). Used for every way of
 * leaving a controller: Back, the "Switch Controller" menu item and a lost
 * USB connection, so all of them stop the emulation the same way. */
static void goto_select(ControllerDucky* app, bool lost) {
    app->select_sel = (uint8_t)app->profile->type;
    app->conn_lost = lost;
    app->lost_pending = false;
    /* Screen first: the GUI thread redraws on its own schedule, and the
     * menu / wait / controller screens all read app->profile, which
     * usb_release() clears (and which can take a while to return). */
    set_screen(app, ScreenSelect);
    usb_release(app);
}

/* Makes the Flipper show up as the given controller. Picking the controller
 * that is already active just carries on. */
static void profile_activate(ControllerDucky* app, ControllerType t) {
    const Profile* pf = &profiles[t];
    app->select_sel = (uint8_t)t;
    app->menu_sel = MenuController; /* the menu always opens on "Controller Mode" */
    app->conn_lost = false;

    if(app->profile == pf) {
        set_screen(app, usb_link_up() ? ScreenMenu : ScreenWait);
        return;
    }

    app->profile = pf;
    app->streaming = false;
    app->last_tx = 0;
    sw_usb.profile = pf; /* before set_config: the USB callbacks read it */
    sw_usb.configured = false;
    sw_usb.reconfigured = false;

    if(!app->usb_changed) {
        furi_hal_usb_unlock();
        app->usb_changed = true;
    }
    if(!furi_hal_usb_set_config(pf->usb_if, NULL)) {
        FURI_LOG_E(TAG, "Could not switch USB mode");
        app->running = false;
        return;
    }
    set_screen(app, ScreenWait);
}

/* ------------------------------------------------------------------ */
/* Ducky scripts                                                      */
/* ------------------------------------------------------------------ */

/* Remembers the command being executed so the Ducky screen can show it.
 * Shows the line as written, trimmed. Called only for real actions (DELAY,
 * PRESS, HOLD, PUSH, RELEASE), never for REM, CONTROLLER or REPEAT lines. */
static void script_set_action(ControllerDucky* app, const char* text) {
    while(*text == ' ' || *text == '\t')
        text++;
    snprintf(app->script_action, sizeof(app->script_action), "%s", text);
    size_t len = strlen(app->script_action);
    while(len > 0 &&
          (app->script_action[len - 1] == ' ' || app->script_action[len - 1] == '\t' ||
           app->script_action[len - 1] == '\r' || app->script_action[len - 1] == '\n')) {
        app->script_action[--len] = '\0';
    }
    view_port_update(app->view_port);
}

static void script_wait(ControllerDucky* app, uint32_t ms) {
    app->script_until = furi_get_tick() + ms;
    app->script_busy = true;
}

static uint32_t parse_ms(const char* s) {
    int v = atoi(s);
    return v > 0 ? (uint32_t)v : 0;
}

/* Splits buf in place on whitespace, upper-casing as it goes. Returns the
 * number of tokens (at most max). */
static int split_upper(char* buf, char** tok, int max) {
    int n = 0;
    char* p = buf;
    while(*p && n < max) {
        while(*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')
            *p++ = '\0';
        if(!*p) break;
        tok[n++] = p;
        while(*p && *p != ' ' && *p != '\t' && *p != '\r' && *p != '\n') {
            if(*p >= 'a' && *p <= 'z') *p = (char)(*p - 32);
            p++;
        }
    }
    return n;
}

/* Maps the name used in a "CONTROLLER" header line to a controller type.
 * Returns -1 for an unknown name. The name must already be upper-case. */
static int controller_from_name(const char* s) {
    if(strcmp(s, "SWITCH") == 0 || strcmp(s, "SWITCHPRO") == 0 || strcmp(s, "PRO") == 0 ||
       strcmp(s, "NINTENDO") == 0)
        return CtrlSwitch;
    if(strcmp(s, "DUALSENSE") == 0 || strcmp(s, "PS5") == 0 || strcmp(s, "DS") == 0 ||
       strcmp(s, "PLAYSTATION") == 0)
        return CtrlDualSense;
    if(strcmp(s, "XBOX") == 0 || strcmp(s, "XINPUT") == 0) return CtrlXbox;
    return -1;
}

/* Reads the script's header: the first command (REM lines and blank lines
 * are skipped) may be "CONTROLLER name".
 *   returns >= 0 : the controller the script asks for
 *   returns -1   : no header
 *   returns -2   : a CONTROLLER header with a missing or unknown name
 * The caller rewinds the stream afterwards. */
static int script_header_controller(ControllerDucky* app) {
    char buf[96];
    char* tok[3];
    int guard = 64;

    while(guard-- > 0 && stream_read_line(app->script, app->script_line)) {
        snprintf(buf, sizeof(buf), "%s", furi_string_get_cstr(app->script_line));
        int n = split_upper(buf, tok, 3);
        if(n == 0 || strcmp(tok[0], "REM") == 0) continue;
        if(strcmp(tok[0], "CONTROLLER") != 0) return -1;
        if(n < 2) return -2;
        int c = controller_from_name(tok[1]);
        return c >= 0 ? c : -2;
    }
    return -1;
}

/* Checks that every REPEAT START has a matching REPEAT END and that loops do
 * not nest deeper than LOOP_MAX_DEPTH. Returns 0 if fine, otherwise the line
 * number of the problem, with a short message in *err. Line numbers count
 * every line (blank ones too), like the line counter while the script runs.
 * The caller rewinds the stream afterwards. */
static uint32_t script_check_loops(ControllerDucky* app, const char** err) {
    char buf[96];
    char* tok[3];
    uint32_t line_no = 0;
    uint32_t open_line[LOOP_MAX_DEPTH];
    int depth = 0;

    while(stream_read_line(app->script, app->script_line)) {
        line_no++;
        snprintf(buf, sizeof(buf), "%s", furi_string_get_cstr(app->script_line));
        int n = split_upper(buf, tok, 3);
        if(n < 2 || strcmp(tok[0], "REPEAT") != 0) continue;
        if(strcmp(tok[1], "START") == 0) {
            if(depth >= LOOP_MAX_DEPTH) {
                *err = "Loops nested too deep";
                return line_no;
            }
            open_line[depth++] = line_no;
        } else if(strcmp(tok[1], "END") == 0) {
            if(depth == 0) {
                *err = "END without START";
                return line_no;
            }
            depth--;
        }
    }
    if(depth > 0) {
        *err = "START without END";
        return open_line[depth - 1];
    }
    return 0;
}

/* Looks the names up in the selected controller's button table.
 * "ALL" stands for every button of that controller. */
static uint32_t parse_buttons(const ControllerDucky* app, char** tok, int n) {
    const ButtonName* names = app->profile->buttons;
    size_t count = app->profile->button_count;
    uint32_t mask = 0;

    for(int i = 0; i < n; i++) {
        if(strcmp(tok[i], "ALL") == 0) {
            uint32_t all = 0;
            for(size_t j = 0; j < count; j++)
                all |= names[j].mask;
            return all;
        }
        for(size_t j = 0; j < count; j++) {
            if(strcmp(tok[i], names[j].name) == 0) mask |= names[j].mask;
        }
    }
    return mask;
}

static void script_stop(ControllerDucky* app, ScriptState state) {
    if(app->script) {
        stream_free(app->script);
        app->script = NULL;
        furi_record_close(RECORD_STORAGE);
    }
    app->script_busy = false;
    app->script_clear = 0;
    app->script_tail = 0;
    app->script_repeat_left = 0;
    app->script_mask = 0;
    app->loop_depth = 0;
    app->script_action[0] = '\0';
    app->script_state = state;
    pad_update(app);
}

/* Ends the script with an error message on the Ducky screen */
static void script_fail(ControllerDucky* app, const char* msg) {
    snprintf(app->script_note, sizeof(app->script_note), "%s", msg);
    script_stop(app, ScriptError);
}

static void script_start(ControllerDucky* app, const char* name) {
    char path[128];
    snprintf(path, sizeof(path), SCRIPT_DIR "/%s", name);
    /* a rerun passes app->script_name itself: don't copy a string onto itself */
    if(name != app->script_name) {
        snprintf(app->script_name, sizeof(app->script_name), "%s", name);
    }
    app->script_note[0] = '\0';
    app->script_action[0] = '\0';

    app->script_line_no = 0;
    app->script_mask = 0;
    app->script_busy = false;
    app->script_clear = 0;
    app->script_tail = 0;
    app->script_repeat_left = 0;
    app->loop_depth = 0;
    furi_string_reset(app->script_last);

    Storage* storage = furi_record_open(RECORD_STORAGE);
    app->script = file_stream_alloc(storage);
    if(!file_stream_open(app->script, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        stream_free(app->script);
        app->script = NULL;
        furi_record_close(RECORD_STORAGE);
        app->script_state = ScriptFailed;
        return;
    }

    /* CONTROLLER header: refuse to run a script written for another controller */
    int need = script_header_controller(app);
    stream_rewind(app->script);
    if(need == -2 || (need >= 0 && need != (int)app->profile->type)) {
        if(need == -2) {
            snprintf(app->script_note, sizeof(app->script_note), "Bad CONTROLLER line");
        } else {
            snprintf(
                app->script_note, sizeof(app->script_note), "Needs: %s", profiles[need].title);
        }
        stream_free(app->script);
        app->script = NULL;
        furi_record_close(RECORD_STORAGE);
        app->script_state = ScriptWrongCtrl;
        return;
    }

    /* REPEAT START / END must pair up and not nest too deeply */
    const char* loop_err = NULL;
    uint32_t bad_line = script_check_loops(app, &loop_err);
    stream_rewind(app->script);
    if(bad_line) {
        snprintf(app->script_note, sizeof(app->script_note), "%s", loop_err);
        app->script_line_no = bad_line;
        stream_free(app->script);
        app->script = NULL;
        furi_record_close(RECORD_STORAGE);
        app->script_state = ScriptError;
        return;
    }

    app->script_state = app->streaming ? ScriptRunning : ScriptWaitHost;
}

/* Runs one script line. Never blocks: waits are scheduled with script_wait()
 * and the main loop keeps serving USB in the meantime. */
static void script_exec(ControllerDucky* app, const char* text, bool remember) {
    char buf[96];
    snprintf(buf, sizeof(buf), "%s", text);

    /* split in place on whitespace, upper-casing as we go */
    char* tok[8];
    int n = split_upper(buf, tok, 8);
    /* CONTROLLER is a header, it was checked when the script started */
    if(n == 0 || strcmp(tok[0], "REM") == 0 || strcmp(tok[0], "CONTROLLER") == 0) return;

    if(strcmp(tok[0], "REPEAT") == 0) {
        /* REPEAT START [n]: open a loop and remember where its body begins.
         * The START line has already been consumed, so the stream position
         * is the first line of the body. Loops nest: each START pushes a
         * frame, each END works on the innermost one. */
        if(n > 1 && strcmp(tok[1], "START") == 0) {
            if(app->loop_depth >= LOOP_MAX_DEPTH) { /* already checked at start, just in case */
                script_fail(app, "Loops nested too deep");
                return;
            }
            uint32_t times = n > 2 ? parse_ms(tok[2]) : 0;
            LoopFrame* f = &app->loops[app->loop_depth++];
            f->forever = (times == 0);
            f->left = times;
            f->pos = stream_tell(app->script);
            f->line = app->script_line_no;
            return;
        }
        /* REPEAT END: jump back to the start of the innermost loop, or close
         * it and fall through once its requested number of runs is done. */
        if(n > 1 && strcmp(tok[1], "END") == 0) {
            if(app->loop_depth == 0) return; /* stray END: ignore */
            LoopFrame* f = &app->loops[app->loop_depth - 1];
            if(!f->forever) {
                if(f->left > 0) f->left--;
                if(f->left == 0) {
                    app->loop_depth--;
                    return;
                }
            }
            if(stream_seek(app->script, (int32_t)f->pos, StreamOffsetFromStart)) {
                app->script_line_no = f->line;
            } else {
                app->loop_depth--;
            }
            return;
        }
        /* REPEAT n: run the previous command n more times */
        uint32_t times = n > 1 ? parse_ms(tok[1]) : 0;
        app->script_repeat_left = times > 10000 ? 10000 : (uint16_t)times;
        return;
    }
    if(remember) furi_string_set_str(app->script_last, text);

    if(strcmp(tok[0], "DELAY") == 0 && n > 1) {
        script_set_action(app, text);
        script_wait(app, parse_ms(tok[1]));
    } else if(strcmp(tok[0], "PRESS") == 0 && n > 1) {
        script_set_action(app, text);
        uint32_t m = parse_buttons(app, tok + 1, n - 1);
        app->script_mask |= m;
        app->script_clear = m;
        app->script_tail = TAP_MS;
        script_wait(app, TAP_MS);
    } else if(strcmp(tok[0], "HOLD") == 0 && n > 2) {
        script_set_action(app, text);
        uint32_t m = parse_buttons(app, tok + 1, n - 2);
        app->script_mask |= m;
        app->script_clear = m;
        script_wait(app, parse_ms(tok[n - 1]));
    } else if(strcmp(tok[0], "PUSH") == 0 && n > 1) {
        script_set_action(app, text);
        app->script_mask |= parse_buttons(app, tok + 1, n - 1);
    } else if(strcmp(tok[0], "RELEASE") == 0 && n > 1) {
        script_set_action(app, text);
        app->script_mask &= ~parse_buttons(app, tok + 1, n - 1);
    }
    pad_update(app);
}

static void script_step(ControllerDucky* app) {
    if(!app->script) return;

    /* Hold the script until the host is actually polling us */
    ScriptState want = app->streaming ? ScriptRunning : ScriptWaitHost;
    if(app->script_state != want) {
        app->script_state = want;
        view_port_update(app->view_port);
    }
    if(!app->streaming) return;

    uint32_t now = furi_get_tick();
    if(app->script_busy) {
        if((int32_t)(now - app->script_until) < 0) return;
        app->script_busy = false;
        if(app->script_clear) {
            app->script_mask &= ~app->script_clear;
            app->script_clear = 0;
            pad_update(app);
        }
        if(app->script_tail) {
            uint32_t tail = app->script_tail;
            app->script_tail = 0;
            script_wait(app, tail);
            return;
        }
    }

    uint32_t before = app->script_line_no;
    int guard = 64; /* never spend long here, USB must keep being served */
    while(app->script && !app->script_busy && guard-- > 0) {
        if(app->script_repeat_left > 0) {
            app->script_repeat_left--;
            script_exec(app, furi_string_get_cstr(app->script_last), false);
            continue;
        }
        if(!stream_read_line(app->script, app->script_line)) {
            script_stop(app, ScriptDone);
            break;
        }
        app->script_line_no++;
        script_exec(app, furi_string_get_cstr(app->script_line), true);
    }
    if(app->script_line_no != before || !app->script) view_port_update(app->view_port);
}

/* ------------------------------------------------------------------ */
/* Script picker                                                      */
/* ------------------------------------------------------------------ */

static void picker_scan(ControllerDucky* app) {
    app->picker_count = 0;
    app->picker_sel = 0;

    Storage* storage = furi_record_open(RECORD_STORAGE);

    /* make sure the scripts folder exists (APP_DATA_PATH maps to
     * /ext/apps_data/<app id>/ on the SD card) */
    storage_simply_mkdir(storage, SCRIPT_DIR);

    File* f = storage_file_alloc(storage);
    if(storage_dir_open(f, SCRIPT_DIR)) {
        FileInfo info;
        char name[PICKER_NAME];
        while(app->picker_count < PICKER_MAX && storage_dir_read(f, &info, name, sizeof(name))) {
            if(file_info_is_dir(&info)) continue;
            size_t len = strlen(name);
            if(len > 4 && strcmp(name + len - 4, ".txt") == 0) {
                snprintf(app->picker_names[app->picker_count], PICKER_NAME, "%s", name);
                app->picker_count++;
            }
        }
        storage_dir_close(f);
    }
    storage_file_free(f);
    furi_record_close(RECORD_STORAGE);

    /* insertion sort */
    for(int i = 1; i < app->picker_count; i++) {
        char tmp[PICKER_NAME];
        memcpy(tmp, app->picker_names[i], PICKER_NAME);
        int j = i - 1;
        while(j >= 0 && strcmp(app->picker_names[j], tmp) > 0) {
            memcpy(app->picker_names[j + 1], app->picker_names[j], PICKER_NAME);
            j--;
        }
        memcpy(app->picker_names[j + 1], tmp, PICKER_NAME);
    }
}

/* ------------------------------------------------------------------ */
/* GUI                                                                */
/* ------------------------------------------------------------------ */

static void box(Canvas* c, int x, int y, int w, int h, bool filled) {
    if(filled)
        canvas_draw_box(c, x, y, w, h);
    else
        canvas_draw_frame(c, x, y, w, h);
}

/* PlayStation face button symbols, drawn with lines because the Flipper
 * fonts have no triangle / square glyphs. 'T' triangle, 'O' circle,
 * 'X' cross, 'S' square. Drawn in the current color. */
static void face_symbol(Canvas* c, int cx, int cy, char sym) {
    switch(sym) {
    case 'T':
        canvas_draw_line(c, cx, cy - 3, cx - 3, cy + 2);
        canvas_draw_line(c, cx, cy - 3, cx + 3, cy + 2);
        canvas_draw_line(c, cx - 3, cy + 2, cx + 3, cy + 2);
        break;
    case 'O':
        canvas_draw_circle(c, cx, cy, 3);
        break;
    case 'X':
        canvas_draw_line(c, cx - 2, cy - 2, cx + 2, cy + 2);
        canvas_draw_line(c, cx - 2, cy + 2, cx + 2, cy - 2);
        break;
    case 'S':
        canvas_draw_frame(c, cx - 2, cy - 2, 5, 5);
        break;
    default:
        break;
    }
}

static void face_button(Canvas* c, int cx, int cy, const char* label, bool pressed, bool symbols) {
    if(pressed) {
        canvas_draw_disc(c, cx, cy, 5);
        canvas_set_color(c, ColorWhite);
    } else {
        canvas_draw_circle(c, cx, cy, 5);
    }
    if(symbols) {
        face_symbol(c, cx, cy, label[0]);
    } else {
        canvas_set_font(c, FontSecondary);
        /* +1: sits a pixel lower so the text looks centered in the circle */
        canvas_draw_str_aligned(c, cx, cy + 1, AlignCenter, AlignCenter, label);
    }
    canvas_set_color(c, ColorBlack);
}

/* ring + a dot that follows the stick; fully filled when clicked */
static void stick(Canvas* c, int cx, int cy, int dx, int dy, bool clicked) {
    canvas_draw_circle(c, cx, cy, 7);
    if(clicked)
        canvas_draw_disc(c, cx, cy, 6);
    else
        canvas_draw_disc(c, cx + dx * 3, cy - dy * 3, 3);
}

static void draw_controller(Canvas* c, const ControllerDucky* app) {
    uint32_t b = app->buttons;
    const char* const* face = app->profile->face;

    /* body */
    canvas_draw_rframe(c, 2, 13, 124, 50, 12);

    /* shoulders: bumpers (L/R) and triggers (ZL/ZR) */
    box(c, 8, 9, 13, 4, b & BTN_L);
    box(c, 23, 9, 13, 4, b & BTN_ZL);
    box(c, 92, 9, 13, 4, b & BTN_ZR);
    box(c, 107, 9, 13, 4, b & BTN_R);

    /* minus / plus */
    box(c, 54, 21, 6, 2, b & BTN_MINUS);
    box(c, 68, 21, 6, 2, b & BTN_PLUS);

    /* home (PS / Xbox button), between minus and plus */
    if(b & BTN_HOME)
        canvas_draw_disc(c, 64, 22, 2);
    else
        canvas_draw_circle(c, 64, 22, 2);

    /* left stick */
    stick(c, 24, 30, app->lx, app->ly, b & BTN_LSTICK);

    /* d-pad (center 44,47) */
    const int cx = 44, cy = 47;
    box(c, cx - 3, cy - 10, 6, 7, b & BTN_UP);
    box(c, cx - 3, cy + 3, 6, 7, b & BTN_DOWN);
    box(c, cx - 10, cy - 3, 7, 6, b & BTN_LEFT);
    box(c, cx + 3, cy - 3, 7, 6, b & BTN_RIGHT);
    canvas_draw_frame(c, cx - 3, cy - 3, 6, 6);

    /* right stick */
    stick(c, 84, 47, app->rx, app->ry, b & BTN_RSTICK);

    /* face buttons: north, east, south, west */
    bool sym = app->profile->face_symbols;
    face_button(c, 104, 22, face[0], b & BTN_X, sym);
    face_button(c, 115, 33, face[1], b & BTN_A, sym);
    face_button(c, 104, 44, face[2], b & BTN_B, sym);
    face_button(c, 93, 33, face[3], b & BTN_Y, sym);

    /* layout name, centered */
    canvas_set_font(c, FontSecondary);
    canvas_draw_str_aligned(c, 64, 32, AlignCenter, AlignCenter, layouts[app->layout].name);
}

static void draw_select(Canvas* canvas, const ControllerDucky* app) {
    canvas_draw_icon(canvas, 2, 0, &I_icon);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 15, 10, app->conn_lost ? "Connection Lost" : "Select Controller");
    canvas_set_font(canvas, FontSecondary);

    for(int i = 0; i < CtrlCount; i++) {
        int top = 14 + i * 12;
        if(i == app->select_sel) {
            canvas_draw_box(canvas, 0, top, 128, 11);
            canvas_set_color(canvas, ColorWhite);
        }
        canvas_draw_str(canvas, 4, top + 9, profiles[i].title);
        canvas_set_color(canvas, ColorBlack);
    }

    canvas_draw_str(canvas, 2, 63, "OK: select  Back: exit");
}

static void draw_wait(Canvas* canvas, const ControllerDucky* app) {
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 4, AlignCenter, AlignTop, "Connect to Host");
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 22, AlignCenter, AlignTop, app->profile->wait1);
    canvas_draw_str_aligned(canvas, 64, 33, AlignCenter, AlignTop, app->profile->wait2);
    canvas_draw_str_aligned(canvas, 64, 47, AlignCenter, AlignTop, "Waiting for connection...");
    canvas_draw_str_aligned(canvas, 64, 63, AlignCenter, AlignBottom, "Back: controllers");
}

static void draw_menu(Canvas* canvas, const ControllerDucky* app) {
    static const char* const items[MenuCount] = {
        "Controller Mode", "Ducky Mode", "Switch Controller"};

    canvas_draw_icon(canvas, 2, 0, &I_icon);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 0, AlignCenter, AlignTop, app->profile->title);

    canvas_set_font(canvas, FontPrimary);
    for(int i = 0; i < MenuCount; i++) {
        int top = 17 + i * 11; /* block spans y 17..49, centered between title and status */
        if(i == app->menu_sel) {
            canvas_draw_box(canvas, 2, top, 124, 10);
            canvas_set_color(canvas, ColorWhite);
        }
        canvas_draw_str(canvas, 8, top + 8, items[i]);
        canvas_set_color(canvas, ColorBlack);
    }

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 63, usb_status(app));
}

static void draw_picker(Canvas* canvas, const ControllerDucky* app) {
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, "Select Script");
    canvas_set_font(canvas, FontSecondary);

    if(app->picker_count == 0) {
        canvas_draw_str(canvas, 2, 26, "No .txt scripts in:");
        canvas_draw_str(canvas, 2, 38, "/ext/apps_data/");
        canvas_draw_str(canvas, 2, 50, "controller_ducky/scripts");
        return;
    }

    int top_idx = app->picker_sel >= 4 ? app->picker_sel - 3 : 0;
    for(int r = 0; r < 4; r++) {
        int idx = top_idx + r;
        if(idx >= app->picker_count) break;
        int top = 14 + r * 12;
        if(idx == app->picker_sel) {
            canvas_draw_box(canvas, 0, top, 128, 11);
            canvas_set_color(canvas, ColorWhite);
        }
        canvas_draw_str(canvas, 4, top + 9, app->picker_names[idx]);
        canvas_set_color(canvas, ColorBlack);
    }
}

static void draw_ducky(Canvas* canvas, const ControllerDucky* app) {
    char buf[48];

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, "Ducky Mode");
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 22, app->script_name);

    if(app->script_state == ScriptWrongCtrl || app->script_state == ScriptError) {
        if(app->script_state == ScriptError) {
            snprintf(buf, sizeof(buf), "Error on line %lu", (unsigned long)app->script_line_no);
            canvas_draw_str(canvas, 2, 32, buf);
        } else {
            canvas_draw_str(canvas, 2, 32, script_state_names[app->script_state]);
        }
        canvas_draw_str(canvas, 2, 42, app->script_note);
        canvas_draw_str(canvas, 2, 62, "Back: script list");
        return;
    }

    snprintf(
        buf,
        sizeof(buf),
        "Line %lu - %s",
        (unsigned long)app->script_line_no,
        script_state_names[app->script_state]);
    canvas_draw_str(canvas, 2, 32, buf);

    FuriString* action =
        furi_string_alloc_printf("Action: %s", app->script_action[0] ? app->script_action : "-");
    elements_string_fit_width(canvas, action, 124);
    canvas_draw_str(canvas, 2, 44, furi_string_get_cstr(action));
    furi_string_free(action);

    bool active = app->script_state == ScriptRunning || app->script_state == ScriptWaitHost;
    canvas_draw_str(canvas, 2, 62, active ? "Back: stop" : "OK: rerun  Back: list");
}

static void draw_cb(Canvas* canvas, void* ctx) {
    ControllerDucky* app = ctx;

    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    /* every screen except select / picker reads app->profile */
    if(!app->profile && app->screen != ScreenSelect && app->screen != ScreenPicker) return;

    switch(app->screen) {
    case ScreenSelect:
        draw_select(canvas, app);
        break;
    case ScreenWait:
        draw_wait(canvas, app);
        break;
    case ScreenMenu:
        draw_menu(canvas, app);
        break;
    case ScreenController:
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 0, AlignCenter, AlignTop, "Hold Back: menu");
        draw_controller(canvas, app);
        break;
    case ScreenPicker:
        draw_picker(canvas, app);
        break;
    case ScreenDucky:
        draw_ducky(canvas, app);
        break;
    }
}

static void input_cb(InputEvent* event, void* ctx) {
    ControllerDucky* app = ctx;
    AppEvent ev = {.type = EventInput, .input = *event};
    furi_message_queue_put(app->queue, &ev, FuriWaitForever);
}

/* ------------------------------------------------------------------ */
/* Input handling per screen                                          */
/* ------------------------------------------------------------------ */

/* OK and Back double as "next layout" / "menu" on a long press, so they only
 * produce a short pulse when released without a long press having fired. */
static bool is_tap_key(InputKey key) {
    return key == InputKeyOk || key == InputKeyBack;
}

static void controller_input(ControllerDucky* app, const InputEvent* e) {
    InputKey k = e->key;

    if(e->type == InputTypePress) {
        app->seen_press[k] = true;
        if(is_tap_key(k)) {
            app->consumed[k] = false;
        } else {
            app->key_down[k] = true;
        }
    } else if(e->type == InputTypeRelease) {
        /* ignore releases of keys pressed before we got on this screen */
        if(!app->seen_press[k]) return;
        app->seen_press[k] = false;
        if(is_tap_key(k)) {
            if(!app->consumed[k]) {
                app->key_down[k] = true;
                app->tapping[k] = true;
                app->tap_until[k] = furi_get_tick() + TAP_MS;
            }
        } else {
            app->key_down[k] = false;
        }
    } else if(e->type == InputTypeLong) {
        if(k == InputKeyBack) {
            set_screen(app, ScreenMenu);
            return;
        }
        if(k == InputKeyOk) {
            app->consumed[InputKeyOk] = true;
            app->layout = (app->layout + 1) % LAYOUT_COUNT;
            memset(app->key_down, 0, sizeof(app->key_down));
            memset(app->tapping, 0, sizeof(app->tapping));
        }
    }
    pad_update(app);
}

static void select_input(ControllerDucky* app, const InputEvent* e) {
    if(e->type == InputTypePress || e->type == InputTypeRepeat) {
        if(e->key == InputKeyUp) {
            app->select_sel = (app->select_sel + CtrlCount - 1) % CtrlCount;
        } else if(e->key == InputKeyDown) {
            app->select_sel = (app->select_sel + 1) % CtrlCount;
        }
    } else if(e->type == InputTypeShort) {
        if(e->key == InputKeyOk) {
            profile_activate(app, (ControllerType)app->select_sel);
        } else if(e->key == InputKeyBack) {
            app->running = false;
        }
    }
}

static void menu_input(ControllerDucky* app, const InputEvent* e) {
    if(e->type == InputTypePress || e->type == InputTypeRepeat) {
        if(e->key == InputKeyUp) {
            app->menu_sel = (app->menu_sel + MenuCount - 1) % MenuCount;
        } else if(e->key == InputKeyDown) {
            app->menu_sel = (app->menu_sel + 1) % MenuCount;
        }
    } else if(e->type == InputTypeShort) {
        if(e->key == InputKeyOk) {
            switch(app->menu_sel) {
            case MenuController:
                set_screen(app, ScreenController);
                break;
            case MenuDucky:
                picker_scan(app);
                set_screen(app, ScreenPicker);
                break;
            default: /* "Switch Controller": back to the select screen, like Back */
                goto_select(app, false);
                break;
            }
        } else if(e->key == InputKeyBack) {
            goto_select(app, false);
        }
    }
}

static void picker_input(ControllerDucky* app, const InputEvent* e) {
    if(e->type == InputTypePress || e->type == InputTypeRepeat) {
        if(app->picker_count > 0) {
            if(e->key == InputKeyUp) {
                app->picker_sel = (app->picker_sel + app->picker_count - 1) % app->picker_count;
            } else if(e->key == InputKeyDown) {
                app->picker_sel = (app->picker_sel + 1) % app->picker_count;
            }
        }
    } else if(e->type == InputTypeShort) {
        if(e->key == InputKeyOk && app->picker_count > 0) {
            set_screen(app, ScreenDucky);
            script_start(app, app->picker_names[app->picker_sel]);
        } else if(e->key == InputKeyBack) {
            set_screen(app, ScreenMenu);
        }
    }
}

/* Ducky screen:
 *   running          Back stops the script (the screen stays)
 *   done / stopped   OK runs the script again, Back returns to the script list
 *   open failed      OK tries again, Back returns to the script list
 *   wrong controller Back returns to the script list
 *   script error     Back returns to the script list (fix the script first) */
static void ducky_input(ControllerDucky* app, const InputEvent* e) {
    if(e->type != InputTypeShort) return;

    bool active = app->script_state == ScriptRunning || app->script_state == ScriptWaitHost;

    if(e->key == InputKeyBack) {
        if(active) {
            script_stop(app, ScriptStopped);
        } else {
            set_screen(app, ScreenPicker);
        }
    } else if(
        e->key == InputKeyOk && !active && app->script_state != ScriptWrongCtrl &&
        app->script_state != ScriptError) {
        script_start(app, app->script_name);
    }
}

static void handle_input(ControllerDucky* app, const InputEvent* e) {
    if(e->key >= InputKeyMAX) return;

    switch(app->screen) {
    case ScreenSelect:
        select_input(app, e);
        break;
    case ScreenWait:
        if(e->type == InputTypeShort && e->key == InputKeyBack) {
            goto_select(app, false);
        }
        break;
    case ScreenMenu:
        menu_input(app, e);
        break;
    case ScreenController:
        controller_input(app, e);
        break;
    case ScreenPicker:
        picker_input(app, e);
        break;
    case ScreenDucky:
        ducky_input(app, e);
        break;
    }
    view_port_update(app->view_port);
}

/* Once a controller is up and running, losing the host (cable pulled, host
 * asleep or reset for longer than CONN_LOST_MS) sends the user back to the
 * select screen and stops emulating the controller completely, exactly like
 * backing out by hand (see goto_select): the USB connection is released and
 * nothing is sent until a controller is picked again. Picking one there
 * leads to the wait screen until the host is back. The USB stack reports
 * unplugging as a suspend event, the same way the Flipper's own USB HID app
 * notices it. */
static void check_connection(ControllerDucky* app) {
    bool watched = app->screen == ScreenMenu || app->screen == ScreenController ||
                   app->screen == ScreenPicker || app->screen == ScreenDucky;
    bool gone = !host_recognized() || sw_usb.suspended;

    if(!watched || !gone) {
        app->lost_pending = false;
        return;
    }
    uint32_t now = furi_get_tick();
    if(!app->lost_pending) {
        app->lost_pending = true;
        app->lost_since = now;
        return;
    }
    if(now - app->lost_since < CONN_LOST_MS) return;

    app->lost_pending = false;
    script_stop(app, ScriptStopped); /* releases held buttons, closes the file */
    goto_select(app, true); /* same teardown as Back, plus "Connection lost" */
}

/* Ends tap pulses whose time is up. */
static void expire_taps(ControllerDucky* app) {
    uint32_t now = furi_get_tick();
    bool changed = false;
    for(size_t k = 0; k < InputKeyMAX; k++) {
        if(app->tapping[k] && (int32_t)(now - app->tap_until[k]) >= 0) {
            app->tapping[k] = false;
            app->key_down[k] = false;
            changed = true;
        }
    }
    if(changed) {
        pad_update(app);
        view_port_update(app->view_port);
    }
}

/* ------------------------------------------------------------------ */
/* Entry point                                                        */
/* ------------------------------------------------------------------ */

int32_t controller_ducky_main(void* p) {
    UNUSED(p);

    ControllerDucky* app = malloc(sizeof(ControllerDucky));
    memset(app, 0, sizeof(ControllerDucky));
    app->running = true;
    app->screen = ScreenSelect;
    app->script_state = ScriptStopped;
    app->script_line = furi_string_alloc();
    app->script_last = furi_string_alloc();
    app->queue = furi_message_queue_alloc(16, sizeof(AppEvent));

    memset(&sw_usb, 0, sizeof(sw_usb));
    sw_usb.tx_sem = furi_semaphore_alloc(1, 1);
    sw_usb.queue = app->queue;

    app->view_port = view_port_alloc();
    view_port_draw_callback_set(app->view_port, draw_cb, app);
    view_port_input_callback_set(app->view_port, input_cb, app);
    app->gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);

    /* The USB mode is only replaced once a controller has been picked */
    app->usb_prev = furi_hal_usb_get_config();

    AppEvent ev;
    while(app->running) {
        if(furi_message_queue_get(app->queue, &ev, furi_ms_to_ticks(4)) == FuriStatusOk) {
            if(ev.type == EventInput) {
                handle_input(app, &ev.input);
            } else {
                handle_usb_rx(app, ev.rx.data, ev.rx.len);
            }
        }
        if(app->screen == ScreenWait && usb_link_up()) {
            set_screen(app, ScreenMenu);
        }
        check_connection(app);
        expire_taps(app);
        script_step(app);
        pump_reports(app);
    }

    script_stop(app, ScriptStopped);

    /* Restore the previous USB mode first so no more callbacks fire */
    if(app->usb_changed) furi_hal_usb_set_config(app->usb_prev, NULL);
    sw_usb.queue = NULL;

    gui_remove_view_port(app->gui, app->view_port);
    view_port_free(app->view_port);
    furi_record_close(RECORD_GUI);

    furi_semaphore_free(sw_usb.tx_sem);
    furi_message_queue_free(app->queue);
    furi_string_free(app->script_line);
    furi_string_free(app->script_last);
    free(app);
    return 0;
}
