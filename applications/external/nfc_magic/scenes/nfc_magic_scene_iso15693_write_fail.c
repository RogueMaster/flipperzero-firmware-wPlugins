#include "../nfc_magic_app_i.h"
#include "../magic/protocols/iso15693/iso15693_info.h"

// LINE BUDGET for every body in this file. FontSecondary advances 11px and the button box occupies
// rows 52-63, so the body's y decides how many lines clear it. Two y values are used here, and both
// come out at THREE:
//   y=13 -- tops at 13/24/35, and a fourth at 46 puts its lower rows inside the box.
//   y=20 -- tops at 20/31/42, and a fourth at 53 sits inside the box entirely.
//
// The x that rides with each: 0 with y=13, 4 with y=20. The bodies at (4, 20) are not an inconsistency
// to tidy away: that pair appears in exactly three files in the repo, the other two being
// nfc_magic_scene_gen2_wipe_partial.c and nfc_magic_scene_uscuid_ul_partial.c. The partial and
// over-capacity branches here mirror those screens, and clone-complete matches its two neighbours.
// Making every body agree would break that match. It is also why the calls want no wrapper: the
// duplication is in describing the layout, not in the calls.
// Where the body carries a UID that budget decides the prose, because a clipped hex digit is a
// mis-readable UID.

void nfc_magic_scene_iso15693_write_fail_widget_callback(
    GuiButtonType result,
    InputType type,
    void* context) {
    NfcMagicApp* instance = context;

    if(type == InputTypeShort) {
        view_dispatcher_send_custom_event(instance->view_dispatcher, result);
    }
}

// How many blocks succeeded, saturating at zero. Three lines on this screen need that figure and all
// three take it as a subtraction, so the reason it cannot be a bare one lives here.
//
// The total and the deduction are counted independently, whichever pair a caller passes.
// blocks_total has three forms and its contract is in the header rather than restated here -- a
// restatement of a three-way contract is exactly what drifts. The deduction varies too: the clone
// passes failed_count plus over_capacity, not failed_count alone.
//
// So the two are not halves of one count, and they can disagree: the tail-drop's keep branch can count
// a proven-present block as failed while blocks_total stops at the highest block the card proved,
// rendering "Wiped 0/20, not cleared: 44" -- the example that branch carries in its own comment. That
// is guarded where it happens, in the sweep; this is what reaches the user if the guard fails. The
// counts are unsigned, so an inversion would otherwise print near 65535.
static uint16_t nfc_magic_iso15693_blocks_ok(uint16_t total, uint16_t bad) {
    return (total >= bad) ? (uint16_t)(total - bad) : 0;
}

// Is re-running the write the right next action? A run the clock cut may have left real data above the
// cut -- on a wipe that is the privacy failure the sweep exists to remove, and on a clone it is simply
// the rest of the image -- as good a claim on Retry as a card that left mid-write. Both the buttons in
// on_enter and the left-button handler in on_event need this answer.
static bool
    nfc_magic_scene_iso15693_write_fail_is_retryable(NfcMagicApp* instance, uint32_t reason) {
    return reason == NfcMagicIso15693WriteFailReasonCardLost ||
           reason == NfcMagicIso15693WriteFailReasonWipeStopped ||
           // A cut clone, which has no reason code of its own -- it stays on the ordinary partial
           // screen, so the flag is the only thing that distinguishes it there. Re-running is the only
           // thing that CAN write the blocks above the cut, which is a better claim on Retry than the
           // blocks below it have: those were tried and refused. It is not a promise, though -- see
           // pass_truncated -- and the Details note has to say so, because the button cannot.
           (reason == NfcMagicIso15693WriteFailReasonPartial &&
            instance->iso15693_result.pass_truncated);
}

// Does this outcome have anything behind "Details"? One answer, for both the button in on_enter and the
// routing in on_event.
//
// Why it is not simply failed_count: a partial whose only problem is the gen1 data loss or a rejected
// AFI/DSFID has zero failed blocks, and the summary shows only its highest-priority qualifier, so the
// lower one would be reachable nowhere. UID-changed is here because it PRE-EMPTS the partial reason
// code -- without it a wipe that both moved the UID and left blocks uncleared names them nowhere.
static bool
    nfc_magic_scene_iso15693_write_fail_has_details(NfcMagicApp* instance, uint32_t reason) {
    const Iso15693PollerResult* result = &instance->iso15693_result;
    switch((NfcMagicIso15693WriteFailReason)reason) {
    case NfcMagicIso15693WriteFailReasonOverCapacity:
    case NfcMagicIso15693WriteFailReasonCloneComplete:
        return true;
    case NfcMagicIso15693WriteFailReasonPartial:
        // pass_truncated on its own qualifies: on a cut clone the summary's "Not written" count mixes
        // refused blocks with never-attempted ones, and the scroll view is the only place that can
        // separate them.
        return result->failed_count > 0 || (result->used_gen1 && result->gen1_data_lost) ||
               result->identity_failed || result->pass_truncated;
    case NfcMagicIso15693WriteFailReasonWipeStopped:
        // Always: the blocks above the cut were never attempted, so they carry no bitmap bits and the
        // scroll view is the only place that fact can be stated.
        return true;
    case NfcMagicIso15693WriteFailReasonWipeUidChanged:
        return result->failed_count > 0 || result->pass_truncated;
    case NfcMagicIso15693WriteFailReasonCardLost:
        // Only where an identity check never answered, and Details is the only route to the note
        // saying so. A wipe's run returns before Iso15693WriteStateVerifyWipe is entered. A clone's
        // re-read (Iso15693WriteStateVerifyClone) is due only once its pass has sent a frame to 56/57,
        // and every answer it gets ends the run as something other than CardLost, so uid_recheck here
        // means the card left mid-pass or at the re-read without its UID being read back. The
        // uid_verified term is DEFENSIVE, not load-bearing: the flag is set only inside
        // Iso15693WriteStateVerifyWipe, and both of that state's exits report success_or_partial (the
        // case body, and the activation-error path that tests verifying_wipe), so a CardLost result
        // always carries it false and the term cannot currently bite. It is kept because this scene
        // cannot enforce that invariant -- it reads a result struct it did not fill -- and a poller
        // that grew a CardLost exit from that state would need it.
        return instance->iso15693_mode == NfcMagicIso15693ModeWipe ? !result->uid_verified :
                                                                     result->uid_recheck;
    case NfcMagicIso15693WriteFailReasonNotMagic:
    case NfcMagicIso15693WriteFailReasonNothingWiped:
    case NfcMagicIso15693WriteFailReasonEmptySource:
    case NfcMagicIso15693WriteFailReasonNothingCloned:
    case NfcMagicIso15693WriteFailReasonUidUnexpected:
    case NfcMagicIso15693WriteFailReasonGen1Failed:
    case NfcMagicIso15693WriteFailReasonUidUnverifiable:
    case NfcMagicIso15693WriteFailReasonWipeComplete:
        return false;
    case NfcMagicIso15693WriteFailReasonUnset:
        break;
    }
    // No default, so -Wswitch (in -Wall, with -Werror) makes a reason added without an answer here a
    // build error. Unset, or a value outside the enum, is a caller that entered this scene without
    // setting a reason.
    furi_crash("iso15693 write-fail: no reason");
}

// The one thing every screen here has in common: exactly one centred FontPrimary title, so it is named
// here and drawn once, below.
//
// Deliberately NOT a {reason, title, body} table. Most bodies are built from result counts and several
// carry conditional extra lines, so a table holding only the static few would split one screen across
// two mechanisms and turn "what does reason X render?" into a two-place lookup -- worse than one chain.
// The title is the part that really is uniform, so it is the part that gets the table.
static const char* nfc_magic_scene_iso15693_write_fail_title(uint32_t reason, bool wipe_mode) {
    switch((NfcMagicIso15693WriteFailReason)reason) {
    case NfcMagicIso15693WriteFailReasonWipeComplete:
        return "Wipe complete";
    case NfcMagicIso15693WriteFailReasonWipeStopped:
        return "Wipe stopped";
    case NfcMagicIso15693WriteFailReasonOverCapacity:
    case NfcMagicIso15693WriteFailReasonCloneComplete:
        return "Clone finished";
    case NfcMagicIso15693WriteFailReasonPartial:
        // The only mode-dependent one, and the reason this is a function rather than an array.
        return wipe_mode ? "Wipe partial" : "Clone partial";
    case NfcMagicIso15693WriteFailReasonNothingWiped:
        return "Wipe failed";
    case NfcMagicIso15693WriteFailReasonNothingCloned:
        return "Clone failed";
    case NfcMagicIso15693WriteFailReasonWipeUidChanged:
        return "UID changed";
    case NfcMagicIso15693WriteFailReasonUidUnexpected:
        return "Unexpected UID";
    case NfcMagicIso15693WriteFailReasonGen1Failed:
        return "gen1 failed";
    case NfcMagicIso15693WriteFailReasonUidUnverifiable:
        return "UID unchanged";
    case NfcMagicIso15693WriteFailReasonEmptySource:
        return "Nothing to clone";
    case NfcMagicIso15693WriteFailReasonCardLost:
    case NfcMagicIso15693WriteFailReasonNotMagic:
        // They share a screen -- see the final else of the render chain.
        return "Write failed";
    case NfcMagicIso15693WriteFailReasonUnset:
        break;
    }
    // Unset, or a value outside the enum: see nfc_magic_scene_iso15693_write_fail_has_details.
    furi_crash("iso15693 write-fail: no reason");
}

void nfc_magic_scene_iso15693_write_fail_on_enter(void* context) {
    NfcMagicApp* instance = context;
    Widget* widget = instance->widget;

    const uint32_t reason =
        scene_manager_get_scene_state(instance->scene_manager, NfcMagicSceneIso15693WriteFail);
    const bool card_lost = (reason == NfcMagicIso15693WriteFailReasonCardLost);
    const bool partial = (reason == NfcMagicIso15693WriteFailReasonPartial);
    const bool over_capacity = (reason == NfcMagicIso15693WriteFailReasonOverCapacity);
    const bool clone_complete = (reason == NfcMagicIso15693WriteFailReasonCloneComplete);
    const bool nothing_wiped = (reason == NfcMagicIso15693WriteFailReasonNothingWiped);
    const bool empty_source = (reason == NfcMagicIso15693WriteFailReasonEmptySource);
    const bool nothing_cloned = (reason == NfcMagicIso15693WriteFailReasonNothingCloned);
    const bool uid_unexpected = (reason == NfcMagicIso15693WriteFailReasonUidUnexpected);
    const bool gen1_failed = (reason == NfcMagicIso15693WriteFailReasonGen1Failed);
    const bool uid_unverifiable = (reason == NfcMagicIso15693WriteFailReasonUidUnverifiable);
    const bool wipe_uid_changed = (reason == NfcMagicIso15693WriteFailReasonWipeUidChanged);
    const bool wipe_complete = (reason == NfcMagicIso15693WriteFailReasonWipeComplete);
    const bool wipe_mode = (instance->iso15693_mode == NfcMagicIso15693ModeWipe);

    const bool wipe_stopped = (reason == NfcMagicIso15693WriteFailReasonWipeStopped);
    // Over-capacity, a clone that completed with notes, and a wipe that ran to the card's top are
    // successes. Named once because two things follow from it, the tone below and the left button, and
    // a success added to one and not the other would sound an error under "Finish".
    const bool success = over_capacity || clone_complete || wipe_complete;

    // Successes -> success tone. Everything else did not deliver what was asked for -> error tone, the
    // cut sweep included: the poller reports that as Partial, and the tone has to agree with the event
    // rather than contradict it.
    notification_message(instance->notifications, success ? &sequence_success : &sequence_error);

    // One title, drawn once, named by the lookup above. Every branch below adds only its body.
    widget_add_string_element(
        widget,
        64,
        0,
        AlignCenter,
        AlignTop,
        FontPrimary,
        nfc_magic_scene_iso15693_write_fail_title(reason, wipe_mode));

    if(wipe_complete) {
        // A clean wipe, reporting what it actually covered. This screen exists because the sweep's
        // length is measured, not assumed: it stops at the highest block the card answered for, which
        // can be short of the card's claim or past it -- see blocks_advertised. On the bare Success
        // popup those render identically.
        //
        // Both figures, no verdict. blocks_total < advertised is NOT flagged as an error: a card
        // advertising 66 blocks against 64 physical is a normal, undamaged card, and calling its two
        // dropped phantom blocks a failure is the false report the tail-drop rule exists to prevent.
        FuriString* text = furi_string_alloc();
        furi_string_printf(
            text,
            "Cleared %u blocks.\nCard claims %u.",
            instance->iso15693_result.blocks_total,
            instance->iso15693_result.blocks_advertised);
        // Third and last line the body has room for. The wipe itself finished; only the identity check
        // did not, and saying so is what stops this screen asserting a check that never ran.
        if(!instance->iso15693_result.uid_verified) {
            furi_string_cat_str(text, "\nUID not re-checked.");
        }
        widget_add_string_multiline_element(
            widget, 0, 13, AlignLeft, AlignTop, FontSecondary, furi_string_get_cstr(text));
        furi_string_free(text);
    } else if(wipe_stopped) {
        // The clock stopped the sweep short of its natural end. Which blocks were left depends on where
        // the cut fell: below the card's claim there are claimed blocks still unattempted, above it every
        // claimed block was attempted and what is left is past the claim entirely. Either way they have
        // no bitmap bits -- nothing tried them -- so these counts are the whole on-screen story and
        // Details carries the rest.
        const uint16_t reached = instance->iso15693_result.blocks_total;
        const uint16_t failed = instance->iso15693_result.failed_count;
        FuriString* text = furi_string_alloc();
        // The figure is the CUT, not blocks_total: the latter is a COUNT, not an index (see
        // blocks_total in the header), and it sits at or below the cut, so a sweep that attempted 55
        // blocks and proved 50 present would read "Stopped at 50".
        //
        // No "of %u" either. The sweep deliberately runs past the advertised count, so the cut can land
        // above the claim and "Stopped at 200 of 64" parses as a fraction -- as falling short of 64
        // when it had exceeded it. Naming one block index makes no such claim. The comparison against
        // what the card claims still gets made in Details, where there is room to say which side of the
        // claim the cut fell on.
        //
        // "Timed out" rather than "Stopped": the clock is the only thing that sets this reason code, and
        // with a Retry button on screen the user needs to know what they are retrying against. It goes
        // in THIS line rather than a fourth one because this body already reaches three when blocks
        // failed, and three is the whole budget at y=13 -- see the top of the file.
        furi_string_printf(
            text,
            "Cleared %u blocks.\nTimed out at block %u.",
            nfc_magic_iso15693_blocks_ok(reached, failed),
            instance->iso15693_result.cut_block);
        if(failed > 0) furi_string_cat_printf(text, "\nNot cleared: %u", failed);
        widget_add_string_multiline_element(
            widget, 0, 13, AlignLeft, AlignTop, FontSecondary, furi_string_get_cstr(text));
        furi_string_free(text);
    } else if(clone_complete) {
        // Clean success with something the bare popup cannot hold. Three lines fit at y=20: the
        // confirmation plus ONE note, chosen in priority order -- residue first because it is about
        // the user's data, then size because it is about the card, then the block count or IC
        // reference it reports, which is only how the copy presents, then a gen1 card's four register
        // addresses, where the file had nothing to lose. Details carries every one.
        FuriString* text = furi_string_alloc();
        furi_string_cat_str(text, "All data written.");
        if(instance->iso15693_result.residue_found) {
            furi_string_cat_printf(
                text,
                "\nBlocks %u-%u hold\nolder data.",
                instance->iso15693_result.residue_first,
                instance->iso15693_result.residue_last);
        } else if(instance->iso15693_result.holds_more) {
            // What the card SAYS first, then what it is: the reported count is what a tool prints.
            // Both are COUNTS, deliberately -- naming the reported count against a top BLOCK number
            // mixes a count with an index and invites an off-by-one. Neither says who set that count;
            // see the size note in nfc_magic_scene_iso15693_partial_details.c.
            furi_string_cat_printf(
                text,
                "\nCard reports %u blocks,\nbut holds %u.",
                instance->iso15693_result.card_blocks,
                (uint16_t)(instance->iso15693_result.survey_top + 1));
        } else if(
            instance->iso15693_result.memory_differs || instance->iso15693_result.ic_ref_differs) {
            // Neither the data nor the size: what is left is how the card describes itself. Name
            // whichever of the two moved -- both fit on the remaining two lines when both did.
            furi_string_cat_str(text, "\nCard still reports\n");
            if(instance->iso15693_result.memory_differs) {
                furi_string_cat_printf(text, "%u blocks", instance->iso15693_result.card_blocks);
            }
            if(instance->iso15693_result.memory_differs &&
               instance->iso15693_result.ic_ref_differs) {
                furi_string_cat_str(text, ", ");
            }
            if(instance->iso15693_result.ic_ref_differs) {
                furi_string_cat_printf(text, "IC ref %02X", instance->iso15693_result.card_ic_ref);
            }
            furi_string_cat_str(text, ".");
        } else {
            // What is left is a gen1 clone whose file reached 56/57/62/63 with nothing stored there.
            // Nothing was lost, but the copy differs from the file in one way a reader can see: those
            // four are the card's registers, so asking for them returns no block where the file had one.
            furi_string_cat_str(text, "\ngen1: 56/57/62/63 are\nregisters, not data.");
        }
        widget_add_string_multiline_element(
            widget, 4, 20, AlignLeft, AlignTop, FontSecondary, furi_string_get_cstr(text));
        furi_string_free(text);
    } else if(over_capacity) {
        // Clean success: every source block that fits was written; the file is larger than the card,
        // and the blocks past the card's end were empty, so nothing was lost. Concise summary here,
        // gen2-style; the exact empty top blocks are behind "Details". We confirm each write was
        // accepted -- acknowledged, or read back on a card that cannot acknowledge one -- not that the
        // card matches the file, so the wording says "written", not "matches".
        const uint16_t advertised = instance->iso15693_result.blocks_total;
        const uint16_t extra = instance->iso15693_result.over_capacity;
        // The over-capacity gate guarantees >=1 block wrote, so extra < advertised.
        const uint16_t physical = (uint16_t)(advertised - extra);
        FuriString* text = furi_string_alloc();
        furi_string_printf(
            text,
            "All data written.\nHolds %u/%u blocks.\nTop %u were empty.",
            physical,
            advertised,
            extra);
        widget_add_string_multiline_element(
            widget, 4, 20, AlignLeft, AlignTop, FontSecondary, furi_string_get_cstr(text));
        furi_string_free(text);
    } else if(partial) {
        // Summary only -- counts here, the per-block list behind "Details" -- mirroring the Gen2 /
        // USCUID-UL partial screens. Partial means some blocks wouldn't write (real source data lost,
        // or empty failures that weren't a clean capacity tail), or a gen1 clone's file held data at
        // 56/57/62/63.
        const uint16_t total = instance->iso15693_result.blocks_total;
        // Everything that didn't write: real-data losses plus any empty blocks past capacity.
        const uint16_t not_written =
            instance->iso15693_result.failed_count + instance->iso15693_result.over_capacity;
        const uint16_t ok = nfc_magic_iso15693_blocks_ok(total, not_written);
        FuriString* text = furi_string_alloc();
        furi_string_printf(
            text,
            wipe_mode ? "Wiped %u/%u blocks\nNot cleared: %u" :
                        "Cloned %u/%u blocks\nNot written: %u",
            ok,
            total,
            not_written);
        // Three lines fit at y=20 -- see the top of the file -- so this body is the two count lines
        // plus ONE qualifier. Show the most significant, and there are FOUR in priority order: the cut
        // (pass_truncated) > real data loss (capacity_confirmed) > gen1 data loss (used_gen1 &&
        // gen1_data_lost) > AFI/DSFID (identity_failed). A lower one is dropped from THIS screen
        // only -- "Details" below is offered whenever any caveat applies and lists all of them, so
        // nothing is unreachable.
        // The cut ranks FIRST here, and it is a cut CLONE: a cut wipe has its own reason code and
        // screen, but a cut clone has none, so this branch is where it lands.
        if(instance->iso15693_result.pass_truncated) {
            // Outranks the rest on a cut run, because it is the only one that explains the count above
            // it: most of "Not written" is blocks nothing was sent to, not blocks the card refused.
            // Naming the cut here is what stops the number reading as a verdict on the card.
            furi_string_cat_printf(
                text, "\nTimed out at block %u", instance->iso15693_result.cut_block);
        } else if(
            instance->iso15693_result.capacity_confirmed &&
            instance->iso15693_result.failed_count > 0) {
            // Real data was lost because those blocks are a persistent, contiguous run at the top of
            // the card -> the card is physically smaller than the source. (An empty top tail loses
            // nothing and is reported as an over-capacity success, not here.)
            furi_string_cat_str(text, "\nCard too small");
        } else if(instance->iso15693_result.used_gen1 && instance->iso15693_result.gen1_data_lost) {
            // Only where the file held data there; see gen1_data_lost.
            furi_string_cat_str(text, "\ngen1: 56/57/62/63 differ");
        } else if(instance->iso15693_result.identity_failed) {
            // All data blocks took, but the card rejected the AFI/DSFID write.
            furi_string_cat_str(text, "\nAFI/DSFID not set");
        }
        widget_add_string_multiline_element(
            widget, 4, 20, AlignLeft, AlignTop, FontSecondary, furi_string_get_cstr(text));
        furi_string_free(text);
    } else if(nothing_wiped) {
        // A wipe that cleared nothing: the card accepted no zero-write (read-only / no usable
        // geometry). It gets its own branch because the generic "not a magic tag / UID write"
        // message would be wrong for a wipe, which asks the card for nothing but zeros.
        //
        // A card that refuses every write while still answering reads is exactly the card the
        // sweep's time limit exists for, and it is also the one that ends here: nothing accepted,
        // so the wipe short-circuits past the WipeStopped screen, which is where a cut wipe would
        // normally be reported. The cut is NOT lost -- it is reported right below, in place of the
        // prose, because three lines is all the body has.
        FuriString* text = furi_string_alloc();
        if(instance->iso15693_result.pass_truncated) {
            furi_string_printf(
                text,
                "No block accepted the\nzero-write.\nTimed out at block %u.",
                instance->iso15693_result.cut_block);
        } else {
            furi_string_set_str(
                text, "No blocks could be\ncleared -- the card\naccepted no zero-write.");
        }
        widget_add_string_multiline_element(
            widget, 0, 13, AlignLeft, AlignTop, FontSecondary, furi_string_get_cstr(text));
        furi_string_free(text);
    } else if(nothing_cloned) {
        // The gen2/gen1 UID write took, but every data block was rejected. Say what the card now holds:
        // it answers with the source's UID, so a UID-only reader accepts it while anything that reads
        // memory does not. No block list -- it would name every block.
        widget_add_string_multiline_element(
            widget,
            0,
            13,
            AlignLeft,
            AlignTop,
            FontSecondary,
            "The UID was written,\nbut no data block took.\nThe card has UID only.");
    } else if(wipe_uid_changed) {
        // The wipe cleared its blocks and the card came back answering a different UID -- in practice a
        // gen1 card whose 56/57 the sweep reached, which #255 covers and gen2 cannot do at all. Print
        // what it answers to now: without that the card is simply lost, since it no longer responds to
        // the UID the user knows it by.
        // success_or_partial ORs uid_changed with failed_count, so BOTH can hold: a wipe can move the UID
        // AND leave blocks uncleared. Lead with the counts rather than asserting "Data cleared", which
        // would be false in exactly that case -- and it costs nothing, since the counts replace a prose
        // line, which is what keeps the UID on line 3 inside the line budget above.
        const uint16_t wiped_total = instance->iso15693_result.blocks_total;
        const uint16_t wiped_bad = instance->iso15693_result.failed_count;
        FuriString* text = furi_string_alloc();
        furi_string_printf(
            text,
            "Wiped %u/%u. The card's\nUID moved. Now reads:\n",
            nfc_magic_iso15693_blocks_ok(wiped_total, wiped_bad),
            wiped_total);
        iso15693_info_cat_uid(
            text, instance->iso15693_result.uid_readback, Iso15693UidFormatGrouped);
        widget_add_string_multiline_element(
            widget, 0, 13, AlignLeft, AlignTop, FontSecondary, furi_string_get_cstr(text));
        furi_string_free(text);
    } else if(uid_unexpected) {
        // Everything else that lands on "Not a magic tag" is a card that did nothing; this one
        // demonstrably responded to a magic command, so saying it isn't magic would be exactly
        // backwards -- see uid_unexpected for why that is proof rather than an anomaly. It is answering
        // to a UID nobody asked for, and printing it is the only way the user can find the card again.
        // Prose kept to two lines so the UID lands on line 3, per the line budget above.
        FuriString* text = furi_string_alloc();
        furi_string_set_str(text, "Card is magic, but the\nUID it took isn't yours:\n");
        iso15693_info_cat_uid(
            text, instance->iso15693_result.uid_readback, Iso15693UidFormatGrouped);
        widget_add_string_multiline_element(
            widget, 0, 13, AlignLeft, AlignTop, FontSecondary, furi_string_get_cstr(text));
        furi_string_free(text);
    } else if(gen1_failed) {
        // The opt-in gen1 UID sequence didn't verify, so gen1_attempted's four blocks are most likely
        // gone on what is most likely an ordinary tag. The user consented to that risk; they still need
        // to be told it was spent, and on which blocks, to restore them from a backup.
        widget_add_string_multiline_element(
            widget,
            0,
            13,
            AlignLeft,
            AlignTop,
            FontSecondary,
            "UID didn't take, so not\na gen1 card. 56/57/62/63\nmay be overwritten.");
    } else if(uid_unverifiable) {
        // Write UID was asked for the UID the card already has, and nothing was sent -- see
        // uid_unverifiable. Worth a screen of its own rather than a generic Fail because the editor
        // pre-seeds itself from the last Info read, so this is two menu taps away rather than a typo.
        // Tell the user how to get an answer instead.
        widget_add_string_multiline_element(
            widget,
            0,
            13,
            AlignLeft,
            AlignTop,
            FontSecondary,
            "Already this card's UID,\nso nothing was written.\nTry a different one.");
    } else if(empty_source) {
        // A clone whose source image has no data blocks: nothing was written (not even the UID), so
        // the card is untouched. Distinct from a non-magic card.
        widget_add_string_multiline_element(
            widget,
            0,
            13,
            AlignLeft,
            AlignTop,
            FontSecondary,
            "The saved file has no\ndata blocks to clone.\nNothing was written.");
    } else {
        // A card lost during an opt-in gen1 run has already had the four backdoor registers written at,
        // so say so -- this screen is the only report that run produces, and "Card removed" alone would
        // hide damage the user consented to but was never told had been spent.
        const bool gen1_spent = card_lost && instance->iso15693_result.gen1_attempted &&
                                !instance->iso15693_result.used_gen1;
        const char* message =
            gen1_spent ? "Card removed mid-write.\ngen1 was attempted:\n56/57/62/63 may differ." :
            card_lost  ? "Card removed\nbefore the write\ncould finish." :
                         "Not a magic tag.\nThis card doesn't\nsupport UID write.";
        widget_add_icon_element(widget, 83, 22, &I_WarningDolphinFlip_45x42);
        widget_add_string_multiline_element(
            widget, 0, 13, AlignLeft, AlignTop, FontSecondary, message);
    }

    // THE RULE THE TWO FUNCTIONS SHARE, and the reason it has to be written down. The right slot is
    // three-way, not two:
    //   has_details      -> Details
    //   else retryable   -> Exit
    //   else             -> no right button at all
    // on_event decides the same way, in the same order. That third case matters to anyone extending
    // this: a non-retryable reason with nothing behind Details gets NO right button, not an Exit.
    // Nothing enforces it but this comment. With a reason in both predicates, deciding the slot on one
    // test and handling it on the other puts Details behind a control labelled Exit. Add a reason to
    // either predicate and re-read this.
    //
    // Back escapes from anywhere (on_event's SceneManagerEventTypeBack), so a screen that spends both
    // slots on Retry and Details is not a trap -- and Back is precisely what the "Exit" button below
    // duplicates.
    const bool has_details = nfc_magic_scene_iso15693_write_fail_has_details(instance, reason);
    if(nfc_magic_scene_iso15693_write_fail_is_retryable(instance, reason)) {
        // Retry re-runs the write. Matches the generic write-fail screen (Retry left).
        widget_add_button_element(
            widget,
            GuiButtonTypeLeft,
            "Retry",
            nfc_magic_scene_iso15693_write_fail_widget_callback,
            instance);
        // Details outranks Exit here: an outcome with something behind Details is one where the counts
        // on this screen are not the whole story, and Back already leaves. A cut run is the case in
        // point -- its truncation note has no other route, and this screen's own numbers are the ones
        // it qualifies.
        widget_add_button_element(
            widget,
            GuiButtonTypeRight,
            has_details ? "Details" : "Exit",
            nfc_magic_scene_iso15693_write_fail_widget_callback,
            instance);
    } else {
        // The successes and a partial are (qualified) successes -> "Finish"; not-magic is a failure ->
        // "Back". The primary exit sits on the left, like the Gen2/USCUID/gen4 screens.
        widget_add_button_element(
            widget,
            GuiButtonTypeLeft,
            (success || partial) ? "Finish" : "Back",
            nfc_magic_scene_iso15693_write_fail_widget_callback,
            instance);
        if(has_details) {
            widget_add_button_element(
                widget,
                GuiButtonTypeRight,
                "Details",
                nfc_magic_scene_iso15693_write_fail_widget_callback,
                instance);
        }
    }

    view_dispatcher_switch_to_view(instance->view_dispatcher, NfcMagicAppViewWidget);
}

bool nfc_magic_scene_iso15693_write_fail_on_event(void* context, SceneManagerEvent event) {
    NfcMagicApp* instance = context;
    bool consumed = false;

    const uint32_t reason =
        scene_manager_get_scene_state(instance->scene_manager, NfcMagicSceneIso15693WriteFail);

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == GuiButtonTypeLeft) {
            if(nfc_magic_scene_iso15693_write_fail_is_retryable(instance, reason)) {
                // Retry: back to the write scene, which re-runs the write on enter.
                consumed = scene_manager_previous_scene(instance->scene_manager);
            } else {
                // Finish / Back -> the ISO15693 menu.
                consumed = scene_manager_search_and_switch_to_previous_scene(
                    instance->scene_manager, NfcMagicSceneIso15693);
            }
        } else if(event.event == GuiButtonTypeRight) {
            // Same test, same order as the right slot in on_enter -- see the rule stated there. This
            // branch is what the label says only because the two agree; it does NOT get to assume the
            // reason is non-retryable.
            if(nfc_magic_scene_iso15693_write_fail_has_details(instance, reason)) {
                // Details -> the affected-block list (failed blocks, or the empty top blocks).
                scene_manager_next_scene(
                    instance->scene_manager, NfcMagicSceneIso15693PartialDetails);
                consumed = true;
            } else {
                // "Exit" -> the ISO15693 menu. Only rendered when there is nothing behind Details.
                consumed = scene_manager_search_and_switch_to_previous_scene(
                    instance->scene_manager, NfcMagicSceneIso15693);
            }
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        consumed = scene_manager_search_and_switch_to_previous_scene(
            instance->scene_manager, NfcMagicSceneIso15693);
    }
    return consumed;
}

void nfc_magic_scene_iso15693_write_fail_on_exit(void* context) {
    NfcMagicApp* instance = context;

    // NOTE: do not reset the scene state here. The write scene always sets the reason before entering
    // this scene, and the "Details" round-trip re-enters this scene, which must re-read the same
    // reason -- clearing it here would rebuild the wrong screen on return from Details.
    widget_reset(instance->widget);
}
