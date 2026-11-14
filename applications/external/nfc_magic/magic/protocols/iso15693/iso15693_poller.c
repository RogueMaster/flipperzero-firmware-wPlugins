#include "iso15693_poller.h"
#include <furi.h>
#include <nfc/nfc_poller.h>
#include <lib/nfc/protocols/iso15693_3/iso15693_3_poller.h>
#include <toolbox/bit_buffer.h>

#define TAG "Iso15693Poller"

// ISO15693_3_FDT_WRITE_POLL_FC is the WRITE-frame response timeout (271200 carrier cycles ~= 20ms,
// long enough for the tag to program its EEPROM before it answers). Older SDKs predate this macro,
// so keep a fallback with the same value the current SDK defines.
#ifndef ISO15693_3_FDT_WRITE_POLL_FC
#define ISO15693_3_FDT_WRITE_POLL_FC (271200U)
#endif

// Magic ISO15693 ("Chinese magic") backdoor UID write, ported from proxmark3 (GPLv3)
// SetTag15693Uid / SetTag15693Uid_v2 (armsrc/iso15693.c). Frames are sent to hidden backdoor blocks;
// the CRC is appended by iso15693_3_poller_send_frame. Two card generations are handled here -- a
// third, gen3, is neither detected nor supported, and a wipe can destroy one (see the hazard note in
// nfc_magic_scene_write_confirm.c). The write always tries gen2 first, and offers gen1 -- which is
// destructive on a non-magic tag -- only as an explicit user opt-in after gen2 leaves the UID
// unchanged.
//
// Used only by the gen2 sequence (see iso15693_poller_build_gen2_frame), and LITERALLY rather than
// through iso15693_poller_write_flags(): with OPTION set a gen2 card applies the write and answers
// nothing -- on all four cards measured, two of which do not want the flag for ordinary writes.
#define ISO15693_MAGIC_FLAGS (0x02U) // high data rate, unaddressed (ISO15_REQ_DATARATE_HIGH)

// Data blocks are written ADDRESSED: the card's UID travels in the frame and only that card answers.
// That is what #251 asks for. Measured on seven CARDS over four identified chips -- TI Tag-it HF-I
// Plus (by its behaviour, not its UID), NXP ICODE SLIX, NXP ICODE SLIX-S, ST LRi2K -- and two whose
// own silicon was never captured, because each has only ever worn a written UID: the type a reader
// prints for them is decoded from the UID they wear. Every one accepts an addressed WRITE BLOCK,
// and on every one a write to a UID one byte wrong leaves its block unchanged -- read back, not
// inferred from the silence.
//
// On TI Tag-it HF-I Plus the addressing is not what makes the write work -- what that card wants is
// the OPTION flag, measured across all four combinations:
//
//     02 unaddressed, no option   refused, error 0x01      42 unaddressed, OPTION   ACCEPTED
//     22 addressed,   no option   refused, error 0x03      62 addressed,   OPTION   ACCEPTED
//
// So the OPTION flag is necessary and sufficient there, and the addressing is orthogonal to whether
// the write is accepted -- though not to how the card says so; see ISO15693_POLLER_OPTION_FLAG.
// Addressing remains worth doing for what #251 filed it as -- keeping the write off a second tag in
// the field -- and no card measured refuses it.
//
// The SDK cannot send it: iso15693_3_poller_write_block hardcodes SUBCARRIER_1 | DATA_RATE_HI with no
// flags parameter and no UID, and iso15693_3_write_block_response_parse is internal to lib/nfc. So the
// frame and its response check are both built here.
#define ISO15693_POLLER_WRITE_FLAGS                                        \
    (ISO15693_3_REQ_FLAG_SUBCARRIER_1 | ISO15693_3_REQ_FLAG_DATA_RATE_HI | \
     ISO15693_3_REQ_FLAG_T4_ADDRESSED)

// SOME SILICON WANTS THE OPTION FLAG ON WRITES, and says so. TI Tag-it HF-I Plus answers an
// ADDRESSED WRITE BLOCK whose OPTION bit is clear with error 0x03 -- "the option is not supported",
// the tag naming the bit -- and accepts the identical frame with 0x40 set, addressed or not:
// measured on both such cards, bracketed by inventories and read back. This, not the addressing, is
// what makes that chip writable. Unaddressed, the same frame draws 0x01, which names no bit, so the
// detection below works only because these writes are addressed.
//
// NOT decided from the UID, although proxmark's `hf 15 wrbl` decides it that way (it forces the flag
// when the manufacturer byte is TI's 0x07). A UID is not a statement about silicon here, and this app
// is in the business of changing it: a clone writes the SOURCE's UID onto the card, so a TI card
// cloned from an NXP image stops looking like TI exactly before the data pass that needs the flag;
// a wipe zeroing block 57 rewrites uid[3..0], which is where the manufacturer byte lives; and a card
// that arrived already carrying someone else's UID never looked like its own silicon at all.
//
// So it is decided by what the TAG SAYS: the first write of a run goes out without the flag, and a
// 0x03 refusal sets it for the rest of the run. The retry that follows is the attempt that lands, so
// the recovery costs no extra frames.
//
// AND THE FLAG COSTS US THE ACKNOWLEDGEMENT. ISO15693-3 10.3.1: with OPTION set, the card answers a
// WRITE BLOCK only after the READER sends a standalone EOF, and this SDK cannot send one --
// iso15693_3_poller_encode_frame appends exactly one EOF as the tail of the frame and there is no
// call for another. proxmark sends it (SendDataTagEOF, armsrc/iso15693.c), which is the only reason
// its raw write gets an answer.
//
// So on a card that wants OPTION the write LANDS and the ACK never arrives. Measured on the same card:
// block 8 held AA BB CC DD, a wipe from this app reported every block refused, and the block read back
// as zeros. Silence there is not a refusal; the timeout arm of iso15693_poller_write_block_retried
// settles it by reading the block back.
#define ISO15693_POLLER_OPTION_FLAG (ISO15693_3_REQ_FLAG_T4_OPTION)

// gen1: an ordinary ISO15693 WRITE BLOCK aimed at four backdoor blocks; 4 data bytes each, which is
// what ISO15693_MAGIC_REGISTER_SIZE is. The command is the standard one, so the frames go through
// iso15693_poller_write_block_addressed like the data blocks -- see
// iso15693_poller_send_backdoor_uid_gen1. The UID blocks are named by the UID bytes they carry
// (uid[0] is the MSB, so uid[7..4] is the numerically low half of the UID).
#define ISO15693_MAGIC_REGISTER_SIZE (4U)
// MEASURED on five cards across three chips: the sequence sets the UID, it reads back, and the
// original restores byte-identically. On one card of each chip: the UID changes IMMEDIATELY -- an
// inventory in the SAME field session as the write already returns the new one -- so there is no
// power-up latch on any chip tested; and writes to 62/63 are REFUSED while 56/57 are accepted, so
// the UID moves on 56/57 alone and the refusals do not stop it.
//
// ALSO MEASURED, on the three gen1 chips: the four addresses are not memory. They answer no read at
// any point. On those chips the advertised count is fixed silicon rather than the gen2 CFG frame, so
// it enumerates memory only and 56/57/62/63 sit OUTSIDE it -- which is why a 56-block LRi2K can
// report 58 cleared. Do NOT read that as a general rule: on gen2 the claim is PROGRAMMED and a card
// serves reads above it, which is the whole reason the wipe sweeps past the claim (see
// blocks_advertised, and the sweep's own note at the wipe entry point). It is that gap, not the
// registers, that makes "how far past the claim does the sweep run" a question -- see the reach rule
// at the wiped == 0 branch.
//
// STILL INFERENCE: what 0x3E and 0x3F actually do. proxmark sends them first and names neither, and
// doc/magic_cards_notes.md's ISO15693-magic section is a TODO, so "unlock" and "arms" are our reading.
// The names UNLOCK and COMMIT, wherever this app uses them, are speculation about what those two
// blocks do. Neither has been observed to accept a write on a gen1 card, in any form. Where 62/63 are
// memory the frames are ordinary writes, and the one gen3 card tested took them as data. Five gen1
// cards took the write to 56 with neither sent before it -- across all three chips, two of them as a
// session's first frame -- but their histories are unknown, so that says nothing about a card that
// needs them. They stay because proxmark sends them; nothing is claimed for what they do.
#define ISO15693_MAGIC_BLK_UNLOCK    (0x3EU) // written as 0; inferred: unlock
#define ISO15693_MAGIC_BLK_COMMIT    (0x3FU) // written as 0x6996; inferred: arms the UID change
#define ISO15693_MAGIC_BLK_UID_7654  (0x38U) // uid[7..4]
#define ISO15693_MAGIC_BLK_UID_3210  (0x39U) // uid[3..0]

// The same four as one list, because several places ask "is this a backdoor block?": the clone loop
// skips them, its back-fill skips them again, the reported total deducts them, a converted clone takes
// their failures back, and the source inspection reads them. Spelled out per caller, the set would
// exist once for each and could disagree with itself. Membership only -- the gen1 write SEQUENCE is
// ordered and stays written out at its call site, where the order is the point.
// COUNT_OF rather than sizeof at every loop over this list: sizeof is right only while the element
// type is uint8_t, and this file already contemplates block indices above 255 elsewhere. Widened to
// uint16_t, a sizeof-bounded loop would run eight iterations over four elements.
static const uint8_t iso15693_poller_backdoor_blocks[] = {
    ISO15693_MAGIC_BLK_UID_7654,
    ISO15693_MAGIC_BLK_UID_3210,
    ISO15693_MAGIC_BLK_UNLOCK,
    ISO15693_MAGIC_BLK_COMMIT,
};

static bool iso15693_poller_is_backdoor_block(uint16_t block) {
    for(size_t i = 0; i < COUNT_OF(iso15693_poller_backdoor_blocks); i++) {
        if(block == iso15693_poller_backdoor_blocks[i]) return true;
    }
    return false;
}

// How many of the four a source of `count` blocks reaches: what a gen1 clone deducts from its total,
// and what gen1_blocks_skipped is set from, so the figure and the flag cannot disagree.
static uint16_t iso15693_poller_backdoor_blocks_below(uint16_t count) {
    uint16_t n = 0;
    for(size_t i = 0; i < COUNT_OF(iso15693_poller_backdoor_blocks); i++) {
        if(iso15693_poller_backdoor_blocks[i] < count) n++;
    }
    return n;
}

// The two of those four that CARRY the UID, which is a narrower question than being a backdoor
// block: writing 62/63 changes no identity, so an address taken before them is still good.
static bool iso15693_poller_is_uid_block(uint16_t block) {
    return block == ISO15693_MAGIC_BLK_UID_7654 || block == ISO15693_MAGIC_BLK_UID_3210;
}

// Standard ISO15693 identity writes -- see iso15693_poller_write_identity for what they are for.
#define ISO15693_MAGIC_CMD_WRITE_AFI   (0x27U) // ISO15693 WRITE AFI
#define ISO15693_MAGIC_CMD_WRITE_DSFID (0x29U) // ISO15693 WRITE DSFID

// gen2: magic write command (0xE0) with a 0x09 subcommand and a block reference; 4 data
// bytes each. Frame layout: 02 E0 09 <ref> d0 d1 d2 d3 (+CRC).
#define ISO15693_MAGIC_CMD_WRITE_V2     (0xE0U) // ISO15693_MAGIC_WRITE
#define ISO15693_MAGIC_V2_SUB           (0x09U)
#define ISO15693_MAGIC_V2_BLK_CFG       (0x47U) // system-info config: max block / block size / IC ref
#define ISO15693_MAGIC_V2_BLK_CFG2      (0x52U) // written as 0
#define ISO15693_MAGIC_V2_BLK_UID_7654  (0x40U) // uid[7..4]
#define ISO15693_MAGIC_V2_BLK_UID_3210  (0x41U) // uid[3..0]
// Fixed config payload for the CFG block, verbatim from proxmark's gen2 sequence (matches a
// 64-block / 4-byte-block / IC-ref-0x8B card; these values are constant in proxmark too).
#define ISO15693_MAGIC_V2_CFG_MAXBLOCK  (0x3FU)
#define ISO15693_MAGIC_V2_CFG_BLOCKSIZE (0x03U)
#define ISO15693_MAGIC_V2_CFG_IC_REF    (0x8BU)

// Sized to the SDK's own ISO15693_3_POLLER_MAX_BUFFER_SIZE, not to the 1-4 bytes a conforming tag
// answers these frames with. iso15693_3_poller_send_frame ends in
// bit_buffer_copy(caller_rx, instance->rx_buffer), and bit_buffer_copy enforces capacity with a
// furi_check -- a halt, not an error return. Every in-firmware caller passes instance->rx_buffer
// as the destination as well, so copy's `buf == other` short-circuit fires first and that check
// is dead for them; the raw-frame senders below are the first callers to hand it a foreign
// buffer, which makes it live. At 32 a CRC-valid 33-62 byte answer would halt the Flipper, and
// non-conforming tags are what this app exists to talk to.
#define ISO15693_POLLER_BUF_SIZE (64U)

// ISO15693 Get System Info stores (block size - 1) in a 5-bit field, so a block is at most 32 bytes.
#define ISO15693_MAX_BLOCK_SIZE (32U)

// Give up after this many consecutive activation failures so neither the detect popup nor the write
// popup can hang forever with no card. iso15693_3_poller_run delays exactly 100ms after each failed
// activation, so this is roughly a 4 second timeout plus each attempt's own airtime.
#define ISO15693_POLLER_MAX_ACTIVATION_ERRORS (40U)

// ...except after the wipe's power-cycle, where most of that budget buys nothing. That wait has no
// "user hasn't presented the card yet" phase to sit through -- the card was in the field a moment ago
// -- and the sweep already has a result to report, so the only question is whether the card comes back
// to be checked. Spending the full budget there freezes the popup for four seconds on the common case
// of the user lifting the card the instant the wipe finishes.
//
// Not cut to the minimum, though: giving up early on a card that IS coming back skips the UID check on
// precisely the card it exists for. A slow wait costs seconds, a short one costs the check. ~1.5s is
// well past what a present card needs while staying under "the app has hung". If a bench wipe logs
// "card did not return after the field reset" with the card still on the case back, raise this.
#define ISO15693_POLLER_WIPE_VERIFY_ACTIVATIONS (15U)

// The verify read-back runs right after an RF field power-cycle, so retry the inventory a few times:
// a card that is momentarily slow to answer must not be misreported as removed (a false CardLost on
// an otherwise-successful write).
#define ISO15693_POLLER_VERIFY_ATTEMPTS (3U)
#define ISO15693_POLLER_VERIFY_RETRY_MS (5U)

// Retry a failed WRITE BLOCK this many times -- either block pass -- before treating the block as
// genuinely unwritable. Retrying rides out a transient RF error; what a block that fails every
// attempt means is decided by a read afterwards, in each pass, not here.
#define ISO15693_POLLER_WRITE_ATTEMPTS (3U)

// How many 1/N progress bands a block pass is divided into. A pass emits at most N+1 events, not N:
// `step` takes every value in [0, N] as `done` runs from 0 to `total`, and the seed is UINT8_MAX so
// the first one always fires. The derivation is at iso15693_poller_report_progress.
//
// This is a hard safety bound, not a tuning knob. The block loops below run to completion inside a
// single poller callback on the Nfc worker thread, and a progress event ends up in
// view_dispatcher_send_custom_event, which blocks with FuriWaitForever on a queue of
// VIEW_DISPATCHER_QUEUE_LEN (16) entries. A Back press during a write puts the GUI thread inside
// nfc_poller_stop -> furi_thread_join waiting for this very worker, so it stops draining that queue.
// Emit more events than the queue can hold and the worker blocks forever, the join never returns,
// and the device hangs with the write popup on screen.
//
// VIEW_DISPATCHER_QUEUE_LEN, the FuriWaitForever in the send, iso15693_3_poller_filter_error and
// write_block_response_parse are not greppable from the shipped SDK headers: they are in the firmware
// source the app builds against (applications/services/gui/view_dispatcher.c and
// lib/nfc/protocols/iso15693_3/).
//
// Staying well under 16 keeps that impossible. At 8 the whole worst case is 11: CardDetected, then
// 8 + 1 progress events, then one terminal event. Emitting per block does NOT -- if you want that, the
// loop has to yield to the Nfc worker between blocks (return NfcCommandContinue and resume from a
// cursor) the way uscuid_ul_poller.c does, and only then is per-block safe.
#define ISO15693_POLLER_PROGRESS_STEPS (8U)

// The wipe sweeps ABOVE the card's advertised block count, because that count is programmed rather
// than physical -- see blocks_advertised in the header. Cloning a 28-block source onto a 64-block card
// leaves it advertising 28, and a wipe bounded by that clears 28 of 64 and calls it Success.
// Measured: seed all 64 blocks with a marker, clone a 28-block source over it, wipe (screen said
// Success), read with a proxmark -- block 20 zeroed, blocks 28, 40 and 63 still held the marker.
// 36 of 64 blocks survived a "successful" wipe.
//
// Only a WRITE settles whether a block exists. A high block that has never been written may not read
// either (see the read-back note in the loop), so a read-based capacity probe under-detects. Hence:
// write upward, and stop once a run of blocks looks absent.
//
// 256 is the ISO15693 block-number space, the failure bitmap's capacity, and the bound proxmark's own
// `hf 15 wipe` loop carries (cmdhf15.c) -- though it breaks at the FIRST refused write, so the
// tolerance below is ours. It is a ceiling: a card that stops answering ends the sweep
// ISO15693_POLLER_WIPE_ABSENT_RUN blocks past its top or at its claim, whichever is later. The pass
// budget below covers the card that never stops answering.
//
// Writing above physical capacity is inert rather than destructive, and a read there fails
// outright: on a gen2 magic card phantom writes were rejected, phantom reads failed and block 0 was
// unchanged across four runs; a plain NXP ICODE tag was sent writes past its end and its last block
// was unchanged; and full read sweeps of two more cards stop at their top. Not on every card: two
// tested tags mirror their cells across the whole block space and serve a read at every address. A
// card that aliases like that only receives the zeros a wipe is writing anyway.
#define ISO15693_POLLER_WIPE_MAX_BLOCKS ISO15693_POLLER_MAX_BLOCKS

// Wall-clock bound on the sweep, because the block ceiling above is not a tight one.
//
// The absent-run check ends the sweep at the card's top, but only for a card that stops ANSWERING
// there. A card that refuses the write and still serves a read at every address never accumulates a
// run, so it walks all 256 blocks at an estimated 40-70ms a block (see
// ISO15693_POLLER_WIPE_ABSENT_RUN): 10-18 seconds inside a single poller callback, emitting no
// progress past the advertised count.
//
// This is a BACKSTOP, not a tuning knob, and the two errors it sits between are wildly asymmetric:
//   - cutting a legitimate sweep early leaves real data unwiped above the cut -- the privacy failure
//     this whole sweep exists to remove.
//   - letting a pathological card run long makes the user wait, un-abortably, for a wipe they asked for.
// So it is set generously, well clear of any sweep a real card can ask for, and NOT tuned down for the
// second case -- the block ceiling already caps that at roughly the 18s above.
//
// 10 seconds, against the ~3-4s a sweep of 256 accepting blocks costs -- the largest any card can ask
// for, an accepted block being a fraction of a refused one. Those are estimates, which is itself an
// argument for the wide margin.
// Not the true worst case: one re-probe of the trailing run follows it, is not deadline-checked (it
// would have to abandon the run half-classified), and is bounded by the run length.
//
// BOTH passes use this, and the shared name is deliberate: the value has to be defensible for the
// more expensive of the two. Neither dominates. The wipe's extra costs RELATIVE TO THE CLONE are
// driven by absences -- a card-present inventory every ISO15693_POLLER_WIPE_ABSENT_RUN of them,
// and a re-probe of a tripped run -- so on the very card this bound exists for, which accumulates
// none, the two cost the same. The clone pays more only when the source's block size exceeds the
// target's.
//
// The per-block figures are not "a 4-byte write" as a property of the code: the wipe's payload is
// iso15693_3_get_block_size(target) clamped to ISO15693_MAX_BLOCK_SIZE. They were estimated on 4-byte
// blocks.
//
// See the capacity test in write_source_blocks for the one place this bound costs something.
#define ISO15693_POLLER_PASS_MAX_MS (10000U)

// How many CONSECUTIVE absent-looking blocks end the sweep. One is not evidence -- the same reasoning
// as the per-block write retries -- and this is the sole guard against the sweep mistaking a momentary
// RF dropout for the top of the card, so it is set for that, not for speed.
//
// The hazard is a coupling wobble that recovers: a card shifting on the case back for tens of
// milliseconds is too brief for the card-present inventory below to notice anything wrong, but long
// enough to kill a handful of consecutive blocks. Read as the top of the card, that ends the sweep
// early and reports a clean Success over blocks that were never cleared -- the same silent residue this
// sweep exists to remove. (An actual removal is a different case and is already handled: a hand takes
// 200ms+, so the inventory finds the card gone and the wipe reports CardLost.)
//
// Writing one block off costs 40-70ms (three refused writes, three 5ms waits, one refused read). A
// refused write answers IN BAND (Iso15693_3ErrorInternal) rather than burning the full FDT timeout, so
// the waits are a large share of that -- which is what makes ISO15693_POLLER_VERIFY_RETRY_MS move the
// figure materially. A card running with the OPTION flag refuses in silence instead, so there each
// attempt costs a frame timeout and a read-back. Two runs disagree by about 2x and neither was
// instrumented for this, so treat 40-70ms as an estimate, not a measurement. At 8 blocks it is a few
// hundred ms of tolerance and about the same again spent past the card's real top on every wipe,
// against a ~1s wipe. That cost is the reason not to keep raising it.
//
// The run length is deliberately NOT the only guard: above the advertised count the re-probe at the
// trip is what makes a filled run recoverable, so this number sets how much dropout is absorbed
// silently rather than how much is caught. (Below that count the sweep never stops on absence at all.)
// What remains: a card whose memory is present but answers neither a write nor a read across a whole run,
// even on re-probe, is indistinguishable from one that ends there by any means available here. What keeps
// an over-claiming card honest is not this number but the tail-drop rule, which judges a trailing run by
// whether it answers rather than by the advertised count -- see the sweep's absence handling.
#define ISO15693_POLLER_WIPE_ABSENT_RUN (8U)

// Write-mode state machine. Each verify runs after a NfcCommandReset field power-cycle.
typedef enum {
    Iso15693WriteStateStart, // note the current UID, send the backdoor UID (gen2, or gen1 on an
        // opt-in gen1 run), request a field reset
    Iso15693WriteStateVerifyGen2, // verify gen2: on a match write the payload; if the UID is untouched
        // report NotGen2 and STOP so the scene can offer the gen1 opt-in (it is not sent from here)
    Iso15693WriteStateVerifyGen1, // verify the opt-in gen1 UID; on a match write the payload
    Iso15693WriteStateVerifyWipe, // the sweep is done: check the wipe didn't move the card's UID
    Iso15693WriteStateVerifyClone, // a gen2-path clone sent writes to 56/57: re-read the UID first
} Iso15693WriteState;

struct Iso15693Poller {
    NfcPoller* poller;
    Iso15693_3Data* data; // last read result (Info mode), kept for the info scene after poller free
    Iso15693PollerMode mode;
    uint8_t target_uid[ISO15693_3_UID_SIZE];
    uint8_t original_uid[ISO15693_3_UID_SIZE]; // UID before the write, to gate the gen1 fallback
    // The UID every data-block write is addressed to. A field of its own rather than a reuse of
    // original_uid, because a wipe zeroes blocks 56/57 -- which on a gen1 card ARE the UID -- so this
    // one has to move mid-pass while original_uid stays put for VerifyWipe to compare against.
    uint8_t address_uid[ISO15693_3_UID_SIZE];
    // Set once, by a tag answering error 0x03 to a write, and then carried for the rest of the run.
    // See ISO15693_POLLER_OPTION_FLAG for why it is not decided from the UID.
    bool write_option;
    // The addressed WRITE BLOCK frame and the response it gets. Owned for the poller's whole life
    // rather than allocated per block: the clone pass sends up to 256 of these in one loop.
    BitBuffer* frame_tx;
    BitBuffer* frame_rx;
    Iso15693WriteState write_state;
    // This run is the opt-in gen1 attempt, entered from the "not gen2 magic" screen: Start sends the
    // gen1 sequence instead of the gen2 one, and VerifyGen1 rather than VerifyGen2 does the checking.
    bool attempt_gen1;
    uint32_t activation_errors; // consecutive activation failures (no card) -> timeout
    Iso15693_3Data* clone_source; // kept apart from `data` so start_internal's reset cannot wipe it
    // Everything from here down to `callback` is this run's reporting state, and get_result copies
    // it into an Iso15693PollerResult one assignment per field -- except where a field notes that it
    // does not. THAT function is the mapping, and iso15693_poller.h owns what each
    // field means, so this side comments only what the header cannot know. The `clone_` fields
    // serve a wipe too, which is why clone_blocks_total ends up holding a wipe's measured block
    // count.
    uint16_t clone_blocks_total;
    uint16_t clone_failed_count;
    uint16_t clone_over_capacity;
    uint16_t wipe_advertised;
    // Set by the WIPE's sweep and by the CLONE's data pass -- see the truncation break in
    // write_source_blocks -- so a consumer that wants one mode must check the mode itself.
    bool pass_truncated;
    uint16_t pass_cut_block;
    bool uid_verified;
    uint8_t clone_failed_bitmap[ISO15693_POLLER_BLOCK_BITMAP_SIZE];
    bool clone_used_gen1;
    bool clone_gen1_blocks_skipped;
    bool clone_gen1_data_lost;
    // A frame went to 56/57 on the gen2 path, so the identity is re-read before the run reports -- see
    // Iso15693WriteStateVerifyClone.
    bool clone_uid_recheck;
    // The survey's results, copied out by get_result; the result struct says what each one means.
    bool clone_residue_found;
    uint16_t clone_residue_first;
    uint16_t clone_residue_last;
    uint16_t clone_survey_top;
    bool clone_holds_more;
    bool clone_memory_differs;
    bool clone_ic_ref_differs;
    uint16_t clone_file_blocks;
    uint8_t clone_file_ic_ref;
    // A write to 56/57 moved the UID to exactly what that write implies, so those addresses are
    // registers and this is gen1 silicon -- whichever path the run took to get here. Run control:
    // get_result does not report it.
    bool uid_moved_by_write;
    uint16_t clone_card_blocks;
    // Whether clone_card_blocks holds a CLAIM at all. The size finding says the card is bigger than
    // it claims, so it needs one: a GET SYSTEM INFO that did not answer, or a card that does not
    // advertise memory, leaves the count at zero and every readable block above the source would
    // then look like memory the card does not claim. That would be a statement about the user's
    // hardware invented out of one absent frame, which is the inference this whole pass is built to
    // refuse. get_result does not report it; holds_more already carries the answer.
    bool clone_card_blocks_known;
    uint8_t clone_card_ic_ref;
    bool clone_capacity_confirmed;
    // Two flags, one result field: get_result ORs them behind a mode gate. Both are decided by a GET
    // SYSTEM INFO read-back rather than by the write's return value -- see write_identity for why the
    // return value cannot settle it.
    bool clone_afi_failed;
    bool clone_dsfid_failed;
    uint16_t clone_blocks_done;
    // Last progress band emitted this pass (see PROGRESS_STEPS). Internal bookkeeping: get_result
    // does not report it.
    uint8_t progress_step;
    bool uid_unexpected;
    uint8_t uid_readback[ISO15693_3_UID_SIZE];
    // Kept as its own field, separate from attempt_gen1, so the scene needn't know when the gen1
    // frames go out -- and set immediately before that send, so it means they DID. Set any earlier,
    // in start_internal beside attempt_gen1 say, it would be true for a run that merely ASKED for
    // gen1. That includes a run whose card never activates, where write_step is never entered and
    // nothing is transmitted, and that one is reachable straight from the opt-in screen: the field
    // is off while the user reads the consent text, and the budget to re-present the card is then
    // ISO15693_POLLER_MAX_ACTIVATION_ERRORS x ~100 ms, about four seconds. The scene reports spent
    // gen1 registers on card-lost, so an early flag would assert destroyed blocks on the likeliest
    // outcome of that screen. Setting it at the send also covers the two paths that return from
    // Start before it -- a Write-UID asking for the card's own UID, and an empty clone source.
    bool gen1_attempted;
    bool uid_unverifiable;
    bool uid_changed; // set only from a positive observation -- see Iso15693WriteStateVerifyWipe
    Iso15693PollerCallback callback;
    void* context;
    bool running;
};

// gen2 frame: 02 E0 09 <ref> d0 d1 d2 d3 (+CRC).
//
// UNADDRESSED, AND NOT BY CHOICE. MEASURED on all four gen2 cards tested: each takes this form and
// refuses the addressed one, with the correct UID, with and without OPTION -- and the three asked
// also refuse the address bit set with no UID in the frame. All four take an addressed standard
// WRITE BLOCK and go silent on a wrong address, so the frames are well formed and it is the
// backdoor that is unreachable that way. Adding an address here stops the sequence writing. gen1
// can be addressed because it uses the STANDARD write command; see
// iso15693_poller_send_backdoor_uid_gen1.
//
// SO A BYSTANDER THAT IS ITSELF A GEN2 MAGIC CARD TAKES THESE FRAMES and nothing here prevents it.
// Narrower than an unaddressed standard write, which any writable tag in the field takes, and not
// zero.
static void iso15693_poller_build_gen2_frame(
    BitBuffer* tx,
    uint8_t ref,
    uint8_t d0,
    uint8_t d1,
    uint8_t d2,
    uint8_t d3) {
    bit_buffer_reset(tx);
    bit_buffer_append_byte(tx, ISO15693_MAGIC_FLAGS);
    bit_buffer_append_byte(tx, ISO15693_MAGIC_CMD_WRITE_V2);
    bit_buffer_append_byte(tx, ISO15693_MAGIC_V2_SUB);
    bit_buffer_append_byte(tx, ref);
    bit_buffer_append_byte(tx, d0);
    bit_buffer_append_byte(tx, d1);
    bit_buffer_append_byte(tx, d2);
    bit_buffer_append_byte(tx, d3);
}

// The gen2 CFG block also programs what the card *reports* for system-info geometry / IC ref. For a
// clone these are the source's values (so the copy advertises the same chip identity); otherwise the
// fixed magic defaults.
static void iso15693_poller_send_backdoor_uid_gen2(
    Iso15693_3Poller* iso_poller,
    const uint8_t* uid,
    uint8_t cfg_maxblock,
    uint8_t cfg_blocksize,
    uint8_t cfg_icref) {
    BitBuffer* tx = bit_buffer_alloc(ISO15693_POLLER_BUF_SIZE);
    BitBuffer* rx = bit_buffer_alloc(ISO15693_POLLER_BUF_SIZE);

    iso15693_poller_build_gen2_frame(
        tx, ISO15693_MAGIC_V2_BLK_CFG, cfg_maxblock, cfg_blocksize, cfg_icref, 0x00);
    iso15693_3_poller_send_frame(iso_poller, tx, rx, ISO15693_3_FDT_WRITE_POLL_FC);

    iso15693_poller_build_gen2_frame(tx, ISO15693_MAGIC_V2_BLK_CFG2, 0x00, 0x00, 0x00, 0x00);
    iso15693_3_poller_send_frame(iso_poller, tx, rx, ISO15693_3_FDT_WRITE_POLL_FC);

    iso15693_poller_build_gen2_frame(
        tx, ISO15693_MAGIC_V2_BLK_UID_7654, uid[7], uid[6], uid[5], uid[4]);
    iso15693_3_poller_send_frame(iso_poller, tx, rx, ISO15693_3_FDT_WRITE_POLL_FC);

    iso15693_poller_build_gen2_frame(
        tx, ISO15693_MAGIC_V2_BLK_UID_3210, uid[3], uid[2], uid[1], uid[0]);
    iso15693_3_poller_send_frame(iso_poller, tx, rx, ISO15693_3_FDT_WRITE_POLL_FC);

    bit_buffer_free(tx);
    bit_buffer_free(rx);
}

// Did the tag refuse because the OPTION flag was clear, rather than for any of the other reasons a
// write is refused? Error 0x03 is the only one that means that, and parse_write_response folds it in
// with NOT_SUPPORTED the way the SDK does -- so it is read here, from the frame, before that happens.
static bool iso15693_poller_response_wants_option(const BitBuffer* rx) {
    return bit_buffer_get_size_bytes(rx) >= 2 &&
           (bit_buffer_get_byte(rx, 0) & ISO15693_3_RESP_FLAG_ERROR) != 0 &&
           bit_buffer_get_byte(rx, 1) == ISO15693_3_RESP_ERROR_OPTION;
}

// The flags every addressed write carries: the base set, plus OPTION once the card has asked for it.
static uint8_t iso15693_poller_write_flags(const Iso15693Poller* instance) {
    return instance->write_option ?
               (uint8_t)(ISO15693_POLLER_WRITE_FLAGS | ISO15693_POLLER_OPTION_FLAG) :
               (uint8_t)ISO15693_POLLER_WRITE_FLAGS;
}

// Sticky, and never cleared inside a run: the answer is about the silicon, and the silicon does not
// change when the UID does. Clearing it would re-ask the question on a card whose identity this app
// has just rewritten, which is the one moment the UID cannot be trusted to describe it.
//
// Which command drew the 0x03 does not matter -- the card is objecting to the flag, not to the
// request -- so the identity pass and the block pass answer the same question and share the answer.
static void iso15693_poller_note_option_wanted(Iso15693Poller* instance, const BitBuffer* rx) {
    if(instance->write_option || !iso15693_poller_response_wants_option(rx)) return;
    FURI_LOG_I(TAG, "the card wants the OPTION flag on writes; setting it for this run");
    instance->write_option = true;
}

// The UID goes out LEAST SIGNIFICANT BYTE FIRST -- the reverse of the order every screen, every .nfc
// file and `hf 15 reader` print it in. uid[0] is the 0xE0, so it is the LAST byte on the wire. The
// wrong order does not fail loudly: it addresses a card that is not in the field, so every write is
// met with silence and the card reads as one that refuses everything.
static void iso15693_poller_append_address(BitBuffer* tx, const uint8_t* uid) {
    for(uint8_t i = ISO15693_3_UID_SIZE; i > 0; i--) {
        bit_buffer_append_byte(tx, uid[i - 1]);
    }
}

// One identity write: 22 27|29 <uid, LSB first> <value>, or 62 with the OPTION flag.
//
// ADDRESSED, like the data blocks: WRITE AFI and WRITE DSFID are STANDARD commands, so a bystander of
// any size takes them, and a changed AFI can drop a tag out of a selective inventory -- a card that
// then looks absent to the reader that uses it. Measured accepted on TI Tag-it, the one chip tested that
// wants the OPTION flag.
//
// The send's return is discarded on purpose -- see write_identity below for why it cannot settle
// anything either way -- but the RESPONSE is still worth reading for that one thing.
static void iso15693_poller_send_identity_frame(
    Iso15693Poller* instance,
    Iso15693_3Poller* iso_poller,
    uint8_t command,
    uint8_t value) {
    bit_buffer_reset(instance->frame_tx);
    bit_buffer_append_byte(instance->frame_tx, iso15693_poller_write_flags(instance));
    bit_buffer_append_byte(instance->frame_tx, command);
    iso15693_poller_append_address(instance->frame_tx, instance->address_uid);
    bit_buffer_append_byte(instance->frame_tx, value);
    if(iso15693_3_poller_send_frame(
           iso_poller, instance->frame_tx, instance->frame_rx, ISO15693_3_FDT_WRITE_POLL_FC) ==
       Iso15693_3ErrorNone) {
        iso15693_poller_note_option_wanted(instance, instance->frame_rx);
    }
}

// Make the clone match the source's AFI / DSFID via the standard ISO15693 WRITE AFI / WRITE DSFID
// commands (only for fields the source actually reported), sent by iso15693_poller_send_identity_frame.
// Each field is then READ BACK with GET SYSTEM INFO and compared; a field that doesn't match is
// recorded (clone_afi_failed / clone_dsfid_failed), which downgrades the clone to Partial but never
// fails it, since these are identity extras, not the core UID/data payload.
static void
    iso15693_poller_write_identity(Iso15693Poller* instance, Iso15693_3Poller* iso_poller) {
    const Iso15693_3SystemInfo* sys = &instance->clone_source->system_info;
    const bool want_dsfid = (sys->flags & ISO15693_3_SYSINFO_FLAG_DSFID) != 0;
    const bool want_afi = (sys->flags & ISO15693_3_SYSINFO_FLAG_AFI) != 0;
    if(!want_dsfid && !want_afi) return; // source reported neither field: nothing to reproduce

    // A field is only "written" once we have READ IT BACK and it matches, exactly like the UID. The
    // send return can't be trusted on its own: a tag refuses in-band, answering with the error flag set
    // in the response's flags byte plus an error code -- a well-formed, CRC-valid frame -- so
    // iso15693_3_poller_send_frame returns None and the refusal is invisible. (The SDK's own
    // write_block catches that by following send_frame with iso15693_3_write_block_response_parse;
    // there is no equivalent for WRITE AFI / WRITE DSFID.) The converse is just as wrong: a tag that
    // applies the write without answering looks like a failure. GET SYSTEM INFO is the only way to
    // read AFI/DSFID back, and it returns both, so one call verifies both fields.
    //
    // Retry the write+verify pair so a transient RF error isn't mistaken for a refusal -- the same
    // reasoning as the block loop's retries.
    bool dsfid_ok = !want_dsfid;
    bool afi_ok = !want_afi;
    for(uint32_t attempt = 0; attempt < ISO15693_POLLER_WRITE_ATTEMPTS && (!dsfid_ok || !afi_ok);
        attempt++) {
        if(!dsfid_ok) {
            iso15693_poller_send_identity_frame(
                instance, iso_poller, ISO15693_MAGIC_CMD_WRITE_DSFID, sys->dsfid);
        }
        if(!afi_ok) {
            iso15693_poller_send_identity_frame(
                instance, iso_poller, ISO15693_MAGIC_CMD_WRITE_AFI, sys->afi);
        }

        Iso15693_3SystemInfo readback = {0};
        if(iso15693_3_poller_get_system_info(iso_poller, &readback) == Iso15693_3ErrorNone) {
            // Require the target to ADVERTISE the field as well as hold the right value: a copy that
            // no longer reports the AFI/DSFID the source reported isn't a faithful clone either.
            if(!dsfid_ok && (readback.flags & ISO15693_3_SYSINFO_FLAG_DSFID) &&
               readback.dsfid == sys->dsfid) {
                dsfid_ok = true;
            }
            if(!afi_ok && (readback.flags & ISO15693_3_SYSINFO_FLAG_AFI) &&
               readback.afi == sys->afi) {
                afi_ok = true;
            }
        }
        if(dsfid_ok && afi_ok) break;
        furi_delay_ms(ISO15693_POLLER_VERIFY_RETRY_MS);
    }

    if(!dsfid_ok) FURI_LOG_W(TAG, "DSFID did not read back as written");
    if(!afi_ok) FURI_LOG_W(TAG, "AFI did not read back as written");
    instance->clone_dsfid_failed = !dsfid_ok;
    instance->clone_afi_failed = !afi_ok;
}

static void iso15693_poller_report(Iso15693Poller* instance, Iso15693PollerEvent event) {
    if(instance->callback) {
        instance->callback(event, instance->context);
    }
}

// Publish how far the block pass has got, at most ISO15693_POLLER_PROGRESS_STEPS + 1 times per pass.
// The bound is what keeps a Back press from deadlocking the app -- read the comment on that macro
// before changing anything here.
static void
    iso15693_poller_report_progress(Iso15693Poller* instance, uint16_t done, uint16_t total) {
    instance->clone_blocks_done = done;
    if(total == 0) return;

    // Emit only when we cross into a new 1/STEPS band. `step` is monotone in `done` and capped at
    // STEPS, so this can fire at most STEPS+1 times per pass -- the safety bound is structural
    // rather than something a second guard has to enforce.
    const uint8_t step = (uint8_t)(((uint32_t)done * ISO15693_POLLER_PROGRESS_STEPS) / total);
    if(step == instance->progress_step) return;
    instance->progress_step = step;

    iso15693_poller_report(instance, Iso15693PollerEventWriteProgress);
}

// Both defined below, at the inventory helper the first of them wraps.
static bool iso15693_poller_card_still_present(Iso15693_3Poller* iso_poller);
static Iso15693_3Error
    iso15693_poller_verify_inventory(Iso15693_3Poller* iso_poller, uint8_t* uid);

// Addressed WRITE BLOCK: 22 21 <uid, LSB first> <block> <data...>, with the CRC appended by
// iso15693_3_poller_send_frame. The address byte order is at iso15693_poller_append_address.
static void iso15693_poller_build_write_frame(
    BitBuffer* tx,
    uint8_t flags,
    const uint8_t* uid,
    uint8_t block,
    const uint8_t* data,
    uint8_t size) {
    bit_buffer_reset(tx);
    bit_buffer_append_byte(tx, flags);
    bit_buffer_append_byte(tx, ISO15693_3_CMD_WRITE_BLOCK);
    iso15693_poller_append_address(tx, uid);
    bit_buffer_append_byte(tx, block);
    bit_buffer_append_bytes(tx, data, size);
}

// What iso15693_3_write_block_response_parse would return, written out here because that function is
// internal to lib/nfc and this file builds its own frame.
//
// The case it exists for: a tag that REFUSES in band answers with a well-formed, CRC-valid error
// frame, so send_frame reports success and the refusal is invisible without this. A write that took
// answers with a lone zero flags byte. The error mapping is the SDK's, so the same tag response means
// the same thing on both paths -- nothing downstream reads more than "is this None", but a log line
// naming a different error from the rest of the app would send a bench session after the wrong thing.
static Iso15693_3Error iso15693_poller_parse_write_response(const BitBuffer* rx) {
    const size_t len = bit_buffer_get_size_bytes(rx);
    if(len == 0) return Iso15693_3ErrorBufferEmpty;

    if((bit_buffer_get_byte(rx, 0) & ISO15693_3_RESP_FLAG_ERROR) == 0) {
        return len == 1 ? Iso15693_3ErrorNone : Iso15693_3ErrorUnexpectedResponse;
    }
    if(len < 2) return Iso15693_3ErrorUnexpectedResponse; // error flag set, no code to read
    const uint8_t code = bit_buffer_get_byte(rx, 1);
    if(code >= ISO15693_3_RESP_ERROR_CUSTOM_START && code <= ISO15693_3_RESP_ERROR_CUSTOM_END) {
        return Iso15693_3ErrorCustom;
    }
    switch(code) {
    case ISO15693_3_RESP_ERROR_NOT_SUPPORTED:
    case ISO15693_3_RESP_ERROR_OPTION:
        return Iso15693_3ErrorNotSupported;
    case ISO15693_3_RESP_ERROR_FORMAT:
        return Iso15693_3ErrorFormat;
    case ISO15693_3_RESP_ERROR_BLOCK_UNAVAILABLE:
    case ISO15693_3_RESP_ERROR_BLOCK_ALREADY_LOCKED:
    case ISO15693_3_RESP_ERROR_BLOCK_LOCKED:
    case ISO15693_3_RESP_ERROR_BLOCK_WRITE:
    case ISO15693_3_RESP_ERROR_BLOCK_LOCK:
        return Iso15693_3ErrorInternal;
    default:
        return Iso15693_3ErrorUnknown;
    }
}

static Iso15693_3Error iso15693_poller_write_block_addressed(
    Iso15693Poller* instance,
    Iso15693_3Poller* iso_poller,
    const uint8_t* data,
    uint8_t block,
    uint8_t size) {
    iso15693_poller_build_write_frame(
        instance->frame_tx,
        iso15693_poller_write_flags(instance),
        instance->address_uid,
        block,
        data,
        size);
    const Iso15693_3Error error = iso15693_3_poller_send_frame(
        iso_poller, instance->frame_tx, instance->frame_rx, ISO15693_3_FDT_WRITE_POLL_FC);
    if(error != Iso15693_3ErrorNone) return error;

    iso15693_poller_note_option_wanted(instance, instance->frame_rx);
    return iso15693_poller_parse_write_response(instance->frame_rx);
}

// What the UID BECOMES if this write lands on gen1 silicon. The mapping is fixed: block 56 carries
// uid[7..4] and block 57 uid[3..0], each byte in frame order. Computable because we know what we sent.
static void iso15693_poller_predict_uid(
    const uint8_t* current,
    uint8_t block,
    const uint8_t* data,
    uint8_t* predicted) {
    memcpy(predicted, current, ISO15693_3_UID_SIZE);
    if(block == ISO15693_MAGIC_BLK_UID_7654) {
        predicted[7] = data[0];
        predicted[6] = data[1];
        predicted[5] = data[2];
        predicted[4] = data[3];
    } else {
        predicted[3] = data[0];
        predicted[2] = data[1];
        predicted[1] = data[2];
        predicted[0] = data[3];
    }
}

// Take the address again after a write that may have moved it -- but only to one of the two values
// that write could legitimately have produced.
//
// There are exactly two honest outcomes. The UID is UNCHANGED: the write did not move it -- those
// addresses are ordinary memory (gen2 or a plain tag), or the write did not land -- and the address
// stands. Or it is EXACTLY what the write implies, which means they are registers: gen1, and the
// address must follow. Anything else did not come from our write. The inventory is the SDK's 1-slot
// one, so with a second tag in the field it can answer for the bystander (#251) -- and
// re-addressing to a stranger would point every later frame at the wrong card. Keep the address we
// have and say so.
//
// The prediction does not replace the inventory: it holds only if the card is gen1, which is what is
// being tested. It lets the answer be checked rather than believed. Not airtight: a bystander holding
// the predicted UID would still pass, and nothing can fix that -- magic cards make UIDs non-unique by
// construction and a 1-slot inventory cannot tell two cards apart.
static void iso15693_poller_readdress(
    Iso15693Poller* instance,
    Iso15693_3Poller* iso_poller,
    const uint8_t* predicted) {
    uint8_t uid[ISO15693_3_UID_SIZE] = {0};
    if(iso15693_poller_verify_inventory(iso_poller, uid) != Iso15693_3ErrorNone) {
        // Nothing answered, so there is no new address to take and the old one stays. Do NOT fall
        // back to an unaddressed write: that puts the frames back on every tag in the field, at the
        // one moment this card's own identity is in doubt. Whether the run continues is for the
        // sweep's card-present check to decide, on the same inventory this just failed.
        FURI_LOG_W(TAG, "no inventory answer after a UID register write; address unchanged");
        return;
    }
    if(memcmp(instance->address_uid, uid, ISO15693_3_UID_SIZE) == 0) return;

    if(memcmp(uid, predicted, ISO15693_3_UID_SIZE) != 0) {
        FURI_LOG_W(
            TAG, "a UID answered that our own write does not account for; address unchanged");
        return;
    }
    FURI_LOG_W(TAG, "the card's UID moved to what the write implies; re-addressing");
    memcpy(instance->address_uid, uid, ISO15693_3_UID_SIZE);
    instance->uid_moved_by_write = true;
}

// The gen1 backdoor UID sequence: unlock, commit, then the two halves of the UID. The ORDER is the
// point, which is why it is written out here rather than driven off the membership list.
//
// Each frame is an ordinary WRITE BLOCK, sent ADDRESSED through the data blocks' own helper: on any
// tag that big, 56/57/62/63 are user data, and the opt-in's warning covers only the card in the
// user's hand (#251). Each answer is read for one thing, a 0x03, so a card that wants the OPTION flag
// gets it.
//
// Otherwise the results are ignored: no gen1 card tested has accepted 62 or 63, and the UID moves
// anyway, so acting on them would abort a run that worked. The UID read-back is the only honest
// check. NXP ICODE SLIX and SLIX-S refuse in band (0x0F) only when addressed and are silent
// unaddressed; ST LRi2K answers either form with 0x10.
//
// THE RE-ADDRESS IN THE MIDDLE IS LOAD-BEARING. Block 56 carries uid[7..4] and takes effect
// immediately (see ISO15693_MAGIC_BLK_UNLOCK), so by the time block 57 goes out the card has
// already stopped answering to the address the frame before it used. Without it 57 goes to a card
// that is no longer there, and the run ends with half a UID written: on the opt-in path, an
// identity that is neither the one the card had nor the one asked for. readdress accepts only the
// UID this write implies, so on a card that is not gen1 the address simply stands.
static void iso15693_poller_send_backdoor_uid_gen1(
    Iso15693Poller* instance,
    Iso15693_3Poller* iso_poller,
    const uint8_t* uid) {
    static const uint8_t unlock[ISO15693_MAGIC_REGISTER_SIZE] = {0x00, 0x00, 0x00, 0x00};
    static const uint8_t commit[ISO15693_MAGIC_REGISTER_SIZE] = {0x69, 0x96, 0x00, 0x00};
    const uint8_t uid_7654[ISO15693_MAGIC_REGISTER_SIZE] = {uid[7], uid[6], uid[5], uid[4]};
    const uint8_t uid_3210[ISO15693_MAGIC_REGISTER_SIZE] = {uid[3], uid[2], uid[1], uid[0]};

    iso15693_poller_write_block_addressed(
        instance, iso_poller, unlock, ISO15693_MAGIC_BLK_UNLOCK, ISO15693_MAGIC_REGISTER_SIZE);
    iso15693_poller_write_block_addressed(
        instance, iso_poller, commit, ISO15693_MAGIC_BLK_COMMIT, ISO15693_MAGIC_REGISTER_SIZE);

    iso15693_poller_write_block_addressed(
        instance, iso_poller, uid_7654, ISO15693_MAGIC_BLK_UID_7654, ISO15693_MAGIC_REGISTER_SIZE);

    uint8_t predicted[ISO15693_3_UID_SIZE] = {0};
    iso15693_poller_predict_uid(
        instance->address_uid, ISO15693_MAGIC_BLK_UID_7654, uid_7654, predicted);
    iso15693_poller_readdress(instance, iso_poller, predicted);

    iso15693_poller_write_block_addressed(
        instance, iso_poller, uid_3210, ISO15693_MAGIC_BLK_UID_3210, ISO15693_MAGIC_REGISTER_SIZE);
}

// The two UIDs the gen1 sequence can leave when only one of its halves lands: 56's frame without 57's,
// or 57's without 56's. Each is predicted from the original the way the sequence predicts 56, so these
// are the only answers a half-written card can give. A half the two UIDs already share makes one of
// them the original or the target, which the caller rules out first.
static bool
    iso15693_poller_uid_is_half_written(const Iso15693Poller* instance, const uint8_t* uid) {
    const uint8_t* target = instance->target_uid;
    const uint8_t uid_7654[ISO15693_MAGIC_REGISTER_SIZE] = {
        target[7], target[6], target[5], target[4]};
    const uint8_t uid_3210[ISO15693_MAGIC_REGISTER_SIZE] = {
        target[3], target[2], target[1], target[0]};
    uint8_t half[ISO15693_3_UID_SIZE] = {0};
    iso15693_poller_predict_uid(
        instance->original_uid, ISO15693_MAGIC_BLK_UID_7654, uid_7654, half);
    if(memcmp(uid, half, ISO15693_3_UID_SIZE) == 0) return true;
    iso15693_poller_predict_uid(
        instance->original_uid, ISO15693_MAGIC_BLK_UID_3210, uid_3210, half);
    return memcmp(uid, half, ISO15693_3_UID_SIZE) == 0;
}

// Did the block end up holding what we sent? The only question left on a card whose acknowledgement
// we cannot collect. Reads need no OPTION flag and are answered normally, which is what makes this
// available at all.
static bool iso15693_poller_write_landed(
    Iso15693_3Poller* iso_poller,
    const uint8_t* data,
    uint8_t block,
    uint8_t size) {
    uint8_t readback[ISO15693_MAX_BLOCK_SIZE] = {0};
    if(iso15693_3_poller_read_block(iso_poller, readback, block, size) != Iso15693_3ErrorNone) {
        return false;
    }
    return memcmp(readback, data, size) == 0;
}

// The one write both block passes make; ISO15693_POLLER_WRITE_ATTEMPTS says why it retries at all.
// Retries only ever run on a failure, so a card that takes its writes pays nothing for them. There is
// deliberately no break before the last delay, which is where the per-refused-block figure comes from.
//
// Every DATA-block write in this file funnels through here, addressed to instance->address_uid. Note
// that is about the SENDERS, not the blocks -- the wipe's sweep zeroes 56/57/62/63 through this
// function like any other block.
static Iso15693_3Error iso15693_poller_write_block_retried(
    Iso15693Poller* instance,
    Iso15693_3Poller* iso_poller,
    const uint8_t* data,
    uint8_t block,
    uint8_t size) {
    Iso15693_3Error error = Iso15693_3ErrorNone;
    for(uint32_t attempt = 0; attempt < ISO15693_POLLER_WRITE_ATTEMPTS; attempt++) {
        error = iso15693_poller_write_block_addressed(instance, iso_poller, data, block, size);
        if(error == Iso15693_3ErrorNone) break;

        // Silence from a card that asked for the OPTION flag is the one failure not taken at face
        // value: that card is waiting for an EOF this SDK cannot send (ISO15693_POLLER_OPTION_FLAG),
        // so it never answers a write at all. The block is read back instead.
        //
        // Deliberately NOT extended to a card that never asked for the flag. There, silence means the
        // card is gone or the coupling is bad, the retries are the right response, and adding a read
        // to every failed write would double the cost of exactly the path the pass budget is there to
        // bound. And NOT extended to an in-band error either: a card that answers has decided, and a
        // read-back would overrule it.
        if(error == Iso15693_3ErrorTimeout && instance->write_option &&
           iso15693_poller_write_landed(iso_poller, data, block, size)) {
            error = Iso15693_3ErrorNone;
            break;
        }
        furi_delay_ms(ISO15693_POLLER_VERIFY_RETRY_MS);
    }

    // Blocks 56 and 57 ARE the gen1 UID registers, so a write there can move the card's identity --
    // immediately, with no power-cycle, measured on NXP ICODE SLIX, NXP ICODE SLIX-S and ST LRi2K by
    // writing 56 and re-reading the UID in the same field session. Left alone, every later frame would
    // carry an address the card no longer answers to: the wipe's absent run would trip, the tail-drop
    // would discard everything above 56, and a gen1 card -- the one case this whole path exists for --
    // would report a card shorter than the one in the field.
    //
    // Retaken on the ATTEMPT, not on a success: a tag can apply a write without answering, so no
    // answer is not taken to mean no write. If the UID did not move, the inventory returns the same
    // bytes and the cost is one frame. Twice per sweep at most.
    //
    // It lives with the write rather than in the sweep because the rule is about the write: a clone
    // whose source reaches block 56 on a card that is gen1 magic underneath goes through the same
    // line, without that loop having to know it might.
    //
    // Only a full-width write can carry a whole UID half, and the registers are four bytes. A narrower
    // block size cannot produce a predictable move, so there is nothing to check against.
    if(iso15693_poller_is_uid_block(block) && size >= 4) {
        uint8_t predicted[ISO15693_3_UID_SIZE] = {0};
        iso15693_poller_predict_uid(instance->address_uid, block, data, predicted);
        iso15693_poller_readdress(instance, iso_poller, predicted);
    }
    return error;
}

// Mark a block as failed, or take the mark back. A pair rather than a set/clear/test trio because the
// poller never asks whether a bit is set -- only the report does, in another translation unit, through
// nfc_magic_partial_details_any_index.
//
// Named rather than inline bit ops because a slip -- `mark_failed` where `unmark_failed` belongs --
// reads correct in any branch that takes a mark back: `|=` against `&= ~` differs by two characters
// and gives no signal.
//
// Both check the index. The callers keep it in range, but an index past the bitmap would write past
// it, so a slip there is a crash here rather than a corrupted poller.
static void iso15693_poller_mark_failed(Iso15693Poller* instance, uint16_t block) {
    furi_check(block < ISO15693_POLLER_MAX_BLOCKS);
    instance->clone_failed_bitmap[block / 8] |= (uint8_t)(1u << (block % 8));
}

static void iso15693_poller_unmark_failed(Iso15693Poller* instance, uint16_t block) {
    furi_check(block < ISO15693_POLLER_MAX_BLOCKS);
    instance->clone_failed_bitmap[block / 8] &= (uint8_t) ~(1u << (block % 8));
}

static bool iso15693_poller_block_is_empty(const uint8_t* block, uint8_t size) {
    for(uint8_t i = 0; i < size; i++) {
        if(block[i] != 0) return false;
    }
    return true;
}

// Clamp a block size to what a fixed ISO15693_MAX_BLOCK_SIZE buffer -- or a five-bit CFG field --
// can hold. Against the MACRO, never sizeof a particular buffer, which ties the bound to one
// declaration. Four callers, and they are not one case:
//
//   - the CLONE's comes from a loaded .nfc. The SDK's loader gates allocation on block_size > 0 but
//     loads either way, so 0..255 arrives and this is the only bound before a 32-byte stack buffer.
//   - the gen2 CFG derivation reads the same field and matters most: its two bytes are PROGRAMMED
//     INTO THE CARD and outlive the run.
//   - the WIPE's comes off the wire as (size - 1) in five bits, so it is at most 32 and cannot fire.
//   - the SURVEY's is the clone's, already clamped, so it cannot fire either.
//
// Keep the two that cannot fire: each bounds a stack buffer in its own function.
static uint8_t iso15693_poller_clamp_block_size(uint8_t block_size) {
    return block_size > ISO15693_MAX_BLOCK_SIZE ? (uint8_t)ISO15693_MAX_BLOCK_SIZE : block_size;
}

// If this pass has spent its wall-clock budget, CUT IT: record the cut and return true. Both the
// clone's write loop and the wipe's sweep ask, and must answer identically.
//
// pass_cut_block is the EXCLUSIVE END of the range the caller attempted, not a count: the clone
// `continue`s over the four backdoor blocks on a gen1 run, so its cursor runs ahead of what it
// tried, and `done` is its count. A caller passing anything but its own block cursor -- the re-probe
// is the tempting one -- records an index the screens will state as fact.
//
// Call it BEFORE the block is attempted. Elapsed-against-budget, never an absolute deadline, which
// would not survive a tick wraparound. The budget is a parameter so the clone's can be raised on
// its own, which is the trade the KNOWN OVERLAP note at failures_are_top_tail weighs. Logging stays
// at the call sites, the one thing that differs.
static bool iso15693_poller_cut_pass_if_expired(
    Iso15693Poller* instance,
    uint32_t pass_start,
    uint32_t pass_budget,
    uint16_t block) {
    if(furi_get_tick() - pass_start <= pass_budget) return false;
    instance->pass_truncated = true;
    instance->pass_cut_block = block;
    return true;
}

// Does the card hold readable blocks ABOVE the source, and do any of them still carry data?
//
// A clone writes the source's blocks and nothing else, which is right -- destroying what the user did
// not ask about is the wipe's job, not this one. But on a card bigger than the source, everything
// above it keeps the PREVIOUS card's contents, and the user is not otherwise told: on a gen2 clone the
// CFG frame rewrites the advertised count down to the source's, so the card then claims to end where
// the source did and an ordinary dump shows a clean copy. Measured on a 64-block card cloned from a
// 28-block source -- it reported 28, and blocks 28..63 still read the data written before the clone.
//
// THE ADVERTISED COUNT CANNOT BE THE TEST, before or after: on a magic card it is whatever was last
// programmed. So this READS, which is non-destructive (the wipe's sweep writes because it is
// wiping), and on the cards measured a block past physical capacity refuses reads outright (see
// ISO15693_POLLER_WIPE_MAX_BLOCKS) -- the same discriminator the data pass below already uses when it
// asks whether a refused block is even there.
//
// It runs on the gen1 path too. That gen1 cannot misstate its count is known only for the cards
// measured, and the check costs ISO15693_POLLER_WIPE_ABSENT_RUN reads past the card's top.
//
// TWO FACTS COME OUT OF THIS, and they are separate. A block that answers above the count the card
// reports is a statement about its size; one above the source that holds DATA is a statement about
// what the card held before. Conflating them would either warn about every clone onto a wiped card,
// or say nothing at all about a 64-block card now presenting as 28 with a clean tail.
static void iso15693_poller_survey_above_source(
    Iso15693Poller* instance,
    Iso15693_3Poller* iso_poller,
    uint16_t source_count,
    uint8_t block_size) {
    const uint8_t size = iso15693_poller_clamp_block_size(block_size);
    if(size == 0) return;

    const uint32_t start = furi_get_tick();
    const uint32_t budget = furi_ms_to_ticks(ISO15693_POLLER_PASS_MAX_MS);
    uint16_t absent_run = 0;

    for(uint16_t block = source_count; block < ISO15693_POLLER_WIPE_MAX_BLOCKS; block++) {
        // Not cut_pass_if_expired: running out of time says nothing about the clone's own blocks --
        // the survey keeps what it read and claims nothing beyond it -- and marking the PASS truncated
        // would downgrade a clone whose every block landed.
        if(furi_get_tick() - start > budget) break;

        uint8_t probe[ISO15693_MAX_BLOCK_SIZE] = {0};
        if(iso15693_3_poller_read_block(iso_poller, probe, (uint8_t)block, size) !=
           Iso15693_3ErrorNone) {
            if(++absent_run >= ISO15693_POLLER_WIPE_ABSENT_RUN) {
                // A run of absences is the card's top only while the card is still in the field: a
                // lifted card fails every read from wherever it left. What it answered stands -- the
                // residue it held is data that was read -- but the size finding is a claim about
                // where the card ENDS, and a card that left says nothing about that.
                if(!iso15693_poller_card_still_present(iso_poller)) {
                    instance->clone_holds_more = false;
                }
                break;
            }
            continue;
        }
        absent_run = 0;
        // It answered, so it is there -- whatever it holds. That alone settles the size question.
        instance->clone_survey_top = block;
        if(instance->clone_card_blocks_known && block >= instance->clone_card_blocks) {
            instance->clone_holds_more = true;
        }
        if(iso15693_poller_block_is_empty(probe, size)) continue;

        if(!instance->clone_residue_found) {
            instance->clone_residue_found = true;
            instance->clone_residue_first = block;
        }
        instance->clone_residue_last = block;
    }
}

// Does the card report the block count and IC reference the source did? Claims on both sides,
// because a reader shows claims.
//
// gen2 programs its answer through the CFG register, so there the two agree by construction. gen1 has
// no such register, so a gen1 clone carries the source's UID and data on a card that goes on reporting
// its own size and IC reference -- measured on a 40-block SLIX-S wearing a 28-block SLIX's UID.
static void iso15693_poller_compare_reported_geometry(
    Iso15693Poller* instance,
    Iso15693_3Poller* iso_poller) {
    const Iso15693_3SystemInfo* src = &instance->clone_source->system_info;
    Iso15693_3SystemInfo card = {0};
    if(iso15693_3_poller_get_system_info(iso_poller, &card) != Iso15693_3ErrorNone) return;

    // Only what the source actually reported can be compared; a field it never carried is not a
    // mismatch, it is a field with no claim on either side of the comparison.
    //
    // The block count, not the block size. The notes print counts, so a size that alone disagreed
    // would read as two equal numbers -- and on the cards measured no clone that succeeds carries
    // one: a gen2 magic card handed 8-byte blocks took that geometry through its CFG frame and then
    // refused every write, and a gen1 SLIX refused them outright.
    const bool memory_differs = (src->flags & ISO15693_3_SYSINFO_FLAG_MEMORY) &&
                                (card.flags & ISO15693_3_SYSINFO_FLAG_MEMORY) &&
                                card.block_count != src->block_count;
    const bool ic_ref_differs = (src->flags & ISO15693_3_SYSINFO_FLAG_IC_REF) &&
                                (card.flags & ISO15693_3_SYSINFO_FLAG_IC_REF) &&
                                card.ic_ref != src->ic_ref;
    instance->clone_card_blocks = card.block_count;
    instance->clone_card_blocks_known = (card.flags & ISO15693_3_SYSINFO_FLAG_MEMORY) != 0;
    instance->clone_card_ic_ref = card.ic_ref;
    instance->clone_memory_differs = memory_differs;
    instance->clone_ic_ref_differs = ic_ref_differs;
    instance->clone_file_blocks = src->block_count;
    instance->clone_file_ic_ref = src->ic_ref;
}

// The card turned out to be gen1 after the pass had already fed the file into its UID registers. Put
// the identity back and make the accounting say what a gen1 clone's would have said.
//
// The repair writes the whole gen1 sequence, unlock and commit included -- and the gen1 opt-in
// screen was never shown, because this run went down the gen2 path. That consent exists to warn that
// gen1 writes destroy four blocks of user data on a tag that is not gen1. This card has just proved it
// IS gen1, by moving its UID to the value our write implied, so 56/57 are registers -- and on every
// gen1 chip measured 62/63 are too, refusing writes and answering no read -- rather than anyone's
// data, and the warning's premise does not hold. Leaving the card answering to bytes lifted out of
// the file would be the worse outcome by a distance.
//
// backdoor_failed and backdoor_over_capacity are the failures the pass recorded against the four
// before the UID moved, split by the count it put each in. Both counts are needed, not one: a
// register answers no read, so the pass files an empty one as past capacity rather than as a failure.
static void iso15693_poller_finish_conversion(
    Iso15693Poller* instance,
    Iso15693_3Poller* iso_poller,
    uint16_t source_count,
    uint16_t backdoor_failed,
    uint16_t backdoor_over_capacity) {
    instance->clone_used_gen1 = true;

    // The four are registers here, so the file's data at those addresses was never storable -- exactly
    // as on the gen1 path, and reported the same way. Deduct them from the total and take back every
    // failure recorded against them, from the count it went into, so the counts match what a gen1
    // clone would have produced.
    const uint16_t skipped = iso15693_poller_backdoor_blocks_below(source_count);
    for(size_t i = 0; i < COUNT_OF(iso15693_poller_backdoor_blocks); i++) {
        if(iso15693_poller_backdoor_blocks[i] < source_count) {
            iso15693_poller_unmark_failed(instance, iso15693_poller_backdoor_blocks[i]);
        }
    }
    instance->clone_failed_count = (uint16_t)(instance->clone_failed_count - backdoor_failed);
    instance->clone_over_capacity =
        (uint16_t)(instance->clone_over_capacity - backdoor_over_capacity);
    instance->clone_gen1_blocks_skipped = skipped > 0;
    instance->clone_gen1_data_lost =
        skipped > 0 && iso15693_poller_source_uses_gen1_blocks(instance->clone_source);
    instance->clone_blocks_total = (uint16_t)(instance->clone_blocks_total - skipped);

    // Nothing on screen says the identity was disturbed and put back, deliberately. What the user
    // needs is what the CARD is, and the run now reports exactly as a gen1 clone of this file would.
    // The rest is how we got here.
    iso15693_poller_send_backdoor_uid_gen1(instance, iso_poller, instance->target_uid);
    // The sequence re-addresses itself after block 56; this takes the other half, which block 57 has
    // just written. Unconditional rather than an inventory here: the run reached here BECAUSE the UID
    // followed a write to 56/57, so the registers are proven. Whether each frame LANDED is another
    // question: one lost frame is enough to leave the file's bytes in the UID, so the identity is
    // read back after the pass; see Iso15693WriteStateVerifyClone.
    memcpy(instance->address_uid, instance->target_uid, ISO15693_3_UID_SIZE);
}

// Clone mode: write every data block from the source image with the standard ISO15693 WRITE BLOCK.
// Real write errors are counted into the failure bitmap for Partial reporting. Runs synchronously on
// the Nfc worker thread. When `skip_backdoor` is set (the gen1 path), blocks 56/57/62/63 are left
// untouched -- gen1 keeps the UID and its backdoor registers there, so writing source data over them
// would clobber the UID -- and they are excluded from the reported total, so "Cloned X/Y" counts only
// the blocks gen1 can carry. (gen2 passes false: its UID lives in a separate register space, so
// 56/57/62/63 are ordinary data blocks there.)

// Returns false if the card left the field during the loop (the caller reports CardLost instead of a
// write result); true otherwise, with the counters/bitmap describing what happened.
static bool iso15693_poller_write_source_blocks(
    Iso15693Poller* instance,
    Iso15693_3Poller* iso_poller,
    bool skip_backdoor) {
    const Iso15693_3Data* source = instance->clone_source;
    uint16_t source_count = iso15693_3_get_block_count(source);
    // Note this is the SOURCE's geometry: on gen2 the CFG frame makes the target match it, but a gen1
    // target keeps its own block size, so a mismatch there makes every empty failure read as absent
    // and fabricates an over-capacity "Holds X/Y". Why it needs clamping is at the helper.
    const uint8_t block_size = iso15693_poller_clamp_block_size(iso15693_3_get_block_size(source));

    // A block number is a uint8_t on the wire and the failure bitmap holds this many bits, so only the
    // first 256 blocks can be attempted or accounted for. Real ISO15693 tags never exceed this; clamp
    // defensively so a corrupt/hand-edited source can't make clone_blocks_total overstate what was
    // actually written (which would skew the over-capacity "holds X of Y" report).
    //
    // Clamped ONCE, here, and never reassigned -- which is what lets both loops below bound themselves
    // on source_count alone. Re-testing the bitmap size in their headers restated this in two more
    // places without adding a guarantee.
    //
    // The clamp covers runs that REACH this pass. A clone that fails before it -- uid_unexpected, say --
    // reports clone_blocks_total straight from iso15693_3_get_block_count(), unclamped, at the
    // NotGen2/Fail assignment. Nothing indexes an array with it there, so it is a reporting figure
    // only; it is named because "clamped once" reads as a whole-run guarantee and is not one.
    if(source_count > ISO15693_POLLER_MAX_BLOCKS) {
        source_count = ISO15693_POLLER_MAX_BLOCKS;
    }

    // Report the count of blocks we actually attempt: for gen1, exclude whichever of the 4 backdoor
    // registers fall below source_count, so the "Cloned X/Y" total isn't inflated by blocks that only
    // ever hold the UID. Nothing is deducted from a source that ends before block 56.
    const uint16_t skipped = skip_backdoor ? iso15693_poller_backdoor_blocks_below(source_count) :
                                             0;
    const uint16_t total = (uint16_t)(source_count - skipped);
    instance->clone_blocks_total = total;
    // Both set from the SAME count that produced the total. Derived anywhere else, the figure and the
    // flags can disagree, and the scenes cannot catch that: on a source below block 56 the deduction
    // is a no-op, so blocks_total reads identically either way. The gen1 caveat is a claim about the
    // SOURCE -- that it held data at those addresses and the card does not -- so it rests on
    // gen1_data_lost; a file that only reaches them loses nothing there.
    instance->clone_gen1_blocks_skipped = skipped > 0;
    instance->clone_gen1_data_lost = skipped > 0 &&
                                     iso15693_poller_source_uses_gen1_blocks(source);
    instance->clone_failed_count = 0;
    instance->clone_over_capacity = 0;
    instance->clone_capacity_confirmed = false;
    memset(instance->clone_failed_bitmap, 0, sizeof(instance->clone_failed_bitmap));

    if(source_count == 0 || block_size == 0) return true;

    // Attempt every source block; deliberately NOT capped at the target's advertised count. On these
    // magic cards WRITE BLOCK is gated by physical memory, not the reported count (measured on the
    // gen2 cards tested: writes succeed well past it), and capping leaves stale data in the
    // reachable gap when re-cloning onto a card that currently advertises fewer blocks. Only a
    // write tests capacity.
    //
    // A block that survives the retries is a capacity candidate, and only that: the read-probe
    // below decides. Classify each persistent failure by whether the source had data there
    // (non-empty -> data lost; empty -> nothing lost), and track position to confirm below that the
    // failures are a contiguous run at the TOP -- the capacity signature. A scattered failure is
    // not capacity.
    //
    // Do NOT skip blocks locked in the SOURCE image: the source's lock bits describe the ORIGINAL
    // card, not the magic target (which is writable regardless), and locked blocks are exactly where
    // real tags keep provisioned data. Attempt every block.
    //
    // skip_backdoor is the CALLER's decision, made before the card had a chance to contradict it. A
    // write landing in 56/57 and moving the UID contradicts it, so the run switches here and repairs
    // the identity afterwards.
    //
    // Do NOT pre-empt that by skipping these four whenever the card already wears the target UID.
    // That condition does not mean gen1: a real gen2 card re-cloned from a DIFFERENT image sharing
    // its UID reaches it too, and there 56/57/62/63 are ordinary memory. The skip would deduct them
    // from the total and report a clean "Cloned 60/60" over four blocks of the file it chose not to
    // write. Reacting costs a UID that moves and is put back; predicting costs user data, silently.
    bool skipping = skip_backdoor;
    bool converted = false;
    bool counted_56 = false; // converted at 57, so `done` counted 56 too -- see the conversion
    bool uid_block_sent = false; // a frame went to 56/57, which on gen1 silicon is the UID
    // Failures recorded against the four before the UID moved, by the count each went into -- what a
    // conversion has to take back. See finish_conversion for why both are needed.
    uint16_t backdoor_failed = 0;
    uint16_t backdoor_over_capacity = 0;
    bool wrote_any = false; // at least one block accepted a write
    bool wrote_above_failure = false; // a block wrote ABOVE one that failed -> not a capacity tail
    bool any_failure_answered =
        false; // a failed block answered a read -> it exists, so no capacity edge
    uint16_t done =
        0; // blocks attempted -- the NUMERATOR the progress popup shows; `total` above is
    // its denominator, and report_progress takes them in that order
    // Same wall-clock bound the wipe sweep carries, and sharper here: Back is swallowed for the whole
    // ISO15693 write, so this loop is time the user cannot escape. It is bounded only by the source's
    // block count and its card-present check is after the loop, so a card slipping off one block in
    // costs the full write timeout on every remaining block -- about 19s at the 256-block ceiling.
    //
    // Usually this fires on a card that has gone, where its effect is to reach the card-present check
    // below sooner and report CardLost. But it CAN fire with the card still present -- marginal coupling
    // makes blocks succeed only after retries, and enough of those on a large source spends the budget
    // while every write lands -- which is why the cut has to be remembered rather than just logged.
    const uint32_t pass_start = furi_get_tick();
    const uint32_t pass_budget = furi_ms_to_ticks(ISO15693_POLLER_PASS_MAX_MS);
    uint16_t block = 0;
    for(; block < source_count; block++) {
        if(iso15693_poller_cut_pass_if_expired(instance, pass_start, pass_budget, block)) {
            FURI_LOG_W(TAG, "clone: time limit reached at block %u of %u", block, source_count);
            break;
        }
        if(skipping && iso15693_poller_is_backdoor_block(block)) {
            continue; // gen1 owns these; the gen1 UID sequence already wrote them
        }
        // Before the write, so a clean run reports progress too -- the success path below continues
        // straight to the next block.
        iso15693_poller_report_progress(instance, done++, total);
        const uint8_t* block_data = iso15693_3_get_block_data(source, (uint8_t)block);
        const Iso15693_3Error error = iso15693_poller_write_block_retried(
            instance, iso_poller, block_data, (uint8_t)block, block_size);
        if(iso15693_poller_is_uid_block(block)) uid_block_sent = true;
        // A write to 56/57 moved the UID to what that write implies, on a run that reached them
        // because the gen2 verify passed. So this is gen1 silicon that the gen2 path was taken on --
        // possible whenever the card already wore the target UID, since then the verify proves only
        // that the UID matches, not that anything magic happened. From here the run is a gen1 clone:
        // stop feeding the file into registers, and repair the identity after the pass. Decided
        // before the result is read: the UID moving is what shows the write landed, answered or not.
        const bool moved_uid = !skipping && instance->uid_moved_by_write;
        if(moved_uid) {
            FURI_LOG_W(TAG, "clone: 56/57 are UID registers here; continuing as a gen1 clone");
            skipping = true;
            converted = true;
            // That write went to a register, not to memory, answered or not, so it is neither a block
            // written nor a failure. Counted as a success it would suppress the capacity finding: a
            // 28-block card fed a 64-block source is too small whether or not 56 happens to answer.
            // Counted as a failure -- its acknowledgement lost, and the retries sent to the address the
            // card has just left -- it would put an empty block in the over-capacity count and make
            // every later success look like memory above a failure. Nor is it one of the blocks the
            // total counts once finish_conversion deducts the four, so it comes off `done` too. At 57
            // so does 56, counted as memory before the UID moved -- after the loop, though: the figure
            // reported at 57 already includes it, and taking it back here would send the next one
            // backwards.
            done--;
            counted_56 = (block == ISO15693_MAGIC_BLK_UID_3210);
            continue;
        }
        if(error == Iso15693_3ErrorNone) {
            // A success ABOVE an earlier failure means the failures are not a run at the top, so
            // they cannot be the card's capacity edge. Not one recorded at the four before the UID
            // moved, though: finish_conversion takes those back, since they were registers all along.
            const uint16_t taken_back =
                converted ? (uint16_t)(backdoor_failed + backdoor_over_capacity) : 0;
            if(instance->clone_failed_count + instance->clone_over_capacity > taken_back) {
                wrote_above_failure = true;
            }
            wrote_any = true;
            continue;
        }
        FURI_LOG_W(TAG, "clone: block %u refused (err %d)", block, error);
        // Record every failed block in the bitmap so a result screen can name it, whichever bucket
        // it ends up in.
        iso15693_poller_mark_failed(instance, block);

        const bool non_empty = !iso15693_poller_block_is_empty(block_data, block_size);

        // Ask the card whether the refused block is even there, rather than inferring it from the
        // source being empty plus the failures' position. The same question iso15693_poller_wipe_blocks
        // asks, for a different purpose: the wipe asks "does this block still hold data", the clone
        // asks "does this block exist at all".
        //
        // It discriminates because a block past physical capacity refuses reads as well as writes
        // on the cards measured (see ISO15693_POLLER_WIPE_MAX_BLOCKS): writes come back
        // Iso15693_3ErrorInternal, or time out on a card running with the OPTION flag, and reads
        // fail outright. So a block that ANSWERS a read exists, which means its write failure was
        // something else -- a transient that outlasted every retry, or a block the card genuinely
        // refuses -- and calling that the card's capacity edge would be a fabricated claim about
        // the user's hardware. Without this, a transient near the end of the pass produces a
        // perfect contiguous empty tail and the app reports "All data written. Holds 60/64 blocks."
        // about a card that holds 64.
        //
        // Paid on EVERY persistent failure, not only the empty ones: emptiness decides which BUCKET a
        // failure lands in, but a run containing ANY block that answers is not a capacity edge whatever
        // its shape or contents, and "Card too small" is the strongest claim this feature makes about
        // the user's hardware.
        uint8_t probe[ISO15693_MAX_BLOCK_SIZE] = {0};
        const bool block_absent =
            iso15693_3_poller_read_block(iso_poller, probe, (uint8_t)block, block_size) !=
            Iso15693_3ErrorNone;
        if(!block_absent) {
            FURI_LOG_W(
                TAG, "clone: block %u refused the write but answers reads", (unsigned)block);
            any_failure_answered = true;
        }

        const bool backdoor = iso15693_poller_is_backdoor_block(block);
        if(non_empty || !block_absent) {
            instance->clone_failed_count++;
            if(backdoor) backdoor_failed++;
        } else {
            instance->clone_over_capacity++;
            if(backdoor) backdoor_over_capacity++;
        }
    }
    // Blocks the clock cut off were never attempted, so nothing has recorded them. Left alone they
    // would be counted as written -- the partial screen derives its "cloned" figure by subtracting the
    // failures from the total -- so a clone stopped at block 10 of 256 would claim all 256 landed.
    // Record them as failures, which is what they are, and the bitmap then names them in Details.
    // Only MEANINGFUL when the card is still present -- not only reachable. This back-fill runs after
    // every break, the card-present check is below it, and a cut caused by the card leaving is
    // reported as CardLost, whose counters the caller discards. So the work is done either way and is
    // simply thrown away on that path.
    for(; block < source_count; block++) {
        if(skipping && iso15693_poller_is_backdoor_block(block)) {
            continue;
        }
        iso15693_poller_mark_failed(instance, block);
        instance->clone_failed_count++;
    }

    if(converted) {
        iso15693_poller_finish_conversion(
            instance, iso_poller, source_count, backdoor_failed, backdoor_over_capacity);
    }
    instance->clone_uid_recheck = uid_block_sent;
    // The popup's last frame is not sent from here -- see iso15693_poller_finish_progress.
    instance->clone_blocks_done = counted_56 ? (uint16_t)(done - 1) : done;

    // Second half of the capacity test, answering a different question from the first. The per-block
    // read-back above establishes each excused block is ABSENT; this establishes they form a contiguous
    // run at the very top with at least one block below them having written -- the shape of "source
    // bigger than the card". A hole in otherwise-writable memory passes the first test, not this one.
    //
    // Tracked as "did anything write above a failure" rather than by comparing block indices: on the
    // gen1 path blocks 56/57/62/63 are skipped, so a capacity edge landing near them leaves a gap
    // between the last success and the first failure, and an index comparison reads that gap as a
    // scattered failure and downgrades a lossless clone to Partial.
    const bool any_failure = (instance->clone_failed_count + instance->clone_over_capacity) > 0;

    // A card lifted mid-loop makes every remaining block fail, which is EXACTLY the shape of the card's
    // physical capacity ending there -- and if the source's tail happens to be empty it would be
    // classified as over-capacity and reported as a clean "All data written." So before making any
    // capacity claim, check the card is still there. Only on the failure path, so a clean write pays
    // nothing. (Bail before classifying: the caller reports CardLost and ignores these counters.)
    if(any_failure && !iso15693_poller_card_still_present(iso_poller)) return false;

    // Position AND evidence: a contiguous run at the top is the shape of a capacity edge, but only a
    // run whose members all refused reads as well as writes is one.
    //
    // And only if they were ASKED. A pass the clock cut records its unreached blocks as failures, which
    // is the same top-tail shape with none of the meaning -- those blocks refused nothing, they were
    // never attempted. Left in, a slow-but-healthy card gets "Card too small", which is the fabricated
    // hardware claim the read-probe above exists to prevent, reached from the other direction.
    //
    // KNOWN OVERLAP, and deliberate. A genuinely-too-small card presents one long run of failing
    // blocks, and failing blocks cost an estimated 40-70ms each, so a source ~150+ blocks larger
    // than the target spends ISO15693_POLLER_PASS_MAX_MS inside that run, truncates, and loses the
    // "Card too small" line to this guard. Retrying is cut in the same place, so nothing ever
    // names the cause.
    //
    // Left as it is because of the direction of the error: dropping the guard would let a cut claim
    // capacity from partial evidence, since the run below the cut looks like a top tail whether or not
    // memory resumes above it. Under-claiming leaves a correct report and a Retry, which clears
    // only a transient; over-claiming gives a verdict about the user's hardware the pass never
    // finished testing. A longer budget for the clone alone would close it, at the cost of more
    // seconds swallowing Back -- #253.
    const bool failures_are_top_tail = any_failure && wrote_any && !wrote_above_failure &&
                                       !any_failure_answered && !instance->pass_truncated;
    if(failures_are_top_tail) {
        instance->clone_capacity_confirmed = true;
    } else if(instance->clone_over_capacity > 0) {
        // Not a clean capacity tail (scattered/anomalous) -> don't report an over-capacity "success";
        // fold the empty failures into the plain failure count (the bitmap already carries them).
        instance->clone_failed_count += instance->clone_over_capacity;
        instance->clone_over_capacity = 0;
    }
    iso15693_poller_compare_reported_geometry(instance, iso_poller);
    iso15693_poller_survey_above_source(instance, iso_poller, source_count, block_size);
    return true;
}

// Did this block hold data at activation? Only TRUE means anything: a non-zero byte can only have come
// from a real read, so it proves the block existed and held data. FALSE proves nothing -- see the
// read-back note in the sweep for why a zeroed entry is a failed read and an empty block alike.
// The range check is not defensive; iso15693_3_get_block_data furi_check()s its index.
static bool
    iso15693_poller_block_held_data(const Iso15693_3Data* data, uint16_t block, uint8_t size) {
    if(block >= iso15693_3_get_block_count(data)) return false;
    const uint8_t* cached = iso15693_3_get_block_data(data, (uint8_t)block);
    return !iso15693_poller_block_is_empty(cached, size);
}

// Resolves a whole run at once, which the tail-drop at the end of the sweep does NOT -- it decides
// block by block and can drop a block below one this kept. See the reconciliation there; the two rules
// only agree because the activation cache is a prefix.
//
// A block just proved it exists. Every absence still open below it is therefore interior memory that
// answered nothing -- a fault, not the space above the card -- so those count (their bits are already
// set) and the run closes. This block becomes the top the report is measured against.
static void iso15693_poller_wipe_note_present(
    Iso15693Poller* instance,
    uint16_t block,
    uint16_t* absent_run,
    bool* any_present,
    uint16_t* highest_present) {
    instance->clone_failed_count += *absent_run;
    *absent_run = 0;
    *any_present = true;
    if(block > *highest_present) *highest_present = block;
}

// The wipe's block sweep. Its contract is iso15693_poller_start_wipe's, and the bound it runs to is
// ISO15693_POLLER_WIPE_MAX_BLOCKS, with the hardware measurement behind it. Decided HERE:
// We attempt every block rather than pre-skipping the target's locked ones: a magic card often ignores
// its own lock bits and accepts the write. Each refused write is then classified by a FRESH read-back
// (not the copy taken at activation -- see below), which answers a question the write cannot:
//   read OK, block non-zero -> write-protected and still holding data. A real failure; the wipe's
//                              promise wasn't kept there. Counted, and named in the bitmap.
//   read OK, block zero     -> already clear. Nothing was lost, so not a failure.
//   read FAILS              -> the block answers nothing at all, so it probably isn't there. Provisional:
//                              position decides. A later block that DOES answer proves this one interior
//                              and unreadable, which is not something to write off -- counted, fail
//                              closed. A run that the sweep ends on is the space above the card's real
//                              top, which was never the card's to clear -- dropped entirely.
// That last distinction is what stops a card advertising more blocks than it holds from reporting a
// partial wipe for blocks that do not exist.
// The gen1 backdoor registers live in this same block-number space and ARE cleared, deliberately:
// skipping them would leave four blocks of real user data behind on every gen2 card, where they are
// ordinary memory, to hedge a hazard only gen1 has. That hazard is the OPEN QUESTION in the loop below.
// Returns the number of blocks that actually accepted the zero-write, so the caller can tell a
// genuine wipe from one where nothing could be cleared.
static uint16_t iso15693_poller_wipe_blocks(
    Iso15693Poller* instance,
    Iso15693_3Poller* iso_poller,
    bool* card_lost) {
    const Iso15693_3Data* target = nfc_poller_get_data(instance->poller);
    const uint16_t advertised = iso15693_3_get_block_count(target);
    const uint8_t block_size = iso15693_3_get_block_size(target);

    instance->wipe_advertised = advertised;
    instance->clone_failed_count = 0;
    instance->clone_over_capacity = 0; // clone-only: a wipe never reports an over-capacity success
    memset(instance->clone_failed_bitmap, 0, sizeof(instance->clone_failed_bitmap));

    // No usable geometry to start from: report nothing wiped rather than sweeping a card that has told
    // us it has no blocks (and block_size is what the zero-write needs).
    if(advertised == 0 || block_size == 0) return 0;

    // The advertised count, as a LIVE denominator for the progress popup: the scene reads blocks_total
    // on every WriteProgress event, so leaving it 0 until the sweep ends renders "Wiping 33 / 0" for the
    // whole pass. Overwritten at the end with what the card actually proved it holds, before any terminal
    // event -- so the popup counts against the claim and the result reports against the measurement.
    //
    // Below the geometry guard, because a card that returns there leaves this unoverwritten and the
    // wiped == 0 path would carry the card's own claim into a terminal event -- which blocks_total's
    // doc forbids.
    instance->clone_blocks_total = advertised;

    uint8_t zeros[ISO15693_MAX_BLOCK_SIZE] = {0};
    const uint8_t size = iso15693_poller_clamp_block_size(block_size);
    uint16_t wiped = 0;

    // OPEN QUESTION, gen1 only. The full argument, the gen3 case beside it and what would settle either
    // are in #255. In brief: this loop zeroes blocks 56/57, which on gen1 silicon ARE the UID
    // registers, so a sweep that reaches them moves the card's identity.
    //
    // WHICH CARDS: any gen1 card the sweep reaches. Every gen1 card measured took a write to 56/57 with
    // no unlock or commit sent before it (see ISO15693_MAGIC_BLK_UNLOCK), and a card's history
    // cannot be known, so the bound is the reach rule at the wiped == 0 branch. Reproduced end to
    // end on an LRi2K: "Wiped 58/58", the UID changed immediately, and the re-read below caught it
    // as Partial.
    //
    // The sweep cannot defend itself by clearing block 63 on the way past: writes to 62/63 are refused
    // on every gen1 chip measured. Nor by reordering its writes: neither register has ever been
    // observed to accept one, so there is no sequence here to undo, and any order but proxmark's is
    // untested. No blind ordering is safe.
    //
    // So the order is left alone, matching proxmark's `hf 15 wipe`, and what ships is the reporting:
    // Iso15693WriteStateVerifyWipe re-reads the UID after the sweep. Its field power-cycle is not what
    // makes the change visible -- that is immediate -- it just gives a clean re-activation to read
    // from. This reports a silent identity change; it does not prevent it.
    bool any_present = false; // has any block answered at all?
    uint16_t highest_present = 0; // top block proven to exist -> the reported total
    // Consecutive absent-looking blocks. Doubles as the count of absences not yet resolved as
    // interior-or-tail, because the two are the same number by construction: a block that answers
    // resolves every absence below it and zeroes the counter, so unresolved absences are always
    // exactly the current run.
    uint16_t absent_run = 0;
    // INVARIANT, and every piece of arithmetic after this loop depends on it: when the loop ends,
    // `block` is the NUMBER OF BLOCKS ATTEMPTED. This sweep attempts every index it reaches -- unlike
    // the clone, which skips the four backdoor registers on a gen1 run and so has a cursor that runs
    // ahead of its count -- so here the two are the same number and the tail arithmetic can read it
    // either way. Each break that has already attempted its block increments before leaving, and the
    // three below say only "attempted; see the invariant at the loop head".
    //
    // The deadline check is the one break that does NOT increment, because it runs BEFORE its block is
    // attempted. That asymmetry is the whole reason this needs stating once rather than per exit.
    uint16_t block = 0;
    const uint32_t sweep_start = furi_get_tick();
    const uint32_t sweep_budget = furi_ms_to_ticks(ISO15693_POLLER_PASS_MAX_MS);
    for(; block < ISO15693_POLLER_WIPE_MAX_BLOCKS; block++) {
        if(iso15693_poller_cut_pass_if_expired(instance, sweep_start, sweep_budget, block)) {
            FURI_LOG_W(
                TAG, "wipe: time limit reached at block %u (advertised %u)", block, advertised);
            break;
        }

        // Progress only while inside the advertised count. The sweep's real length isn't known until
        // it ends, so there is no honest denominator past that point, and the tail is a handful of
        // blocks. Bounding the emitting range this way also keeps the PROGRESS_STEPS safety bound
        // structural rather than something this loop has to re-argue.
        if(block < advertised) iso15693_poller_report_progress(instance, block + 1, advertised);

        // Retried at every block, not only above the advertised count: a transient below it reports a
        // false "wouldn't clear" AND leaves a block unwiped that a second attempt would have cleared.
        const Iso15693_3Error error =
            iso15693_poller_write_block_retried(instance, iso_poller, zeros, (uint8_t)block, size);

        if(error == Iso15693_3ErrorNone) {
            // Took the zero-write, so it exists and is now clear. Anything absent below it therefore
            // wasn't the top of the card: those blocks are interior and unreadable, which we do NOT
            // write off -- fail closed and count them (their bits are already set).
            wiped++;
            iso15693_poller_wipe_note_present(
                instance, block, &absent_run, &any_present, &highest_present);
            continue;
        }

        // Re-read rather than trusting the copy taken at activation. That copy is not trustworthy:
        // iso15693_3_poller_activate passes its read_blocks result through
        // iso15693_3_poller_filter_error, which maps Timeout and NotSupported to None, so activation
        // can report success having stopped reading at the FIRST failed block, leaving every later
        // block at its zeroed allocation value. Believing that cache would score a block that still
        // holds data as empty and report a hollow "wipe complete" -- and the blocks most likely to
        // fail a read are exactly the high ones that don't read until first written.
        //
        // "Zeroed" is a fact, not an assumption: simple_array_init does a bare malloc with no
        // per-element init for a byte array, but Flipper's pvPortMalloc tail-calls memset(p, 0, size).
        // So an unread entry is genuinely zeros rather than heap residue, which is what makes
        // block_held_data deterministic -- load-bearing for the tail-drop's keep branch below.
        uint8_t remaining[ISO15693_MAX_BLOCK_SIZE] = {0};
        if(iso15693_3_poller_read_block(iso_poller, remaining, (uint8_t)block, size) ==
           Iso15693_3ErrorNone) {
            // It answered a read, so it is there: the write was refused, not addressed to nothing.
            iso15693_poller_wipe_note_present(
                instance, block, &absent_run, &any_present, &highest_present);

            if(!iso15693_poller_block_is_empty(remaining, size)) {
                FURI_LOG_W(TAG, "wipe: block %u refused and still holds data", block);
                instance->clone_failed_count++;
                iso15693_poller_mark_failed(instance, block);
            }
            continue;
        }

        // Answered neither a write nor a read, so it probably isn't there. Provisional: set the bit and
        // let position decide (a later success folds it into the failures above; a run the sweep ends
        // on is dropped below).
        iso15693_poller_mark_failed(instance, block);
        if(++absent_run < ISO15693_POLLER_WIPE_ABSENT_RUN) continue;

        // Below the advertised count a long run is not a capacity signal at all: the card itself says
        // those blocks exist, so every one of them is owed an attempt.
        // Stopping here would abandon the rest and report a bare Success over them -- a 64-block
        // card with a coupling wobble at block 15 would report "wiped 15 of 15" while 15..63 still held
        // the previous card's data. So keep sweeping. The absences stay provisional and the ordinary
        // rules resolve them: a later block that answers folds them in as interior faults, and a run
        // still open when the sweep ends is dropped as the space above the card. This changes only where
        // the sweep may STOP; the tail-drop rule is untouched, so a card claiming 66 blocks against 64
        // physical still drops 64/65 rather than reporting them as "not cleared".
        //
        // The card-present check is still worth making while this holds -- a lifted card looks exactly
        // like a dead stretch -- but only once per run length, so a mostly-dead card doesn't pay an
        // inventory per block.
        const bool claimed_range_attempted = (block + 1 >= advertised);
        if(!claimed_range_attempted) {
            if(absent_run % ISO15693_POLLER_WIPE_ABSENT_RUN == 0 &&
               !iso15693_poller_card_still_present(iso_poller)) {
                *card_lost = true;
                block++; // attempted; see the invariant at the loop head
                break;
            }
            continue;
        }

        // The run may now be the card's top. The clone loop's ambiguity first: a card lifted mid-sweep
        // makes every remaining write AND read fail, which is exactly what running out of card looks
        // like. Ask before concluding anything.
        if(!iso15693_poller_card_still_present(iso_poller)) {
            *card_lost = true;
            block++; // attempted; see the invariant at the loop head
            break;
        }

        // The card answered, but that does NOT make this run the top of it, and the tail-drop below is
        // about to delete the whole run from the report. Two ways that would be wrong:
        //   - the run is a dropout, not an edge. A wobble long enough to fill the run leaves the sweep
        //     concluding the card ended where the wobble started, silently abandoning everything above
        //     it. Past the advertised count there is no other guard against that.
        //   - the run's lowest members are real and only its top is absent. A single transient on the
        //     block just below the physical top gets padded out to a full run by the nonexistent blocks
        //     above it, and the drop then discards a block that still holds data.
        // So re-probe the run now that the card is known to be answering. Any member that reads is there
        // after all, so it is not past the card's top and nothing below it in the run can be the top
        // either -- THAT is what fails closed. Whether it CLEARED is a separate question, and it is
        // decided by content below, not by the fact that it answered: a block that answers and reads
        // empty did clear, and is unmarked. Then re-derive the trailing absence; if the run no longer
        // reaches the threshold, this was not the edge and the sweep carries on.
        //
        // Bounded by the run length, and not deadline-checked -- it would have to abandon the run
        // half-classified. On a card advertising far more than it holds the run never trips below the
        // claim (the trip needs block + 1 >= advertised), so it can grow to the whole claimed range and
        // be re-probed in one go, with Back swallowed throughout -- the worst case stated beside
        // ISO15693_POLLER_PASS_MAX_MS, rather than the one short run a healthy card gives.
        const uint16_t run_start = (uint16_t)(block + 1 - absent_run);
        uint16_t still_absent = 0;
        for(uint16_t probe = run_start; probe <= block; probe++) {
            uint8_t recheck[ISO15693_MAX_BLOCK_SIZE] = {0};
            if(iso15693_3_poller_read_block(iso_poller, recheck, (uint8_t)probe, size) !=
               Iso15693_3ErrorNone) {
                still_absent++;
                continue;
            }
            FURI_LOG_W(TAG, "wipe: block %u answered on re-probe -- not the card's top", probe);
            // Same rule, with still_absent standing in for the run while the re-probe re-derives it.
            iso15693_poller_wipe_note_present(
                instance, probe, &still_absent, &any_present, &highest_present);
            // Classify this one by CONTENT, the way the main loop does. A glitch that swallowed both a
            // write ACK and its read-back, on a block whose zero-write actually landed, answers here as
            // present-and-empty: reporting that as "not cleared" manufactures a failure. Only a block
            // that answers AND still holds data failed to clear.
            if(!iso15693_poller_block_is_empty(recheck, size)) {
                instance->clone_failed_count++;
            } else {
                iso15693_poller_unmark_failed(instance, probe);
            }
        }
        absent_run = still_absent;
        if(absent_run < ISO15693_POLLER_WIPE_ABSENT_RUN) continue;

        FURI_LOG_I(
            TAG, "wipe: card ends at block %u (advertised %u)", highest_present, advertised);
        block++; // attempted; see the invariant at the loop head
        break;
    }

    // Whatever absence is still unresolved is the run the sweep ended on, contiguous by construction.
    // Above the advertised count it is the space beyond the card's real top -- never the card's to
    // clear, and dropping it is what stops a card advertising 66 blocks against 64 physical reporting
    // "Wiped 64/66, not cleared: 2".
    //
    // Below that count the premise inverts and collides with the floor, which keeps sweeping there
    // precisely because the card's claim is evidence the blocks exist. Unqualified, the drop wins and a
    // card whose blocks stop answering mid-range reports a clean Success over data it never cleared.
    // block_held_data is the discriminator: proof of existence keeps a block, absence of proof drops
    // it, so the phantoms of a card claiming more than it holds still go.
    //
    // Not closed: a block already dead when the card was presented is indistinguishable from a phantom
    // here and drops with them. Different symptom -- degraded before the wipe rather than during it.
    //
    // One premise this cannot establish: when the clock ends the sweep, the run it ends on is wherever
    // the budget ran out rather than the card's top, so dropping the part of it above the advertised
    // count is unverified. pass_truncated is what carries that -- the outcome is reported as
    // Partial, or as Fail if nothing cleared, and either names where the sweep stopped, so the drop
    // is not passing those blocks off as absent memory.
    //
    // WHY THIS LOOP MAY DISAGREE WITH wipe_note_present, AND WHY THAT IS SAFE. note_present resolves a
    // whole run at once: a block proved present, therefore every absence still open BELOW it is
    // interior memory, so all of them count. This loop decides block by block and drops each unproven
    // one -- including ones below a proven-present member of the same run. On a run 56..63 where only
    // block 60 reads non-zero, 56-59 are dropped and 60 is kept. Stated as two rules those contradict.
    //
    // What reconciles them is a property of the CACHE: iso15693_3_poller_read_blocks returns at the
    // first block that fails, so the activation copy is a PREFIX, and a block proven present implies
    // every block below it was also read. A block dropped below a proven one was read and read back as
    // zeros. It held nothing, and dropping it conceals nothing.
    //
    // That is NOT what makes an unread cache entry read as empty -- that is pvPortMalloc's memset,
    // cited at the read-back note. The keep branch leans on both and neither implies the other.
    //
    // No i < advertised guard: block_held_data range-checks against the same object, so the two say the
    // same thing and one of them would drift.
    for(uint16_t i = block - absent_run; i < block; i++) {
        if(iso15693_poller_block_held_data(target, i, size)) {
            // Proven present, so it also has to count toward the measured total -- otherwise
            // failed_count can exceed blocks_total and the partial screen renders "Wiped 0/20,
            // not cleared: 44".
            instance->clone_failed_count++;
            any_present = true;
            if(i > highest_present) highest_present = i;
            continue;
        }
        iso15693_poller_unmark_failed(instance, i);
    }

    // Report against the range the card proved, not the count it advertises.
    instance->clone_blocks_total = any_present ? (uint16_t)(highest_present + 1) : 0;

    // One line covering every exit -- the card's top, the block ceiling, the time limit, and a card
    // lifted mid-sweep, which is why those two paths break rather than return: the lifted card is the
    // case where the time spent before noticing matters most. The wall-clock cost of a sweep is then
    // measurable on any card rather than estimated, which ISO15693_POLLER_PASS_MAX_MS needs -- it is
    // set from figures nobody has instrumented. (The caller discards the counters on a card-lost exit,
    // so the tail arithmetic above running for it is harmless.)
    FURI_LOG_I(
        TAG,
        "wipe: %u blocks attempted, %u cleared, %lums (advertised %u)",
        block,
        wiped,
        (unsigned long)((furi_get_tick() - sweep_start) * 1000UL /
                        furi_kernel_get_tick_frequency()),
        advertised);

    // A card lifted mid-wipe can also surface as a pile of blocks that "wouldn't clear" -- its blocks
    // still read their old data right up until it leaves the field -- without ever tripping the
    // absent-run check above. Only asked when something failed, so a clean wipe pays nothing.
    if(instance->clone_failed_count > 0 && !iso15693_poller_card_still_present(iso_poller)) {
        *card_lost = true;
    }
    return wiped;
}

// The terminal outcome once a write finishes:
//  - a NON-EMPTY block we couldn't write means real data was lost -> Partial. An empty block that
//    wouldn't take is treated as past the card's real capacity ONLY when the empty failures form a
//    contiguous tail above the last block that wrote -> clean Success: the source held nothing
//    there, so there was nothing to lose. That is NOT a claim that the clone behaves identically --
//    the source reads those blocks back as zeros, whereas past a card's real capacity the read
//    itself fails on the cards measured (see ISO15693_POLLER_WIPE_MAX_BLOCKS). That difference is
//    why this outcome carries a note on screen rather than being reported as an unqualified
//    success. Any other empty failure is folded into clone_failed_count -> Partial (see
//    iso15693_poller_write_source_blocks).
//  - a CLONE that fell back to gen1 AND whose source held data at 56/57/62/63 -> Partial: on gen1
//    those addresses are registers, so that data was not written. A source that reaches them with
//    nothing there, or ends before block 56, loses nothing there, so it is not Partial on their
//    account; see gen1_data_lost. (A bare Write-UID has no source data to disturb, so gen1 there is a
//    clean Success either way.)
//  - a CLONE whose AFI/DSFID write was rejected -> Partial (that identity field may not be set).
static Iso15693PollerEvent iso15693_poller_success_or_partial(Iso15693Poller* instance) {
    const bool clone = (instance->mode == Iso15693PollerModeClone);
    const bool gen1_clone = clone && instance->clone_used_gen1 && instance->clone_gen1_data_lost;
    const bool identity_failed = clone &&
                                 (instance->clone_afi_failed || instance->clone_dsfid_failed);
    // Terminal guard, the clone-side counterpart of the wipe's "did anything accept a write": a clone
    // whose UID took but whose every data block was rejected has written no data at all. Calling that
    // Partial would put a Finish button under "Cloned 0/28 blocks", and the card would be carrying the
    // source's UID with none of its data -- which reads correct to a UID-only reader and fails anything
    // that reads memory. That is a failure, not a qualified success. Wipe is unaffected: it reaches
    // here only when at least one block accepted, and its failed/accepted sets are disjoint.
    //
    // A CUT run is excluded, and the counts are exactly why it has to be. The back-fill records every
    // block above the cut as a failure, so on a clone that accepted nothing failed_count reaches
    // blocks_total whether the card refused those blocks or nothing was ever sent to them. Judged on
    // counts alone this branch then reports "no data block took" about a source the radio never
    // addressed -- the same fabrication the truncation flag exists to prevent, one branch earlier in
    // this same function, and on the outcome with the MOST blocks above the cut. The guard's own
    // purpose survives: a cut run falls through to Partial, which names the clock and offers Retry
    // rather than a Finish button under "Cloned 0/N".
    if(clone && !instance->pass_truncated && instance->clone_blocks_total > 0 &&
       instance->clone_failed_count >= instance->clone_blocks_total) {
        return Iso15693PollerEventFail;
    }
    // A wipe that cleared its blocks but moved the card's UID is not a clean success, whatever the
    // block counts say -- the card's identity changed under an operation that doesn't claim to touch it.
    // Nor is one the clock cut short: its range is a cut rather than a finding about the card, so no
    // report may pass it off as one -- which is what blocks_total's doc means by a cut run's figures
    // being unusable as a claim. Stated that way rather than as "blocks the card claims were never
    // attempted", which is false when the cut lands ABOVE the advertised count: there every claimed
    // block WAS attempted, and the run is still Partial. The UID check failing to reach an answer is
    // deliberately NOT in this list -- that check is best-effort, the wipe itself finished, and making
    // it Partial would downgrade every wipe where the user lifts the card as it completes.
    if(instance->clone_failed_count > 0 || gen1_clone || identity_failed ||
       instance->uid_changed || instance->pass_truncated) {
        return Iso15693PollerEventPartial;
    }
    return Iso15693PollerEventSuccess;
}

// Read the UID back for verification, retrying a few times so a momentary miss right after the field
// power-cycle isn't mistaken for a removed card. Runs on the Nfc worker thread (furi_delay_ms is the
// same primitive the SDK poller uses between activation attempts).
//
// The SDK's inventory is 1-SLOT (INVENTORY_T5 | T5_N_SLOTS_1), so with two tags in the field it returns
// whichever wins the slot instead of reporting a collision. After a wipe that is worse than it sounds:
// the answer can come from the bystander, and the "UID changed" screen would print a UID belonging to
// a different card as though it were this one -- strictly worse than printing nothing, since the whole
// point of that screen is to be the route back to a card whose identity moved. Addressing cannot fix
// it: this read exists to find out whether the UID changed, so it cannot be aimed at a UID already in
// doubt. It needs the inventory widened or STAY QUIET, neither of which is done here; #251.
static Iso15693_3Error
    iso15693_poller_verify_inventory(Iso15693_3Poller* iso_poller, uint8_t* uid) {
    Iso15693_3Error error = Iso15693_3ErrorNone;
    for(uint32_t attempt = 0; attempt < ISO15693_POLLER_VERIFY_ATTEMPTS; attempt++) {
        error = iso15693_3_poller_inventory(iso_poller, uid);
        if(error == Iso15693_3ErrorNone) break;
        furi_delay_ms(ISO15693_POLLER_VERIFY_RETRY_MS);
    }
    return error;
}

// Is the card still in the field? Used by the write loops to tell a mid-loop removal (every remaining
// block fails) from the card's real capacity ending there -- the two are indistinguishable from the
// write results alone. Uses the same retried inventory as the UID verify, so a momentary miss isn't
// mistaken for a removal.
static bool iso15693_poller_card_still_present(Iso15693_3Poller* iso_poller) {
    uint8_t uid[ISO15693_3_UID_SIZE] = {0};
    return iso15693_poller_verify_inventory(iso_poller, uid) == Iso15693_3ErrorNone;
}

// A clone's last progress frame, sent only once the card work is over -- after the survey, and after
// Iso15693WriteStateVerifyClone where that runs. Sent any earlier, the popup reads 100% while the card
// is still being asked things, which is what invites the lift that loses the survey's notes or the
// re-read.
//
// The numerator is the blocks ATTEMPTED only on a pass the clock cut: there the popup's last frame is
// the last honest thing it can say, and a cut pass jumping to 100% would contradict the screen that
// follows it. A pass that ran to the end attempted every block its total counts, so it shows the
// total.
static void iso15693_poller_finish_progress(Iso15693Poller* instance) {
    const uint16_t total = instance->clone_blocks_total;
    iso15693_poller_report_progress(
        instance, instance->pass_truncated ? instance->clone_blocks_done : total, total);
}

// The tail both UID verifies share, once the UID has read back as the target, so a change to the
// gen2 arm reaches the gen1 one.
//
// skip_backdoor is the whole difference: a gen2 UID lives in a separate register space, so data-block
// writes cannot disturb it; a gen1 UID lives IN blocks 56/57/62/63, so a gen1 clone must skip them.
static NfcCommand iso15693_poller_finish_write(
    Iso15693Poller* instance,
    Iso15693_3Poller* iso_poller,
    bool skip_backdoor) {
    // Both callers reach here only after an inventory read the target UID back, so that is the card
    // the data pass is talking to -- not the one Start addressed, which a gen2 or gen1 UID write has
    // just replaced.
    memcpy(instance->address_uid, instance->target_uid, ISO15693_3_UID_SIZE);
    if(instance->mode == Iso15693PollerModeClone) {
        iso15693_poller_write_identity(instance, iso_poller);
        if(!iso15693_poller_write_source_blocks(instance, iso_poller, skip_backdoor)) {
            iso15693_poller_report(instance, Iso15693PollerEventCardLost);
            return NfcCommandStop;
        }
        if(instance->clone_uid_recheck) {
            instance->write_state = Iso15693WriteStateVerifyClone;
            return NfcCommandReset;
        }
        iso15693_poller_finish_progress(instance);
    }
    iso15693_poller_report(instance, iso15693_poller_success_or_partial(instance));
    return NfcCommandStop;
}

// Drives one write-mode step. Runs on the Nfc worker thread with the field active. Returns the
// NfcCommand for the poller: Reset power-cycles the field (so the next Ready verifies a freshly
// re-powered card), Stop ends the operation.
static NfcCommand
    iso15693_poller_write_step(Iso15693Poller* instance, Iso15693_3Poller* iso_poller) {
    uint8_t readback[ISO15693_3_UID_SIZE] = {0};

    switch(instance->write_state) {
    case Iso15693WriteStateStart: {
        // Note the UID the card presented, which the poller read into its data during activation:
        // the card's identity before anything here was sent, which the later checks compare
        // against. (nfc_poller_get_data returns void*, so it has to land in a typed pointer before
        // being dereferenced.) address_uid starts from the same bytes and diverges from here, since
        // the writes readdress themselves off 56/57 while original_uid keeps that identity.
        const Iso15693_3Data* presented = nfc_poller_get_data(instance->poller);
        memcpy(instance->original_uid, presented->uid, ISO15693_3_UID_SIZE);
        memcpy(instance->address_uid, presented->uid, ISO15693_3_UID_SIZE);

        // Wipe zeros the card's own blocks and sends no UID command, so it's a single pass with no
        // backdoor write or field reset.
        if(instance->mode == Iso15693PollerModeWipe) {
            bool card_lost = false;
            const uint16_t wiped = iso15693_poller_wipe_blocks(instance, iso_poller, &card_lost);
            if(card_lost) {
                // The card left mid-wipe: don't blame it for blocks that never got the chance.
                iso15693_poller_report(instance, Iso15693PollerEventCardLost);
                return NfcCommandStop;
            }

            // If not a single block accepted the zero-write, nothing was wiped: the card reported
            // no usable geometry, or every block is read-only / write-protected. Report Fail
            // rather than a hollow Success, and skip the power-cycle -- but NOT because the UID is
            // safe. "No write landed, so the UID cannot have moved" is the one inference this file
            // declines to draw anywhere else: on a card the sweep reached index 56/57 on, three
            // WRITE BLOCKs each went out there before it gave up, and a tag can apply a write without
            // answering. Only three things stop the sweep short of 56/57: the reach rule below leaves
            // it short, or the clock cuts the sweep below 56 (see the backstop note at
            // ISO15693_POLLER_PASS_MAX_MS), or the geometry guard above returned before the first
            // write, which also lands here, since it returns 0.
            //
            // THE REACH RULE, and 49 is not a threshold about the claim CONTAINING 56. THREE gates
            // must all pass before the sweep can stop, and the last block attempted is the later of
            // the first two:
            //   - ISO15693_POLLER_WIPE_ABSENT_RUN blocks in a row answer nothing. ANSWER, not take the
            //     write: a read is enough, and wipe_note_present zeroes the run from the read path and
            //     from the re-probe as well as from a landed write. A card silent from block A is
            //     therefore attempted through A+7.
            //   - the claimed range has been attempted, block + 1 >= advertised. Below the claim the
            //     trip falls through to `continue`, because the card says those blocks exist.
            //   - the run SURVIVES the re-probe that follows the trip. That third gate is what makes
            //     A terminal, and it is why A is the run the sweep ENDS on rather than the first
            //     silence anywhere: a run that recovers is zeroed and sets no floor at all.
            // So with A the first block of the FINAL unbroken run of non-answering blocks and `claim`
            // the advertised count, the last block attempted is
            //
            //     L = min(max(A + 7, claim - 1), ISO15693_POLLER_WIPE_MAX_BLOCKS - 1)
            //         =>  56 is reached from A >= 49 OR claim >= 57
            //             57 is reached from A >= 50 OR claim >= 58
            //
            // The ceiling binds the A term alone; claim - 1 cannot exceed it, since the wire caps a
            // block count at 256. Taking A as the first silence instead understates the reach: a card
            // ADVERTISING 56 that reads 0-4, is silent at 5, reads 6-59 and is silent from 60 reaches
            // 56 and 57, while A = 5 with claim = 56 makes both disjuncts false. The claim has to be
            // named for that example to bite -- at a higher claim the claim term carries it anyway.
            //
            // Either disjunct is enough on its own. A card that answers no read at all still has 56
            // attempted once it advertises 57 or more; a card that refuses every write while serving a
            // read never accumulates a run, so it walks past 56/57 whatever it claims. Staying short
            // of 56 takes A < 49 AND claim < 57 together.
            //
            // So on a gen1 card this path can move the UID, report "Wipe failed", never run the check
            // and never say the check did not run -- the one path where the mitigation #255 describes
            // does not run at all. The short-circuit is left as it is.
            if(wiped == 0) {
                iso15693_poller_report(instance, Iso15693PollerEventFail);
                return NfcCommandStop;
            }

            // Blocks were cleared, so the UID check below is worth making. Power-cycle the field first,
            // like every other UID verify in this file.
            instance->write_state = Iso15693WriteStateVerifyWipe;
            return NfcCommandReset;
        }
        // A UID read-back only proves the card is magic when the UID it is compared against is one the
        // card did not already have. Write UID pre-seeds its editor with the UID a previous Info read
        // returned, so "Info -> Write UID -> Save" asks to write the card's own UID back onto it -- and
        // then ANY tag passes the verify, having ignored every backdoor frame, and the app would report
        // a Success that says nothing about the card. Stop before writing and say so. (Not applied to a
        // clone: its target is the source's UID, so cloning an image back onto the card it came from
        // hits the same ambiguity, but there the data blocks are the payload and the user gets exactly
        // what they asked for either way.)
        if(instance->mode == Iso15693PollerModeWriteUid &&
           memcmp(instance->target_uid, instance->original_uid, ISO15693_3_UID_SIZE) == 0) {
            instance->uid_unverifiable = true;
            iso15693_poller_report(instance, Iso15693PollerEventFail);
            return NfcCommandStop;
        }

        // A clone with no writable geometry has nothing to clone -- refuse before touching the card so
        // we don't stamp a UID and report a misleading "clone complete". (Use Write UID for a UID-only
        // change.) clone_blocks_total stays 0 so the scene picks the "empty source" reason.
        if(instance->mode == Iso15693PollerModeClone &&
           (iso15693_3_get_block_count(instance->clone_source) == 0 ||
            iso15693_3_get_block_size(instance->clone_source) == 0)) {
            instance->clone_blocks_total = 0;
            iso15693_poller_report(instance, Iso15693PollerEventFail);
            return NfcCommandStop;
        }

        // Record the source block count now, before any write. write_source_blocks (its normal setter)
        // runs only on the paths that reach the data-write stage, so a failure before then -- e.g. a
        // gen2 backdoor that changes the UID to neither original nor target -- would otherwise leave
        // this 0 and be misreported as an empty source. A genuine empty source already returned above.
        if(instance->mode == Iso15693PollerModeClone) {
            instance->clone_blocks_total = iso15693_3_get_block_count(instance->clone_source);
        }

        if(instance->attempt_gen1) {
            // Opt-in gen1 run (the user accepted it on the "not gen2 magic" screen) -- a clone or a
            // bare Write-UID; both re-enter here with attempt_gen1 set. Mirror the gen2 flow: write
            // ONLY the gen1 UID sequence now, verify it in VerifyGen1, and write the data blocks there
            // only if the UID took -- so a non-magic tag that can't do gen1 loses at most the four
            // backdoor registers, not all its data.
            // Set at the send, since the flag means these frames went out -- see gen1_attempted.
            instance->gen1_attempted = true;
            iso15693_poller_send_backdoor_uid_gen1(instance, iso_poller, instance->target_uid);
            instance->write_state = Iso15693WriteStateVerifyGen1;
            return NfcCommandReset;
        }

        // gen2 first: send only the UID + geometry (NO data yet). For a clone the data blocks are
        // written afterwards, once VerifyGen2 confirms the card actually took the gen2 UID -- so a tag
        // that does not take that UID is never clobbered by a doomed clone. The CFG block programs
        // what the card reports for geometry / IC ref: the source's values for a clone (same chip
        // identity), the fixed magic default otherwise. (gen1 has no geometry block.)
        uint8_t cfg_maxblock = ISO15693_MAGIC_V2_CFG_MAXBLOCK;
        uint8_t cfg_blocksize = ISO15693_MAGIC_V2_CFG_BLOCKSIZE;
        uint8_t cfg_icref = ISO15693_MAGIC_V2_CFG_IC_REF;
        if(instance->mode == Iso15693PollerModeClone) {
            const Iso15693_3SystemInfo* sys = &instance->clone_source->system_info;
            if(sys->flags & ISO15693_3_SYSINFO_FLAG_MEMORY) {
                // Clamped for the same reason the clone pass clamps them: straight out of a
                // loaded .nfc, so hand-editable and unbounded by anything this app controls. This is
                // the site where it matters most -- the other two clamps guard a stack buffer and a
                // screen, while these two bytes are PROGRAMMED INTO THE CARD and outlive the run.
                // Unclamped, a source claiming 257 blocks would wrap the cast to 0 and program the
                // card to advertise a single block while the pass wrote 256.
                const uint16_t cfg_count = sys->block_count > ISO15693_POLLER_MAX_BLOCKS ?
                                               (uint16_t)ISO15693_POLLER_MAX_BLOCKS :
                                               sys->block_count;
                const uint8_t cfg_size = iso15693_poller_clamp_block_size(sys->block_size);
                if(cfg_count > 0) cfg_maxblock = (uint8_t)(cfg_count - 1);
                if(cfg_size > 0) cfg_blocksize = (uint8_t)(cfg_size - 1);
            }
            if(sys->flags & ISO15693_3_SYSINFO_FLAG_IC_REF) cfg_icref = sys->ic_ref;
        }

        iso15693_poller_send_backdoor_uid_gen2(
            iso_poller, instance->target_uid, cfg_maxblock, cfg_blocksize, cfg_icref);
        instance->write_state = Iso15693WriteStateVerifyGen2;
        return NfcCommandReset;
    }

    case Iso15693WriteStateVerifyGen2: {
        if(iso15693_poller_verify_inventory(iso_poller, readback) != Iso15693_3ErrorNone) {
            iso15693_poller_report(instance, Iso15693PollerEventCardLost);
            return NfcCommandStop;
        }
        if(memcmp(readback, instance->target_uid, ISO15693_3_UID_SIZE) == 0) {
            // The UID now reads back as the target, so the card accepted a gen2 magic command:
            // write the clone payload (AFI/DSFID + data blocks). The gen2 UID lives in a separate
            // backdoor register space, so data-block writes can't disturb it. A bare Write-UID has
            // no payload. Strictly this proves "the UID is now the target", not "the card is
            // magic": if the tag presented already had that UID the comparison passes without the
            // write having done anything. That tag may be gen1 silicon, which the data pass detects
            // at 56/57 and repairs (see iso15693_poller_write_source_blocks), or a card re-cloned
            // from a different image with the same UID; either way the payload is the file.
            return iso15693_poller_finish_write(instance, iso_poller, false);
        }
        if(memcmp(readback, instance->original_uid, ISO15693_3_UID_SIZE) == 0) {
            // gen2 changed nothing: a gen1 card, or a non-magic tag. Nothing has been written yet, so
            // the card is untouched. Stop here and let the scene offer the opt-in gen1 retry. This
            // applies to a bare Write-UID as well as a clone: gen1 sets the UID with ordinary WRITE
            // BLOCK into blocks 56/57/62/63, which ANY writable tag accepts, so on a non-magic tag it
            // destroys four blocks of user data. That is destructive, so it needs the same explicit
            // consent in both flows.
            iso15693_poller_report(instance, Iso15693PollerEventNotGen2);
            return NfcCommandStop;
        }
        // gen2 changed the UID but not to the target: stop rather than compound it with gen1. Record
        // what came back. This proves the card is magic -- an inert tag cannot change its UID at
        // all -- so it must not fall through to "not a magic tag", and the UID the card is now
        // answering with is the only way the user can find it again.
        instance->uid_unexpected = true;
        memcpy(instance->uid_readback, readback, ISO15693_3_UID_SIZE);
        iso15693_poller_report(instance, Iso15693PollerEventFail);
        return NfcCommandStop;
    }

    case Iso15693WriteStateVerifyWipe: {
        // Did the wipe move the card's UID? On gen2 it cannot -- the wipe sends no UID command and
        // the gen2 UID lives in a separate register space -- so on a gen2 card this is a regression
        // test that should never fire. On gen1 it is the point: blocks 56/57 ARE the UID registers,
        // which can take a write with no unlock or commit before it (see
        // ISO15693_MAGIC_BLK_UNLOCK), so a sweep that reaches them can leave the card wearing a
        // different UID. The OPEN QUESTION in iso15693_poller_wipe_blocks has the mechanism,
        // including why the sweep reaching 63 does not help: reordering blind writes cannot fix it,
        // but reporting it can, and the screens do not promise the UID is left alone.
        //
        // This is a state of its own, entered after NfcCommandReset, for the reason
        // iso15693_poller.h gives for the gen2 and gen1 UID verifies. It is a separate state
        // rather than an inline read because the sweep has just written to 56/57, and on a gen1 card
        // that IS a UID write -- so this is precisely the card the check exists for, and it should
        // read from a cleanly re-activated tag rather than mid-sequence.
        //
        // uid_changed is set only from a positive observation, and the cost of that rule is here: a
        // card bricked so thoroughly that it no longer inventories goes unreported. Lifting the card
        // the instant a wipe finishes is something users do on every protocol, so taking silence as
        // failure would raise a false error there to catch a card already unreachable by any means.
        // A card that never comes back from the reset is the same case; see the activation-error path in
        // iso15693_poller_nfc_callback.
        if(iso15693_poller_verify_inventory(iso_poller, readback) != Iso15693_3ErrorNone) {
            FURI_LOG_W(TAG, "wipe: card did not answer the UID read-back");
        } else {
            instance->uid_verified = true;
            if(memcmp(readback, instance->original_uid, ISO15693_3_UID_SIZE) != 0) {
                FURI_LOG_E(TAG, "wipe: the UID CHANGED");
                instance->uid_changed = true;
                memcpy(instance->uid_readback, readback, ISO15693_3_UID_SIZE);
            }
        }
        iso15693_poller_report(instance, iso15693_poller_success_or_partial(instance));
        return NfcCommandStop;
    }

    case Iso15693WriteStateVerifyGen1: {
        if(iso15693_poller_verify_inventory(iso_poller, readback) != Iso15693_3ErrorNone) {
            iso15693_poller_report(instance, Iso15693PollerEventCardLost);
            return NfcCommandStop;
        }
        if(memcmp(readback, instance->target_uid, ISO15693_3_UID_SIZE) != 0) {
            // gen1 UID didn't take -- not a gen1 card either. No data blocks were written, but the
            // gen1 UID sequence itself already went out as four ordinary WRITE BLOCKs into 56/57/62/63,
            // which any writable tag accepts. So on the tag this most likely is -- an ordinary one --
            // four blocks of user data are now gone. gen1_attempted carries that to the result screen;
            // reporting only "not a magic tag" here would hide the damage the user consented to.
            //
            // Unless the UID MOVED, just not to the target -- one half written and not the other, if a
            // frame was lost or the re-address between 56 and 57 missed. That card is gen1, and the
            // UID it now answers to is the only way to find it again, so it is reported the way
            // VerifyGen2 reports one. Only as one of the two UIDs a half can leave, though, the rule
            // readdress applies: any other answer did not come from these frames -- a second tag
            // answering the 1-slot inventory, most likely (#251) -- and printed as this card's it would
            // replace the screen that names the four blocks the user's consent has just cost.
            if(memcmp(readback, instance->original_uid, ISO15693_3_UID_SIZE) != 0 &&
               iso15693_poller_uid_is_half_written(instance, readback)) {
                instance->uid_unexpected = true;
                memcpy(instance->uid_readback, readback, ISO15693_3_UID_SIZE);
            }
            iso15693_poller_report(instance, Iso15693PollerEventFail);
            return NfcCommandStop;
        }
        // gen1 set the UID. Record it so a clone whose source held data at 56/57/62/63 reports
        // Partial: on gen1 those addresses are registers, so the file's data there is not written.
        instance->clone_used_gen1 = true;
        // UID took -> now write the payload. For a clone: AFI/DSFID + every data block EXCEPT the gen1
        // backdoor registers 56/57/62/63 (writing those would clobber the UID we just set). A bare
        // Write-UID has no payload.
        return iso15693_poller_finish_write(instance, iso_poller, true);
    }

    case Iso15693WriteStateVerifyClone: {
        // The data pass sent frames to 56/57 on the gen2 path. On gen2 silicon those are memory and
        // the UID cannot have moved. On gen1 silicon they are the UID, and whether the run noticed
        // rests on an inventory that can miss, or be answered by a second tag in the field (#251),
        // and on a repair whose frames each go out once. So before anything is reported, the identity
        // is read back from a cleanly re-activated card: the target is the only answer under which
        // the run's own report is true.
        if(iso15693_poller_verify_inventory(iso_poller, readback) != Iso15693_3ErrorNone) {
            // Nothing confirmed, so nothing to report but the lost card. clone_uid_recheck still
            // stands, and it is what tells that screen the UID was never read back.
            iso15693_poller_report(instance, Iso15693PollerEventCardLost);
            return NfcCommandStop;
        }
        if(memcmp(readback, instance->target_uid, ISO15693_3_UID_SIZE) != 0) {
            // Answering to a UID nobody asked for -- most likely one made of the file's bytes. Printed
            // for the reason VerifyGen2 prints one: it is how the user finds the card again.
            instance->uid_unexpected = true;
            memcpy(instance->uid_readback, readback, ISO15693_3_UID_SIZE);
            iso15693_poller_report(instance, Iso15693PollerEventFail);
            return NfcCommandStop;
        }
        iso15693_poller_finish_progress(instance);
        iso15693_poller_report(instance, iso15693_poller_success_or_partial(instance));
        return NfcCommandStop;
    }
    }
    // No default above, so -Wswitch (in -Wall, with -Werror) makes a forgotten state a build error.
    // A default sharing an arm would run that arm silently instead, and the worst state to land in by
    // accident is VerifyGen1: it sets clone_used_gen1 and writes the clone payload.
    furi_crash("iso15693: unreachable write state");
}

// Runs on the Nfc worker thread. Returns NfcCommand to control the poller.
static NfcCommand iso15693_poller_nfc_callback(NfcGenericEvent event, void* context) {
    Iso15693Poller* instance = context;
    furi_assert(instance);

    if(event.protocol != NfcProtocolIso15693_3) {
        return NfcCommandContinue;
    }

    Iso15693_3PollerEvent* iso_event = event.event_data;

    // Activation error => no card in the field (or removed). Retry a bounded number of times so the
    // popup can't hang forever, then report CardLost.
    if(iso_event->type == Iso15693_3PollerEventTypeError) {
        const bool verifying_wipe = (instance->write_state == Iso15693WriteStateVerifyWipe);
        const uint32_t budget = verifying_wipe ? ISO15693_POLLER_WIPE_VERIFY_ACTIVATIONS :
                                                 ISO15693_POLLER_MAX_ACTIVATION_ERRORS;
        if(++instance->activation_errors >= budget) {
            if(verifying_wipe) {
                // The wipe already ran; only its UID check is outstanding, and that check reports
                // nothing unless it POSITIVELY observes a different UID (see VerifyWipe). A card that
                // doesn't come back from the power-cycle is the same non-observation as an inventory
                // that fails, so report the wipe rather than throwing its result away for a card the
                // user has most likely just picked up.
                FURI_LOG_W(TAG, "wipe: card did not return after the field reset");
                iso15693_poller_report(instance, iso15693_poller_success_or_partial(instance));
                return NfcCommandStop;
            }
            iso15693_poller_report(instance, Iso15693PollerEventCardLost);
            return NfcCommandStop;
        }
        return NfcCommandContinue;
    }

    if(iso_event->type != Iso15693_3PollerEventTypeReady) {
        return NfcCommandContinue;
    }

    instance->activation_errors = 0;

    if(instance->mode == Iso15693PollerModeInfo) {
        // The poller filled Iso15693_3Data (UID + system info) during activation.
        const Iso15693_3Data* poller_data = nfc_poller_get_data(instance->poller);
        iso15693_3_copy(instance->data, poller_data);
        iso15693_poller_report(instance, Iso15693PollerEventSuccess);
        return NfcCommandStop;
    }

    // On the FIRST activation of any write mode (write_state still Start, before any write step), tell
    // the scene a card was detected so its popup switches from "apply the card" to "writing". Fires
    // once, because write_step advances the state.
    // No Info-mode test needed: Info returns unconditionally above, which is what guarantees the
    // "Not sent in Info mode" half of CardDetected's contract.
    if(instance->write_state == Iso15693WriteStateStart) {
        iso15693_poller_report(instance, Iso15693PollerEventCardDetected);
    }

    // Write mode. event.instance is the concrete Iso15693_3Poller; raw frames must be sent there.
    return iso15693_poller_write_step(instance, event.instance);
}

Iso15693Poller* iso15693_poller_alloc(Nfc* nfc) {
    Iso15693Poller* instance = malloc(sizeof(Iso15693Poller));
    instance->poller = nfc_poller_alloc(nfc, NfcProtocolIso15693_3);
    instance->data = iso15693_3_alloc();
    instance->clone_source = iso15693_3_alloc();
    instance->frame_tx = bit_buffer_alloc(ISO15693_POLLER_BUF_SIZE);
    instance->frame_rx = bit_buffer_alloc(ISO15693_POLLER_BUF_SIZE);
    // Only what must hold BEFORE a start. Everything else is set by start_internal, which is the
    // authoritative reset list -- no public entry point reaches the struct without going through
    // it, and it asserts !running. Zeroing a subset here too would read as a second reset list
    // while covering only part of the real one, so someone adding a field would find two and have
    // to work out which is binding.
    //
    // A NEW FIELD BELONGS IN start_internal unless it is one of the six it deliberately leaves
    // alone: `poller`, `clone_source`, `frame_tx` and `frame_rx` are owned allocations, and
    // `target_uid` / `original_uid` are set by the caller and by write_step, so resetting them would
    // discard the run's own inputs.
    instance->running = false;
    instance->callback = NULL;
    instance->context = NULL;
    return instance;
}

void iso15693_poller_free(Iso15693Poller* instance) {
    furi_assert(instance);
    if(instance->running) {
        iso15693_poller_stop(instance);
    }
    nfc_poller_free(instance->poller);
    iso15693_3_free(instance->data);
    iso15693_3_free(instance->clone_source);
    bit_buffer_free(instance->frame_tx);
    bit_buffer_free(instance->frame_rx);
    free(instance);
}

// nfc_poller_start is non-blocking: the callback fires on the Nfc worker thread and returns
// NfcCommandStop when finished. The owning scene must still call iso15693_poller_stop() on exit
// so the NfcPoller session state is reset before the next start.
static void iso15693_poller_start_internal(
    Iso15693Poller* instance,
    Iso15693PollerMode mode,
    bool gen1,
    Iso15693PollerCallback callback,
    void* context) {
    furi_assert(instance);
    furi_assert(!instance->running);
    instance->mode = mode;
    instance->attempt_gen1 = gen1;
    instance->callback = callback;
    instance->context = context;
    instance->write_state = Iso15693WriteStateStart;
    instance->activation_errors = 0;
    instance->clone_blocks_total = 0;
    instance->clone_failed_count = 0;
    instance->clone_over_capacity = 0;
    instance->wipe_advertised = 0;
    instance->pass_truncated = false;
    instance->pass_cut_block = 0;
    instance->uid_verified = false;
    instance->clone_used_gen1 = false;
    instance->clone_gen1_blocks_skipped = false;
    instance->clone_gen1_data_lost = false;
    instance->clone_uid_recheck = false;
    instance->clone_residue_found = false;
    instance->clone_residue_first = 0;
    instance->clone_residue_last = 0;
    instance->clone_survey_top = 0;
    instance->clone_holds_more = false;
    instance->clone_memory_differs = false;
    instance->clone_ic_ref_differs = false;
    instance->clone_file_blocks = 0;
    instance->clone_file_ic_ref = 0;
    instance->uid_moved_by_write = false;
    instance->clone_card_blocks = 0;
    instance->clone_card_blocks_known = false;
    instance->clone_card_ic_ref = 0;
    instance->clone_capacity_confirmed = false;
    instance->clone_blocks_done = 0;
    instance->progress_step = UINT8_MAX; // no band emitted yet, so the first call fires
    instance->clone_afi_failed = false;
    instance->clone_dsfid_failed = false;
    instance->uid_unexpected = false;
    instance->gen1_attempted = false; // set at the send site in write_step, not here
    instance->uid_unverifiable = false;
    instance->uid_changed = false;
    memset(instance->uid_readback, 0, sizeof(instance->uid_readback));
    // Set in write_step from the card's own answer, so it is reset here with the rest of the run state.
    memset(instance->address_uid, 0, sizeof(instance->address_uid));
    instance->write_option = false; // re-asked on every run, since the card may be a different one
    memset(instance->clone_failed_bitmap, 0, sizeof(instance->clone_failed_bitmap));
    iso15693_3_reset(instance->data);
    instance->running = true;
    nfc_poller_start(instance->poller, iso15693_poller_nfc_callback, instance);
}

void iso15693_poller_start(
    Iso15693Poller* instance,
    Iso15693PollerCallback callback,
    void* context) {
    iso15693_poller_start_internal(instance, Iso15693PollerModeInfo, false, callback, context);
}

void iso15693_poller_start_write_uid(
    Iso15693Poller* instance,
    const uint8_t* uid,
    Iso15693PollerCallback callback,
    void* context) {
    furi_assert(instance);
    furi_assert(uid);
    memcpy(instance->target_uid, uid, ISO15693_3_UID_SIZE);
    iso15693_poller_start_internal(instance, Iso15693PollerModeWriteUid, false, callback, context);
}

void iso15693_poller_start_write_uid_gen1(
    Iso15693Poller* instance,
    const uint8_t* uid,
    Iso15693PollerCallback callback,
    void* context) {
    furi_assert(instance);
    furi_assert(uid);
    // Opt-in gen1 retry after gen2 left the UID unchanged. write_step sends ONLY the gen1 UID sequence
    // and verifies it.
    memcpy(instance->target_uid, uid, ISO15693_3_UID_SIZE);
    iso15693_poller_start_internal(instance, Iso15693PollerModeWriteUid, true, callback, context);
}

void iso15693_poller_start_clone(
    Iso15693Poller* instance,
    const Iso15693_3Data* source,
    Iso15693PollerCallback callback,
    void* context) {
    furi_assert(instance);
    furi_assert(source);
    // Hold our own copy of the source so it survives the async write; target UID = the source's UID.
    iso15693_3_copy(instance->clone_source, source);
    memcpy(instance->target_uid, source->uid, ISO15693_3_UID_SIZE);
    iso15693_poller_start_internal(instance, Iso15693PollerModeClone, false, callback, context);
}

void iso15693_poller_start_clone_gen1(
    Iso15693Poller* instance,
    const Iso15693_3Data* source,
    Iso15693PollerCallback callback,
    void* context) {
    furi_assert(instance);
    furi_assert(source);
    // Opt-in gen1 retry after gen2 was rejected. Same source/target as a normal clone; write_step
    // writes ONLY the gen1 UID first, verifies it, then writes the data (skipping 56/57/62/63) only if
    // the UID took.
    iso15693_3_copy(instance->clone_source, source);
    memcpy(instance->target_uid, source->uid, ISO15693_3_UID_SIZE);
    iso15693_poller_start_internal(instance, Iso15693PollerModeClone, true, callback, context);
}

void iso15693_poller_start_wipe(
    Iso15693Poller* instance,
    Iso15693PollerCallback callback,
    void* context) {
    furi_assert(instance);
    iso15693_poller_start_internal(instance, Iso15693PollerModeWipe, false, callback, context);
}

void iso15693_poller_get_result(Iso15693Poller* instance, Iso15693PollerResult* result) {
    furi_assert(instance);
    furi_assert(result);
    result->blocks_total = instance->clone_blocks_total;
    result->failed_count = instance->clone_failed_count;
    result->over_capacity = instance->clone_over_capacity;
    result->blocks_advertised = instance->wipe_advertised;
    result->pass_truncated = instance->pass_truncated;
    result->cut_block = instance->pass_cut_block;
    result->uid_verified = instance->uid_verified;
    result->uid_recheck = instance->clone_uid_recheck;
    memcpy(result->failed_bitmap, instance->clone_failed_bitmap, sizeof(result->failed_bitmap));
    result->used_gen1 = instance->clone_used_gen1;
    result->gen1_blocks_skipped = instance->clone_gen1_blocks_skipped;
    result->gen1_data_lost = instance->clone_gen1_data_lost;
    result->residue_found = instance->clone_residue_found;
    result->residue_first = instance->clone_residue_first;
    result->residue_last = instance->clone_residue_last;
    result->holds_more = instance->clone_holds_more;
    result->survey_top = instance->clone_survey_top;
    result->memory_differs = instance->clone_memory_differs;
    result->ic_ref_differs = instance->clone_ic_ref_differs;
    result->file_blocks = instance->clone_file_blocks;
    result->file_ic_ref = instance->clone_file_ic_ref;
    result->card_blocks = instance->clone_card_blocks;
    result->card_ic_ref = instance->clone_card_ic_ref;
    result->capacity_confirmed = instance->clone_capacity_confirmed;
    // Clone-mode only, mirroring success_or_partial: the flags are reset per run and written only in
    // the clone path, so this guard is future-proofing against them ever leaking cross-mode.
    result->identity_failed = (instance->mode == Iso15693PollerModeClone) &&
                              (instance->clone_afi_failed || instance->clone_dsfid_failed);
    result->blocks_done = instance->clone_blocks_done;
    result->uid_unexpected = instance->uid_unexpected;
    memcpy(result->uid_readback, instance->uid_readback, sizeof(result->uid_readback));
    result->gen1_attempted = instance->gen1_attempted;
    result->uid_unverifiable = instance->uid_unverifiable;
    result->uid_changed = instance->uid_changed;
}

// True if the source image has non-empty data in any of the gen1 backdoor blocks (56/57/62/63) that
// actually exist within its block count. On gen1 those addresses are registers, and on any other tag
// the gen1 sequence overwrites them, so a gen1 clone cannot reproduce a source that stores real data
// there: the opt-in warns about it up front, and the result reports it as Partial. (Purely a source
// inspection; touches no hardware.)
bool iso15693_poller_source_uses_gen1_blocks(const Iso15693_3Data* source) {
    if(!source) return false;
    const uint16_t block_count = iso15693_3_get_block_count(source);
    const uint8_t block_size = iso15693_3_get_block_size(source);
    if(block_size == 0) return false;
    for(size_t i = 0; i < COUNT_OF(iso15693_poller_backdoor_blocks); i++) {
        const uint16_t block = iso15693_poller_backdoor_blocks[i];
        if(block >= block_count) continue;
        const uint8_t* data = iso15693_3_get_block_data(source, (uint8_t)block);
        if(!iso15693_poller_block_is_empty(data, block_size)) return true;
    }
    return false;
}

void iso15693_poller_stop(Iso15693Poller* instance) {
    furi_assert(instance);
    if(instance->running) {
        nfc_poller_stop(instance->poller);
        instance->running = false;
    }
}

const Iso15693_3Data* iso15693_poller_get_data(Iso15693Poller* instance) {
    furi_assert(instance);
    return instance->data;
}
