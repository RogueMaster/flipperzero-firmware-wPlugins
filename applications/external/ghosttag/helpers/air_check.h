#pragma once

#include <furi.h>
#include <stdbool.h>
#include <stdint.h>

/**
 * Air Check - the one thing GhostTag CAN do with the Flipper's own radio.
 *
 * WHAT THIS IS NOT
 * ----------------
 * This is not a scanner and it can never become one. The Flipper's BLE stack
 * (STM32WB55) exposes advertising, a peripheral profile, and RF test mode -
 * furi_hal_bt.h has no GAP observer role, no central role, and no way to hand
 * an advertisement back to an application. So the onboard radio cannot read a
 * MAC address, cannot see a vendor signature, and cannot tell an AirTag from a
 * laptop. Finding trackers needs the ESP32 companion, and always will.
 *
 * WHAT IT IS
 * ----------
 * Test mode CAN park the receiver on a channel and report raw energy
 * (furi_hal_bt_start_rx + furi_hal_bt_get_rssi). Hopping the three BLE
 * ADVERTISING channels - 37, 38 and 39, the only channels a tracker ever
 * beacons on - measures how much advertising traffic is in the air right here.
 *
 * That is genuinely useful before a hunt, because it is the context a hunt is
 * read against:
 *
 *   BUSY   a cafe, a train, an office. Dozens of tags are around you and a
 *          single one is hard to single out, so expect a crowded list.
 *   QUIET  a garage, a car park at night, your own hallway. Almost nothing
 *          should be beaconing here, so anything persistent is worth a look.
 *
 * It also proves the Flipper's radio is alive, which is worth knowing before
 * blaming the app.
 *
 * HONESTY CEILING: this module reports ENERGY. It never returns a device, a
 * count of devices, or a verdict containing the word tracker, and the view
 * that draws it carries that caveat on screen at all times. An energy
 * measurement cannot be attributed to a transmitter, and saying otherwise
 * would be the most dangerous thing this app could do.
 */

#define AIR_ADV_CHANNELS 3

/** Which of the two RF receive paths is currently in use. */
typedef enum {
    AirRfListen, /* furi_hal_bt_start_rx - the plain listener */
    AirRfPacket, /* furi_hal_bt_start_packet_rx - the BLE receiver test */
} AirRfMode;

typedef struct {
    bool valid;
    uint8_t busy_pct[AIR_ADV_CHANNELS]; /* share of samples above the floor */
    int8_t peak_dbm[AIR_ADV_CHANNELS]; /* strongest sample on that channel */
    int8_t floor_dbm; /* the band's own noise floor this sweep */
    uint32_t samples; /* how much evidence this is built on */
    uint32_t elapsed_s;
    bool dead; /* running a while and the radio has returned nothing usable */
    bool radio_ready; /* the second core reported itself ready for RF test mode */
    AirRfMode rf_mode; /* which receive path produced these numbers */
} AirSnapshot;

/** Overall band occupancy, in plain words. Never mentions trackers. */
typedef enum {
    AirBandUnknown, /* not enough samples yet */
    AirBandNoReading, /* the radio is not giving us anything */
    AirBandQuiet,
    AirBandModerate,
    AirBandBusy,
} AirBand;

typedef struct AirCheck AirCheck;

AirCheck* air_check_alloc(void);
void air_check_free(AirCheck* air);

void air_check_start(AirCheck* air);
void air_check_stop(AirCheck* air);
bool air_check_is_running(AirCheck* air);

/** Latest sweep. Safe to call from the GUI thread at any time. */
void air_check_snapshot(AirCheck* air, AirSnapshot* out);

AirBand air_check_band(const AirSnapshot* snap);
const char* air_band_label(AirBand band);

/** The MHz centre of each advertising channel, for the display. */
extern const uint16_t air_adv_mhz[AIR_ADV_CHANNELS];
extern const char* const air_adv_label[AIR_ADV_CHANNELS];
