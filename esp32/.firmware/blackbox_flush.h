#pragma once
/*
 * blackbox_flush.h — ring bookkeeping for saving a black-box event a slice at
 * a time (blackbox.cpp). Header-only and Arduino-free, so
 * test/test_esp32_core.cpp runs it on the host.
 *
 * The ring holds frames from the oldest (tail) to the newest (head), in time
 * order. A save writes the frames of its window a slice per loop pass, so the
 * loop, CAN included, runs between slices, and the ring keeps recording
 * meanwhile. Only the frames the save has not written yet are protected: with
 * the ring full and the oldest frame being the next one to write, a new frame
 * is dropped instead of evicting it.
 *
 * A slice ends after a fixed number of lines or once a time budget has gone,
 * whichever comes first, and the wait before the next step shrinks with the
 * lines written, so a short slice does not slow the save down.
 *
 * A trigger while an event is armed: up to the end of that event's post-roll
 * it is inside the window being recorded, and part of that event. After that
 * the event is being saved; the first trigger then waits, and is armed with
 * its own window when the save is over. Its pre-roll is whatever the ring
 * still holds of it by then. A further trigger during the same save is part
 * of the waiting event when it falls inside that event's post-roll, and is
 * missed otherwise.
 */

#include <stdint.h>

// `cap` slots hold at most cap - 1 frames: head == tail is an empty ring.
static inline uint32_t bb_ring_next(uint32_t cap, uint32_t i) { return (i + 1u) % cap; }

static inline uint32_t bb_ring_count(uint32_t cap, uint32_t head, uint32_t tail) {
    return (head + cap - tail) % cap;
}

// Slot for a new frame, evicting the oldest one when the ring is full.
// `keep` is the slot of the next frame a running save still has to write (cap
// when no save runs): it is never evicted, and the new frame is dropped
// instead (returns false).
static inline bool bb_ring_push(uint32_t cap, uint32_t &head, uint32_t &tail,
                                uint32_t keep, uint32_t *slot) {
    if (bb_ring_next(cap, head) == tail) {
        if (tail == keep) return false;
        tail = bb_ring_next(cap, tail);
    }
    *slot = head;
    head = bb_ring_next(cap, head);
    return true;
}

// Largest internal-RAM ring (in frames) that still leaves `margin` bytes of heap
// free after the allocation, clamped to [min_frames, max_frames]. The classic
// ESP32 that #191 was filed on has only ~80-90 KB free after the WiFi AP + web
// server, so a fixed 3000-frame / 57 KB ring plus the 60 KB WiFi margin (117 KB)
// never fit and enable was always refused; sizing the ring to the heap we have
// lets the recorder run with a proportionally shorter window instead. Returns 0
// — refuse, as before — when even the floor ring (min_frames, a ~4 s window)
// plus the margin won't fit: that board is genuinely too tight for a black box.
// Above the gate the computed count is > min_frames, so the lower clamp is a
// safety net; the upper clamp holds the per-target save-copy cap (<= margin).
static inline uint32_t bb_ram_ring_frames(size_t avail, size_t margin,
                                          size_t frame_size,
                                          uint32_t min_frames, uint32_t max_frames) {
    if (avail <= margin + (size_t)min_frames * frame_size) return 0u;
    uint32_t frames = (uint32_t)((avail - margin) / frame_size);
    if (frames > max_frames) frames = max_frames;
    if (frames < min_frames) frames = min_frames;
    return frames;
}

// The frames with lo_ms <= ts <= hi_ms, as `count` frames from logical position
// `first` (0 = the oldest frame in the ring).
struct BBWindow {
    uint32_t first;
    uint32_t count;
};

// The frames are in time order, so two binary searches find the window: the
// cost grows with log(frames in the ring), not with the ring or the window.
// ts_at(k) is the timestamp of the frame at logical position k, k < n.
template <typename TsAt>
static inline BBWindow bb_find_window(uint32_t n, uint32_t lo_ms, uint32_t hi_ms, TsAt ts_at) {
    uint32_t a = 0, b = n;  // first frame at or after lo_ms
    while (a < b) {
        uint32_t m = a + (b - a) / 2u;
        if (ts_at(m) < lo_ms) a = m + 1u; else b = m;
    }
    uint32_t c = a, d = n;  // first frame after hi_ms
    while (c < d) {
        uint32_t m = c + (d - c) / 2u;
        if (ts_at(m) <= hi_ms) c = m + 1u; else d = m;
    }
    BBWindow w = {a, c - a};
    return w;
}

// Frames written in this pass.
static inline uint32_t bb_slice(uint32_t left, uint32_t per_pass) {
    return left < per_pass ? left : per_pass;
}

// Whether a slice that has written `lines` of its `n` (bb_slice()) ends here:
// all written, or `budget_us` gone. Asked after each line, so one always goes.
static inline bool bb_slice_over(uint32_t lines, uint32_t n, uint32_t elapsed_us,
                                 uint32_t budget_us) {
    return lines >= n || elapsed_us >= budget_us;
}

// Wait after a slice of `lines`: `gap_ms` for `per_pass` lines, the same share
// (rounded up, at least 1 ms) for fewer, so lines go out no faster than
// per_pass per gap_ms however the budget cuts the slices.
static inline uint32_t bb_slice_gap_ms(uint32_t lines, uint32_t per_pass, uint32_t gap_ms) {
    if (lines >= per_pass) return gap_ms;
    uint32_t g = (gap_ms * lines + per_pass - 1u) / per_pass;
    return g ? g : 1u;
}

// What a new trigger does (see the top of this file).
enum BBArmAction : uint8_t {
    BB_ARM_NOW = 0,     // nothing armed: arm it
    BB_ARM_IN_WINDOW,   // inside the armed or the waiting event's window
    BB_ARM_QUEUE,       // the armed event is being saved: wait for it
    BB_ARM_MISSED,      // one is already waiting, and this is outside its window
};

// True while `now_ms` is at most `post_ms` after `trig_ms` (millis() wrap-safe).
static inline bool bb_in_post_roll(uint32_t trig_ms, uint32_t post_ms, uint32_t now_ms) {
    return (int32_t)(now_ms - trig_ms - post_ms) <= 0;
}

static inline BBArmAction bb_arm_action(bool armed, uint32_t trig_ms, bool queued,
                                        uint32_t queued_ms, uint32_t post_ms,
                                        uint32_t now_ms) {
    if (!armed) return BB_ARM_NOW;
    if (bb_in_post_roll(trig_ms, post_ms, now_ms)) return BB_ARM_IN_WINDOW;
    if (!queued) return BB_ARM_QUEUE;
    return bb_in_post_roll(queued_ms, post_ms, now_ms) ? BB_ARM_IN_WINDOW : BB_ARM_MISSED;
}
