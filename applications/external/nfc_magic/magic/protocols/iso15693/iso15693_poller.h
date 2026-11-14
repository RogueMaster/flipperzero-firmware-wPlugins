#pragma once

#include <nfc/nfc_poller.h>
#include <lib/nfc/protocols/iso15693_3/iso15693_3.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Iso15693Poller Iso15693Poller;

// Size (bytes) of the per-block failure bitmap; covers up to 256 blocks (the ISO15693 max).
#define ISO15693_POLLER_BLOCK_BITMAP_SIZE (32U)
// The block range that bitmap can address, which is also the ISO15693 block-number space. Every
// FIXED block ceiling in this feature is this number -- the card-derived bounds, source_count and
// advertised, are not -- so it is named once here rather than spelled BITMAP_SIZE * 8 at each
// site. ISO15693_POLLER_WIPE_MAX_BLOCKS in the .c is the same value and carries the hardware
// argument for using it as the sweep's ceiling.
#define ISO15693_POLLER_MAX_BLOCKS        (ISO15693_POLLER_BLOCK_BITMAP_SIZE * 8U)

typedef enum {
    Iso15693PollerModeInfo, // detect + read UID / system info
    Iso15693PollerModeWriteUid, // magic backdoor UID write (gen2 only; gen1 is a separate opt-in run)
    Iso15693PollerModeClone, // write UID + all data blocks from a source image
    Iso15693PollerModeWipe, // zero every data block, the gen1 registers included -- no UID command
        // is sent, but that is not the same as leaving the UID alone; see iso15693_poller_start_wipe
} Iso15693PollerMode;

typedef enum {
    Iso15693PollerEventSuccess, // Info: card read. Write/clone: the target UID read back and matched
        // (the UID, plus the AFI/DSFID on a clone, are re-read; a data block counts as written when
        // the card ACKs it, and its CONTENTS are compared only on a card that cannot acknowledge a
        // write -- see ISO15693_POLLER_OPTION_FLAG in the .c). Wipe: the sweep ended without the
        // clock cutting it, and nothing it reached is known to still hold data -- which is weaker than
        // "reached the card's top and cleared everything", because the sweep can also end at the
        // 256-block ceiling, and a block dropped below the advertised count was reached without being
        // proven clear. What the drop does guarantee is that a block proven to hold data is never
        // dropped. Says nothing about whether the identity re-check ran -- that is uid_verified.
    Iso15693PollerEventPartial, // the operation mostly worked but isn't a clean result: a clone lost
        // some data blocks, fell back to gen1 over a source holding data at 56/57/62/63, or had its
        // AFI/DSFID write rejected; or a wipe couldn't clear every block, or moved the card's UID.
        // ALSO either mode cut short by the wall-clock bound -- the run's own job is left undone
        // whatever the counts say. A cut CLONE is Partial even if no block took, because its counts
        // cannot separate a refusal from a block that was never sent; the Fail guard excludes cut
        // runs for exactly that reason. A wipe cut short having cleared NOTHING reports Fail
        // instead: the wiped == 0 short-circuit runs first, which is the card this bound was
        // written for (refuses every write, answers every read). Which of those it was is in
        // Iso15693PollerResult, and its flags are not interchangeable: pass_truncated in particular
        // is a qualifier no block figure can show.
    Iso15693PollerEventFail, // the operation didn't take: the gen2 write changed the UID to neither
        // the original nor the target (a card that simply isn't magic leaves it UNCHANGED, which is
        // NotGen2 and lands on the opt-in screen, not here), an opt-in gen1 UID didn't take, the
        // clone source had no data blocks, a clone's UID took but no data block did, a wipe cleared
        // nothing, or a Write UID asked for the UID the card already has. Those are NOT the same
        // thing to a user -- read Iso15693PollerResult (uid_unexpected / gen1_attempted /
        // uid_unverifiable) to tell them apart before picking a message.
        // A clone's re-read after a pass that reached 56/57 is a Fail too, on a UID other than the
        // target (uid_unexpected).
        // So is a gen1 sequence that leaves half a UID (uid_unexpected).
    Iso15693PollerEventCardLost, // no card in the field, or removed before the operation finished.
        // Covers a card lifted DURING a block pass, which makes every remaining block fail and so is
        // indistinguishable from the card's capacity ending there: both loops re-check that the card
        // is present before making any capacity claim, and report this instead of a write result.
    Iso15693PollerEventCardDetected, // first activation of any write mode -- clone, wipe AND Write
        // UID. Flips the shared write popup off "apply the card" onto "Writing" -- or "Wiping", which
        // is the same popup picking its verb. Not sent in Info mode.
    Iso15693PollerEventWriteProgress, // some blocks done; read the result for the running counts.
        // Emitted a bounded number of times per pass, NOT per block -- see
        // ISO15693_POLLER_PROGRESS_STEPS in the .c for why that bound is a correctness constraint
    Iso15693PollerEventNotGen2, // gen2 left the UID unchanged (not a gen2 magic card, or not magic
        // at all). Nothing was written; the scene offers the opt-in gen1 retry. Emitted for a clone AND
        // for a bare Write-UID -- both gate the destructive gen1 attempt behind that consent.
} Iso15693PollerEvent;

typedef void (*Iso15693PollerCallback)(Iso15693PollerEvent event, void* context);

Iso15693Poller* iso15693_poller_alloc(Nfc* nfc);

void iso15693_poller_free(Iso15693Poller* instance);

// Detect + read (Info mode). Emits Success once a card is read, or CardLost after a bounded number
// of activation attempts with no card in the field.
void iso15693_poller_start(
    Iso15693Poller* instance,
    Iso15693PollerCallback callback,
    void* context);

// Magic UID write (gen2 attempt). `uid` is ISO15693_3_UID_SIZE bytes, MSB-first (uid[0] must be 0xE0).
// Writes ONLY the gen2 backdoor sequence -- a harmless custom command on a non-magic tag. Before
// the read-back it power-cycles the field, like proxmark's switch_off + getUID. There is no
// power-up latch on the gen1 chips tested (see ISO15693_MAGIC_BLK_UNLOCK in the .c); it is kept
// anyway because it re-activates the card for a clean read, and a gen2 UID lives in a register
// space never tested for a latch.
// Emits CardDetected, then Success (the read-back inventory returns the requested UID), Fail,
// CardLost, or NotGen2 -- the last offering the destructive gen1 retry via
// iso15693_poller_start_write_uid_gen1(). Two distinct Fails, both flagged in the result:
// uid_unexpected, and uid_unverifiable, which is refused BEFORE anything is sent.
// The byte-level frames are defined in iso15693_poller.c (ported from proxmark3 armsrc/iso15693.c,
// SetTag15693Uid / SetTag15693Uid_v2).
void iso15693_poller_start_write_uid(
    Iso15693Poller* instance,
    const uint8_t* uid,
    Iso15693PollerCallback callback,
    void* context);

// Opt-in gen1 UID-write retry (call after start_write_uid reported NotGen2 and the user confirmed).
// Writes the destructive gen1 sequence -- ordinary WRITE BLOCK into blocks 56/57/62/63, the four
// registers the rest of this header calls the gen1 registers (ISO15693_MAGIC_BLK_* in the .c says which
// is which). ANY writable tag accepts an ordinary write, so on a non-magic tag this destroys four
// blocks of user data. ADDRESSED: a tag in the field with a different UID ignores these frames, so a
// caller's consent text need cover only the card in hand. A Write-UID has no payload to follow, so a
// verified UID is a clean Success.
// Emits CardDetected, then Success, Fail (the gen1 UID didn't take) or CardLost. The sequence goes out
// before anything is verified, so a Fail still carries gen1_attempted -- see that field for what the
// caller then owes the user. Hardware-validated across three chips -- ISO15693_MAGIC_BLK_UNLOCK in
// the .c says what that does and does not cover.
void iso15693_poller_start_write_uid_gen1(
    Iso15693Poller* instance,
    const uint8_t* uid,
    Iso15693PollerCallback callback,
    void* context);

// Full clone (gen2 attempt): write `source`'s UID via the gen2 backdoor FIRST, and only once that UID
// reads back does it write every data block (standard WRITE BLOCK) -- so a tag that does not take the
// gen2 UID is never clobbered by a doomed clone. `source` is an ISO15693-3 image loaded from a saved
// .nfc. Emits CardDetected, then Success (UID + all blocks), Partial, Fail (the source has no data
// blocks, no data block would take, or uid_unexpected), CardLost, or NotGen2 -- the last offering the
// destructive gen1 retry via iso15693_poller_start_clone_gen1().
void iso15693_poller_start_clone(
    Iso15693Poller* instance,
    const Iso15693_3Data* source,
    Iso15693PollerCallback callback,
    void* context);

// Opt-in gen1 clone retry (call after start_clone reported NotGen2 and the user confirmed). Writes the
// destructive gen1 UID sequence FIRST and, only if that UID reads back, writes the data blocks --
// skipping the gen1 registers, which cannot hold the source's data. A source holding data there
// therefore reports Partial; one that reaches them with nothing there, or ends before block 56, loses
// nothing there. A card that cannot do gen1 loses at most those four blocks. Emits CardDetected, then
// Success, Partial, Fail (the gen1 UID didn't take, the source had no data blocks, or every data
// block was rejected -- gen1_attempted separates the first, since those four blocks are gone either
// way) or CardLost. Hardware-validated as above.
void iso15693_poller_start_clone_gen1(
    Iso15693Poller* instance,
    const Iso15693_3Data* source,
    Iso15693PollerCallback callback,
    void* context);

// The per-block write result of a clone or a wipe. A "failure" here means the block failed EVERY write
// retry (a transient glitch that later succeeded is not a failure).
typedef struct {
    // The blocks this run attempted and reports against, which is mode-dependent: the source block
    // count for a gen2 clone, that count minus whichever of the 4 gen1 registers fall BELOW it for
    // a gen1 clone (on a source smaller than 57 blocks that is none of them), or, for a
    // wipe, the advertised count WHILE the sweep runs (the progress popup needs a denominator
    // before the sweep's true length is known) and then highest_present + 1. That last one is a
    // RANGE SIZE, not a tally: it spans up to the highest block the card proved it holds, and
    // interior absences fall inside the span rather than reducing it -- they are carried by
    // failed_count instead. Terminal events therefore always report the measured figure; only
    // WriteProgress can see the advertised one. A count, never an index (a different axis, and
    // also true). See ISO15693_POLLER_WIPE_MAX_BLOCKS in the .c.
    uint16_t blocks_total;
    // Wipe only: the block count the card ADVERTISED, so a report can put the measured figure beside
    // the claim. The gen2 CFG frame programs this number, which is why the two differing is
    // information rather than an error: a card cloned from a smaller source advertises less than it
    // still holds and serves reads for, and a card that over-claims -- from the factory, or cloned from
    // a larger source -- advertises more. 0 for a clone.
    uint16_t blocks_advertised;
    // The run stopped on its wall-clock bound (ISO15693_POLLER_PASS_MAX_MS) rather than at its natural
    // end, so its range is a cut and no report may pass that range off as a finding about the card.
    // BOTH modes carry the same bound, but what the flag SAVES differs. The clone's back-fill records
    // every block above the cut as a failure, so without the flag a reader cannot tell a block the card
    // REFUSED from one nothing was sent to, and every screen downstream states the stronger claim. A
    // cut WIPE has no back-fill -- both of its bit-setting sites are inside the body the deadline
    // gates, so its unreached blocks fall outside the denominator rather than inside the numerator --
    // so there the flag is the only thing that mentions them at all.
    //
    // What a re-run can do about it, since two screens have to answer that: the bound is a WALL CLOCK,
    // not a position, so a card that is consistently this slow is cut in the same place every time and
    // only a transient -- marginal coupling forcing per-block retries -- clears on a second pass.
    // Observed: a retried wipe stopped at the same block. So Retry may be offered but never promised.
    bool pass_truncated;
    // Where the clock cut the run: the first block index NOT attempted. Only meaningful when the flag
    // above is set, and NOT derivable from blocks_total -- after a wipe that is highest_present + 1 and
    // sits at or below the cut -- structurally, so no card can put blocks_total above it. What two
    // cards DO differ in is which side of the ADVERTISED COUNT the cut lands on, and only one of them
    // opens a gap between the cut and the total:
    //   past the claim -- the gap is bounded by ISO15693_POLLER_WIPE_ABSENT_RUN - 1, so
    //     cut_block - blocks_total <= 7, because above the claim a tripped run is either re-probed
    //     back under the threshold or ends the sweep.
    //   below the claim -- THAT BOUND DOES NOT HOLD. The trip falls through without clearing
    //     absent_run, which is the advertised-count floor doing its job, so the run grows unchecked.
    //     Claiming 200, holding 10, clock at block 150: the tail-drop clears 10..149, blocks_total
    //     stuck at 10, cut_block 150 -- a gap of 140.
    // Either way, any string naming where the run stopped has to read this, not blocks_total.
    uint16_t cut_block;
    // Wipe only: the post-power-cycle UID check reached an answer. When false it did not run -- the card
    // did not come back, or did not answer the inventory -- so uid_changed being false is the absence of
    // an observation rather than a clean result, and a caller reporting success should say so.
    bool uid_verified;
    // Clone only: the data pass sent a frame to block 56 or 57, which on a gen1 card IS the UID, so the
    // run re-reads the UID before it reports. Every answer to that re-read ends the run as Success,
    // Partial or Fail, so on CardLost this means it never answered -- the card left mid-pass or at the
    // re-read -- and whether the card still answers to the target is unknown.
    bool uid_recheck;
    // Blocks that failed and count as a real problem: they held source data (lost), or were empty
    // failures that weren't a clean top-of-card tail. -> Partial. In wipe mode this is every block the
    // report holds against the card, which is more than the ones that still held data: interior blocks
    // that answered nothing are folded in too (they cannot be shown clear, so they fail closed), as are
    // blocks proven present by the activation cache. It prints to the user as "Not cleared: %u" and
    // names indices in Details, so it has to be the whole set.
    uint16_t failed_count;
    // Empty blocks that failed and form a contiguous run at the top of the card (past physical
    // capacity; nothing lost) -> Success with a note. Unused by a wipe.
    uint16_t over_capacity;
    // Bit N set = block N failed, at its TRUE block index, covering both buckets above. A set bit can
    // therefore sit ABOVE blocks_total -- scan the whole bitmap, not [0, blocks_total).
    uint8_t failed_bitmap[ISO15693_POLLER_BLOCK_BITMAP_SIZE];
    // The gen1 fallback set the UID. Implies gen1_attempted; the difference is that here it read back.
    bool used_gen1;
    // ...and the source reached one or more of 56/57/62/63, which on gen1 are registers, so those
    // blocks of the file were not written. Narrower than used_gen1: on a source below block 56 those
    // addresses are not in the file at all. Set from the same expression that deducts them from
    // blocks_total, so the count and the flag cannot disagree.
    bool gen1_blocks_skipped;
    // ...and the file held DATA in at least one of them, so the clone lost it: the one case those four
    // addresses make a clone Partial. A file that reaches them with nothing there loses nothing, and a
    // screen saying they "differ from the source" would be describing a loss that did not happen.
    bool gen1_data_lost;
    // SURVEY RESULTS, not write results. How a clone reports them is at
    // NfcMagicIso15693WriteFailReasonCloneComplete; why the survey reads rather than trusting the
    // card's count, and why these are separate findings, is at iso15693_poller_survey_above_source.
    //
    // Readable blocks above the source that still hold data from before the clone. first/last bound
    // the non-empty ones and are meaningless while residue_found is false.
    bool residue_found;
    uint16_t residue_first;
    uint16_t residue_last;
    // The card answered a read ABOVE the count it now reports, so it is bigger than it claims.
    // survey_top is the highest block proven readable.
    bool holds_more;
    uint16_t survey_top;
    // The card goes on reporting a different block count or IC reference from the source's -- see
    // iso15693_poller_compare_reported_geometry in the .c. Two flags, because they move independently:
    // a file and a card can agree on block count and disagree on IC reference, and a screen naming
    // both would then describe a mismatch that is not there. Either, neither or both.
    //
    // card_blocks / card_ic_ref are what the CARD reported, and 0 when GET SYSTEM INFO did not answer
    // or did not carry that field -- which a scene cannot tell apart, so each note that prints one is
    // gated on a flag that needed a real value: memory_differs, ic_ref_differs, or holds_more for the
    // size note, which reads card_blocks whether or not either flag here is set.
    bool memory_differs;
    bool ic_ref_differs;
    uint16_t card_blocks;
    uint8_t card_ic_ref;
    // ...and what the FILE said, so the note can show both sides rather than leaving the reader to
    // remember what they asked for.
    uint16_t file_blocks;
    uint8_t file_ic_ref;
    // The failures are a persistent, contiguous run at the very top of the card, i.e. the source is
    // genuinely larger than the card's physical capacity. False for a scattered or anomalous failure,
    // which is reported generically with no capacity claim -- and false for ANY cut run, however
    // tail-shaped its failures look, since a cut cannot establish that memory stops rather than that
    // time did. That costs a real "Card too small" on a card so much smaller than its source that the
    // failure run alone spends the budget; the .c argues the trade at failures_are_top_tail.
    bool capacity_confirmed;
    // The source reported an AFI / DSFID, but after the write GET SYSTEM INFO did not read that field
    // back with the source's value, so the copy does not carry it. Verified by read-back, not inferred
    // from the write's return. -> Partial.
    bool identity_failed;
    // Running position of the block pass, for the live progress popup. Meaningful from the first
    // WriteProgress event. Converges on blocks_total for a clone that RUNS TO THE END; a cut clone
    // deliberately stops short and the last frame does not jump to 100%, which the .c argues at the
    // point it declines to. For a wipe the two need not end up equal either, for the reason
    // blocks_total gives.
    uint16_t blocks_done;
    // Fail: the gen2 backdoor moved the UID to neither the original nor the target. That PROVES the
    // card is magic -- an inert tag cannot change its UID -- so it is not "not a magic tag".
    // uid_readback holds what the card answered with, which is the only way back to it.
    // A clone's re-read after a pass that reached 56/57 sets it too, on a UID other than the target.
    // So does a gen1 sequence that leaves half a UID.
    bool uid_unexpected;
    uint8_t uid_readback[ISO15693_3_UID_SIZE];
    // This run SENT the destructive gen1 UID sequence, so the gen1 registers have had the
    // sequence's bytes written at them whatever the outcome. Whether the tag took them is
    // not known -- the frames' return values are discarded, because a refusal does not mean the
    // write did not land. And any writable tag accepts an ordinary WRITE BLOCK, so on a Fail the
    // honest report is that those four blocks may have been overwritten on what is most likely an
    // ordinary tag. Set immediately before those frames go out, so a run that never activated a
    // card cannot claim them.
    bool gen1_attempted;
    // Fail, Write UID only: the requested UID is the one the card already has, so nothing was written.
    // A read-back against a UID the card already carries is passed by any tag, magic or not, so a
    // Success there would be unearned -- the run stops instead of claiming one.
    bool uid_unverifiable;
    // Partial, wipe only: the UID read back after the wipe is not the one the card presented
    // before it, so the wipe moved the card's identity. uid_readback holds the UID it now answers
    // to. Only ever set from a positive observation of a different UID -- an inventory that fails
    // outright is logged and ignored, since it cannot be told from the card being lifted the
    // moment the wipe finished.
    //
    // "Moved" is the generous reading. Observed on an LRi2K: the UID went to ALL ZEROS, and an
    // ISO15693 UID must begin 0xE0, so the card was left with no valid identity rather than a
    // different one. It still answered inventory, so it stayed reachable -- which is why
    // uid_readback is printed.
    bool uid_changed;
} Iso15693PollerResult;

// Fill `result` with the outcome of the last clone or wipe. Valid once a terminal event has been
// reported; the poller resets every field at the start of each run.
void iso15693_poller_get_result(Iso15693Poller* instance, Iso15693PollerResult* result);

// True if the source stores real data in one of the gen1 registers, which a gen1 fallback would
// overwrite -- so the write flow can warn before a gen1 clone. Inspects the source image only; it
// touches no hardware and says nothing about whether the target is magic.
bool iso15693_poller_source_uses_gen1_blocks(const Iso15693_3Data* source);

// Wipe: write zeros to every data block the card PHYSICALLY holds. (proxmark's `hf 15 wipe` does not:
// its 0..0xFF loop breaks at the first refused write.)
// It does NOT stop at the card's advertised block count, because that number is programmable (see
// blocks_advertised) and a card still serves reads above it. The sweep runs upward until a run of
// ISO15693_POLLER_WIPE_ABSENT_RUN blocks answers neither a write nor a read, or it hits the 256-block
// ceiling, or the clock cuts it -- see ISO15693_POLLER_WIPE_MAX_BLOCKS in the .c for the hardware
// measurement behind the first of those. BELOW the advertised count it never stops on absence alone:
// the card's own claim is evidence those blocks exist.
// The gen1 registers are cleared too: on gen2 they are ordinary user data and the wipe performs no
// magic detection, so it cannot spare them on the chance the card is gen1 -- meaning it cannot
// guarantee a gen1 UID survives. It re-reads the UID afterwards and reports rather than promises;
// uid_changed and uid_verified carry the answer. See the open question in iso15693_poller_wipe_blocks.
// Emits CardDetected, then Success / Partial / Fail (nothing could be wiped) / CardLost. Per-block
// detail is in iso15693_poller_get_result(), whose blocks_total spans up to the highest block the card
// proved it holds, so blocks that do not exist are never reported as blocks that wouldn't clear.
void iso15693_poller_start_wipe(
    Iso15693Poller* instance,
    Iso15693PollerCallback callback,
    void* context);

void iso15693_poller_stop(Iso15693Poller* instance);

// The last Info-mode read result (UID + system info + block data). Owned by the poller; valid until
// the poller is freed, so the Info scene copies it out. Read-only.
const Iso15693_3Data* iso15693_poller_get_data(Iso15693Poller* instance);

#ifdef __cplusplus
}
#endif
