#include "../nfc_magic_app_i.h"
#include "nfc_magic_scene_partial_details_common.h"

// The per-block "which blocks didn't write/clear" list for an ISO15693 partial clone/wipe, reached
// via "Details" on the partial summary -- mirrors the Gen2 / USCUID-UL partial-details screens.

// Opens one entry on the page: a blank line first unless it is the first thing there, then its
// bullet. The block list's label and every note open this way, so the page's format is one rule.
static void nfc_magic_scene_iso15693_partial_details_begin_note(FuriString* message) {
    if(furi_string_size(message) > 0) furi_string_cat_str(message, "\n\n");
    furi_string_cat_str(message, "- ");
}

void nfc_magic_scene_iso15693_partial_details_on_enter(void* context) {
    NfcMagicApp* instance = context;
    Widget* widget = instance->widget;

    // Reached from the write-fail summary (still on the stack), whose reason says whether the card was
    // lost. The block list's label is decided from the capacity facts below, not from that reason.
    const uint32_t reason =
        scene_manager_get_scene_state(instance->scene_manager, NfcMagicSceneIso15693WriteFail);
    const bool card_lost = (reason == NfcMagicIso15693WriteFailReasonCardLost);
    const bool wipe_mode = (instance->iso15693_mode == NfcMagicIso15693ModeWipe);
    // Bound the list at the cut, for the reason pass_truncated gives: on a clone the bitmap holds two
    // different things at two different addresses, and only the group below the cut is a fact about the
    // card. Listing them together would name the back-filled group as refusals. A cut wipe records
    // nothing above its cut, so the bound is inert there -- applied in both modes anyway, because a
    // rule that holds in one and is inert in the other beats a mode test.
    //
    // Nothing at all on a card-lost run, wipe or clone: a lifted card makes blocks that never answered
    // look like refusals, and the poller documents those counters as the caller's to discard on that
    // exit.
    // Zero suppresses the list and has_block_list together, since both go through list_upto.
    const uint16_t list_upto = card_lost ? 0 :
                               instance->iso15693_result.pass_truncated ?
                                           instance->iso15693_result.cut_block :
                                           (uint16_t)ISO15693_POLLER_MAX_BLOCKS;
    // A partial can reach this screen with NO failed blocks -- when its only problem is the gen1 data
    // loss, a rejected AFI/DSFID, or a cut that happened before anything was refused -- so the list,
    // and the label that opens it, appear only when there is something to list.
    //
    // Asked over the SAME range that will be printed, structurally: both go through list_upto. Not
    // from failed_count, which on a cut run includes every unattempted block above the cut.
    //
    // No `+ over_capacity` term: over_capacity survives non-zero only down the failures_are_top_tail
    // branch, which requires an uncut pass and so forces list_upto to the full range, where every one
    // of those blocks already has its bit set. The sum would read as though the two were complementary
    // when one contains the other.
    const bool has_block_list =
        nfc_magic_partial_details_any_index(instance->iso15693_result.failed_bitmap, list_upto);
    // Whether the listed blocks are merely EMPTY ones past the card's physical capacity (nothing lost)
    // has to be decided from the capacity facts, not from the reason code: a Partial can consist purely
    // of an empty capacity tail plus a gen1 / AFI-DSFID caveat, and in that state over_capacity
    // only survived because the failures were a confirmed contiguous top tail. Calling those blocks
    // "not written" with no softening would overstate the damage on a clone that lost nothing.
    const bool only_empty_tail = (instance->iso15693_result.failed_count == 0) &&
                                 (instance->iso15693_result.over_capacity > 0);
    // One title whatever the page holds: its notes are independent of one another, and each opens
    // with its own label.
    const char* title = wipe_mode ? "Wipe notes" : "Clone notes";
    widget_add_string_element(widget, 0, 0, AlignLeft, AlignTop, FontPrimary, title);

    FuriString* message = furi_string_alloc();
    // Empty top blocks are past the card's physical capacity and lost nothing, so they are labelled
    // as not fitting rather than as failures. ("Card too small" is reserved for the partial screen,
    // where data IS lost.)
    if(has_block_list) {
        nfc_magic_scene_iso15693_partial_details_begin_note(message);
        furi_string_cat_str(
            message,
            only_empty_tail ? "Didn't fit on the card: " :
            wipe_mode       ? "Blocks not cleared: " :
                              "Blocks not written: ");
    }
    // The bound is list_upto, never blocks_total: a gen1 clone's blocks_total excludes the skipped
    // backdoor blocks (56/57/62/63) and a wipe's stops at the highest block proven present, yet
    // failures are recorded at their TRUE block index, which can exceed either. Unused bits are 0,
    // so on an uncut run scanning the whole bitmap prints only real failures, each at its true
    // index.
    nfc_magic_partial_details_append_indices(
        message, instance->iso15693_result.failed_bitmap, list_upto, 0);
    // Separate the caveats from whatever precedes them, but don't open with a blank line when there is
    // no block list above (the notes-only case).
    if(instance->iso15693_result.pass_truncated && !card_lost) {
        // The blocks above the cut are the ones the list above deliberately excludes, which is exactly
        // why they need saying here. This is also the only route to that fact on the UID-changed
        // screen, whose reason code pre-empts the partial one, and the only place either summary's
        // count can be qualified.
        //
        // Not on a card-lost run, which gets no list either. A lifted card makes every block after it
        // time out, so on a long enough file the clock cuts the pass before the card is found gone,
        // and this note would put the lift down to the time budget and the card's speed.
        nfc_magic_scene_iso15693_partial_details_begin_note(message);
        // The cut index, never blocks_total: this is a claim about which blocks were TRIED, and
        // blocks_total is a COUNT, one past the highest block that answered. It leaves out the
        // trailing run the tail-drop discards, which was attempted -- three writes and a read each
        // -- and below the claim that run can be most of the sweep: a card claiming 200 while
        // holding 10, cut at block 150, would say "time limit at block 10" about the 140 blocks
        // between that were attempted and answered nothing (the header's example at cut_block).
        if(wipe_mode) {
            // Which side of the claim the cut lands on changes what is true, so it changes the
            // sentence. The sweep runs past the advertised count deliberately, so "of the N this card
            // claims" is only a frame when the cut is actually inside it.
            // STRICT <, and the boundary is why: blocks_advertised is a COUNT and cut_block is an INDEX,
            // so at equality the claimed blocks are 0..N-1 and the cut sits at index N -- the first block
            // PAST the claim. "at block N of the N this card claims" would name an index that is not one
            // of the N, and read as a completed fraction on the one boundary where the sweep was cut.
            if(instance->iso15693_result.cut_block < instance->iso15693_result.blocks_advertised) {
                furi_string_cat_printf(
                    message,
                    "Sweep hit its time limit at block %u of the %u this card claims. Blocks above "
                    "that were never attempted and may still hold data.",
                    instance->iso15693_result.cut_block,
                    instance->iso15693_result.blocks_advertised);
            } else {
                furi_string_cat_printf(
                    message,
                    "Sweep hit its time limit at block %u, past the %u this card claims. Every "
                    "claimed block was attempted; anything above the cut was not.",
                    instance->iso15693_result.cut_block,
                    instance->iso15693_result.blocks_advertised);
            }
        } else {
            // A clone has no advertised count to measure against -- its denominator is the source -- so
            // it states the cut alone. What re-running can do is the shared clause below; it is the same
            // answer in both modes.
            furi_string_cat_printf(
                message,
                "Clone hit its time limit at block %u. Blocks from there up were never sent to the "
                "card -- they are counted as not written, but the card did not refuse them.",
                instance->iso15693_result.cut_block);
        }
        // What Retry can and cannot do, said once for both modes -- see pass_truncated for why a re-run
        // is not a promise. The wording therefore may not claim a retry succeeds: the Retry button
        // already implies it, and this is the only place that can qualify it.
        furi_string_cat_str(
            message,
            "\nRetrying may get further, but the limit is a time budget rather than a position: if it "
            "stops at the same block, the card is too slow to finish in one pass rather than refusing.");
    }
    if(wipe_mode && !instance->iso15693_result.uid_verified) {
        // Six wipe reason codes are reachable and this route covers the ones that need it: WipeComplete
        // states it inline. NothingWiped is the one wipe reason with no route here, and there
        // uid_verified is false BY CONSTRUCTION -- see the wiped == 0 short-circuit in write_step,
        // which owns why that is a hazard and why it is left alone.
        //
        // The wording is chosen by route. A CardLost wipe never entered the verify state, so "did not
        // answer after the field reset" would name a reset that never happened; the other reasons got
        // there and were met with silence. VerifyWipe does not downgrade to CardLost on that silence,
        // it logs and falls through, which is why the second wording belongs to them.
        nfc_magic_scene_iso15693_partial_details_begin_note(message);
        furi_string_cat_str(
            message,
            card_lost ? "UID not re-checked: the card stopped answering before the identity check "
                        "could finish, so whether the wipe changed the card's UID is unknown." :
                        "UID not re-checked: the card did not answer after the field reset, so "
                        "whether the wipe changed the card's UID is unknown.");
    }
    if(!wipe_mode && card_lost && instance->iso15693_result.uid_recheck) {
        // The clone's counterpart, on the one route that leaves its re-read unanswered -- has_details
        // in the write-fail scene says why a CardLost with uid_recheck is that route. uid_recheck alone
        // is not enough: it stays set on the Success, Partial and Fail that an answer produces.
        nfc_magic_scene_iso15693_partial_details_begin_note(message);
        furi_string_cat_str(
            message,
            "UID not re-checked: the clone sent writes to blocks 56/57, which on a gen1 card are its "
            "UID, and the card stopped answering before the UID was read back, so whether it still "
            "answers to the file's UID is unknown.");
    }
    if(instance->iso15693_result.used_gen1 && instance->iso15693_result.gen1_blocks_skipped) {
        // Only where the file reached those blocks; see gen1_blocks_skipped.
        nfc_magic_scene_iso15693_partial_details_begin_note(message);
        furi_string_cat_str(
            message, "gen1: 56/57/62/63 are the UID / backdoor registers, not file data.");
    }
    if(instance->iso15693_result.residue_found) {
        nfc_magic_scene_iso15693_partial_details_begin_note(message);
        furi_string_cat_printf(
            message,
            "Blocks %u-%u are readable and still hold non-zero data that was on the card before. "
            "Only the blocks from the file were written. To clear the rest, use 'Wipe' first then "
            "'Write' again.",
            instance->iso15693_result.residue_first,
            instance->iso15693_result.residue_last);
    }
    if(instance->iso15693_result.holds_more) {
        nfc_magic_scene_iso15693_partial_details_begin_note(message);
        // "The same as the file" only when the two numbers are equal. Not "configured to match": that
        // claims this app set the count, and only the gen2 CFG frame does, only on a magic card. A
        // non-magic tag already wearing the file's UID, and a gen1 clone, both reach here with a count
        // nothing here wrote.
        const uint16_t held = (uint16_t)(instance->iso15693_result.survey_top + 1);
        const bool same = instance->iso15693_result.card_blocks ==
                          instance->iso15693_result.file_blocks;
        furi_string_cat_printf(
            message,
            "The card reports %u blocks%s but holds %u, and still answers individual reads to those "
            "higher blocks. Some readers may detect this.",
            instance->iso15693_result.card_blocks,
            same ? ", the same as the file," : "",
            held);
    }
    if(instance->iso15693_result.memory_differs || instance->iso15693_result.ic_ref_differs) {
        // Both sides, and only the halves that moved.
        //
        // "This card", not "this gen1 card": the note fires whenever the two disagree, and gen1 is the
        // usual cause but not the only one -- a card that is not magic at all, whose UID already
        // matched the file's, reaches here too, with no configuration write ever taking. The gen1
        // sentence below is a general fact about why such a card cannot be made to match, which is
        // true wherever it is read.
        const Iso15693PollerResult* r = &instance->iso15693_result;
        const bool both = r->memory_differs && r->ic_ref_differs;
        nfc_magic_scene_iso15693_partial_details_begin_note(message);
        furi_string_cat_str(message, "The file says ");
        if(r->memory_differs) furi_string_cat_printf(message, "%u blocks", r->file_blocks);
        if(both) furi_string_cat_str(message, " and ");
        if(r->ic_ref_differs) furi_string_cat_printf(message, "IC ref %02X", r->file_ic_ref);
        furi_string_cat_str(message, ". This card reports ");
        if(r->memory_differs) furi_string_cat_printf(message, "%u blocks", r->card_blocks);
        if(both) furi_string_cat_str(message, " and ");
        if(r->ic_ref_differs) furi_string_cat_printf(message, "IC ref %02X", r->card_ic_ref);
        furi_string_cat_str(
            message,
            ". gen1 cards have no configuration register, so a gen1 clone copies the UID and the "
            "data but not how the card describes itself. Some readers may detect this. For a closer "
            "copy use a gen1 card that already matches, or a gen2 card, which can be told what to "
            "report.");
    }
    if(instance->iso15693_result.identity_failed && !card_lost) {
        // The card rejected the standard WRITE AFI / WRITE DSFID, so those identity fields may not
        // match the source. Not on a card-lost run: a card lifted before the read-back fails it too,
        // and "rejected" would be false.
        nfc_magic_scene_iso15693_partial_details_begin_note(message);
        furi_string_cat_str(message, "AFI/DSFID: card rejected the write.");
    }
    widget_add_text_scroll_element(widget, 0, 13, 128, 51, furi_string_get_cstr(message));
    furi_string_free(message);

    view_dispatcher_switch_to_view(instance->view_dispatcher, NfcMagicAppViewWidget);
}

bool nfc_magic_scene_iso15693_partial_details_on_event(void* context, SceneManagerEvent event) {
    NfcMagicApp* instance = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeBack) {
        consumed = scene_manager_previous_scene(instance->scene_manager);
    }
    return consumed;
}

void nfc_magic_scene_iso15693_partial_details_on_exit(void* context) {
    NfcMagicApp* instance = context;
    widget_reset(instance->widget);
}
