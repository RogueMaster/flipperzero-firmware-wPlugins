#pragma once

#include "shared/target.h"
#include "shared/uart_reply.h"

namespace airrounds {
namespace flipper {

enum class Link {
    connecting,
    ready,
    lost,
    incompatible
};
enum class Request {
    none,
    hello,
    status,
    start,
    stop,
    scan,
    scan_read,
    scan_stop,
    firmware,
    modes,
    card,
    selftest
};
// The board firmware version: not asked for, being read, not reported by an older board, or known.
enum class BoardFirmware {
    none,
    reading,
    unknown,
    known
};
// The board microSD state: not asked for, being read, confirmed absent, or confirmed present.
enum class BoardCard {
    none,
    reading,
    absent,
    present
};
// A board self-test run: not started, in flight, finished with a result, unsupported, or no answer.
enum class SelfTestRun {
    idle,
    running,
    done,
    unsupported,
    failed
};

class SessionClient {
public:
    using Send = void (*)(void*, const char*, size_t);

    SessionClient(Send send, void* context, uint64_t nonce)
        : send_(send)
        , context_(context)
        , nonce_(nonce & ~uart::readRequestBit) {
    }

    Link link() const {
        return link_;
    }
    Request pending() const {
        return pending_;
    }
    Request rejected() const {
        return rejected_;
    }
    uart::VisitId visitId() const {
        return {boot_, snapshot_.session};
    }
    uart::State state() const {
        return snapshot_.state;
    }
    uart::Outcome outcome() const {
        return snapshot_.outcome;
    }
    uart::JournalState journalState() const {
        return snapshot_.journal;
    }

    const uart::RoundStatus& round() const {
        return snapshot_.round;
    }
    bool supportsRounds() const {
        return link_ == Link::ready && supportsRounds_;
    }

    bool supportsScan() const {
        return supportsScan_;
    }
    bool supportsRoundDetail() const {
        return supportsRoundDetail_;
    }
    uart::ScanState scanState() const {
        return scanState_;
    }

    BoardFirmware boardFirmware() const {
        return firmwareState_;
    }
    const char* firmware() const {
        return firmware_;
    }

    // Feature bit 6: the board runs a self-test and reports the microSD mount state.
    bool supportsSelfTest() const {
        return supportsSelfTest_;
    }
    BoardCard cardState() const {
        return cardState_;
    }
    // Block Scan and Capture only on a confirmed absent card; an unknown state does not block. The
    // board reports the card once after the hello, so the gate uses the connect-time state; the
    // firmware round preflight is the backstop for a card pulled after the connect.
    bool cardMissing() const {
        return cardState_ == BoardCard::absent;
    }

    SelfTestRun selfTestRun() const {
        return selfTestRun_;
    }
    const uart::SelfTest& selfTest() const {
        return selfTest_;
    }
    // Run the board self-test once; the result reaches selfTest() when selfTestRun() is done.
    void runSelfTest() {
        if(!supportsSelfTest_) {
            selfTestRun_ = SelfTestRun::unsupported;
        } else if(selfTestRun_ != SelfTestRun::running) {
            selfTestRun_ = SelfTestRun::running;
            selfTestPending_ = true;
        }
    }

    // Feature bit 5: the board lists capture modes and runs R5.
    bool supportsCaptureModes() const {
        return supportsCaptureModes_;
    }
    // True while a bit-5 board has not yet answered a modes read for this boot: the mode choice is
    // not confirmed, so the app shows "Mode pending" for a non-Beacon choice.
    bool modesPending() const {
        return supportsCaptureModes_ && modesPending_;
    }
    uint8_t boardModeCount() const {
        return boardModeCount_;
    }
    const uint8_t* boardModeIds() const {
        return boardModeIds_;
    }

    // The capture-mode wire ID a round_status_mode reply reported for the current round, when one
    // did. Until then reportedModeKnown() is false and the app uses the chosen mode's labels.
    bool reportedModeKnown() const {
        return snapshot_.hasMode;
    }
    uint8_t reportedMode() const {
        return snapshot_.mode;
    }

    // Reads the board firmware version once, before the next status read.
    void requestFirmware() {
        if(firmwareState_ != BoardFirmware::known) {
            firmwareState_ = BoardFirmware::reading;
            firmware_[0] = '\0';
        }
    }
    const uart::ScanList& scanResults() const {
        return scanResults_;
    }

    bool canStart(size_t count = 1) const {
        const size_t limit = supportsScan_    ? session::Profile::capacity :
                             supportsRounds() ? session::Profile::legacyCapacity :
                                                1U;
        return count > 0 && count <= limit && canMutate() && snapshot_.session != UINT32_MAX &&
               state() != uart::State::running && state() != uart::State::finalizing &&
               scanState_ != uart::ScanState::running;
    }

    bool scan(uint32_t now) {
        if(!supportsScan_ || !canMutate() || state() == uart::State::running ||
           state() == uart::State::finalizing || scanState_ == uart::ScanState::running) {
            return false;
        }
        scanResults_.clear();
        scanExpectedCount_ = 0;
        scanState_ = uart::ScanState::running;
        request(Request::scan, now);
        return true;
    }

    bool cancelScan(uint32_t now) {
        if(scanState_ != uart::ScanState::running || link_ != Link::ready) {
            return false;
        }
        if(pending_ == Request::scan) {
            cancelScanPending_ = true;
        } else if(pending_ != Request::scan_stop && canMutate()) {
            request(Request::scan_stop, now);
        }
        return true;
    }

    bool canStop() const {
        return canMutate() &&
               (state() == uart::State::running || (state() == uart::State::finalizing &&
                                                    round().next() + 1 < round().profile.size()));
    }

    bool start(const session::Target& target, uint32_t now) {
        session::Profile profile;
        return profile.add(target) && start(profile, now);
    }

    bool start(const session::Profile& profile, uint32_t now, uint8_t modeId = 0) {
        if(!canStart(profile.size())) {
            return false;
        }
        requestedProfile_ = profile;
        requestedMode_ = modeId;
        request(Request::start, now, &requestedProfile_);
        return true;
    }

    bool stop(const uart::VisitId& expected, uint32_t now) {
        if(!canStop() || !expected.matches(visitId())) {
            return false;
        }
        request(Request::stop, now);
        return true;
    }

    void refresh(uint32_t now) {
        // A new read can replace an old read. Keep any command already on the wire.
        if(!mutationPending()) {
            pending_ = Request::none;
        }
        rejected_ = Request::none;
        link_ = Link::connecting;
        wait_ = {now, 0};
    }

    void transportLost(uint32_t now) {
        reconnect(Link::lost, now);
    }

    void poll(uint32_t now) {
        if(pending_ != Request::none) {
            // The board runs the self-test inline and answers only when it finishes, so its reply
            // gets a longer deadline than a plain read.
            const uint32_t deadline = pending_ == Request::selftest ? selfTestTimeoutMs :
                                                                      requestTimeoutMs;
            if(now - sentAt_ < deadline) {
                return;
            }
            // A read without a usable reply must not drop the link, or a board that answers in an
            // unknown form would never reach a status read.
            if(pending_ == Request::firmware) {
                firmwareState_ = BoardFirmware::unknown;
                pending_ = Request::none;
                wait_ = {now, 0};
            } else if(pending_ == Request::modes) {
                // A modes timeout keeps the link up and leaves the choice pending; the status poll
                // runs next (modesTried_ stays set) and modes retries after it.
                pending_ = Request::none;
                wait_ = {now, 0};
            } else if(pending_ == Request::card) {
                cardState_ = BoardCard::none;
                pending_ = Request::none;
                wait_ = {now, 0};
            } else if(pending_ == Request::selftest) {
                selfTestRun_ = SelfTestRun::failed;
                pending_ = Request::none;
                wait_ = {now, 0};
            } else {
                transportLost(now);
            }
            return;
        }
        if(now - wait_.since >= wait_.duration) {
            if(boot_ == 0) {
                request(Request::hello, now);
            } else if(firmwareState_ == BoardFirmware::reading) {
                request(Request::firmware, now);
            } else if(supportsSelfTest_ && cardState_ == BoardCard::reading) {
                request(Request::card, now);
            } else if(selfTestPending_) {
                selfTestPending_ = false;
                request(Request::selftest, now);
            } else if(cancelScanPending_) {
                cancelScanPending_ = false;
                request(Request::scan_stop, now);
            } else if(scanState_ == uart::ScanState::running) {
                request(Request::scan_read, now);
            } else if(supportsCaptureModes_ && modesPending_ && !modesTried_) {
                // One modes attempt per cycle, then the status poll clears modesTried_, so a board
                // that keeps failing modes cannot starve the status poll.
                modesTried_ = true;
                request(Request::modes, now);
            } else {
                request(Request::status, now);
            }
        }
    }

    void receive(uart::TimedByte input) {
        const uint8_t byte = input.value;
        if(byte == '!') {
            line_[0] = '!';
            length_ = 1;
            return;
        }
        if(length_ == 0) {
            return;
        }
        if(length_ >= sizeof(line_) - 1 || (byte != '\n' && (byte < ' ' || byte > '~'))) {
            length_ = 0;
            return;
        }
        line_[length_++] = static_cast<char>(byte);
        if(byte == '\n') {
            line_[length_] = '\0';
            length_ = 0;
            handleReply(input.timeMs);
        }
    }

private:
    static constexpr uint32_t requestTimeoutMs = 900;
    // The board runs the self-test inline (microSD mount, radio, a short probe, a test write), so
    // its reply can take a few seconds; give it a wider deadline than a plain read.
    static constexpr uint32_t selfTestTimeoutMs = 4000;
    static constexpr uint32_t reconnectMs = 300;
    static constexpr uint32_t pollMs = 350;

    bool mutationPending() const {
        return pending_ == Request::start || pending_ == Request::stop ||
               pending_ == Request::scan || pending_ == Request::scan_stop;
    }

    bool canMutate() const {
        return link_ == Link::ready && !mutationPending() &&
               mutationHighWater_ < uart::readRequestBit - 1;
    }

    void reconnect(Link reason, uint32_t now) {
        boot_ = 0;
        // Another boot can run other firmware, so read the version again after the next hello.
        if(firmwareState_ != BoardFirmware::none) {
            firmwareState_ = BoardFirmware::reading;
            firmware_[0] = '\0';
        }
        if(scanState_ == uart::ScanState::running) {
            scanState_ = uart::ScanState::error;
        }
        cancelScanPending_ = false;
        modesTried_ = false;
        cardState_ = BoardCard::none;
        selfTestPending_ = false;
        selfTestRun_ = SelfTestRun::idle;
        link_ = reason;
        pending_ = Request::none;
        length_ = 0;
        wait_ = {now, reconnectMs};
    }

    template <size_t width>
    static char* hex(char* output, uint64_t value) {
        constexpr char digits[] = "0123456789abcdef";
        for(size_t i = width; i > 0; --i) {
            output[i - 1] = digits[value & 0x0fU];
            value >>= 4U;
        }
        return output + width;
    }

    // The status read verb by the board's features. With bit 5 every round status comes from
    // round_status_mode, Beacon included.
    const char* statusVerb() const {
        if(supportsCaptureModes_) {
            return "round_status_mode";
        }
        if(supportsRoundDetail_) {
            return "round_status_detail";
        }
        if(supportsJournal_) {
            return "round_status_log";
        }
        return supportsRounds_ ? "round_status" : "status";
    }

    // The verb for a request, with its scope, and the expects flags it sets for reply validation.
    const char* verbFor(Request kind, uint64_t id, uint32_t& scope) {
        expectsRound_ = false;
        expectsJournal_ = false;
        expectsMode_ = false;
        scope = 0;
        switch(kind) {
        case Request::start:
            scope = snapshot_.session + 1U;
            expectsRound_ = supportsRounds_;
            return expectsRound_ ? "round" : "start";
        case Request::stop:
            scope = snapshot_.session;
            expectsRound_ = supportsRounds_;
            return expectsRound_ ? "round_stop" : "stop";
        case Request::scan:
            scanId_ = id;
            return "scan";
        case Request::scan_read:
            return "scan_read";
        case Request::scan_stop:
            return "scan_stop";
        case Request::firmware:
            return "firmware";
        case Request::modes:
            return "modes";
        case Request::card:
            return "card";
        case Request::selftest:
            return "selftest";
        case Request::status:
            expectsRound_ = supportsRounds_;
            expectsJournal_ = supportsJournal_;
            expectsMode_ = supportsCaptureModes_;
            return statusVerb();
        default:
            return "hello";
        }
    }

    void sendFrame(Request kind, const char* verb, const session::Profile* profile) {
        char frame[uart::frameCapacity + 1];
        char* out = frame;
        *out++ = '@';
        out = hex<2>(out, uart::version);
        *out++ = ' ';
        out = hex<16>(out, key_.boot);
        *out++ = ' ';
        out = hex<16>(out, key_.request);
        *out++ = ' ';
        out = hex<8>(out, key_.scope);
        *out++ = ' ';
        const size_t verbLength = strlen(verb);
        memcpy(out, verb, verbLength);
        out += verbLength;
        if(kind == Request::scan_read || kind == Request::scan_stop) {
            *out++ = ' ';
            out = hex<16>(out, scanId_);
        }
        if(kind == Request::scan_read) {
            *out++ = ' ';
            out = hex<2>(out, scanResults_.size());
        }
        for(size_t index = 0; profile != nullptr && index < profile->size(); ++index) {
            const auto& target = (*profile)[index];
            *out++ = ' ';
            out = hex<12>(out, session::addressValue(target));
            *out++ = ' ';
            out = hex<2>(out, target.channel);
        }
        // A non-Beacon round carries a mode <id> tail; Beacon (0) keeps the plain round line.
        if(kind == Request::start && expectsRound_ && requestedMode_ != 0) {
            const char marker[] = " mode ";
            memcpy(out, marker, sizeof(marker) - 1);
            out += sizeof(marker) - 1;
            out = hex<2>(out, requestedMode_);
        }
        *out++ = '\n';
        send_(context_, frame, static_cast<size_t>(out - frame));
    }

    void request(Request kind, uint32_t now, const session::Profile* profile = nullptr) {
        pending_ = kind;
        sentAt_ = now;
        const bool mutation = mutationPending();
        const auto id = mutation ? ++mutationHighWater_ : nextNonce();
        if(mutation) {
            rejected_ = Request::none;
        }
        uint32_t scope = 0;
        const char* verb = verbFor(kind, id, scope);
        key_ = {boot_, id, scope};
        sendFrame(kind, verb, profile);
    }

    uint64_t nextNonce() {
        nonce_ = (nonce_ + 1) & ~uart::readRequestBit;
        return nonce_ | uart::readRequestBit;
    }

    void handleReply(uint32_t now) {
        uart::Reply reply;
        if(pending_ == Request::none || !uart::readReply(line_, reply) ||
           !uart::matchesReplyKey(reply.envelope.key, key_)) {
            return;
        }
        // The self-test reply is slow by design; match the per-request deadline poll() waits, or a
        // reply that legitimately arrives after requestTimeoutMs would match the key and then drop
        // the link, discarding the result.
        const uint32_t deadline = pending_ == Request::selftest ? selfTestTimeoutMs :
                                                                  requestTimeoutMs;
        if(now - sentAt_ >= deadline) {
            transportLost(now);
            return;
        }
        if(reply.envelope.wireVersion != uart::version ||
           reply.code == uart::Code::unsupported_version) {
            reconnect(Link::incompatible, now);
            return;
        }
        if(reply.code == uart::Code::wrong_boot) {
            transportLost(now);
            return;
        }
        if(pending_ == Request::hello) {
            acceptHello(reply, now);
            return;
        }
        routeReply(reply, now);
    }

    void acceptHello(const uart::Reply& reply, uint32_t now) {
        if(reply.hasFirmware) {
            return;
        }
        if(reply.code != uart::Code::ok || (reply.features & uart::passiveVisitFeature) == 0) {
            reconnect(Link::incompatible, now);
            return;
        }
        supportsRounds_ = (reply.features & uart::passiveRoundFeature) != 0;
        supportsScan_ = supportsRounds_ && (reply.features & uart::scanFeature) != 0;
        supportsRoundDetail_ = supportsRounds_ && (reply.features & uart::roundDetailFeature) != 0;
        supportsJournal_ = supportsRounds_ && (reply.features & uart::sessionJournalFeature) != 0;
        supportsCaptureModes_ = supportsRounds_ &&
                                (reply.features & uart::captureModeFeature) != 0;
        if(supportsCaptureModes_) {
            // Read the mode list again for this boot; keep any stored list and the choice.
            modesPending_ = true;
            modesTried_ = false;
        } else {
            boardModeCount_ = 0;
            modesPending_ = false;
        }
        supportsSelfTest_ = (reply.features & uart::selfTestFeature) != 0;
        // Read the microSD state once for this boot; the self-test waits for the operator.
        cardState_ = supportsSelfTest_ ? BoardCard::reading : BoardCard::none;
        selfTestRun_ = SelfTestRun::idle;
        selfTestPending_ = false;
        boot_ = reply.envelope.key.boot;
        if(boot_ != mutationBoot_) {
            mutationBoot_ = boot_;
            mutationHighWater_ = 0;
            rejected_ = Request::none;
        }
        rememberMutation(reply.lastMutation);
        pending_ = Request::none;
        link_ = Link::connecting;
        wait_ = {now, 0};
    }

    void routeReply(const uart::Reply& reply, uint32_t now) {
        if(pending_ == Request::firmware) {
            acceptFirmware(reply, now);
        } else if(pending_ == Request::modes) {
            acceptModes(reply, now);
        } else if(pending_ == Request::card) {
            acceptCard(reply, now);
        } else if(pending_ == Request::selftest) {
            acceptSelfTest(reply, now);
        } else if(
            pending_ == Request::scan || pending_ == Request::scan_read ||
            pending_ == Request::scan_stop) {
            acceptScan(reply, now);
        } else {
            acceptStatus(reply, now);
        }
    }

    void acceptCard(const uart::Reply& reply, uint32_t now) {
        if(reply.hasRound || reply.hasJournal || reply.hasScan || reply.hasFirmware ||
           reply.hasCard != (reply.code == uart::Code::ok)) {
            return;
        }
        // An older or busy board refuses and leaves the state unknown; the link stays up.
        cardState_ = !reply.hasCard ? BoardCard::none :
                     reply.card     ? BoardCard::present :
                                      BoardCard::absent;
        rememberMutation(reply.lastMutation);
        pending_ = Request::none;
        wait_ = {now, 0};
    }

    void acceptSelfTest(const uart::Reply& reply, uint32_t now) {
        if(reply.hasRound || reply.hasJournal || reply.hasScan || reply.hasFirmware ||
           reply.hasSelfTest != (reply.code == uart::Code::ok)) {
            return;
        }
        if(reply.hasSelfTest) {
            selfTest_ = reply.selfTest;
            selfTestRun_ = SelfTestRun::done;
        } else {
            // A refusal, a busy board, or an older board: report the run as failed.
            selfTestRun_ = SelfTestRun::failed;
        }
        rememberMutation(reply.lastMutation);
        pending_ = Request::none;
        wait_ = {now, 0};
    }

    void acceptFirmware(const uart::Reply& reply, uint32_t now) {
        if(reply.hasRound || reply.hasJournal || reply.hasScan ||
           reply.hasFirmware != (reply.code == uart::Code::ok)) {
            return;
        }
        // An older board does not know the read and answers bad_request. Any other refusal also
        // leaves the version unknown for this boot; the link stays up either way.
        if(reply.hasFirmware) {
            memcpy(firmware_, reply.firmware, sizeof(firmware_));
            firmwareState_ = BoardFirmware::known;
        } else {
            firmwareState_ = BoardFirmware::unknown;
        }
        rememberMutation(reply.lastMutation);
        pending_ = Request::none;
        wait_ = {now, 0};
    }

    void acceptModes(const uart::Reply& reply, uint32_t now) {
        // Only an ok reply that carries the modes list, and nothing else, updates the stored list.
        // Any other reply keeps the link up and leaves the choice pending for the next retry.
        if(reply.code == uart::Code::ok && reply.hasModes && !reply.hasRound &&
           !reply.hasJournal && !reply.hasScan && !reply.hasFirmware && !reply.hasMode) {
            boardModeCount_ = reply.modeCount;
            for(size_t index = 0; index < reply.modeCount && index < sizeof(boardModeIds_);
                ++index) {
                boardModeIds_[index] = reply.modeIds[index];
            }
            modesPending_ = false;
        }
        rememberMutation(reply.lastMutation);
        pending_ = Request::none;
        wait_ = {now, 0};
    }

    void acceptScan(const uart::Reply& reply, uint32_t now) {
        using uart::ScanState;
        if(reply.hasRound || reply.hasJournal || reply.hasFirmware ||
           (reply.code == uart::Code::ok &&
            (reply.hasScan != (pending_ == Request::scan_read) ||
             (mutationPending() && reply.lastMutation != key_.request)))) {
            return;
        }
        rememberMutation(reply.lastMutation);
        if(reply.code != uart::Code::ok) {
            scanState_ = ScanState::error;
        } else if(pending_ == Request::scan_stop) {
            scanState_ = ScanState::idle;
        } else if(pending_ == Request::scan_read && !cancelScanPending_) {
            collectScan(reply.scan);
        }
        pending_ = Request::none;
        if(scanState_ != ScanState::running) {
            cancelScanPending_ = false;
        }
        wait_ = {
            now,
            reply.hasScan && reply.scan.state == ScanState::running && !cancelScanPending_ ?
                pollMs :
                0};
    }

    void collectScan(const uart::ScanReply& result) {
        using uart::ScanState;
        if(result.state == ScanState::running && scanExpectedCount_ == 0) {
            return;
        }
        if(result.state != ScanState::complete || result.index != scanResults_.size() ||
           (scanExpectedCount_ != 0 && result.count != scanExpectedCount_)) {
            scanState_ = ScanState::error;
            return;
        }
        scanExpectedCount_ = result.count;
        if(result.count != 0 &&
           !scanResults_.add(result.entry.target, result.entry.name, result.entry.nameSize)) {
            scanState_ = ScanState::error;
        } else if(scanResults_.size() == result.count) {
            scanState_ = ScanState::complete;
        }
    }

    static bool validSnapshot(const uart::Reply& reply) {
        using uart::Outcome;
        using uart::State;
        if((reply.session == 0) != (reply.state == State::idle)) {
            return false;
        }
        switch(reply.state) {
        case State::idle:
        case State::running:
        case State::finalizing:
            return reply.outcome == Outcome::none;
        case State::complete:
        case State::stopped:
            return reply.outcome == Outcome::saved || reply.outcome == Outcome::not_observed;
        case State::error:
            return reply.outcome == Outcome::radio_error ||
                   reply.outcome == Outcome::storage_error;
        }
        return false;
    }

    static bool validRound(const uart::Reply& reply) {
        using uart::Outcome;
        using uart::State;
        const auto& roundStatus = reply.round;
        const size_t count = roundStatus.profile.size();
        const size_t next = roundStatus.next();
        if((count == 0) != (reply.state == State::idle) ||
           (roundStatus.interruption == uart::VisitInterruption::yes &&
            (reply.state != State::stopped || next == 0 ||
             roundStatus.results[next - 1] != Outcome::not_observed))) {
            return false;
        }
        bool saved = false;
        for(size_t index = 0; index < count; ++index) {
            const auto result = roundStatus.results[index];
            if(index >= next) {
                if(result != Outcome::none) {
                    return false;
                }
            } else if(reply.state == State::error && index + 1 == next) {
                if(result != reply.outcome) {
                    return false;
                }
            } else if(
                result == Outcome::saved ||
                (reply.hasMode && result == Outcome::absence_confirmed)) {
                // Only a round_status_mode reply (the mode marker) may carry digit 5; it counts as
                // a success, as the board reports a confirmed absence for a future absence mode.
                saved = true;
            } else if(result != Outcome::not_observed) {
                return false;
            }
        }
        switch(reply.state) {
        case State::idle:
            return true;
        case State::running:
        case State::finalizing:
            return next < count;
        case State::error:
            return next > 0;
        case State::complete:
        case State::stopped:
            return next > 0 && (reply.state == State::stopped || next == count) &&
                   reply.outcome == (saved ? Outcome::saved : Outcome::not_observed);
        }
        return false;
    }

    bool consistentRound(const uart::Reply& reply) const {
        if(pending_ == Request::start && !requestedProfile_.matches(reply.round.profile)) {
            return false;
        }
        if(!snapshot_.hasRound || snapshot_.envelope.key.boot != reply.envelope.key.boot ||
           snapshot_.session != reply.session) {
            return true;
        }

        using uart::State;
        const bool finished = state() == State::complete || state() == State::stopped ||
                              state() == State::error;
        if(!round().profile.matches(reply.round.profile) || (finished && state() != reply.state)) {
            return false;
        }
        // With bit 5 a round or round_stop reply carries the old per-point encoding, which the
        // round_status_mode snapshot may differ from (e.g. a confirmed absence is 1 there and 5
        // here), so skip the per-point comparison; round_status_mode fills the results.
        if(supportsCaptureModes_ && (pending_ == Request::start || pending_ == Request::stop)) {
            return true;
        }
        for(size_t index = 0; index < round().profile.size(); ++index) {
            const auto result = round().results[index];
            if((finished || result != uart::Outcome::none) &&
               result != reply.round.results[index]) {
                return false;
            }
        }
        return true;
    }

    // Store the reply as the snapshot. With bit 5, a round or round_stop reply carries the old
    // per-point encoding in its aps, so for the same round keep the point results and the reported
    // mode from the last round_status_mode poll and take only the state, outcome, and profile.
    void acceptSnapshot(const uart::Reply& reply) {
        const bool fromRoundMutation = pending_ == Request::start || pending_ == Request::stop;
        // Merge only within one round: the same boot and session. A new Start reuses the previous
        // profile but is a new session, so it must not inherit the old round's mode or digit-5
        // results; it takes the fresh reply whole.
        const bool sameRound = snapshot_.hasRound &&
                               snapshot_.envelope.key.boot == reply.envelope.key.boot &&
                               snapshot_.session == reply.session;
        if(supportsCaptureModes_ && fromRoundMutation && sameRound &&
           snapshot_.round.profile.matches(reply.round.profile)) {
            uart::Reply merged = reply;
            for(size_t index = 0; index < reply.round.profile.size(); ++index) {
                // Keep a confirmed absence (digit 5), which the round/round_stop encoding cannot
                // carry; take every other point result, and the state and outcome, from the reply.
                if(snapshot_.round.results[index] == uart::Outcome::absence_confirmed) {
                    merged.round.results[index] = uart::Outcome::absence_confirmed;
                }
            }
            merged.hasMode = snapshot_.hasMode;
            merged.mode = snapshot_.mode;
            snapshot_ = merged;
        } else {
            snapshot_ = reply;
        }
    }

    void acceptStatus(const uart::Reply& reply, uint32_t now) {
        const bool mutation = mutationPending();
        if(reply.code != uart::Code::ok) {
            if(!mutation) {
                transportLost(now);
                return;
            }
            rejected_ = pending_;
            pending_ = Request::none;
            link_ = Link::connecting;
            wait_ = {now, 0};
            return;
        }
        // round_status_mode carries the interrupted marker too, so a bit-5 status read expects it.
        const bool expectsDetail = pending_ == Request::status &&
                                   (supportsRoundDetail_ || supportsCaptureModes_);
        const bool hasDetail = reply.round.interruption != uart::VisitInterruption::unknown;
        if(!validSnapshot(reply) || reply.hasScan || reply.hasFirmware ||
           reply.hasRound != expectsRound_ || reply.hasJournal != expectsJournal_ ||
           hasDetail != expectsDetail || reply.hasMode != expectsMode_ ||
           (reply.hasRound && (!validRound(reply) || !consistentRound(reply))) ||
           (mutation && (reply.session != key_.scope || reply.lastMutation != key_.request))) {
            return;
        }
        acceptSnapshot(reply);
        rememberMutation(reply.lastMutation);
        if(!mutation) {
            link_ = Link::ready;
        }
        // The status poll ran this cycle; let the next cycle retry a still-pending modes read.
        modesTried_ = false;
        pending_ = Request::none;
        wait_ = {now, link_ == Link::ready ? pollMs : 0};
    }

    void rememberMutation(uint64_t boardId) {
        if(boardId > mutationHighWater_) {
            mutationHighWater_ = boardId;
        }
    }

    Send send_;
    void* context_;
    uint64_t nonce_;
    uint64_t boot_ = 0;
    uint64_t mutationBoot_ = 0;
    uint64_t mutationHighWater_ = 0;
    uart::ReplyKey key_{};
    uart::Reply snapshot_;
    session::Profile requestedProfile_;
    bool supportsRounds_ = false;
    bool supportsScan_ = false;
    bool supportsRoundDetail_ = false;
    bool cancelScanPending_ = false;
    uint64_t scanId_ = 0;
    size_t scanExpectedCount_ = 0;
    uart::ScanState scanState_ = uart::ScanState::idle;
    uart::ScanList scanResults_;
    bool expectsRound_ = false;
    bool supportsJournal_ = false;
    bool expectsJournal_ = false;
    bool supportsCaptureModes_ = false;
    bool expectsMode_ = false;
    bool modesPending_ = false;
    bool modesTried_ = false;
    uint8_t requestedMode_ = 0;
    uint8_t boardModeCount_ = 0;
    uint8_t boardModeIds_[8]{};
    Link link_ = Link::connecting;
    Request pending_ = Request::none;
    Request rejected_ = Request::none;
    struct Delay {
        uint32_t since;
        uint32_t duration;
    };
    Delay wait_{};
    uint32_t sentAt_ = 0;
    BoardFirmware firmwareState_ = BoardFirmware::none;
    bool supportsSelfTest_ = false;
    BoardCard cardState_ = BoardCard::none;
    SelfTestRun selfTestRun_ = SelfTestRun::idle;
    bool selfTestPending_ = false;
    uart::SelfTest selfTest_;
    char firmware_[sizeof(uart::Reply::firmware)]{};
    char line_[uart::wireReplyCapacity]{};
    size_t length_ = 0;
};

} // namespace flipper
} // namespace airrounds
