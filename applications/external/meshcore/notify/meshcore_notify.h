/*
 * Field feedback — sound, vibration and the RGB LED, on the events that matter
 * with cold hands and glances at the screen. Every aspect is gated on
 * MeshCoreSettings, so the user can keep just the LED, just vibro, or silence.
 *
 * Two kinds of signal:
 *   - events (meshcore_notify_event): a short blip for something that happened.
 *   - status (meshcore_notify_led_status): the LED held to a colour that reflects
 *     link quality, so the logger can be read at arm's length or in the dark.
 *
 * The status LED and the event blinks are kept apart on purpose: status is used
 * by the logger (which does not fire event blinks), events by the messenger and
 * configurator (which do not hold a status colour). A blink still resets the RGB
 * layer, so an event invalidates the cached status colour and the next reading
 * repaints it — the held colour cannot be left dark. None of these touch the
 * display backlight.
 */
#pragma once

#include "../config/meshcore_settings.h"
#include "../logger/meshcore_rxlog.h" /* MeshCoreSnrGrade */

typedef struct MeshCoreNotify MeshCoreNotify;

typedef enum {
    MeshCoreNotifyIncoming, /* a message arrived (messenger)                 */
    MeshCoreNotifyPingOk, /* a ping came back                               */
    MeshCoreNotifyPingFail, /* a ping was lost                                */
    MeshCoreNotifyConnected, /* the node came up on the UART                   */
    MeshCoreNotifyDisconnected, /* the node dropped off, or a Connect failed     */
    MeshCoreNotifyMark, /* a point mark was placed (logger button)        */
    MeshCoreNotifyApplyOk, /* a preset applied and verified                  */
    MeshCoreNotifyError, /* SD missing / node silent — the alarm pattern   */
} MeshCoreNotifyEvent;

/* Reads `settings` live on every call, so a toggle takes effect at once. */
MeshCoreNotify* meshcore_notify_alloc(const MeshCoreSettings* settings);
void meshcore_notify_free(MeshCoreNotify* notify);

/** A short sound/vibro/LED blip for `event`, filtered by the settings. */
void meshcore_notify_event(MeshCoreNotify* notify, MeshCoreNotifyEvent event);

/** Hold the LED to a colour for the current link quality (logger). Green for a
 *  good SNR, yellow at the margin, red for weak/no signal. No-op if the LED is
 *  switched off, or if that colour is already held — re-sending an identical
 *  solid colour is a visual no-op, so only a grade change touches the LED. */
void meshcore_notify_led_status(MeshCoreNotify* notify, MeshCoreSnrGrade grade);

/** Turn the held status LED off — call when leaving the logger. */
void meshcore_notify_led_off(MeshCoreNotify* notify);

/** Easter egg: play a recognisable tune with the motor and LED dancing to it.
 *  Ignores the toggles and forces volume — it is an explicit request, not a
 *  field cue. Blocks until it finishes. Reached by the "party" launch argument. */
void meshcore_notify_play_song(MeshCoreNotify* notify);
