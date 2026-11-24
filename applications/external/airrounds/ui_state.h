#pragma once

#include "capture_mode.h"
#include "session_client.h"

namespace airrounds {
namespace flipper {

enum class HomeEntry {
    status,
    setup,
    start,
    board
};

inline bool sessionBusy(uart::State state) {
    return state == uart::State::running || state == uart::State::finalizing;
}

inline HomeEntry homeEntry(Request pending, uart::State state, size_t count, bool canStart) {
    if(pending == Request::start || pending == Request::stop || sessionBusy(state)) {
        return HomeEntry::status;
    }
    if(count == 0) {
        return HomeEntry::setup;
    }
    return canStart ? HomeEntry::start : HomeEntry::board;
}

// What the Capture mode screen needs to know about the board to allow a change of mode.
struct ModeLockInput {
    bool uartOpen = true;
    Link link = Link::ready;
    Request pending = Request::none;
    bool commandUnconfirmed = false;
    uart::State session = uart::State::idle;
    uart::ScanState scan = uart::ScanState::idle;
};

// The first lock that applies, or nullptr when the operator can change the mode. An unconfirmed
// board state wins over the last known session, because a round can still run on the board.
inline const char* captureModeLock(const ModeLockInput& board) {
    if(!board.uartOpen || board.link != Link::ready || board.pending == Request::start ||
       board.pending == Request::stop || board.commandUnconfirmed) {
        return "Board state unknown";
    }
    if(sessionBusy(board.session)) {
        return "Session active";
    }
    if(board.scan == uart::ScanState::running || board.pending == Request::scan ||
       board.pending == Request::scan_stop) {
        return "Scan active";
    }
    return nullptr;
}

struct RunReport {
    uart::JournalState journal = uart::JournalState::none;
    uart::VisitId id{};
    uart::State state = uart::State::idle;
    uart::Outcome outcome = uart::Outcome::none;
    uart::RoundStatus round{};
};

inline size_t reportSize(const RunReport& report) {
    return report.round.profile.size() == 0 ? 1 : report.round.profile.size();
}

inline uart::Outcome pointOutcome(const RunReport& report, size_t point) {
    return report.round.profile.size() == 0 ? report.outcome : report.round.results[point];
}

// Whether a point counts as a success by the mode's meaning: a frame-found mode on a saved frame,
// an absence mode on a confirmed absence (digit 5), an unknown mode on either.
inline bool pointSucceeded(uart::Outcome outcome, const CaptureModeInfo& mode) {
    switch(mode.meaning) {
    case SuccessMeaning::absence_confirmed:
        return outcome == uart::Outcome::absence_confirmed;
    case SuccessMeaning::packet_found:
        return outcome == uart::Outcome::saved;
    case SuccessMeaning::any_result:
        return outcome == uart::Outcome::saved || outcome == uart::Outcome::absence_confirmed;
    }
    return false;
}

inline unsigned savedPoints(const RunReport& report, const CaptureModeInfo& mode = beaconMode()) {
    unsigned saved = 0;
    for(size_t index = 0; index < reportSize(report); ++index) {
        if(pointSucceeded(pointOutcome(report, index), mode)) {
            ++saved;
        }
    }
    return saved;
}

inline const char* outcomeTitle(const RunReport& report) {
    if(report.state == uart::State::error) {
        return report.outcome == uart::Outcome::storage_error ? "SD write failed" : "Radio failed";
    }
    return report.state == uart::State::stopped ? "Session stopped" : "Session ended";
}

inline const char* pointResult(
    uart::Outcome outcome,
    uart::State state,
    const CaptureModeInfo& mode = beaconMode()) {
    switch(outcome) {
    case uart::Outcome::saved:
        return mode.meaning == SuccessMeaning::absence_confirmed ? mode.resultMiss :
                                                                   mode.resultSuccess;
    case uart::Outcome::absence_confirmed:
        return mode.meaning == SuccessMeaning::packet_found ? mode.resultMiss : mode.resultSuccess;
    case uart::Outcome::not_observed:
        return mode.resultMiss;
    case uart::Outcome::radio_error:
        return "Radio error";
    case uart::Outcome::storage_error:
        return "SD error";
    case uart::Outcome::none:
        return state == uart::State::stopped ? "Stopped" : "Skipped";
    }
    return "Unknown";
}

// The right-hand text of a Beacons list row after a round, by the mode's meaning of success.
inline const char*
    networkRowStatus(uart::Outcome outcome, const CaptureModeInfo& mode = beaconMode()) {
    const bool absence = mode.meaning == SuccessMeaning::absence_confirmed;
    switch(outcome) {
    case uart::Outcome::none:
        return "";
    case uart::Outcome::saved:
        return absence ? mode.rowMiss : mode.rowSuccess;
    case uart::Outcome::absence_confirmed:
        return mode.meaning == SuccessMeaning::packet_found ? mode.rowMiss : mode.rowSuccess;
    case uart::Outcome::not_observed:
        return mode.rowMiss;
    default:
        return pointResult(outcome, uart::State::complete, mode);
    }
}

inline const char* pointDetail(
    uart::Outcome outcome,
    uart::State state,
    const CaptureModeInfo& mode = beaconMode()) {
    switch(outcome) {
    case uart::Outcome::saved:
        return mode.meaning == SuccessMeaning::absence_confirmed ? mode.detailMiss :
                                                                   mode.detailSuccess;
    case uart::Outcome::absence_confirmed:
        return mode.meaning == SuccessMeaning::packet_found ? mode.detailMiss : mode.detailSuccess;
    case uart::Outcome::not_observed:
        return mode.detailMiss;
    case uart::Outcome::storage_error:
        return "SD write failed";
    case uart::Outcome::none:
        return state == uart::State::stopped ? "Stopped before result" : "Skipped after error";
    default:
        return pointResult(outcome, state, mode);
    }
}

} // namespace flipper
} // namespace airrounds
