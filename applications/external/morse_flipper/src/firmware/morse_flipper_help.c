/*
 * Purpose: Populate and navigate the in-app help pages.
 * Owns: help text arrays, selected chapter/page state, and canvas refresh.
 * Depends on: morse_flipper_app_i.h and cw markdown rendering.
 * Tests: firmware build; UI text flow is hardware-only.
 */

#include "morse_flipper_app_i.h"

static const uint8_t morse_help_arrow_right_xbm[] = {
    0x10,
    0x30,
    0x7f,
    0x30,
    0x10,
};

static const uint8_t morse_help_micro_xbm[] = {
    0x09,
    0x09,
    0x09,
    0x09,
    0x07,
    0x01,
    0x01,
};

static const uint8_t morse_help_back_triangle_xbm[] = {
    0x08,
    0x0c,
    0x0e,
    0x0f,
    0x0e,
    0x0c,
    0x08,
};

static const uint8_t morse_help_dit_xbm[] = {
    0x06,
    0x0f,
    0x0f,
    0x06,
};

static const uint8_t morse_help_dah_xbm[] = {
    0x7e,
    0xff,
    0xff,
    0x7e,
};

static const uint8_t morse_help_slash_xbm[] = {
    0x04,
    0x04,
    0x02,
    0x02,
    0x02,
    0x01,
    0x01,
};

static const CwmdIcon morse_help_icons[] = {
    {
        .id = 1U,
        .width = 7U,
        .height = 5U,
        .y_offset = 0,
        .left_bearing = 1U,
        .right_bearing = 1U,
        .xbm = morse_help_arrow_right_xbm,
    },
    {
        .id = 2U,
        .width = 4U,
        .height = 7U,
        .y_offset = 2,
        .left_bearing = 1U,
        .right_bearing = 1U,
        .xbm = morse_help_micro_xbm,
    },
    {
        .id = 3U,
        .width = 4U,
        .height = 7U,
        .y_offset = 0,
        .left_bearing = 1U,
        .right_bearing = 1U,
        .xbm = morse_help_back_triangle_xbm,
    },
    {
        .id = 4U,
        .width = 4U,
        .height = 4U,
        .y_offset = 1,
        .left_bearing = 1U,
        .right_bearing = 1U,
        .xbm = morse_help_dit_xbm,
    },
    {
        .id = 5U,
        .width = 8U,
        .height = 4U,
        .y_offset = 1,
        .left_bearing = 1U,
        .right_bearing = 1U,
        .xbm = morse_help_dah_xbm,
    },
    {
        .id = 6U,
        .width = 3U,
        .height = 7U,
        .y_offset = 0,
        .left_bearing = 1U,
        .right_bearing = 1U,
        .xbm = morse_help_slash_xbm,
    },
};

static const char* const morse_help_first_steps[] = {
    "Welcome to Morse Flipper.\nThis is a practical little CW trainer for learning to copy real Morse with your own ears.",
    "Good telegraphy is less about keying and more about learning to copy with the bit between your ears.",
    "Morse is not dead. It is still used in ham radio, prepper circles, and now and then as a bit of drama in film and media.",
    "There are plenty of software decoders, but the one that truly matters is the one between your ears. Train that every day.",
    "Small daily practice beats a grand weekly binge. Morse code likes repetition, not theatre.",
    "Perhaps a small orange thing in your pocket can help with that.",
    "In ham radio, Morse is usually called CW, short for continuous wave. You will hear that term far more often than \"Morse\". Hams have a long tradition of making plain things sound more technical than they really are.",
    "This is the only screen where the app will show dits \ex4 and dahs \ex5. You will not see them anywhere else. This is by design.",
    "Think of how you learnt the multiplication table. Nobody sensible counts up to work out 8 times 3; they just know it is 24. Morse should become the same sort of reflex: hear the sound, get the letter. No thinking involved.",
    "You didn't learn the multiplication table by counting, but by repetition, and you shouldn't learn Morse code by counting.",
    "This is important: if you catch yourself thinking out in dots and dashes, you are counting. Stop and make the exercise harder. Practise only outside the comfort zone."};

static const char* const morse_help_input_keys[] = {
    "Practical telegraphy always uses a device called a key, manipulator or tapper.",
    "The Flipper's buttons do work as a key. They are still a fairly rotten excuse for one.",
    "The classic movie key is called a straight key (SK). You press it down, it closes a contact, and that is the whole trick.",
    "This is what movies like Lincoln, Titanic, On the beach, Western Union or the 1899 series show when someone is operating a telegraph",
    "With a SK, you control the timing - marks, gaps, all of it.",
    "No straight key operator is perfectly identical to another. That personal style is called a fist, and experienced operators can often recognise one another by it.",
    "Do not confuse a fist with bad sending. A good operator still sends cleanly; the differences are just small human ones.",
    "Hours on a straight key will do your wrist no favours.That is one reason paddles and bugs turned up so early - think Vibroplex.",
    "The next keyers were devices with two contacts, one for dots and one for dashes. They are pushed from the side with a precise finger motion.",
    "This device is called a paddle and needs support electronics. With a paddle and keyer, the code is usually cleaner and more even. That makes it easier on the ears and easier to copy.\n\nMost transceivers have these electronics inside them - you just connect a paddle and it works.",
    "This app supports both SK and paddle inputs, and has multiple keyers available. They can simply be connected to GPIO.",
    "Most people start without a proper key. That is fine. The Flipper can stand in for a while, but it gets bloody annoying before long, and you will want the real thing.",
    "Because of how the joystick is made, you cannot sensibly pinch two of its buttons at once.\nSo the app steals the Back button.",
    "If you are using buttons as an input source and you are in paddle mode, Back becomes one paddle and OK becomes the other.\n\nYou can key, pinch, and feed a real keyer.",
    "To exit such a screen, long press Left.\n\nWhen Back is being nicked as a paddle input, this symbol appears on the right: \ex3\n\nThis is your reminder how to leave the screen.",
    "It is a compromise solution, but there is sadly no way around it - keyers want two buttons pressed at once.",
};

static const char* const morse_help_connecting_paddle[] = {
    "Use a 6.5 mm female jack, solder header pins on it, and plug it straight into the GPIO port. Simple is good here.",
    "Use this wiring:\n- P3 \ex1 gnd (sleeve)\n- P5 \ex1 dah (ring)\n- P7 \ex1 dit (tip)",
    "With this header layout, ground sits on P3. That is fine physically. The app should still use P3 as the default virtual ground.",
    "The jack and cable should point outward, away from the screen and buttons. It is neater, sturdier, and less annoying than loose GPIO wires.",
    "If your paddle uses a 3.5 mm plug, use a simple 3.5 mm to 6.5 mm adapter, just the usual audio sort.",
    "You can also solder a suitable 3.5mm TRS jack - the GPIO can be reassigned in settings.",
    "A straight key fits the same header:\n- P3 \ex1 gnd\n- P7 \ex1 dit\ex6SK",
    "One adapter covers both cases:\n- Paddle \ex1 P3\ex6P5\ex6P7\n- Straight key \ex1 P3\ex6P7",
    "If your wiring does not match this layout, change it in Settings \ex1 Keying \ex1 GPIO. You can move dit\ex6SK, dah and the ground pin in there. Straight mode always uses the dit\ex6SK pin from that menu. This layout matches the female jack to the GPIO header with the least swaps.",
    "Regardless of layout, you always need a ground. P3 can be reassigned if that is more convenient, or you can use the Flipper's normal ground pins. If your key has no ground contact, it will not work.",
};

static const char* const morse_help_practice[] = {
    "You can become perfectly competent at CW and still do most of your sending from a computer. I rarely touch my key unless I have to.",
    "LCWO.net is the gold standard for learning to tell letters apart by sound. It starts with two letters.",
    "Once you stop making mistakes on those two, you add another.",
    "When you add a new letter, resist the urge to translate it into dots and dashes. Learn the sound directly. This app avoids showing the pattern on purpose, because once you start reading Morse as dots and dashes, you have made the whole thing harder than it should be.",
    "There is an LCWO style trainer in here too: same lesson order, same general idea, just on a small orange gadget instead of a laptop.",
    "Keep the WPM high. You want to recognise letters by sound, not count dits and dahs. Stay above 20 wpm; start at 30, or 25 if 30 is simply too much.",
    "Do not lower the WPM below 30 before you reach lesson 10.",
    "If you catch yourself spelling it out as dash dot dash and then deciding it is K, the speed is too low. Turn it up.",
    "You __REALLY__ want to stop thinking out the sound. Remember how you were taught the multiplication table.",
    "Use Farnsworth to open up the gaps. The spacing should help you hear the rhythm, not tempt you back into counting.",
    "You need to hear letters, not inventory. If you only copy shapes back as \"three dots, one dash\", you are practising parroting, not Morse. Doing this will not help you.",
    "You can also use this app to sharpen straight key sending and practise five character sending groups competitively.\n\nDo not make those the main event too early. Copying characters by ear comes first. If you cannot hear the letters reliably, better sending just makes tidier nonsense.",
    "Straight key sending is a skill of its own. The line between a recognisable personal fist and sloppy sending is subtle, and beginners usually cannot hear the difference yet.\n\nThe Straight trainer grades your marks, gaps and dah\ex6dit ratio. Groups of 5 has Easy, Medium and IARU HST scoring. Use Medium as the first serious target and start around 15 WPM. Drop to Easy if the timing feels impossible, but remember that Easy still permits sending that would sound rough on the air.",
    "On the Straight trainer result line, S means spacing, di means dit length, da means dah length, and r is the dah\ex6dit ratio. The ideal ratio is 3. OK means that part was close enough.\n\nWhen you see a number instead of OK, it is a percentage score. Higher is better. A low number means that part of your sending was far from the expected timing.",
    "For Groups of 5, Easy is a forgiving warm up. Medium expects clean practice timing and is the sensible everyday target.\n\nIARU HST uses competition style scoring. Use it when Medium stops being useful and you want the app to be rude but fair.",
    "Music practice is oddly useful, especially rhythm work with a metronome. Pitch drills, not so much. Do use a metronome if you want clean skill transfer. You should use a metronome anyway during music practice. Hanons helped me with transmit speed.",
};

static const char* const morse_help_prepping[] = {
    "In prepper and survivalist circles, CW has a certain reputation: harder than voice, but one of the most self sufficient modes left when things go wrong.",
    "This app can serve as an emergency CW transmitter, but let us be sensible: it would not be my first choice. Nor my second.\nThe transmitter chip in Flipper also drifts when keying below 25 wpm.",
    "That drift is a hardware problem, so you do not really fix it in software.\nA human can tolerate it; cheap decoders will have a fit.\nIt drifts back after pauses, which makes it sound worse still.",
    "Voice needs a lot of bandwidth. Digital is fussier. CW can get by with a simple transmitter, a key, and a very small pile of kit.",
    "The Titanic's operators kept transmitting long after everything else failed. Morse was one of the last things still working, and the disaster proved ships needed proper battery backed emergency radio.",
    "People do in fact build and sell radios for exactly this sort of thing. Small CW QRP rigs, often sold to the prepper crowd, sometimes with decoders built in for when the wetware is having an off day.",
    "Low power CW on HF with a wire in the trees is hard to locate. A voice call on a known emergency frequency is not.",
    "Pair CW with NVIS: a wire aimed almost straight up, bouncing signals off the ionosphere. Roughly 500-1500 km, two trees, one wire.",
    "Sort the plan out now, not during the emergency: frequency, time, fallback. For example: 7.030, daily at 20:00, listen two minutes, then try again tomorrow.",
    "Your messages will be short.\n\e*\ecALL OK\n is two words.\n\e*\ecQTH DAMAGE NEED MED FER 3\n is six. Plan them like you plan the rest of your kit.",
    "Use voice for everyday comms. Use CW when power is scarce, conditions are bad, or you want low profile. Digital if you have the gear. Use the simplest thing that works. This is emergency radio, not rocket surgery.",
    "An agreed code can be better still. If both sides know that \"SAQ\" means QTH DAMAGE NEED MED FER 3, sending SAQ over and over may get through more easily than a full sentence.",
    "The realistic takeaway is this: CW is worth having as a backup skill. Not your first tool, not your main plan, but occasionally the only thing still working.",
};

static const char* const morse_help_contact[] = {
    "CW has its own little language.\nKeep it short, assume the other fellow is copying by ear, and do not waste his patience.",
    "Every extra word is something the other operator has to copy. Cut it down hard and leave the grammar at the door.",
    "Common abbreviations:\n\n- FER - for\n- DE - from\n- R - roger, received\n- K - over\n- ES - and\n- GA - good afternoon\n- GM - good morning\n- GE - good evening\n- FB - good (fine business)\n- 73 - bye bye\n- TU, THX - thank you\n- 88 - hugs\n- 99 - usually means 'go away', but can be interpreted as a more vulgar 'fuck off'",
    "More abbreviations:\n\n- HW - how\n- SK - logging off\n- NIL - nothing\n- LID - newb\n- BK - break\n- DX - someone far away\n- CQ - calling everyone\n- CFM - confirmed\n- AGN - again\n- UR RST - your signal is\n- ANT - antenna\n- QSO - 2 way radio contact",
    "A few more:\n\n- 5NN - very good signal\n- 3NN - average signal\n- 2NN \ex6 1NN - weak signal\n- = new thought, period\n- E E - two dots right at the end of a contact.\n\nOften one side sends slash - the old \"shave and a haircut\" rhythm - and the other replies E E.",
    "OM\ex6YL. The old CW shorthand makes men \"old men\" and women \"young ladies\", which gives the game away rather nicely. XYL means wife, as in ex YL. Roughly the same energy as calling your wife your ex girlfriend.",
    "Here is an absolutely real exchange from my log.",
    "\e*\ecCQ CQ CQ DE YO3GND YO3GND K\n\nThis is YO3GND calling. Is anyone listening?",
    "\e*\ecDE P5KIM K\n\nThis is P5KIM answering.",
    "\e*\ecRR P5KIM DE YO3GND = GM UR 5NN HW IS KOREA? BK\n\nRoger P5KIM from YO3GND.\nGood morning, your signal is strong. How is Korea? Back to you.",
    "\e*\ecBK GM KOREA FB = NEW ANT NEW ICBM NUKE BK\n\nGood morning.\nKorea is doing well, I have a new antenna and a new nuclear ballistic missile. Back to you.",
    "\e*\ecRR NEAT TOYS = TU FER QSO ES 73\n\nRoger. Interesting gear you have there. Thank you for the contact, and goodbye.",
    "\e*\ecRR TU 73 E E\n\nRoger. Thank you and goodbye.",
    "At that point the contact between P5KIM and YO3GND is complete.",
    "Some people are chatty and send full phrases. That will be slow. Do not expect it as the default.",
    "A contact can be even shorter than that. Just callsigns and signal reports still count perfectly well.",
};

static const char* const morse_help_contesting[] = {
    "Telegraphy is still very much alive as a performance sport in the ham bands.",
    "Contests are yearly events when the world's radio amateurs all pile onto the bands for a weekend and try to work one another.",
    "During a contest weekend, CW gets everywhere. Tune about a bit and you will hear TEST - short for contest - until it starts haunting your sleep.",
    "Contesting rewards practice, but anyone can have a go.",
    "Some operators might slow down for you. It is considered polite. It is also not something to count on during a contest.",
    "Unlike the previous P5KIM contact, contest exchanges are usually very quick.",
    "The standard contest exchange is this:\n\e*\ecYO3GND TEST\n\e*\ecP5KIM\n\e*\ecP5KIM 599 23\n\e*\ec599 37\n\e*\ecTU\n",
    "That means:\n\n1. I am YO3GND in the current contest.\n2. I am P5KIM, also in the contest.\n3. Put number 23 in your log.\n4. Roger, put 37 in yours for me.\n5. Thanks, goodbye",
    "Anything else except this standard exchange is not expected in a contest. Do not do this, unless you personally know the other fellow and you are sure he does not mind. Certainly do not do it with strangers.",
    "The receiver is expected to know which contest is happening.\nNobody explains it while it's happening, certainly not in Morse code.",
    "Contests do not allow duplicate contacts. Some penalise them. If you try one, you will receive NIL or DUPE. Keep at it and they may send LID, 99 or ignore you.",
    "Contesting rarely happens with a paddle and paper. Most people use logging software such as N1MM to log, check dupes and automate the boring bits.",
    "Contesting can be done with a paddle, but most high speed operators send by keyboard and macros.",
    "They still decode with their own brain, and that is exactly what this app is trying to train",
    "Contesting with a straight key is very rare. I know just one person who does it.",
    "Contesting tends to happen around 25-30 wpm. Expect 30+ early on.",
};

static const char* const morse_help_usb_live[] = {
    "Apps like vail.woozle.org, v-band or iCW let you practise with other people using a real paddle.",
    "To do this, you need a computer adapter for your SK or paddle. This app can also pretend to be an adapter for CW practice sites. You can still use the Flipper's buttons if you must.",
    "Most simple adapters lie to the computer and pretend to be a keyboard or mouse. That is how they get a key or paddle in.",
    "The site reads those key events, makes the tones locally, and sends the result to the other participants.",
    "The nicer modern option is\nMIDI: lower latency, quicker response, and better timing between the browser and the adapter.",
    "This app supports a Vail style MIDI adapter properly, including keyers, WPM and tone control.",
    "You do not need a callsign to practise on those sites, nor a transmitter. However, the experience is similar to a real QSO.",
    "CW has its own language.\nIf you want to operate on those sites, you will be expected to understand it. Read \"A complete Morse contact\" to understand it.",
};

static const char* const morse_help_moving_forward[] = {
    "This is only a small app, really, just a practical way into CW. I made it for people who are curious about Morse and RF and would rather prod the thing than read about it forever.",
    "Seriously, get a telegraphy key; I suggest paddles over a straight key. The Flipper's silicone membrane buttons will eventually wear out, especially when a practice plateau tempts you to press harder, as if force were the missing skill. Keying from the joystick gets annoying quickly too.",
    "lcwo.net is the easy daily practice route on a computer. It lets you type what you hear, which means you can practise and make mistakes faster than you can on the Flipper.",
    "Morse Runner Community Edition will simulate live contest traffic, exchanges and general unpleasantness.",
    "rufzxp trains upper end decoding speed. It is official software in the CW IARU HST World Championship.\n\nYes, CW olympiads are a real thing.",
    "vail.woozle.net works nicely with this app in USB MIDI mode, and lets you practise live contacts with other people. Some might even send you a QSL card.",
    "It is worth trying for a ham licence. In much of Europe it costs almost nothing, and plenty of clubs are happy to help if you turn up actually wanting to learn CW.",
    "Being licensed on HF means that, at times, you can reach the world with five watts.\nWith a modest hundred watt rig, CW gets surprisingly far surprisingly often.",
    "Find the Twente WebSDR. Tune around 14.000-14.070 MHz and 7.000-7.050 MHz. On a weekend or during a contest, you will hear plenty of live CW.",
    "Try CW Academy if you want something more organised than poking at apps and websites on your own. This app can help, but the course is not casual. They expect a real key, regular practice, and a fair bit of homework.",
    "The CWA course material is free, and you can also work through it yourself at your own pace.",
    "If you do get licensed, send me a note at yo3gnd.dev.fzcw@yo3gnd.ro. I would be glad to hear how it went.\n\n\n\nIf you read this far, there is a QSL waiting for you. Send in the secret code 821073 and your new callsign.",
};

static const char* const morse_help_ham_usage[] = {
    "Ham Keyer is a field keyer and logger in one slightly opinionated box. It can send fixed lines like RR UR 5NN BK or POTA RO 0038 K, while also keying and logging whatever you send on the paddles.\n\nBKIN off logs only. BKIN enabled transmits and logs. Handy for POTA\ex6SOTA, where the operator already has enough plates wobbling. Logs are dated on the SD card, and each change is timestamped.",
    "Rig keying uses P15 for key and P16 for PTT. Do not wire a radio on optimism.\n\nCheck the rig input, polarity, and ground arrangement. Use a transistor or optocoupler if the noise or RFI makes Flipper misbehave.",
    "Back toggles BKIN. Long press Left exits.\n\nWith BKIN off, paddle input is logged but not sent to the rig.",
    "Assign short messages to Up, Down, Left, Right, and OK. Use them for field exchanges, not essays.\n\nIf logging is enabled, entries go to \ex6ext\ex6ham\ex6morse flipper ham keyer YYYY MM DD.txt.",
};

static const char* const morse_help_troubleshooting[] = {
    "If keying doesn't work, start simple: joystick OK is a straight key. Then try OK + Back as button paddles. Then plug in a real key.\n\nSettings \ex1 Keying is always source plus mode: where the signal comes from, and how the app turns it into Morse.",
    "A straight key uses dit\ex6SK. A paddle needs dit, dah, and ground.\n\nThe default jack wiring is:\n- P3 \ex1 ground\n- P5 \ex1 dah\n- P7 \ex1 dit or SK",
    "The paddles can be swapped from settings, if needed.\n\nWhen Back is not exiting, it is being used as a paddle. You will see this symbol on the right: \ex3 This is your reminder to long press Left for back.",
    "A GPIO short warning means the jack wiring looks wrong for the chosen input. Unplug the key and check it.\n\nA mono straight key in the paddle jack will be treated as a straight key.",
    "For USB output, choose Keyboard, Mouse, or MIDI, then wait a bit for the computer to reconnect with the new settings.\n\nCustom character sets and ham logs live on the SD card under \ex6ext\ex6ham.",
    "High quality audio is output only on P2. Connect it to an audio jack. It will sound better if you can add a small filtering capacitor, 1-50\ex2F, between P2 and ground.\n\nYou don't have to use the P2 output; you can always fall back to the internal buzzer for the sidetone.",
};

uint8_t morse_flipper_help_card_count(uint8_t t) {
    switch(t) {
    case MorseFlipperHelpFirstSteps:
        return COUNT_OF(morse_help_first_steps);
    case MorseFlipperHelpInputKeys:
        return COUNT_OF(morse_help_input_keys);
    case MorseFlipperHelpConnectingPaddle:
        return COUNT_OF(morse_help_connecting_paddle);
    case MorseFlipperHelpPractice:
        return COUNT_OF(morse_help_practice);
    case MorseFlipperHelpPrepping:
        return COUNT_OF(morse_help_prepping);
    case MorseFlipperHelpContact:
        return COUNT_OF(morse_help_contact);
    case MorseFlipperHelpContesting:
        return COUNT_OF(morse_help_contesting);
    case MorseFlipperHelpUsbLive:
        return COUNT_OF(morse_help_usb_live);
    case MorseFlipperHelpMovingForward:
        return COUNT_OF(morse_help_moving_forward);
    case MorseFlipperHelpHamUsage:
        return COUNT_OF(morse_help_ham_usage);
    case MorseFlipperHelpTroubleshooting:
        return COUNT_OF(morse_help_troubleshooting);
    default:
        return COUNT_OF(morse_help_moving_forward);
    }
}

static const char* morse_flipper_help_card(uint8_t t, uint8_t i) {
    switch(t) {
    case MorseFlipperHelpFirstSteps:
        return i < COUNT_OF(morse_help_first_steps) ? morse_help_first_steps[i] : "";
    case MorseFlipperHelpInputKeys:
        return i < COUNT_OF(morse_help_input_keys) ? morse_help_input_keys[i] : "";
    case MorseFlipperHelpConnectingPaddle:
        return i < COUNT_OF(morse_help_connecting_paddle) ? morse_help_connecting_paddle[i] : "";
    case MorseFlipperHelpPractice:
        return i < COUNT_OF(morse_help_practice) ? morse_help_practice[i] : "";
    case MorseFlipperHelpPrepping:
        return i < COUNT_OF(morse_help_prepping) ? morse_help_prepping[i] : "";
    case MorseFlipperHelpContact:
        return i < COUNT_OF(morse_help_contact) ? morse_help_contact[i] : "";
    case MorseFlipperHelpContesting:
        return i < COUNT_OF(morse_help_contesting) ? morse_help_contesting[i] : "";
    case MorseFlipperHelpUsbLive:
        return i < COUNT_OF(morse_help_usb_live) ? morse_help_usb_live[i] : "";
    case MorseFlipperHelpMovingForward:
        return i < COUNT_OF(morse_help_moving_forward) ? morse_help_moving_forward[i] : "";
    case MorseFlipperHelpHamUsage:
        return i < COUNT_OF(morse_help_ham_usage) ? morse_help_ham_usage[i] : "";
    case MorseFlipperHelpTroubleshooting:
        return i < COUNT_OF(morse_help_troubleshooting) ? morse_help_troubleshooting[i] : "";
    default:
        return i < COUNT_OF(morse_help_moving_forward) ? morse_help_moving_forward[i] : "";
    }
}

void morse_flipper_help_open(MorseFlipperApp* app) {
    uint8_t n;

    if(app == NULL) return;
    n = morse_flipper_help_card_count(app->help_topic);
    if(n == 0U) n = 1U;
    if(app->help_page >= n) app->help_page = 0U;
    view_dispatcher_switch_to_view(app->view_dispatcher, MorseFlipperViewLive);
    morse_flipper_view_dirty(app);
}

void morse_flipper_about_open(MorseFlipperApp* app) {
    UNUSED(app);
}

static void morse_flipper_help_cfg(
    const MorseFlipperApp* app,
    CwmdConfig* cfg,
    char* page,
    size_t page_sz) {
    uint8_t n = morse_flipper_help_card_count(app->help_topic);

    if(n == 0U) n = 1U;
    cwmd_config_default(cfg, true);
    cfg->height = 48U;
    cfg->scrollbar = true;
    cfg->chrome = CwmdChromeCenter;
    cfg->icons = morse_help_icons;
    cfg->icon_count = COUNT_OF(morse_help_icons);
    snprintf(page, page_sz, "%u/%u", (unsigned)(app->help_page + 1U), (unsigned)n);
    cfg->center_label = page;

    if(app->help_page > 0U) {
        static char left[8];
        snprintf(left, sizeof(left), "%u", (unsigned)app->help_page);
        cfg->left_label = left;
        cfg->chrome |= CwmdChromeLeft;
    }

    if(app->help_page + 1U < n) {
        static char right[8];
        if(app->help_page == 0U) {
            cfg->right_label = "Next";
        } else {
            snprintf(right, sizeof(right), "%u", (unsigned)(app->help_page + 2U));
            cfg->right_label = right;
        }
        cfg->chrome |= CwmdChromeRight;
    }
}

void morse_flipper_draw_help(Canvas* canvas, MorseFlipperApp* app) {
    CwmdConfig cfg;
    char page[12];

    if(canvas == NULL || app == NULL) return;
    morse_flipper_help_cfg(app, &cfg, page, sizeof(page));
    cwmd_draw(
        canvas, &cfg, &app->help_md, morse_flipper_help_card(app->help_topic, app->help_page));
}

int16_t morse_flipper_help_max_scroll(Canvas* canvas, const MorseFlipperApp* app) {
    CwmdConfig cfg;
    char page[12];

    if(canvas == NULL || app == NULL) return 0;
    morse_flipper_help_cfg(app, &cfg, page, sizeof(page));
    return cwmd_max_scroll_px(
        canvas, &cfg, morse_flipper_help_card(app->help_topic, app->help_page));
}
