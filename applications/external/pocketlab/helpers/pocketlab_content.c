#include <furi.h>

#include "pocketlab_content.h"

static const char* const track_names[PocketLabTrackCount] = {
    [PocketLabTrackBasics] = "Basics",
    [PocketLabTrackInfrared] = "Infrared",
    [PocketLabTrackSubGhz] = "Sub-GHz",
    [PocketLabTrackRfid] = "RFID 125 kHz",
    [PocketLabTrackNfc] = "NFC",
    [PocketLabTrackIbutton] = "iButton",
    [PocketLabTrackUsb] = "USB / HID",
    [PocketLabTrackGpio] = "GPIO",
    [PocketLabTrackBluetooth] = "Bluetooth",
    [PocketLabTrackSecurity] = "Security",
    [PocketLabTrackSystem] = "System",
};

const char* pocketlab_track_name(PocketLabTrack track) {
    return track < PocketLabTrackCount ? track_names[track] : "";
}

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
        .title = "Extra features are:",
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

static const PocketLabStep whatis_steps[] = {
    {
        .type = PocketLabStepText,
        .title = "Meet Flipper",
        .body = "Flipper Zero is a pocket multi-tool for radio, cards and hardware.",
    },
    {
        .type = PocketLabStepText,
        .title = "What it does",
        .body = "It reads, saves and replays signals from devices you own, and runs apps.",
    },
    {
        .type = PocketLabStepQuiz,
        .title = "Flipper Zero is a:",
        .correct = "Pocket multi-tool",
        .distractors = {"Game console", "Phone", "Router", "Camera", "Toy only"},
        .distractor_count = 5,
        .feedback_ok = "Right, a pocket multi-tool.",
        .feedback_no = "It is a pocket multi-tool.",
    },
    {.type = PocketLabStepReward},
};

static const PocketLabStep sdcard_steps[] = {
    {
        .type = PocketLabStepText,
        .title = "The SD card",
        .body = "Everything you save lives on the microSD card: keys, signals and apps.",
    },
    {
        .type = PocketLabStepText,
        .title = "Back it up",
        .body = "Copy the SD card to your PC now and then, so you never lose your saves.",
    },
    {
        .type = PocketLabStepQuiz,
        .title = "Your saves live on:",
        .correct = "The SD card",
        .distractors = {"The cloud", "Internal only", "A server", "RAM", "The battery"},
        .distractor_count = 5,
        .feedback_ok = "Yes, on the microSD card.",
        .feedback_no = "They live on the SD card.",
    },
    {
        .type = PocketLabStepTry,
        .title = "Try it",
        .body = "Open Settings, Storage to see your SD card usage.",
    },
    {.type = PocketLabStepReward},
};

static const PocketLabStep universal_steps[] = {
    {
        .type = PocketLabStepText,
        .title = "Universal remotes",
        .body = "Flipper ships with IR codes for thousands of TVs, ACs and projectors.",
    },
    {
        .type = PocketLabStepText,
        .title = "Power off anything",
        .body = "Cycle through codes to turn off a nearby TV, even without its remote.",
    },
    {
        .type = PocketLabStepQuiz,
        .title = "Universal remotes use:",
        .correct = "Infrared",
        .distractors = {"Wi-Fi", "Sub-GHz", "Bluetooth", "NFC", "GPS"},
        .distractor_count = 5,
        .feedback_ok = "Yes, infrared codes.",
        .feedback_no = "They use infrared.",
    },
    {
        .type = PocketLabStepTry,
        .title = "Try it",
        .body = "Open Infrared, Universal Remotes, TV, and press Power.",
    },
    {.type = PocketLabStepReward},
};

static const PocketLabStep freqanalyzer_steps[] = {
    {
        .type = PocketLabStepText,
        .title = "Find the frequency",
        .body = "The Frequency Analyzer finds which Sub-GHz frequency a remote uses.",
    },
    {
        .type = PocketLabStepText,
        .title = "Why it helps",
        .body = "Knowing the frequency lets you read the signal on the right band.",
    },
    {
        .type = PocketLabStepQuiz,
        .title = "The analyzer finds:",
        .correct = "The frequency",
        .distractors = {"The password", "The brand", "The distance", "The owner", "The battery"},
        .distractor_count = 5,
        .feedback_ok = "Right, the active frequency.",
        .feedback_no = "It finds the frequency.",
    },
    {
        .type = PocketLabStepTry,
        .title = "Try it",
        .body = "Open Sub-GHz, Frequency Analyzer, and hold your remote's button.",
    },
    {.type = PocketLabStepReward},
};

static const PocketLabStep subreplay_steps[] = {
    {
        .type = PocketLabStepText,
        .title = "Read a signal",
        .body = "Sub-GHz Read captures a nearby signal so you can save and study it.",
    },
    {
        .type = PocketLabStepText,
        .title = "Replay your own",
        .body = "Send a saved signal back to control your own gate or doorbell.",
    },
    {
        .type = PocketLabStepQuiz,
        .title = "Never replay:",
        .correct = "Others' devices",
        .distractors =
            {"Your own remote", "A test signal", "Your doorbell", "Your gate", "A saved file"},
        .distractor_count = 5,
        .feedback_ok = "Right, only your own devices.",
        .feedback_no = "Do not replay others' devices.",
    },
    {
        .type = PocketLabStepTry,
        .title = "Try it",
        .body = "Open Sub-GHz, Read, press your own remote, then Save.",
    },
    {.type = PocketLabStepReward},
};

static const PocketLabStep rfidsave_steps[] = {
    {
        .type = PocketLabStepText,
        .title = "Save a tag",
        .body = "After reading a 125 kHz tag you can save it under a name.",
    },
    {
        .type = PocketLabStepText,
        .title = "Emulate it",
        .body = "Flipper can pretend to be your saved tag for readers you own.",
    },
    {
        .type = PocketLabStepQuiz,
        .title = "125 kHz tags can be:",
        .correct = "Saved and emulated",
        .distractors = {"Charged", "Printed", "Called", "Painted", "Read only"},
        .distractor_count = 5,
        .feedback_ok = "Yes, saved and emulated.",
        .feedback_no = "They can be saved and emulated.",
    },
    {
        .type = PocketLabStepTry,
        .title = "Try it",
        .body = "Open 125 kHz RFID, Saved, pick a tag, then Emulate.",
    },
    {.type = PocketLabStepReward},
};

static const PocketLabStep mifare_steps[] = {
    {
        .type = PocketLabStepText,
        .title = "MIFARE Classic",
        .body = "A common 13.56 MHz card split into sectors, each locked by keys.",
    },
    {
        .type = PocketLabStepText,
        .title = "Keys and dictionary",
        .body = "Flipper tries a dictionary of common keys to read known sectors.",
    },
    {
        .type = PocketLabStepQuiz,
        .title = "MIFARE is split into:",
        .correct = "Sectors",
        .distractors = {"Frames", "Pixels", "Channels", "Tracks", "Pages only"},
        .distractor_count = 5,
        .feedback_ok = "Right, sectors locked by keys.",
        .feedback_no = "It is split into sectors.",
    },
    {
        .type = PocketLabStepText,
        .title = "Be ethical",
        .body = "Read only cards you own. Do not clone other people's cards.",
    },
    {
        .type = PocketLabStepTry,
        .title = "Try it",
        .body = "Open NFC, Read, and hold your own MIFARE card.",
    },
    {.type = PocketLabStepReward},
};

static const PocketLabStep ducky_steps[] = {
    {
        .type = PocketLabStepText,
        .title = "DuckyScript",
        .body = "Bad USB scripts use simple lines like STRING and ENTER.",
    },
    {
        .type = PocketLabStepText,
        .title = "Delays matter",
        .body = "Add DELAY so the target PC keeps up before the next keystroke.",
    },
    {
        .type = PocketLabStepQuiz,
        .title = "DuckyScript writes:",
        .correct = "Keystrokes",
        .distractors = {"Radio", "Images", "Sound", "Cards", "Firmware"},
        .distractor_count = 5,
        .feedback_ok = "Yes, keystrokes.",
        .feedback_no = "It writes keystrokes.",
    },
    {
        .type = PocketLabStepText,
        .title = "Consent first",
        .body = "Run scripts only on computers you own or may test.",
    },
    {
        .type = PocketLabStepTry,
        .title = "Try it",
        .body = "Open Bad USB, pick a demo script, plug into your own PC.",
    },
    {.type = PocketLabStepReward},
};

static const PocketLabStep pinlock_steps[] = {
    {
        .type = PocketLabStepText,
        .title = "Lock your Flipper",
        .body = "Set a PIN so nobody can use your Flipper if it is lost.",
    },
    {
        .type = PocketLabStepText,
        .title = "Safety first",
        .body = "A locked Flipper protects your saved keys and cards.",
    },
    {
        .type = PocketLabStepQuiz,
        .title = "A PIN lock protects:",
        .correct = "Your saved data",
        .distractors = {"The battery", "The screen", "The antenna", "The speaker", "The case"},
        .distractor_count = 5,
        .feedback_ok = "Right, your saved data.",
        .feedback_no = "It protects your saved data.",
    },
    {
        .type = PocketLabStepTry,
        .title = "Try it",
        .body = "Open Settings, Desktop, PIN Setup.",
    },
    {.type = PocketLabStepReward},
};

static const PocketLabStep navigation_steps[] = {
    {
        .type = PocketLabStepText,
        .title = "The buttons",
        .body = "Use the D-pad to move, OK to select, and Back to go up a level.",
    },
    {
        .type = PocketLabStepText,
        .title = "The main menu",
        .body = "Each icon opens a tool: Sub-GHz, RFID, NFC, Apps and more.",
    },
    {
        .type = PocketLabStepQuiz,
        .title = "The Back button:",
        .correct = "Goes up a level",
        .distractors = {"Turns off", "Deletes", "Opens apps", "Locks it", "Reboots"},
        .distractor_count = 5,
        .feedback_ok = "Right, Back goes up a level.",
        .feedback_no = "Back goes up a level.",
    },
    {.type = PocketLabStepReward},
};

static const PocketLabStep rolling_steps[] = {
    {
        .type = PocketLabStepText,
        .title = "Rolling codes",
        .body = "Modern car fobs change their code every press, so a replay will not work.",
    },
    {
        .type = PocketLabStepText,
        .title = "Why replay fails",
        .body = "The receiver expects a new code each time and rejects an old one.",
    },
    {
        .type = PocketLabStepQuiz,
        .title = "Rolling codes change:",
        .correct = "Every press",
        .distractors = {"Never", "Once a year", "On charge", "By color", "At night"},
        .distractor_count = 5,
        .feedback_ok = "Right, every press.",
        .feedback_no = "They change every press.",
    },
    {
        .type = PocketLabStepText,
        .title = "Stay legal",
        .body = "Do not attack cars. Study rolling codes in theory or on your own gear.",
    },
    {.type = PocketLabStepReward},
};

static const PocketLabStep cardtypes_steps[] = {
    {
        .type = PocketLabStepText,
        .title = "Card families",
        .body = "125 kHz tags come in types like EM4100 and HID Prox.",
    },
    {
        .type = PocketLabStepText,
        .title = "Different formats",
        .body = "Each stores its ID a bit differently, so Flipper labels the type.",
    },
    {
        .type = PocketLabStepQuiz,
        .title = "EM4100 is a:",
        .correct = "125 kHz tag",
        .distractors = {"NFC card", "Bank card", "SIM card", "QR code", "Barcode"},
        .distractor_count = 5,
        .feedback_ok = "Yes, a 125 kHz tag.",
        .feedback_no = "It is a 125 kHz tag.",
    },
    {
        .type = PocketLabStepTry,
        .title = "Try it",
        .body = "Open 125 kHz RFID, Read, and note the detected type.",
    },
    {.type = PocketLabStepReward},
};

static const PocketLabStep nfcemulate_steps[] = {
    {
        .type = PocketLabStepText,
        .title = "Emulate a card",
        .body = "Flipper can present a saved NFC card to a reader you own.",
    },
    {
        .type = PocketLabStepText,
        .title = "UID emulation",
        .body = "Simple readers check the card's UID, which Flipper can replay.",
    },
    {
        .type = PocketLabStepQuiz,
        .title = "Emulating means:",
        .correct = "Act as the card",
        .distractors = {"Print it", "Call it", "Charge it", "Delete it", "Scan it"},
        .distractor_count = 5,
        .feedback_ok = "Right, it acts as the card.",
        .feedback_no = "It acts as the card.",
    },
    {
        .type = PocketLabStepText,
        .title = "Be ethical",
        .body = "Emulate only your own cards, on your own readers.",
    },
    {
        .type = PocketLabStepTry,
        .title = "Try it",
        .body = "Open NFC, Saved, pick your card, then Emulate.",
    },
    {.type = PocketLabStepReward},
};

static const PocketLabStep ibtypes_steps[] = {
    {
        .type = PocketLabStepText,
        .title = "iButton families",
        .body = "Common types are Dallas, Cyfral and Metakom.",
    },
    {
        .type = PocketLabStepText,
        .title = "Regional use",
        .body = "Different intercom brands use different iButton types.",
    },
    {
        .type = PocketLabStepQuiz,
        .title = "A common iButton type:",
        .correct = "Dallas",
        .distractors = {"MIFARE", "EM4100", "U2F", "LoRa", "Zigbee"},
        .distractor_count = 5,
        .feedback_ok = "Yes, Dallas is common.",
        .feedback_no = "Dallas is a common type.",
    },
    {
        .type = PocketLabStepTry,
        .title = "Try it",
        .body = "Open iButton, Read, and note the detected type.",
    },
    {.type = PocketLabStepReward},
};

static const PocketLabStep jiggler_steps[] = {
    {
        .type = PocketLabStepText,
        .title = "Mouse Jiggler",
        .body = "Plugged into a PC, Flipper can nudge the mouse to keep it awake.",
    },
    {
        .type = PocketLabStepText,
        .title = "Handy uses",
        .body = "Stops the screen from sleeping during a long download or read.",
    },
    {
        .type = PocketLabStepQuiz,
        .title = "Mouse Jiggler keeps the:",
        .correct = "PC awake",
        .distractors = {"Fob charged", "Card warm", "Radio on", "Screen off", "App open"},
        .distractor_count = 5,
        .feedback_ok = "Right, it keeps the PC awake.",
        .feedback_no = "It keeps the PC awake.",
    },
    {
        .type = PocketLabStepTry,
        .title = "Try it",
        .body = "Open Bad USB, Mouse Jiggler, and plug into your own PC.",
    },
    {.type = PocketLabStepReward},
};

static const PocketLabStep wifiboard_steps[] = {
    {
        .type = PocketLabStepText,
        .title = "WiFi Dev Board",
        .body = "An ESP32 board on the GPIO adds Wi-Fi to your Flipper.",
    },
    {
        .type = PocketLabStepText,
        .title = "What it enables",
        .body = "With companion apps it scans and works with Wi-Fi networks.",
    },
    {
        .type = PocketLabStepQuiz,
        .title = "The WiFi board adds:",
        .correct = "Wi-Fi",
        .distractors = {"A battery", "A screen", "GPS", "A camera", "Storage"},
        .distractor_count = 5,
        .feedback_ok = "Yes, it adds Wi-Fi.",
        .feedback_no = "It adds Wi-Fi.",
    },
    {
        .type = PocketLabStepText,
        .title = "Use it right",
        .body = "Test only networks you own or are allowed to.",
    },
    {.type = PocketLabStepReward},
};

static const PocketLabStep sensors_steps[] = {
    {
        .type = PocketLabStepText,
        .title = "Read sensors",
        .body = "Over GPIO, Flipper can read sensors like temperature and humidity.",
    },
    {
        .type = PocketLabStepText,
        .title = "The I2C bus",
        .body = "Many sensors talk over I2C, a two-wire bus on the GPIO pins.",
    },
    {
        .type = PocketLabStepQuiz,
        .title = "I2C uses how many wires:",
        .correct = "Two",
        .distractors = {"One", "Four", "Eight", "Zero", "Ten"},
        .distractor_count = 5,
        .feedback_ok = "Right, two wires.",
        .feedback_no = "I2C uses two wires.",
    },
    {
        .type = PocketLabStepTry,
        .title = "Try it",
        .body = "Wire an I2C sensor and open a sensor app like Unitemp.",
    },
    {.type = PocketLabStepReward},
};

static const PocketLabStep bleremote_steps[] = {
    {
        .type = PocketLabStepText,
        .title = "BLE remote",
        .body = "Over Bluetooth, Flipper acts as a remote for your phone or PC.",
    },
    {
        .type = PocketLabStepText,
        .title = "Media and camera",
        .body = "Use it to skip tracks, control slides, or trigger the camera.",
    },
    {
        .type = PocketLabStepQuiz,
        .title = "BLE Remote controls:",
        .correct = "Your phone",
        .distractors = {"A car", "A gate", "A TV via IR", "A fob", "A card"},
        .distractor_count = 5,
        .feedback_ok = "Yes, your phone or PC.",
        .feedback_no = "It controls your phone or PC.",
    },
    {
        .type = PocketLabStepTry,
        .title = "Try it",
        .body = "Open Bluetooth Remote and pair with your phone.",
    },
    {.type = PocketLabStepReward},
};

static const PocketLabStep weather_steps[] = {
    {
        .type = PocketLabStepText,
        .title = "Weather sensors",
        .body = "Many wireless weather sensors broadcast on Sub-GHz bands.",
    },
    {
        .type = PocketLabStepText,
        .title = "Read the air",
        .body = "The Weather Station app decodes temperature and humidity nearby.",
    },
    {
        .type = PocketLabStepQuiz,
        .title = "Weather sensors use:",
        .correct = "Sub-GHz radio",
        .distractors = {"NFC", "Infrared", "Bluetooth", "Wi-Fi", "GPS"},
        .distractor_count = 5,
        .feedback_ok = "Right, Sub-GHz radio.",
        .feedback_no = "They use Sub-GHz radio.",
    },
    {
        .type = PocketLabStepTry,
        .title = "Try it",
        .body = "Open Sub-GHz, Weather Station, and wait for a reading.",
    },
    {.type = PocketLabStepReward},
};

const PocketLabLab pocketlab_labs[] = {
    {
        .id = "what-is-flipper",
        .title = "What is Flipper",
        .badge = "First Steps",
        .track = PocketLabTrackBasics,
        .xp = 20,
        .icon = {252, 258, 561, 561, 513, 561, 561, 561, 378, 252},
        .steps = whatis_steps,
        .step_count = COUNT_OF(whatis_steps),
    },
    {
        .id = "sd-card",
        .title = "SD Card & Backups",
        .badge = "Data Keeper",
        .track = PocketLabTrackBasics,
        .xp = 25,
        .icon = {254, 341, 257, 385, 257, 257, 257, 257, 511, 0},
        .steps = sdcard_steps,
        .step_count = COUNT_OF(sdcard_steps),
    },
    {
        .id = "ir-basics",
        .title = "IR Basics",
        .badge = "IR Initiate",
        .track = PocketLabTrackInfrared,
        .xp = 30,
        .icon = {0, 6, 38, 86, 86, 86, 38, 6, 0, 0},
        .steps = ir_basics_steps,
        .step_count = COUNT_OF(ir_basics_steps),
    },
    {
        .id = "ir-universal",
        .title = "Universal Remotes",
        .badge = "Remote Ninja",
        .track = PocketLabTrackInfrared,
        .xp = 35,
        .icon = {0, 6, 38, 86, 86, 86, 38, 6, 0, 0},
        .steps = universal_steps,
        .step_count = COUNT_OF(universal_steps),
    },
    {
        .id = "subghz-basics",
        .title = "Sub-GHz Basics",
        .badge = "RF Scout",
        .track = PocketLabTrackSubGhz,
        .xp = 40,
        .icon = {561, 306, 180, 693, 378, 48, 48, 48, 120, 252},
        .steps = subghz_steps,
        .step_count = COUNT_OF(subghz_steps),
    },
    {
        .id = "subghz-analyzer",
        .title = "Frequency Analyzer",
        .badge = "Signal Hunter",
        .track = PocketLabTrackSubGhz,
        .xp = 40,
        .icon = {561, 306, 180, 693, 378, 48, 48, 48, 120, 252},
        .steps = freqanalyzer_steps,
        .step_count = COUNT_OF(freqanalyzer_steps),
    },
    {
        .id = "subghz-replay",
        .title = "Read & Replay",
        .badge = "Replayer",
        .track = PocketLabTrackSubGhz,
        .xp = 40,
        .icon = {561, 306, 180, 693, 378, 48, 48, 48, 120, 252},
        .steps = subreplay_steps,
        .step_count = COUNT_OF(subreplay_steps),
    },
    {
        .id = "rfid-125",
        .title = "125 kHz RFID",
        .badge = "Tag Reader",
        .track = PocketLabTrackRfid,
        .xp = 40,
        .icon = {0, 510, 258, 282, 258, 322, 290, 510, 0, 0},
        .steps = rfid125_steps,
        .step_count = COUNT_OF(rfid125_steps),
    },
    {
        .id = "rfid-emulate",
        .title = "Save & Emulate",
        .badge = "Cloner",
        .track = PocketLabTrackRfid,
        .xp = 40,
        .icon = {0, 510, 258, 282, 258, 322, 290, 510, 0, 0},
        .steps = rfidsave_steps,
        .step_count = COUNT_OF(rfidsave_steps),
    },
    {
        .id = "nfc-basics",
        .title = "NFC Basics",
        .badge = "NFC Explorer",
        .track = PocketLabTrackNfc,
        .xp = 45,
        .icon = {128, 320, 672, 336, 680, 336, 672, 320, 128, 0},
        .steps = nfc_steps,
        .step_count = COUNT_OF(nfc_steps),
    },
    {
        .id = "nfc-mifare",
        .title = "MIFARE Classic",
        .badge = "Sector Sleuth",
        .track = PocketLabTrackNfc,
        .xp = 45,
        .icon = {128, 320, 672, 336, 680, 336, 672, 320, 128, 0},
        .steps = mifare_steps,
        .step_count = COUNT_OF(mifare_steps),
    },
    {
        .id = "ibutton",
        .title = "iButton Keys",
        .badge = "Key Master",
        .track = PocketLabTrackIbutton,
        .xp = 40,
        .icon = {384, 576, 576, 384, 64, 32, 16, 24, 20, 6},
        .steps = ibutton_steps,
        .step_count = COUNT_OF(ibutton_steps),
    },
    {
        .id = "bad-usb",
        .title = "Bad USB",
        .badge = "HID Novice",
        .track = PocketLabTrackUsb,
        .xp = 45,
        .icon = {252, 258, 717, 717, 513, 633, 330, 180, 120, 0},
        .steps = badusb_steps,
        .step_count = COUNT_OF(badusb_steps),
    },
    {
        .id = "usb-ducky",
        .title = "DuckyScript",
        .badge = "Ducky Writer",
        .track = PocketLabTrackUsb,
        .xp = 45,
        .icon = {252, 258, 717, 717, 513, 633, 330, 180, 120, 0},
        .steps = ducky_steps,
        .step_count = COUNT_OF(ducky_steps),
    },
    {
        .id = "gpio-uart",
        .title = "GPIO and UART",
        .badge = "Pin Wrangler",
        .track = PocketLabTrackGpio,
        .xp = 45,
        .icon = {146, 146, 146, 1023, 513, 341, 513, 1023, 0, 0},
        .steps = gpio_steps,
        .step_count = COUNT_OF(gpio_steps),
    },
    {
        .id = "bluetooth",
        .title = "Bluetooth LE",
        .badge = "BLE Tinkerer",
        .track = PocketLabTrackBluetooth,
        .xp = 40,
        .icon = {24, 40, 74, 52, 56, 52, 74, 40, 24, 0},
        .steps = ble_steps,
        .step_count = COUNT_OF(ble_steps),
    },
    {
        .id = "u2f",
        .title = "U2F and 2FA",
        .badge = "2FA Guardian",
        .track = PocketLabTrackSecurity,
        .xp = 45,
        .icon = {124, 130, 313, 297, 313, 146, 146, 68, 40, 16},
        .steps = u2f_steps,
        .step_count = COUNT_OF(u2f_steps),
    },
    {
        .id = "pin-lock",
        .title = "PIN Lock & Safety",
        .badge = "Guardian",
        .track = PocketLabTrackSecurity,
        .xp = 30,
        .icon = {120, 132, 132, 510, 258, 378, 330, 378, 510, 0},
        .steps = pinlock_steps,
        .step_count = COUNT_OF(pinlock_steps),
    },
    {
        .id = "ethics",
        .title = "Ethics and Law",
        .badge = "Responsible",
        .track = PocketLabTrackBasics,
        .xp = 30,
        .icon = {124, 130, 257, 265, 277, 265, 130, 130, 68, 56},
        .steps = ethics_steps,
        .step_count = COUNT_OF(ethics_steps),
    },
    {
        .id = "firmware-apps",
        .title = "Firmware and Apps",
        .badge = "Power User",
        .track = PocketLabTrackSystem,
        .xp = 35,
        .icon = {341, 510, 765, 258, 645, 258, 645, 510, 341, 0},
        .steps = firmware_steps,
        .step_count = COUNT_OF(firmware_steps),
    },
    {
        .id = "navigation",
        .title = "Navigation",
        .badge = "Navigator",
        .track = PocketLabTrackBasics,
        .xp = 20,
        .icon = {252, 258, 561, 561, 513, 561, 561, 561, 378, 252},
        .steps = navigation_steps,
        .step_count = COUNT_OF(navigation_steps),
    },
    {
        .id = "subghz-rolling",
        .title = "Rolling Codes",
        .badge = "Rolling Master",
        .track = PocketLabTrackSubGhz,
        .xp = 45,
        .icon = {561, 306, 180, 693, 378, 48, 48, 48, 120, 252},
        .steps = rolling_steps,
        .step_count = COUNT_OF(rolling_steps),
    },
    {
        .id = "subghz-weather",
        .title = "Weather Station",
        .badge = "Weather Watcher",
        .track = PocketLabTrackSubGhz,
        .xp = 35,
        .icon = {561, 306, 180, 693, 378, 48, 48, 48, 120, 252},
        .steps = weather_steps,
        .step_count = COUNT_OF(weather_steps),
    },
    {
        .id = "rfid-types",
        .title = "Card Types",
        .badge = "Tag Sorter",
        .track = PocketLabTrackRfid,
        .xp = 40,
        .icon = {0, 510, 258, 282, 258, 322, 290, 510, 0, 0},
        .steps = cardtypes_steps,
        .step_count = COUNT_OF(cardtypes_steps),
    },
    {
        .id = "nfc-emulate",
        .title = "Emulate NFC",
        .badge = "NFC Emulator",
        .track = PocketLabTrackNfc,
        .xp = 45,
        .icon = {128, 320, 672, 336, 680, 336, 672, 320, 128, 0},
        .steps = nfcemulate_steps,
        .step_count = COUNT_OF(nfcemulate_steps),
    },
    {
        .id = "ibutton-types",
        .title = "iButton Types",
        .badge = "Key Collector",
        .track = PocketLabTrackIbutton,
        .xp = 40,
        .icon = {384, 576, 576, 384, 64, 32, 16, 24, 20, 6},
        .steps = ibtypes_steps,
        .step_count = COUNT_OF(ibtypes_steps),
    },
    {
        .id = "usb-jiggler",
        .title = "Mouse Jiggler",
        .badge = "Jiggler",
        .track = PocketLabTrackUsb,
        .xp = 30,
        .icon = {252, 258, 717, 717, 513, 633, 330, 180, 120, 0},
        .steps = jiggler_steps,
        .step_count = COUNT_OF(jiggler_steps),
    },
    {
        .id = "gpio-wifi",
        .title = "WiFi Dev Board",
        .badge = "Board Builder",
        .track = PocketLabTrackGpio,
        .xp = 45,
        .icon = {146, 146, 146, 1023, 513, 341, 513, 1023, 0, 0},
        .steps = wifiboard_steps,
        .step_count = COUNT_OF(wifiboard_steps),
    },
    {
        .id = "gpio-sensors",
        .title = "Sensors & I2C",
        .badge = "Sensor Sage",
        .track = PocketLabTrackGpio,
        .xp = 45,
        .icon = {146, 146, 146, 1023, 513, 341, 513, 1023, 0, 0},
        .steps = sensors_steps,
        .step_count = COUNT_OF(sensors_steps),
    },
    {
        .id = "ble-remote",
        .title = "BLE Remote",
        .badge = "Remote Master",
        .track = PocketLabTrackBluetooth,
        .xp = 40,
        .icon = {24, 40, 74, 52, 56, 52, 74, 40, 24, 0},
        .steps = bleremote_steps,
        .step_count = COUNT_OF(bleremote_steps),
    },
};

const size_t pocketlab_labs_count = COUNT_OF(pocketlab_labs);
