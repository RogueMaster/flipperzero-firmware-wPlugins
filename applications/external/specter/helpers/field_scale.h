#pragma once

#include <stdbool.h>
#include <stdint.h>

/* Turning carrier duty-cycle into a meter reading.
 *
 * The detector measures one physical thing: what fraction of the time an
 * external 13.56 MHz carrier is up. That number is genuinely proximity-related,
 * but only over part of its range, and understanding why is the whole reason
 * this file exists:
 *
 *   - Far out, the reader's field only trips the chip's detect threshold during
 *     the strongest part of each polling burst, so the measured duty climbs as
 *     you close in. This is the useful signal.
 *   - Up close, every burst trips it for the burst's full width - and then the
 *     duty stops climbing. It has hit the reader's *own* poll duty cycle.
 *
 * A typical access reader or payment terminal polls roughly 20-35% of the time
 * (a burst every ~100-300 ms), so a Flipper laid directly on top of one reads
 * about 30% and cannot go higher. Nothing is wrong: 30% duty *is* saturation.
 *
 * Showing that raw number on a 0-100 gauge was a presentation bug. It made a
 * perfect detection look like a third of a detection, squashed the whole useful
 * range into the bottom of the dial, and left the upper proximity words
 * (CLOSE / STRONG) and the survey's peak test unreachable in practice.
 *
 * So the meter is scaled: raw duty is mapped onto the full 0-100 display range
 * against a documented full-scale point. Saturating on a reader now reads near
 * 100%, which is what "you are on top of it" should look like.
 *
 * The raw figure is never thrown away - the noise floor, the calibration and
 * the emitter classifier all keep working in raw duty, because those are
 * statements about the signal itself rather than about proximity. */

/* Raw duty at which the display reads 100%.
 *
 * Set from measurements on real hardware: a contactless terminal with the
 * Flipper laid on it measures around 30-32% duty, so a full scale of 35 left
 * the meter stuck in the low 90s at the point where it should obviously peg.
 * 30 is at the middle of the observed saturation band, which means "resting on
 * a reader" reads 100 / MAX as a user expects, while a sparsely-polling emitter
 * still climbs most of the dial and a continuous-wave one pegs from further
 * away - correct, since CW really is a stronger exposure. */
#define SPECTER_FULL_SCALE_DUTY 30u

/* Passing this as the full scale disables scaling (display == raw duty). */
#define SPECTER_SCALE_RAW 100u

/* Map raw duty-cycle (0..100) onto the display range (0..100).
 *
 * Linear up to full_scale, clamped above it. A full_scale of 0 is treated as
 * SPECTER_SCALE_RAW so a mis-set value can never divide by zero or blank the
 * meter. Pure and total - host-tested in test/. */
uint8_t field_scale_apply(uint8_t raw_duty, uint8_t full_scale);

/* True when this reading is at or above the full-scale point, i.e. the meter is
 * pegged and getting closer will not move it. Lets the UI say so honestly
 * instead of the user wondering why 100% stopped responding. */
bool field_scale_is_saturated(uint8_t raw_duty, uint8_t full_scale);
