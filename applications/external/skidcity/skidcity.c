/*
 * SKIDcity — Flipper Zero Educational App
 * "Don't be a SKID!"
 *
 * Every fake "hack" on the main menu either runs a harmless
 * Flipper demo or shows legal/educational information about
 * why that action is illegal or impossible.
 */

#include "skidcity.h"
#include <stdlib.h>
#include <string.h>

#define SKIDCITY_VERSION "1.2"

/* Scrolling header: full string rotates through a display window */
#define HEADER_STR  "   SKIDcity v" SKIDCITY_VERSION "  ~  Don't Be A SKID!  "
#define HEADER_WIN  22  /* characters visible at once */

/* ── Forward-declare all scene handlers via X-macro ── */
#define ADD_SCENE(module, name, id) \
    void module##_scene_##name##_on_enter(void*); \
    bool module##_scene_##name##_on_event(void*, SceneManagerEvent); \
    void module##_scene_##name##_on_exit(void*);
#include "skidcity_scene_config.h"
#undef ADD_SCENE

/* ── Scene handler tables ── */
static void (*const skidcity_on_enter_handlers[])(void*) = {
#define ADD_SCENE(module, name, id) module##_scene_##name##_on_enter,
#include "skidcity_scene_config.h"
#undef ADD_SCENE
};
static bool (*const skidcity_on_event_handlers[])(void*, SceneManagerEvent) = {
#define ADD_SCENE(module, name, id) module##_scene_##name##_on_event,
#include "skidcity_scene_config.h"
#undef ADD_SCENE
};
static void (*const skidcity_on_exit_handlers[])(void*) = {
#define ADD_SCENE(module, name, id) module##_scene_##name##_on_exit,
#include "skidcity_scene_config.h"
#undef ADD_SCENE
};
static const SceneManagerHandlers skidcity_scene_handlers = {
    .on_enter_handlers = skidcity_on_enter_handlers,
    .on_event_handlers = skidcity_on_event_handlers,
    .on_exit_handlers  = skidcity_on_exit_handlers,
    .scene_num         = SkidCitySceneCount,
};

/* ═══════════════════════════════════════════════════════
 * LED NOTIFICATION SEQUENCES
 * ═══════════════════════════════════════════════════════ */
/* ═══════════════════════════════════════════════════════
 * IR DEMO — NEC signal: Samsung TV Power (addr=0x07 cmd=0x02)
 * Pre-computed alternating mark/space durations in µs.
 * Even indices = mark (carrier on), odd = space (carrier off).
 * ═══════════════════════════════════════════════════════ */
static const uint32_t skidcity_ir_signal[] = {
    /* Leader */
    9000, 4500,
    /* Address 0x07 = 0000 0111 (LSB first) */
    560,560,  560,560,  560,560,  560,1690,  560,1690,  560,1690,  560,560,  560,560,
    /* ~Address 0xF8 = 1111 1000 (LSB first) */
    560,1690, 560,1690, 560,1690, 560,560,   560,560,   560,560,   560,1690, 560,1690,
    /* Command 0x02 = 0000 0010 (LSB first) */
    560,560,  560,1690, 560,560,  560,560,   560,560,   560,560,   560,560,  560,560,
    /* ~Command 0xFD = 1111 1101 (LSB first) */
    560,1690, 560,560,  560,1690, 560,1690,  560,1690,  560,1690,  560,1690, 560,1690,
    /* Stop bit */
    560
};
static const size_t skidcity_ir_signal_len =
    sizeof(skidcity_ir_signal) / sizeof(skidcity_ir_signal[0]);

/* ISR state — volatile stop flag is the only safe cross-thread signal */
typedef struct {
    size_t           pos;
    volatile bool    stop_requested;
} SkidCityIrTxState;
static SkidCityIrTxState skidcity_ir_tx_state;

/*
 * Signature must exactly match FuriHalInfraredTxGetDataISRCallback:
 *   FuriHalInfraredTxGetDataState (*)(void* ctx, uint32_t* duration, bool* level)
 *
 * STOP STRATEGY:
 * Calling furi_hal_infrared_async_tx_stop() while the ISR is actively
 * feeding pulses causes a race that freezes the Flipper. The only safe
 * way to stop is from WITHIN the ISR itself, between pulses.
 *
 * The main thread sets stop_requested = true on key release.
 * The ISR checks the flag at the inter-frame gap (between repeats) and
 * returns Done there — a safe, quiet moment with no carrier active.
 * The main thread then calls stop() AFTER the ISR has returned Done,
 * which is a no-op that completes instantly without a race.
 *
 * A 40ms silent gap is inserted between repeats (NEC inter-frame spacing).
 */
#define SKIDCITY_IR_GAP_US 40000UL

static FuriHalInfraredTxGetDataState
    skidcity_ir_isr_cb(void* ctx, uint32_t* duration, bool* level) {
    UNUSED(ctx);
    SkidCityIrTxState* s = &skidcity_ir_tx_state;

    /* Between frames: check stop flag — safe to return Done here */
    if(s->pos >= skidcity_ir_signal_len) {
        if(s->stop_requested) {
            *duration = 0;
            *level    = false;
            return FuriHalInfraredTxGetDataStateDone;
        }
        /* Not stopping: insert inter-frame gap then restart */
        s->pos    = 0;
        *duration = SKIDCITY_IR_GAP_US;
        *level    = false;
        return FuriHalInfraredTxGetDataStateOk;
    }

    *level    = (s->pos % 2 == 0); /* even index = mark (carrier on) */
    *duration = skidcity_ir_signal[s->pos++];
    return FuriHalInfraredTxGetDataStateOk;
}

/* ═══════════════════════════════════════════════════════
 * EDUCATIONAL TEXT CONTENT
 * ═══════════════════════════════════════════════════════ */
#define ABOUT_TRAFFIC \
    "THIS IS EXTREMELY\n" \
    "DANGEROUS.\n" \
    "Interfering with traffic\n" \
    "signals can kill people.\n\n" \
    "18 U.S.C. S1362 -\n" \
    "Malicious interference\n" \
    "with transport\n" \
    "infrastructure.\n" \
    "Federal crime.\n" \
    "State reckless\n" \
    "endangerment charges\n" \
    "stack on top.\n" \
    "People HAVE gone to\n" \
    "prison.\n\n" \
    "Traffic lights run on\n" \
    "closed wired NTCIP\n" \
    "networks - not Sub-GHz,\n" \
    "IR, or NFC. Flipper\n" \
    "cannot reach them.\n" \
    "The 'police strobe'\n" \
    "trick is a complete\n" \
    "urban legend.\n\n" \
    "LEGAL: Study ITS/NTCIP\n" \
    "specs, pursue a career\n" \
    "in traffic engineering,\n" \
    "or contact your city's\n" \
    "transportation dept."

#define ABOUT_WIFI \
    "18 U.S.C. S1030 - CFAA:\n" \
    "Unauthorized access.\n" \
    "First offense: 5 yrs.\n" \
    "With damage: 10 yrs.\n" \
    "Heavy fines on top.\n\n" \
    "Deauth, evil-twin, and\n" \
    "packet flooding attacks\n" \
    "destroy others' property.\n\n" \
    "Flipper cannot do most\n" \
    "of these without an\n" \
    "external WiFi dev board.\n\n" \
    "LEGAL: Use YOUR OWN\n" \
    "network. Earn CompTIA\n" \
    "Sec+, CEH, or OSCP."

#define ABOUT_CAR \
    "CAR KEY LAWS:\n" \
    "18 U.S.C. S1029 -\n" \
    "Access Device Fraud:\n" \
    "up to 15 yrs.\n" \
    "State vehicle theft +\n" \
    "burglary charges stack.\n" \
    "18 U.S.C. S2312 applies\n" \
    "if stolen vehicle crosses\n" \
    "state lines.\n\n" \
    "Modern fobs use ROLLING\n" \
    "CODES (KeeLoq, Hitag2,\n" \
    "DST40). Every press\n" \
    "generates a new one-time\n" \
    "cryptographic code.\n\n" \
    "Flipper CAN capture the\n" \
    "raw Sub-GHz signal.\n" \
    "It CANNOT break rolling\n" \
    "code crypto.\n" \
    "Replay = useless AND\n" \
    "risks de-syncing your\n" \
    "fob so it stops working.\n\n" \
    "Older FIXED-CODE systems\n" \
    "(some garage doors,\n" \
    "cheap legacy fobs):\n" \
    "replay CAN work.\n" \
    "That is theft or\n" \
    "criminal trespass.\n\n" \
    "REAL THREAT: RELAY\n" \
    "ATTACKS amplify your\n" \
    "key's passive signal to\n" \
    "unlock the car while the\n" \
    "key is inside your home.\n" \
    "Flipper can't do this -\n" \
    "needs two devices\n" \
    "+ range.\n\n" \
    "LEGAL: Research your OWN\n" \
    "vehicle RF system. Study\n" \
    "automotive cybersecurity."

#define ABOUT_CARDS \
    "18 U.S.C. S1029 -\n" \
    "Access Device Fraud:\n" \
    "up to 15 yrs +\n" \
    "$250,000 fine per count.\n" \
    "18 U.S.C. S1344 -\n" \
    "Bank Fraud: up to 30 yrs.\n" \
    "Often charged alongside.\n\n" \
    "EMV CHIP transactions\n" \
    "use a new cryptographic\n" \
    "token every time. Cannot\n" \
    "be cloned or replayed.\n\n" \
    "HOWEVER: the magstripe\n" \
    "on the same card CAN be\n" \
    "skimmed if a terminal\n" \
    "falls back to swipe mode.\n" \
    "Still fraud.\n\n" \
    "LEGAL: Read your OWN\n" \
    "NFC or transit card for\n" \
    "learning. Study\n" \
    "ISO/IEC 14443."

#define ABOUT_DOORS \
    "State criminal trespass\n" \
    "and burglary: 5-20+ yrs.\n" \
    "18 U.S.C. S1029 applies\n" \
    "to cloning access\n" \
    "devices.\n" \
    "18 U.S.C. S1030 (CFAA)\n" \
    "applies if the system is\n" \
    "networked to a protected\n" \
    "computer.\n\n" \
    "Modern RFID systems\n" \
    "(HID, MIFARE Plus,\n" \
    "DESFire) use AES-128\n" \
    "encryption + mutual\n" \
    "authentication.\n\n" \
    "Flipper CAN read legacy\n" \
    "125kHz cards (EM4100,\n" \
    "HID Prox). It CANNOT\n" \
    "crack AES-128 or forge\n" \
    "an authenticated session\n" \
    "on any modern system.\n\n" \
    "LEGAL: Lock sport on\n" \
    "locks YOU OWN. Only\n" \
    "pen-test with written\n" \
    "authorization."

#define ABOUT_TV \
    "Controlling your OWN TV\n" \
    "with IR is 100% legal.\n\n" \
    "WHERE IS THE IR LED?\n" \
    "It\'s the small clear\n" \
    "dome on the TOP edge\n" \
    "of your Flipper, NOT\n" \
    "the RGB LED on the front.\n\n" \
    "WHY CAN\'T I SEE IT?\n" \
    "IR light is ~940nm,\n" \
    "outside the visible\n" \
    "spectrum (380-700nm).\n" \
    "Human eyes can\'t\n" \
    "detect it at all.\n\n" \
    "TIP: Point a phone\n" \
    "camera at the top IR\n" \
    "LED and hold OK on the\n" \
    "demo screen. Most phone\n" \
    "cameras can see IR -\n" \
    "you\'ll see it pulse\n" \
    "purple/white on screen.\n\n" \
    "LEGAL USE: Build IR\n" \
    "remote databases, learn\n" \
    "NEC/RC5, automate\n" \
    "YOUR home setup."

#define ABOUT_AIRPLANE \
    "18 U.S.C. S32 -\n" \
    "Interfering with aircraft\n" \
    "systems. Federal crime.\n" \
    "Up to 20 yrs.\n\n" \
    "Flipper cannot transmit\n" \
    "on aviation bands:\n" \
    "VHF voice: 118-137 MHz\n" \
    "VOR/ILS: 108-118 MHz\n" \
    "This is the real barrier,\n" \
    "not just power level.\n\n" \
    "Even if it could reach\n" \
    "those bands, avionics\n" \
    "are shielded and\n" \
    "certified to reject\n" \
    "interference.\n\n" \
    "LEGAL: Get your ham\n" \
    "radio ticket! Study\n" \
    "Part 107 for drone\n" \
    "certification instead."

#define ABOUT_JAMMER \
    "47 U.S.C. S333 - FCC:\n" \
    "Jamming ANY signal is\n" \
    "ILLEGAL. $100k/day fine.\n" \
    "Federal prison possible.\n\n" \
    "This includes: cell,\n" \
    "GPS, WiFi, BLE, and\n" \
    "especially 911 services.\n\n" \
    "Flipper CANNOT jam cell\n" \
    "signals - wrong hardware\n" \
    "and wrong power level.\n\n" \
    "LEGAL: Get your ham\n" \
    "license. Build an\n" \
    "SDR receiver."

#define ABOUT_ATM \
    "18 U.S.C. S1029 -\n" \
    "Access Device Fraud:\n" \
    "up to 15 yrs.\n" \
    "18 U.S.C. S1030 - CFAA:\n" \
    "up to 10-20 yrs.\n" \
    "18 U.S.C. S1344 -\n" \
    "Bank Fraud: up to 30 yrs.\n" \
    "Almost always charged\n" \
    "alongside the others.\n\n" \
    "Modern ATMs use EMV +\n" \
    "TLS encryption. NFC\n" \
    "skimming does NOT work\n" \
    "on chip cards. Skimmers\n" \
    "need physical install\n" \
    "= even more crimes.\n\n" \
    "Flipper CAN read the NFC\n" \
    "layer of a card. It\n" \
    "CANNOT extract the EMV\n" \
    "private key or forge a\n" \
    "transaction token. The\n" \
    "data is useless without\n" \
    "the session crypto.\n\n" \
    "NOTE: magstripe can be\n" \
    "physically skimmed at\n" \
    "compromised terminals.\n" \
    "Not a Flipper attack,\n" \
    "but still very real.\n\n" \
    "LEGAL: Study for CISA,\n" \
    "CISSP, or bank certs."

#define ABOUT_RFJAM \
    "47 U.S.C. S333 + Part 15:\n" \
    "Sub-GHz jamming =\n" \
    "FCC felony.\n" \
    "$100,000 fine PER DAY.\n" \
    "Equipment seizure +\n" \
    "prison.\n\n" \
    "Sub-GHz jamming floods\n" \
    "a frequency band with\n" \
    "noise, blocking garage\n" \
    "doors, key fobs, alarm\n" \
    "sensors, and emergency\n" \
    "pagers.\n\n" \
    "Flipper Sub-GHz TX is\n" \
    "~10 mW max. Transmits\n" \
    "on allowed bands only.\n" \
    "It is NOT a true jammer.\n\n" \
    "LEGAL: Get your ham\n" \
    "license. Use SDR++ to\n" \
    "RECEIVE only. Study the\n" \
    "Sub-GHz protocols your\n" \
    "garage door actually\n" \
    "uses."

#define ABOUT_BLESPAM \
    "Unlike most items here,\n" \
    "Flipper CAN do BLE spam.\n" \
    "It has real BLE hardware.\n" \
    "That's exactly why you\n" \
    "need to know why NOT to.\n\n" \
    "LEGAL EXPOSURE:\n" \
    "18 U.S.C. S1030 (CFAA)\n" \
    "MAY apply if spam causes\n" \
    "damage or disruption -\n" \
    "courts are still setting\n" \
    "precedent.\n" \
    "21 U.S.C. S331 (FDA) +\n" \
    "assault charges apply if\n" \
    "medical devices are\n" \
    "harmed.\n\n" \
    "BLE spam can crash\n" \
    "devices, drain batteries,\n" \
    "and interfere with:\n" \
    "insulin pumps, pacemaker\n" \
    "monitors, hearing aids,\n" \
    "CGM sensors.\n\n" \
    "THAT IS NOT A JOKE.\n\n" \
    "LEGAL: Build real BLE\n" \
    "tools. Study Bluetooth\n" \
    "Core Spec. Write your\n" \
    "own GATT profiles for\n" \
    "YOUR OWN projects."

#define ABOUT_IRABUSE \
    "IR CAN be used illegally.\n" \
    "Most IR remotes have NO\n" \
    "authentication. Any\n" \
    "Flipper can send any IR\n" \
    "code to any device\n" \
    "in range.\n\n" \
    "ILLEGAL USES:\n" \
    "- TVs off in public\n" \
    "  (property interference)\n" \
    "- Disrupting digital\n" \
    "  signs or public\n" \
    "  displays\n" \
    "- Targeting someone\'s\n" \
    "  home devices without\n" \
    "  consent\n" \
    "- Interfering with\n" \
    "  IR medical or safety\n" \
    "  equipment\n\n" \
    "These range from civil\n" \
    "liability to criminal\n" \
    "mischief depending on\n" \
    "context + jurisdiction.\n\n" \
    "LEGAL: YOUR OWN devices.\n" \
    "Build IR databases.\n" \
    "Automate your own home.\n" \
    "With consent."

#define CFAA_BODY \
    "Computer Fraud &\n" \
    "Abuse Act\n" \
    "18 U.S.C. S1030\n\n" \
    "Prohibits accessing any\n" \
    "computer, device, or\n" \
    "network WITHOUT\n" \
    "authorization.\n\n" \
    "PENALTIES:\n" \
    "  First offense: 5 yrs\n" \
    "  With damage: 10 yrs\n" \
    "  With fraud:  20 yrs\n" \
    "  + civil suits on top\n\n" \
    "'I was just testing'\n" \
    "is NOT a legal defense.\n\n" \
    "USE YOUR FLIPPER\n" \
    "           LEGALLY.\n" \
    "It's an amazing tool."

#define APP_ABOUT_BODY \
    "SKIDcity v1.0\n" \
    "\"Don't be a SKID!\"\n\n" \
    "A Script Kiddie uses\n" \
    "tools without\n" \
    "understanding them or\n" \
    "the law. Don't be that.\n\n" \
    "Your Flipper Zero is a\n" \
    "POWERFUL learning tool:\n" \
    "  - Sub-GHz RF protocols\n" \
    "  - NFC / RFID research\n" \
    "  - Infrared databases\n" \
    "  - iButton / 1-Wire\n" \
    "  - GPIO & hardware\n" \
    "    hacking\n" \
    "  - BadUSB scripting\n\n" \
    "Every 'hack' in this\n" \
    "app has a LEGAL path.\n" \
    "Choose it.\n\n" \
    "Stay curious. Stay legal."

/* ═══════════════════════════════════════════════════════
 * FEATURE DATA TABLE
 * ═══════════════════════════════════════════════════════ */
const SkidCityFeatureInfo skidcity_features[SkidCityFeatureCount] = {
    [SkidCityFeatureTraffic] = {
        .menu_label     = "Hack Traffic Lights",
        .submenu_header = "Traffic Lights",
        .demo_item      = "Traffic Light Demo",
        .about_label    = "Danger & The Law",
        .about_title    = "Traffic Light Danger",
        .about_body     = ABOUT_TRAFFIC,
        .demo_type      = SkidCityDemoTrafficLed,
    },
    [SkidCityFeatureWifi] = {
        .menu_label     = "Crash WiFi Networks",
        .submenu_header = "WiFi Attacks",
        .demo_item      = "Launch Deauth Attack",
        .about_label    = "Why is this Illegal?",
        .about_title    = "WiFi Attacks & CFAA",
        .about_body     = ABOUT_WIFI,
        .demo_type      = SkidCityDemoBanned,
    },
    [SkidCityFeatureCar] = {
        .menu_label     = "Steal Car Keys",
        .submenu_header = "Car Key Attacks",
        .demo_item      = "Replay Attack",
        .about_label    = "Why is this Illegal?",
        .about_title    = "Car Key Attack Laws",
        .about_body     = ABOUT_CAR,
        .demo_type      = SkidCityDemoBanned,
    },
    [SkidCityFeatureCards] = {
        .menu_label     = "Clone Credit Cards",
        .submenu_header = "Payment Card Cloning",
        .demo_item      = "Clone Any Card",
        .about_label    = "Why is this Illegal?",
        .about_title    = "Payment Card Laws",
        .about_body     = ABOUT_CARDS,
        .demo_type      = SkidCityDemoBanned,
    },
    [SkidCityFeatureDoors] = {
        .menu_label     = "Bypass Smart Locks",
        .submenu_header = "Smart Lock Bypass",
        .demo_item      = "Open Any Lock",
        .about_label    = "Why is this Illegal?",
        .about_title    = "Lock Bypass Laws",
        .about_body     = ABOUT_DOORS,
        .demo_type      = SkidCityDemoBanned,
    },
    [SkidCityFeatureTv] = {
        .menu_label     = "Control Any TV",
        .submenu_header = "IR TV Control",
        .demo_item      = "IR Blast Demo",
        .about_label    = "Legal Uses & IR Facts",
        .about_title    = "IR: The Legal One!",
        .about_body     = ABOUT_TV,
        .demo_type      = SkidCityDemoIrBlink,
    },
    [SkidCityFeatureAirplane] = {
        .menu_label     = "Crash Airplane Systems",
        .submenu_header = "Aviation Systems",
        .demo_item      = "Interfere w/ Aircraft",
        .about_label    = "Why is this Illegal?",
        .about_title    = "Aviation Interference",
        .about_body     = ABOUT_AIRPLANE,
        .demo_type      = SkidCityDemoFcc,
    },
    [SkidCityFeatureJammer] = {
        .menu_label     = "Jam Cell/GPS Signals",
        .submenu_header = "Signal Jamming",
        .demo_item      = "Jam 4G/LTE/GPS",
        .about_label    = "Why is this Illegal?",
        .about_title    = "Signal Jamming Laws",
        .about_body     = ABOUT_JAMMER,
        .demo_type      = SkidCityDemoFcc,
    },
    [SkidCityFeatureAtm] = {
        .menu_label     = "Hack ATM / Bank",
        .submenu_header = "ATM / Bank Fraud",
        .demo_item      = "Skim ATM Cards",
        .about_label    = "Why is this Illegal?",
        .about_title    = "Bank Fraud Laws",
        .about_body     = ABOUT_ATM,
        .demo_type      = SkidCityDemoBanned,
    },
    [SkidCityFeatureRfJam] = {
        .menu_label     = "Jam Sub-GHz / RF",
        .submenu_header = "Sub-GHz RF Jamming",
        .demo_item      = "Jam Garage/Alarms",
        .about_label    = "Why is this Illegal?",
        .about_title    = "Sub-GHz Jamming Laws",
        .about_body     = ABOUT_RFJAM,
        .demo_type      = SkidCityDemoFcc,
    },
    [SkidCityFeatureBleSpam] = {
        .menu_label     = "Spam BLE / Bluetooth",
        .submenu_header = "Bluetooth LE Spam",
        .demo_item      = "Spam BLE Popups",
        .about_label    = "Why is this Illegal?",
        .about_title    = "BLE Spam: Why Not",
        .about_body     = ABOUT_BLESPAM,
        .demo_type      = SkidCityDemoBanned,
    },
    [SkidCityFeatureIrAbuse] = {
        .menu_label     = "Abuse IR Remotes",
        .submenu_header = "IR Abuse",
        .demo_item      = "IR Blast Demo",
        .about_label    = "Illegal IR Uses",
        .about_title    = "Illegal IR Uses",
        .about_body     = ABOUT_IRABUSE,
        .demo_type      = SkidCityDemoIrBlink,
    },
};

/* ═══════════════════════════════════════════════════════
 * HEADER SCROLL TIMER
 * ═══════════════════════════════════════════════════════ */
static void skidcity_header_timer_cb(void* context) {
    SkidCityApp* app = context;
    const char* src = HEADER_STR;
    size_t len = strlen(src);
    for(uint8_t i = 0; i < HEADER_WIN; i++) {
        app->header_buf[i] = src[(app->header_offset + i) % len];
    }
    app->header_buf[HEADER_WIN] = '\0';
    app->header_offset = (app->header_offset + 1) % len;
    submenu_set_header(app->submenu, app->header_buf);
}

/* ═══════════════════════════════════════════════════════
 * SHARED HELPERS
 * ═══════════════════════════════════════════════════════ */
static void skidcity_submenu_cb(void* context, uint32_t index) {
    SkidCityApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

static bool skidcity_custom_event_cb(void* context, uint32_t event) {
    SkidCityApp* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static bool skidcity_back_event_cb(void* context) {
    SkidCityApp* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

static void skidcity_set_led(SkidCityApp* app, uint8_t state) {
    UNUSED(app);
    switch(state) {
    case 0: /* red */
        furi_hal_light_set(LightRed,   0xFF);
        furi_hal_light_set(LightGreen, 0x00);
        furi_hal_light_set(LightBlue,  0x00);
        break;
    case 1: /* yellow */
        furi_hal_light_set(LightRed,   0xFF);
        furi_hal_light_set(LightGreen, 0xFF);
        furi_hal_light_set(LightBlue,  0x00);
        break;
    case 2: /* green */
        furi_hal_light_set(LightRed,   0x00);
        furi_hal_light_set(LightGreen, 0xFF);
        furi_hal_light_set(LightBlue,  0x00);
        break;
    default: /* off */
        furi_hal_light_set(LightRed,   0x00);
        furi_hal_light_set(LightGreen, 0x00);
        furi_hal_light_set(LightBlue,  0x00);
        break;
    }
}

/* ═══════════════════════════════════════════════════════
 * TRAFFIC-LIGHT CUSTOM VIEW
 * ═══════════════════════════════════════════════════════ */
static void skidcity_traffic_draw_cb(Canvas* canvas, void* model) {
    TrafficModel* m = model;
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    /* Info panel (right side) */
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 38, 12, "Traffic Demo");
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 38, 22, "Flipper LED = your");
    canvas_draw_str(canvas, 38, 31, "\"traffic light\".");
    canvas_draw_str(canvas, 38, 42, "UP/DOWN: cycle");
    canvas_draw_str(canvas, 38, 51, "BACK: exit");

    /* Traffic light housing */
    canvas_draw_rframe(canvas, 5, 3, 26, 57, 3);
    canvas_draw_box(canvas, 15, 0, 6, 4);
    canvas_draw_box(canvas, 15, 58, 6, 5);
    canvas_draw_line(canvas, 5, 23, 30, 23);
    canvas_draw_line(canvas, 5, 40, 30, 40);

    /* Red bulb */
    if(m->state == 0) canvas_draw_disc(canvas, 18, 13, 7);
    else              canvas_draw_circle(canvas, 18, 13, 7);

    /* Yellow bulb */
    if(m->state == 1) canvas_draw_disc(canvas, 18, 31, 7);
    else              canvas_draw_circle(canvas, 18, 31, 7);

    /* Green bulb */
    if(m->state == 2) canvas_draw_disc(canvas, 18, 49, 7);
    else              canvas_draw_circle(canvas, 18, 49, 7);

    /* State label */
    const char* labels[] = {"RED", "YELLOW", "GREEN"};
    canvas_set_font(canvas, FontPrimary);
    if(m->state < 3) {
        canvas_draw_str_aligned(canvas, 82, 58, AlignCenter, AlignBottom, labels[m->state]);
    }
}

static bool skidcity_traffic_input_cb(InputEvent* event, void* context) {
    if(event->type != InputTypeShort && event->type != InputTypeRepeat) return false;
    SkidCityApp* app = context;

    if(event->key == InputKeyBack) return false;

    if(event->key == InputKeyUp || event->key == InputKeyRight) {
        uint8_t ns;
        with_view_model(
            app->traffic_view, TrafficModel* model,
            { model->state = (model->state < 2) ? model->state + 1 : 0; ns = model->state; },
            true);
        skidcity_set_led(app, ns);
        return true;
    }
    if(event->key == InputKeyDown || event->key == InputKeyLeft) {
        uint8_t ns;
        with_view_model(
            app->traffic_view, TrafficModel* model,
            { model->state = (model->state > 0) ? model->state - 1 : 2; ns = model->state; },
            true);
        skidcity_set_led(app, ns);
        return true;
    }
    return false;
}

/* ═══════════════════════════════════════════════════════
 * IR DEMO CUSTOM VIEW
 * Shows instructions + "TRANSMITTING" status.
 * OK hold: fires real IR + flashes red LED.
 * ═══════════════════════════════════════════════════════ */
static void skidcity_ir_draw_cb(Canvas* canvas, void* model) {
    IrDemoModel* m = model;
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 0, AlignCenter, AlignTop, "IR Transmitter Demo");

    canvas_draw_line(canvas, 0, 12, 128, 12);

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 23, "The IR LED just blinked?");
    canvas_draw_str(canvas, 2, 33, "Just kidding. That's the");
    canvas_draw_str(canvas, 2, 43, "RGB LED, not the IR TX.");

    if(m->transmitting) {
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_rbox(canvas, 4, 50, 120, 13, 3);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_str_aligned(canvas, 64, 56, AlignCenter, AlignCenter, "*** TRANSMITTING ***");
    } else {
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_str_aligned(canvas, 64, 57, AlignCenter, AlignBottom, "Hold OK to transmit IR");
    }
}

/* Blink timer callback — toggles blue LED at ~8 Hz while transmitting */
static void skidcity_ir_blink_cb(void* context) {
    static bool state = false;
    UNUSED(context);
    state = !state;
    furi_hal_light_set(LightBlue, state ? 0xFF : 0x00);
}

static bool skidcity_ir_input_cb(InputEvent* event, void* context) {
    SkidCityApp* app = context;

    if(event->key == InputKeyOk && event->type == InputTypePress) {
        /* Guard: don't double-start */
        if(app->ir_tx_active) return true;
        skidcity_ir_tx_state.stop_requested = false;
        skidcity_ir_tx_state.pos            = 0;
        furi_hal_infrared_async_tx_set_data_isr_callback(skidcity_ir_isr_cb, NULL);
        furi_hal_infrared_async_tx_start(38000, 0.33f);
        app->ir_tx_active = true;
        /* Start blue LED blink timer (~8 Hz = 125 ms period) */
        app->ir_blink_timer =
            furi_timer_alloc(skidcity_ir_blink_cb, FuriTimerTypePeriodic, NULL);
        furi_timer_start(app->ir_blink_timer, 125);
        with_view_model(
            app->ir_view, IrDemoModel* model,
            { model->transmitting = true; },
            true);
        return true;
    }

    if(event->key == InputKeyOk && event->type == InputTypeRelease) {
        /* Guard: only stop if we actually started */
        if(!app->ir_tx_active) return true;
        skidcity_ir_tx_state.stop_requested = true;
        furi_hal_infrared_async_tx_stop();
        app->ir_tx_active = false;
        if(app->ir_blink_timer) {
            furi_timer_stop(app->ir_blink_timer);
            furi_timer_free(app->ir_blink_timer);
            app->ir_blink_timer = NULL;
        }
        furi_hal_light_set(LightRed,   0x00);
        furi_hal_light_set(LightGreen, 0x00);
        furi_hal_light_set(LightBlue,  0x00);
        with_view_model(
            app->ir_view, IrDemoModel* model,
            { model->transmitting = false; },
            true);
        return true;
    }

    if(event->key == InputKeyBack) return false;
    return false;
}

/* ═══════════════════════════════════════════════════════
 * BANNED CUSTOM VIEW
 * ═══════════════════════════════════════════════════════ */
static void skidcity_banned_draw_cb(Canvas* canvas, void* model) {
    BannedModel* m = model;

    /* Black background */
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_box(canvas, 0, 0, 128, 64);

    /* White border */
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_rframe(canvas, 1, 1, 126, 62, 5);

    canvas_set_font(canvas, FontPrimary);

    if(m->variant == SkidCityBannedTypeFCC) {
        canvas_draw_str_aligned(canvas, 64, 7,  AlignCenter, AlignTop, "FCC VIOLATION");
        canvas_draw_str_aligned(canvas, 64, 20, AlignCenter, AlignTop, "47 U.S.C. S333");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 34, AlignCenter, AlignTop, "Signal jamming is ILLEGAL.");
        canvas_draw_str_aligned(canvas, 64, 43, AlignCenter, AlignTop, "$100,000 fine + prison.");
        canvas_draw_str_aligned(canvas, 64, 54, AlignCenter, AlignTop, "[BACK = learn why]");
    } else if(m->variant == SkidCityBannedTypeSkid) {
        canvas_draw_str_aligned(canvas, 64, 7,  AlignCenter, AlignTop, "DON'T BE A");
        canvas_draw_str_aligned(canvas, 64, 20, AlignCenter, AlignTop, "SCRIPT KIDDIE!");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 34, AlignCenter, AlignTop, "Use tools you UNDERSTAND.");
        canvas_draw_str_aligned(canvas, 64, 43, AlignCenter, AlignTop, "Learn the fundamentals.");
        canvas_draw_str_aligned(canvas, 64, 54, AlignCenter, AlignTop, "[BACK = learn why]");
    } else {
        /* CFAA default */
        canvas_draw_str_aligned(canvas, 64, 7,  AlignCenter, AlignTop, "YOUR FLIPPER");
        canvas_draw_str_aligned(canvas, 64, 20, AlignCenter, AlignTop, "IS BANNED");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 34, AlignCenter, AlignTop, "18 U.S.C. S1030 - CFAA");
        canvas_draw_str_aligned(canvas, 64, 43, AlignCenter, AlignTop, "Unauthorized Access");
        canvas_draw_str_aligned(canvas, 64, 54, AlignCenter, AlignTop, "[BACK = learn why]");
    }
}

static bool skidcity_banned_input_cb(InputEvent* event, void* context) {
    UNUSED(context);
    UNUSED(event);
    /* Let BACK bubble up to the scene manager navigation handler */
    return false;
}

/* ═══════════════════════════════════════════════════════
 * SCENE: MAIN MENU
 * ═══════════════════════════════════════════════════════ */
void skidcity_scene_main_menu_on_enter(void* context) {
    SkidCityApp* app = context;
    submenu_reset(app->submenu);

    /* Seed the first frame of the scrolling header, then start the timer */
    app->header_offset = 0;
    skidcity_header_timer_cb(app);
    app->header_timer = furi_timer_alloc(skidcity_header_timer_cb, FuriTimerTypePeriodic, app);
    furi_timer_start(app->header_timer, 300);     // Scrolling Text speed

    for(uint32_t i = 0; i < SkidCityFeatureCount; i++) {
        submenu_add_item(
            app->submenu, skidcity_features[i].menu_label,
            i, skidcity_submenu_cb, app);
    }
    submenu_add_item(
        app->submenu, "-- About SKIDcity --",
        SkidCityFeatureCount, skidcity_submenu_cb, app);

    submenu_set_selected_item(app->submenu, app->main_menu_selected_index);

    view_dispatcher_switch_to_view(app->view_dispatcher, SkidCityViewSubmenu);
}

bool skidcity_scene_main_menu_on_event(void* context, SceneManagerEvent event) {
    SkidCityApp* app = context;
    if(event.type != SceneManagerEventTypeCustom) return false;

    if(event.event == (uint32_t)SkidCityFeatureCount) {
        scene_manager_next_scene(app->scene_manager, SkidCitySceneAppAbout);
        return true;
    }
    if(event.event < (uint32_t)SkidCityFeatureCount) {
        app->current_feature             = (SkidCityFeature)event.event;
        app->feature_menu_selected_index = 0;
        scene_manager_next_scene(app->scene_manager, SkidCitySceneFeatureMenu);
        return true;
    }
    return false;
}

void skidcity_scene_main_menu_on_exit(void* context) {
    SkidCityApp* app = context;
    app->main_menu_selected_index = submenu_get_selected_item(app->submenu);
    furi_timer_stop(app->header_timer);
    furi_timer_free(app->header_timer);
    app->header_timer = NULL;
    submenu_reset(app->submenu);
}

/* ═══════════════════════════════════════════════════════
 * SCENE: FEATURE SUB-MENU
 * ═══════════════════════════════════════════════════════ */
#define FEAT_EV_DEMO  0
#define FEAT_EV_ABOUT 1
#define FEAT_EV_CFAA  2

void skidcity_scene_feature_menu_on_enter(void* context) {
    SkidCityApp* app = context;
    const SkidCityFeatureInfo* fi = &skidcity_features[app->current_feature];

    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, fi->submenu_header);

    submenu_add_item(
        app->submenu, fi->demo_item,
        FEAT_EV_DEMO, skidcity_submenu_cb, app);
    submenu_add_item(
        app->submenu, fi->about_label,
        FEAT_EV_ABOUT, skidcity_submenu_cb, app);

    if(fi->demo_type == SkidCityDemoBanned ||
       fi->demo_type == SkidCityDemoFcc) {
        submenu_add_item(
            app->submenu, "CFAA / Full Legal Info",
            FEAT_EV_CFAA, skidcity_submenu_cb, app);
    }

    submenu_set_selected_item(app->submenu, app->feature_menu_selected_index);

    view_dispatcher_switch_to_view(app->view_dispatcher, SkidCityViewSubmenu);
}

bool skidcity_scene_feature_menu_on_event(void* context, SceneManagerEvent event) {
    SkidCityApp* app = context;
    if(event.type != SceneManagerEventTypeCustom) return false;

    const SkidCityFeatureInfo* fi = &skidcity_features[app->current_feature];

    if(event.event == FEAT_EV_DEMO) {
        switch(fi->demo_type) {
        case SkidCityDemoTrafficLed:
            with_view_model(
                app->traffic_view, TrafficModel* model,
                { model->state = 0; }, false);
            scene_manager_next_scene(app->scene_manager, SkidCitySceneTrafficDemo);
            return true;
        case SkidCityDemoIrBlink:
            scene_manager_next_scene(app->scene_manager, SkidCitySceneGenericDemo);
            return true;
        case SkidCityDemoBanned:
            with_view_model(
                app->banned_view, BannedModel* model,
                { model->variant = SkidCityBannedTypeCFAA; }, false);
            scene_manager_next_scene(app->scene_manager, SkidCitySceneBanned);
            return true;
        case SkidCityDemoSkid:
            with_view_model(
                app->banned_view, BannedModel* model,
                { model->variant = SkidCityBannedTypeSkid; }, false);
            scene_manager_next_scene(app->scene_manager, SkidCitySceneBanned);
            return true;
        case SkidCityDemoFcc:
            with_view_model(
                app->banned_view, BannedModel* model,
                { model->variant = SkidCityBannedTypeFCC; }, false);
            scene_manager_next_scene(app->scene_manager, SkidCitySceneBanned);
            return true;
        }
    }
    if(event.event == FEAT_EV_ABOUT) {
        scene_manager_next_scene(app->scene_manager, SkidCitySceneFeatureAbout);
        return true;
    }
    if(event.event == FEAT_EV_CFAA) {
        scene_manager_next_scene(app->scene_manager, SkidCitySceneCfaaInfo);
        return true;
    }
    return false;
}

void skidcity_scene_feature_menu_on_exit(void* context) {
    SkidCityApp* app = context;
    app->feature_menu_selected_index = submenu_get_selected_item(app->submenu);
    submenu_reset(app->submenu);
}

/* ═══════════════════════════════════════════════════════
 * SCENE: TRAFFIC DEMO
 * ═══════════════════════════════════════════════════════ */
void skidcity_scene_traffic_demo_on_enter(void* context) {
    SkidCityApp* app = context;
    skidcity_set_led(app, 0); /* start on red */
    view_dispatcher_switch_to_view(app->view_dispatcher, SkidCityViewTrafficDemo);
}

bool skidcity_scene_traffic_demo_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void skidcity_scene_traffic_demo_on_exit(void* context) {
    SkidCityApp* app = context;
    UNUSED(app);
    furi_hal_light_set(LightRed,   0x00);
    furi_hal_light_set(LightGreen, 0x00);
    furi_hal_light_set(LightBlue,  0x00);
}

/* ═══════════════════════════════════════════════════════
 * SCENE: GENERIC DEMO (IR blink)
 * ═══════════════════════════════════════════════════════ */
void skidcity_scene_generic_demo_on_enter(void* context) {
    SkidCityApp* app = context;
    /* Reset state */
    app->ir_tx_active                   = false;
    skidcity_ir_tx_state.stop_requested = false;
    skidcity_ir_tx_state.pos            = 0;
    with_view_model(
        app->ir_view, IrDemoModel* model,
        { model->transmitting = false; },
        false);
    /* Single blue flash on entry — hints the IR LED location */
    furi_hal_light_set(LightBlue, 0xFF);
    furi_delay_ms(120);
    furi_hal_light_set(LightBlue, 0x00);
    view_dispatcher_switch_to_view(app->view_dispatcher, SkidCityViewIrDemo);
}

bool skidcity_scene_generic_demo_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void skidcity_scene_generic_demo_on_exit(void* context) {
    SkidCityApp* app = context;
    /* Only call stop() if TX was actually started — avoids furi_check crash */
    if(app->ir_tx_active) {
        skidcity_ir_tx_state.stop_requested = true;
        furi_hal_infrared_async_tx_stop();
        app->ir_tx_active = false;
    }
    /* Clean up blink timer if BACK was pressed while holding OK */
    if(app->ir_blink_timer) {
        furi_timer_stop(app->ir_blink_timer);
        furi_timer_free(app->ir_blink_timer);
        app->ir_blink_timer = NULL;
    }
    furi_hal_light_set(LightRed,   0x00);
    furi_hal_light_set(LightGreen, 0x00);
    furi_hal_light_set(LightBlue,  0x00);
}

/* ═══════════════════════════════════════════════════════
 * SCENE: FEATURE ABOUT ("Why is this Illegal?")
 * ═══════════════════════════════════════════════════════ */
void skidcity_scene_feature_about_on_enter(void* context) {
    SkidCityApp* app = context;
    const SkidCityFeatureInfo* fi = &skidcity_features[app->current_feature];

    widget_reset(app->widget);
    widget_add_string_element(
        app->widget, 64, 5, AlignCenter, AlignTop, FontPrimary, fi->about_title);
    widget_add_text_scroll_element(
        app->widget, 0, 18, 128, 46, fi->about_body);

    view_dispatcher_switch_to_view(app->view_dispatcher, SkidCityViewWidget);
}

bool skidcity_scene_feature_about_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void skidcity_scene_feature_about_on_exit(void* context) {
    SkidCityApp* app = context;
    widget_reset(app->widget);
}

/* ═══════════════════════════════════════════════════════
 * SCENE: BANNED
 * ═══════════════════════════════════════════════════════ */
void skidcity_scene_banned_on_enter(void* context) {
    SkidCityApp* app = context;
    view_dispatcher_switch_to_view(app->view_dispatcher, SkidCityViewBanned);
}

bool skidcity_scene_banned_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void skidcity_scene_banned_on_exit(void* context) {
    UNUSED(context);
}

/* ═══════════════════════════════════════════════════════
 * SCENE: CFAA INFO (deep-dive legal text)
 * ═══════════════════════════════════════════════════════ */
void skidcity_scene_cfaa_info_on_enter(void* context) {
    SkidCityApp* app = context;
    widget_reset(app->widget);
    widget_add_string_element(
        app->widget, 64, 5, AlignCenter, AlignTop, FontPrimary, "The Law: CFAA");
    widget_add_text_scroll_element(
        app->widget, 0, 18, 128, 46, CFAA_BODY);
    view_dispatcher_switch_to_view(app->view_dispatcher, SkidCityViewWidget);
}

bool skidcity_scene_cfaa_info_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void skidcity_scene_cfaa_info_on_exit(void* context) {
    SkidCityApp* app = context;
    widget_reset(app->widget);
}

/* ═══════════════════════════════════════════════════════
 * SCENE: APP ABOUT
 * ═══════════════════════════════════════════════════════ */
void skidcity_scene_app_about_on_enter(void* context) {
    SkidCityApp* app = context;
    widget_reset(app->widget);
    widget_add_string_element(
        app->widget, 64, 5, AlignCenter, AlignTop, FontPrimary, "About SKIDcity");
    widget_add_text_scroll_element(
        app->widget, 0, 18, 128, 46, APP_ABOUT_BODY);
    view_dispatcher_switch_to_view(app->view_dispatcher, SkidCityViewWidget);
}

bool skidcity_scene_app_about_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void skidcity_scene_app_about_on_exit(void* context) {
    SkidCityApp* app = context;
    widget_reset(app->widget);
}

/* ═══════════════════════════════════════════════════════
 * APP LIFECYCLE
 * ═══════════════════════════════════════════════════════ */
static SkidCityApp* skidcity_app_alloc(void) {
    SkidCityApp* app = malloc(sizeof(SkidCityApp));
    furi_check(app);

    app->current_feature          = SkidCityFeatureTraffic;
    app->header_timer             = NULL;
    app->header_offset            = 0;
    app->ir_blink_timer           = NULL;
    app->ir_tx_active             = false;
    app->main_menu_selected_index     = 0;
    app->feature_menu_selected_index  = 0;

    /* Core services */
    app->gui           = furi_record_open(RECORD_GUI);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    /* SceneManager */
    app->scene_manager = scene_manager_alloc(&skidcity_scene_handlers, app);

    /* ViewDispatcher */
    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, skidcity_custom_event_cb);
    view_dispatcher_set_navigation_event_callback(app->view_dispatcher, skidcity_back_event_cb);
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    /* Submenu view */
    app->submenu = submenu_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, SkidCityViewSubmenu, submenu_get_view(app->submenu));

    /* Widget view */
    app->widget = widget_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, SkidCityViewWidget, widget_get_view(app->widget));

    /* Traffic-light custom view */
    app->traffic_view = view_alloc();
    view_set_context(app->traffic_view, app);
    view_allocate_model(app->traffic_view, ViewModelTypeLocking, sizeof(TrafficModel));
    view_set_draw_callback(app->traffic_view, skidcity_traffic_draw_cb);
    view_set_input_callback(app->traffic_view, skidcity_traffic_input_cb);
    view_dispatcher_add_view(
        app->view_dispatcher, SkidCityViewTrafficDemo, app->traffic_view);

    /* Banned custom view */
    app->banned_view = view_alloc();
    view_set_context(app->banned_view, app);
    view_allocate_model(app->banned_view, ViewModelTypeLocking, sizeof(BannedModel));
    view_set_draw_callback(app->banned_view, skidcity_banned_draw_cb);
    view_set_input_callback(app->banned_view, skidcity_banned_input_cb);
    view_dispatcher_add_view(
        app->view_dispatcher, SkidCityViewBanned, app->banned_view);

    /* IR demo custom view */
    app->ir_view = view_alloc();
    view_set_context(app->ir_view, app);
    view_allocate_model(app->ir_view, ViewModelTypeLocking, sizeof(IrDemoModel));
    view_set_draw_callback(app->ir_view, skidcity_ir_draw_cb);
    view_set_input_callback(app->ir_view, skidcity_ir_input_cb);
    view_dispatcher_add_view(
        app->view_dispatcher, SkidCityViewIrDemo, app->ir_view);

    return app;
}

static void skidcity_app_free(SkidCityApp* app) {
    /* Ensure LED is off on exit */
    furi_hal_light_set(LightRed,   0x00);
    furi_hal_light_set(LightGreen, 0x00);
    furi_hal_light_set(LightBlue,  0x00);

    /* Remove views before freeing */
    view_dispatcher_remove_view(app->view_dispatcher, SkidCityViewSubmenu);
    view_dispatcher_remove_view(app->view_dispatcher, SkidCityViewWidget);
    view_dispatcher_remove_view(app->view_dispatcher, SkidCityViewTrafficDemo);
    view_dispatcher_remove_view(app->view_dispatcher, SkidCityViewBanned);
    view_dispatcher_remove_view(app->view_dispatcher, SkidCityViewIrDemo);

    submenu_free(app->submenu);
    widget_free(app->widget);
    view_free(app->traffic_view);
    view_free(app->banned_view);
    view_free(app->ir_view);

    view_dispatcher_free(app->view_dispatcher);
    scene_manager_free(app->scene_manager);

    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);

    free(app);
}

int32_t skidcity_app(void* p) {
    UNUSED(p);
    SkidCityApp* app = skidcity_app_alloc();
    scene_manager_next_scene(app->scene_manager, SkidCitySceneMainMenu);
    view_dispatcher_run(app->view_dispatcher);
    skidcity_app_free(app);
    return 0;
}
