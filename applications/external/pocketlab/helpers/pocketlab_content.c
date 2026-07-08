#include <furi.h>

#include "pocketlab_content.h"

static const PocketLabStep ir_basics_steps[] = {
    {
        .type = PocketLabStepText,
        .title = "Infrared",
        .body = "Your remote uses invisible infrared light. Flipper can learn and repeat it.",
    },
    {
        .type = PocketLabStepText,
        .title = "How it works",
        .body = "A remote blinks an LED in a pattern. Flipper reads it as a command.",
    },
    {
        .type = PocketLabStepQuiz,
        .title = "TV remote sends:",
        .correct = "Infrared light",
        .distractors = {"Radio waves", "Bluetooth", "Wi-Fi", "Ultrasound", "Microwaves"},
        .distractor_count = 5,
        .feedback_ok = "Yes, invisible IR light, not radio.",
        .feedback_no = "No, a TV remote sends infrared light.",
    },
    {
        .type = PocketLabStepTry,
        .title = "Try it",
        .body = "Open Infrared, Universal Remotes, TV. Aim at a TV and press Power.",
    },
    {.type = PocketLabStepReward},
};

static const PocketLabStep subghz_steps[] = {
    {
        .type = PocketLabStepText,
        .title = "Sub-GHz radio",
        .body = "Sub-GHz is low radio, like 433 MHz. Key fobs and sensors use it.",
    },
    {
        .type = PocketLabStepText,
        .title = "Read and replay",
        .body = "Flipper can read a signal and send it back, great for your own remotes.",
    },
    {
        .type = PocketLabStepQuiz,
        .title = "Common band is:",
        .correct = "433 MHz",
        .distractors = {"2.4 GHz", "60 Hz", "5 GHz", "50 kHz", "1 THz"},
        .distractor_count = 5,
        .feedback_ok = "Right, 433 MHz is a very common band.",
        .feedback_no = "No, 433 MHz is the typical one.",
    },
    {
        .type = PocketLabStepText,
        .title = "Stay legal",
        .body = "Capture only your own devices. Some bands are limited by law.",
    },
    {
        .type = PocketLabStepTry,
        .title = "Try it",
        .body = "Open Sub-GHz, Read, then press your own remote nearby.",
    },
    {.type = PocketLabStepReward},
};

static const PocketLabStep rfid125_steps[] = {
    {
        .type = PocketLabStepText,
        .title = "Low-freq RFID",
        .body = "125 kHz cards are simple ID tags. Office fobs often use EM4100.",
    },
    {
        .type = PocketLabStepText,
        .title = "Read your tag",
        .body = "Flipper reads the ID and can save or emulate your own card.",
    },
    {
        .type = PocketLabStepQuiz,
        .title = "125 kHz RFID is:",
        .correct = "Low frequency",
        .distractors = {"High frequency", "Optical", "Ultra high freq", "Audio band", "Microwave"},
        .distractor_count = 5,
        .feedback_ok = "Yes, 125 kHz is low frequency.",
        .feedback_no = "No, 125 kHz is low frequency.",
    },
    {
        .type = PocketLabStepTry,
        .title = "Try it",
        .body = "Open 125 kHz RFID, Read, and hold your fob to the back.",
    },
    {.type = PocketLabStepReward},
};

static const PocketLabStep nfc_steps[] = {
    {
        .type = PocketLabStepText,
        .title = "NFC cards",
        .body = "NFC works at 13.56 MHz. Transit and bank cards use it.",
    },
    {
        .type = PocketLabStepText,
        .title = "MIFARE",
        .body = "Many access cards are MIFARE. Flipper can read and study your own.",
    },
    {
        .type = PocketLabStepQuiz,
        .title = "NFC frequency:",
        .correct = "13.56 MHz",
        .distractors = {"125 kHz", "433 MHz", "2.4 GHz", "868 MHz", "27 MHz"},
        .distractor_count = 5,
        .feedback_ok = "Correct, NFC runs at 13.56 MHz.",
        .feedback_no = "No, NFC runs at 13.56 MHz.",
    },
    {
        .type = PocketLabStepText,
        .title = "Be ethical",
        .body = "Read only cards you own. Never clone someone else's card.",
    },
    {
        .type = PocketLabStepTry,
        .title = "Try it",
        .body = "Open NFC, Read, and hold your own card to the back.",
    },
    {.type = PocketLabStepReward},
};

static const PocketLabStep ibutton_steps[] = {
    {
        .type = PocketLabStepText,
        .title = "iButton keys",
        .body = "iButton is a metal contact key, used for intercoms and doors.",
    },
    {
        .type = PocketLabStepText,
        .title = "One wire",
        .body = "It talks over one wire. Flipper reads and emulates your own keys.",
    },
    {
        .type = PocketLabStepQuiz,
        .title = "iButton uses:",
        .correct = "Metal contact",
        .distractors = {"Radio", "Camera", "Infrared", "Magnetism", "Bluetooth"},
        .distractor_count = 5,
        .feedback_ok = "Right, it uses physical contact.",
        .feedback_no = "No, it is contact based.",
    },
    {
        .type = PocketLabStepTry,
        .title = "Try it",
        .body = "Open iButton, Read, and touch your key to the pins.",
    },
    {.type = PocketLabStepReward},
};

static const PocketLabStep badusb_steps[] = {
    {
        .type = PocketLabStepText,
        .title = "Bad USB",
        .body = "Plugged into a PC, Flipper can act as a keyboard and type fast.",
    },
    {
        .type = PocketLabStepText,
        .title = "Ducky scripts",
        .body = "Scripts send keystrokes, handy for setup and quick demos.",
    },
    {
        .type = PocketLabStepQuiz,
        .title = "Bad USB is a:",
        .correct = "Keyboard",
        .distractors = {"Mouse", "Printer", "Webcam", "Speaker", "Monitor"},
        .distractor_count = 5,
        .feedback_ok = "Yes, it acts as a keyboard (HID).",
        .feedback_no = "No, it acts as a keyboard.",
    },
    {
        .type = PocketLabStepText,
        .title = "Consent first",
        .body = "Run it only on computers you own or are allowed to test.",
    },
    {
        .type = PocketLabStepTry,
        .title = "Try it",
        .body = "Open Bad USB, pick a demo, and plug into your own PC.",
    },
    {.type = PocketLabStepReward},
};

static const PocketLabStep gpio_steps[] = {
    {
        .type = PocketLabStepText,
        .title = "GPIO pins",
        .body = "The top pins let Flipper talk to chips and external modules.",
    },
    {
        .type = PocketLabStepText,
        .title = "UART",
        .body = "UART is a simple serial link, great for ESP32 boards and sensors.",
    },
    {
        .type = PocketLabStepQuiz,
        .title = "GPIO lets you:",
        .correct = "Add modules",
        .distractors =
            {"Charge faster", "Play music", "Boost Wi-Fi", "Extend battery", "Unlock regions"},
        .distractor_count = 5,
        .feedback_ok = "Yes, you can add external modules.",
        .feedback_no = "No, GPIO connects modules.",
    },
    {
        .type = PocketLabStepTry,
        .title = "Try it",
        .body = "Open GPIO, USB-UART Bridge to see the serial console.",
    },
    {.type = PocketLabStepReward},
};

static const PocketLabStep ble_steps[] = {
    {
        .type = PocketLabStepText,
        .title = "Bluetooth LE",
        .body = "Flipper has BLE. It can act as a remote for your phone or PC.",
    },
    {
        .type = PocketLabStepText,
        .title = "BLE remote",
        .body = "Use it as a media or camera remote over Bluetooth.",
    },
    {
        .type = PocketLabStepQuiz,
        .title = "Flipper BLE is:",
        .correct = "Remote control",
        .distractors = {"Wi-Fi router", "GPS", "Modem", "Speaker", "Charger"},
        .distractor_count = 5,
        .feedback_ok = "Yes, it works as a BLE remote.",
        .feedback_no = "No, it works as a BLE remote.",
    },
    {
        .type = PocketLabStepTry,
        .title = "Try it",
        .body = "Open Bluetooth Remote and pair it with your phone.",
    },
    {.type = PocketLabStepReward},
};

static const PocketLabStep u2f_steps[] = {
    {
        .type = PocketLabStepText,
        .title = "U2F keys",
        .body = "Flipper can act as a security key for two-factor login.",
    },
    {
        .type = PocketLabStepText,
        .title = "How it helps",
        .body = "Instead of typing a code, you confirm login with the device.",
    },
    {
        .type = PocketLabStepQuiz,
        .title = "U2F is used for:",
        .correct = "Secure login",
        .distractors =
            {"Fast charging", "Louder sound", "GPS tracking", "Faster Wi-Fi", "Screen mirror"},
        .distractor_count = 5,
        .feedback_ok = "Right, it is for secure login.",
        .feedback_no = "No, it is for secure login.",
    },
    {
        .type = PocketLabStepTry,
        .title = "Try it",
        .body = "Open U2F and register it on a site that supports keys.",
    },
    {.type = PocketLabStepReward},
};

static const PocketLabStep ethics_steps[] = {
    {
        .type = PocketLabStepText,
        .title = "Use it right",
        .body = "Flipper is a tool. Use it on your own devices only.",
    },
    {
        .type = PocketLabStepText,
        .title = "Know the rules",
        .body = "Scanning or copying other people's cards can be illegal.",
    },
    {
        .type = PocketLabStepQuiz,
        .title = "Best practice:",
        .correct = "Only your own",
        .distractors =
            {"Test anything", "Ask nobody", "Copy any card", "Share all keys", "Ignore the law"},
        .distractor_count = 5,
        .feedback_ok = "Yes, work on your own devices only.",
        .feedback_no = "No, work on your own devices only.",
    },
    {.type = PocketLabStepReward},
};

static const PocketLabStep firmware_steps[] = {
    {
        .type = PocketLabStepText,
        .title = "Apps catalog",
        .body = "Add features with Apps. Browse the catalog in the mobile app.",
    },
    {
        .type = PocketLabStepText,
        .title = "Firmware",
        .body = "Firmware is the system. Keep it updated for new features.",
    },
    {
        .type = PocketLabStepQuiz,
        .title = "Extra features:",
        .correct = "Apps (.fap)",
        .distractors = {"Stickers", "Batteries", "Cables", "Screen film", "Cases"},
        .distractor_count = 5,
        .feedback_ok = "Yes, features come as installable apps.",
        .feedback_no = "No, they come as installable apps.",
    },
    {
        .type = PocketLabStepTry,
        .title = "Try it",
        .body = "Open Apps, browse the list, and launch one you like.",
    },
    {.type = PocketLabStepReward},
};

const PocketLabLab pocketlab_labs[] = {
    {
        .id = "ir-basics",
        .title = "IR Basics",
        .badge = "IR Initiate",
        .xp = 30,
        .icon = {0, 6, 38, 86, 86, 86, 38, 6, 0, 0},
        .steps = ir_basics_steps,
        .step_count = COUNT_OF(ir_basics_steps),
    },
    {
        .id = "subghz-basics",
        .title = "Sub-GHz Basics",
        .badge = "RF Scout",
        .xp = 40,
        .icon = {561, 306, 180, 693, 378, 48, 48, 48, 120, 252},
        .steps = subghz_steps,
        .step_count = COUNT_OF(subghz_steps),
    },
    {
        .id = "rfid-125",
        .title = "125 kHz RFID",
        .badge = "Tag Reader",
        .xp = 40,
        .icon = {0, 510, 258, 282, 258, 322, 290, 510, 0, 0},
        .steps = rfid125_steps,
        .step_count = COUNT_OF(rfid125_steps),
    },
    {
        .id = "nfc-basics",
        .title = "NFC Basics",
        .badge = "NFC Explorer",
        .xp = 45,
        .icon = {128, 320, 672, 336, 680, 336, 672, 320, 128, 0},
        .steps = nfc_steps,
        .step_count = COUNT_OF(nfc_steps),
    },
    {
        .id = "ibutton",
        .title = "iButton Keys",
        .badge = "Key Master",
        .xp = 40,
        .icon = {384, 576, 576, 384, 64, 32, 16, 24, 20, 6},
        .steps = ibutton_steps,
        .step_count = COUNT_OF(ibutton_steps),
    },
    {
        .id = "bad-usb",
        .title = "Bad USB",
        .badge = "HID Novice",
        .xp = 45,
        .icon = {252, 258, 717, 717, 513, 633, 330, 180, 120, 0},
        .steps = badusb_steps,
        .step_count = COUNT_OF(badusb_steps),
    },
    {
        .id = "gpio-uart",
        .title = "GPIO and UART",
        .badge = "Pin Wrangler",
        .xp = 45,
        .icon = {146, 146, 146, 1023, 513, 341, 513, 1023, 0, 0},
        .steps = gpio_steps,
        .step_count = COUNT_OF(gpio_steps),
    },
    {
        .id = "bluetooth",
        .title = "Bluetooth LE",
        .badge = "BLE Tinkerer",
        .xp = 40,
        .icon = {24, 40, 74, 52, 56, 52, 74, 40, 24, 0},
        .steps = ble_steps,
        .step_count = COUNT_OF(ble_steps),
    },
    {
        .id = "u2f",
        .title = "U2F and 2FA",
        .badge = "2FA Guardian",
        .xp = 45,
        .icon = {124, 130, 313, 297, 313, 146, 146, 68, 40, 16},
        .steps = u2f_steps,
        .step_count = COUNT_OF(u2f_steps),
    },
    {
        .id = "ethics",
        .title = "Ethics and Law",
        .badge = "Responsible",
        .xp = 30,
        .icon = {124, 130, 257, 265, 277, 265, 130, 130, 68, 56},
        .steps = ethics_steps,
        .step_count = COUNT_OF(ethics_steps),
    },
    {
        .id = "firmware-apps",
        .title = "Firmware and Apps",
        .badge = "Power User",
        .xp = 35,
        .icon = {341, 510, 765, 258, 645, 258, 645, 510, 341, 0},
        .steps = firmware_steps,
        .step_count = COUNT_OF(firmware_steps),
    },
};

const size_t pocketlab_labs_count = COUNT_OF(pocketlab_labs);
