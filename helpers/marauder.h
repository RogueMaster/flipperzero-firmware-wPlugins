#pragma once

/*
 * Command catalogue for the ESP32 Marauder serial CLI.
 *
 * These are the stable, documented commands from the upstream ESP32 Marauder
 * firmware (github.com/justcallmekoko/ESP32Marauder). Trident sends them over
 * UART with a trailing newline; the board streams its output back, which the
 * console view renders live. Anything not covered by a menu can still be typed
 * by hand in the Console (OK -> Send command), so Trident stays useful across
 * firmware revisions.
 *
 * Newlines are intentionally omitted here — trident_link_send() /
 * trident_launch() append "\n" so a single command constant can be reused.
 */

/* ---- general ---- */
#define MARAUDER_CMD_STOP      "stop"
#define MARAUDER_CMD_REBOOT    "reboot"
#define MARAUDER_CMD_UPDATE    "update"
#define MARAUDER_CMD_HELP      "help"
#define MARAUDER_CMD_SETTINGS  "settings" // print current board settings

/* ---- scan / discovery ---- */
#define MARAUDER_CMD_SCAN_AP   "scanap"
#define MARAUDER_CMD_SCAN_STA  "scansta"
#define MARAUDER_CMD_LIST_AP   "list -a"
#define MARAUDER_CMD_LIST_STA  "list -s"
#define MARAUDER_CMD_LIST_SSID "list -c"

/* ---- channel ---- */
#define MARAUDER_CMD_CHANNEL   "channel" // print the current channel
#define MARAUDER_PFX_CHANNEL   "channel -s " // + <n>  (2.4 GHz 1-14, 5 GHz 36..165)

/* ---- sniffers ---- */
#define MARAUDER_CMD_SNIFF_BEACON "sniffbeacon"
#define MARAUDER_CMD_SNIFF_PROBE  "sniffprobe"
#define MARAUDER_CMD_SNIFF_DEAUTH "sniffdeauth"
#define MARAUDER_CMD_SNIFF_PMKID  "sniffpmkid"
#define MARAUDER_CMD_SNIFF_PWN    "sniffpwn"
#define MARAUDER_CMD_SNIFF_ESP    "sniffesp"
#define MARAUDER_CMD_SNIFF_RAW    "sniffraw"

/* ---- analysis ---- */
#define MARAUDER_CMD_SIGMON       "sigmon" // live channel / signal monitor

/* ---- attacks (gated behind the confirmation prompt) ---- */
#define MARAUDER_CMD_ATTACK_DEAUTH     "attack -t deauth"
#define MARAUDER_CMD_ATTACK_BEACON_L   "attack -t beacon -l"
#define MARAUDER_CMD_ATTACK_BEACON_R   "attack -t beacon -r"
#define MARAUDER_CMD_ATTACK_BEACON_AP  "attack -t beacon -a"
#define MARAUDER_CMD_ATTACK_PROBE      "attack -t probe"
#define MARAUDER_CMD_ATTACK_RICKROLL   "attack -t rickroll"

/* ---- targeting ---- */
#define MARAUDER_CMD_SELECT_AP_ALL "select -a all"
#define MARAUDER_CMD_CLEAR_AP      "clearlist -a"
#define MARAUDER_CMD_CLEAR_STA     "clearlist -s"
#define MARAUDER_CMD_CLEAR_SSID    "clearlist -c"
/* select a single index: printf(MARAUDER_FMT_SELECT, 'a'|'s', index) */
#define MARAUDER_FMT_SELECT        "select -%c %s"

/* ---- SSID list (feeds "Beacon Spam (list)") ---- */
#define MARAUDER_PFX_SSID_GEN      "ssid -a -g " // + <n>     add n random SSIDs
#define MARAUDER_PFX_SSID_NAME     "ssid -a -n " // + <name>  add a named SSID
#define MARAUDER_PFX_SSID_REMOVE   "ssid -r " // + <n>     remove SSID at index n

/* ---- bluetooth ---- */
#define MARAUDER_CMD_BT_SNIFF      "sniffbt"
#define MARAUDER_CMD_BT_SKIMMER    "sniffskim"
#define MARAUDER_CMD_BT_AIRTAG     "sniffairtag"

/* ---- bluetooth spam (attacks) ---- */
#define MARAUDER_CMD_BLE_SOURAPPLE "sourapple" // Apple device advertisement flood
#define MARAUDER_CMD_BLE_SPAM_APPLE   "blespam -t apple"
#define MARAUDER_CMD_BLE_SPAM_SAMSUNG "blespam -t samsung"
#define MARAUDER_CMD_BLE_SPAM_GOOGLE  "blespam -t google"
#define MARAUDER_CMD_BLE_SPAM_WINDOWS "blespam -t windows"
#define MARAUDER_CMD_BLE_SPAM_ALL     "blespam -t all"

/* ---- gps (needs the onboard GPS antenna + a fix) ---- */
#define MARAUDER_CMD_GPS_DATA      "gpsdata"
#define MARAUDER_CMD_WARDRIVE      "wardrive"
#define MARAUDER_CMD_WARDRIVE_STA  "stationwardrive"
