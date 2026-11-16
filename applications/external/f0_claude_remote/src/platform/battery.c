#include "battery.h"

#include <furi_hal_power.h>
#include <power/power_service/power.h>

#include "../core/charging.h"

struct Battery {
    Power* power;
    FuriPubSubSubscription* subscription;
    FuriMessageQueue* charge_queue;
    FuriMessageQueue* charging_queue;
};

static void battery_put_charging(Battery* battery, bool charging) {
    furi_message_queue_put(battery->charging_queue, &charging, 0);
}

/* Runs on the power service's 1 KiB thread under its pubsub's lock (power.c:162-169,
 * pubsub.c:84-98), and event_loop.h binds its functions to the loop's own thread. */
static void battery_event_callback(const void* message, void* context) {
    const PowerEvent* event = message;
    Battery* battery = context;
    switch(event->type) {
    case PowerEventTypeBatteryLevelChanged:
        furi_message_queue_put(battery->charge_queue, &event->data.battery_level, 0);
        break;
    case PowerEventTypeStartCharging:
        battery_put_charging(battery, true);
        break;
    case PowerEventTypeStopCharging:
    case PowerEventTypeFullyCharged:
        battery_put_charging(battery, false);
        break;
    }
}

/* Subscribes before reading: a change published between the two would otherwise be lost. */
Battery* battery_alloc(FuriMessageQueue* charge_queue, FuriMessageQueue* charging_queue) {
    Battery* battery = malloc(sizeof(Battery));
    battery->charge_queue = charge_queue;
    battery->charging_queue = charging_queue;
    battery->power = furi_record_open(RECORD_POWER);
    battery->subscription =
        furi_pubsub_subscribe(power_get_pubsub(battery->power), battery_event_callback, battery);
    PowerInfo info;
    power_get_info(battery->power, &info);
    furi_message_queue_put(charge_queue, &info.charge, 0);
    battery_put_charging(
        battery, charging_now(info.is_charging, info.charge, furi_hal_power_is_charging_done()));
    return battery;
}

void battery_free(Battery* battery) {
    furi_pubsub_unsubscribe(power_get_pubsub(battery->power), battery->subscription);
    furi_record_close(RECORD_POWER);
    free(battery);
}
