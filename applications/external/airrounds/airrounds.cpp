#include <atomic>
#include <cinttypes>
#include <cstdio>
#include <new>

#include <expansion/expansion.h>
#include <furi.h>
#include <furi_hal_random.h>
#include <furi_hal_serial.h>
#include <furi_hal_serial_control.h>
#include <gui/elements.h>
#include <gui/gui.h>
#include <gui/modules/byte_input.h>
#include <gui/view_dispatcher.h>

#include "esp_flasher.h"
#include "network_list.h"
#include "profile_storage.h"
#include "shared/capture_path.h"
#include "ui_state.h"

#ifndef AIRROUNDS_EMULATOR
#include <furi_hal_gpio.h>
#include <furi_hal_resources.h>
#endif

namespace {

using namespace airrounds; // The app uses the shared session and wire types.
using flipper::HomeEntry;
using flipper::Link;
using flipper::ProfileSave;
using flipper::Request;
using flipper::RunReport;
using uart::Outcome;
using uart::State;
using uart::VisitId;

enum class Page {
    home,
    scan,
    networks,
    captureMode,
    placeholder,
    points,
    point,
    address,
    channel,
    remove,
    review,
    board,
    testBoard,
    card,
    flash,
    status,
    last,
    results,
    detail,
    file
};

enum class Action : uint8_t {
    none,
    scan,
    selectNetwork,
    captureMode,
    chooseMode,
    placeholder,
    board,
    testBoard,
    flashBoard,
    flashConfirm,
    start,
    stop,
    refresh,
    points,
    point,
    add,
    edit,
    remove,
    confirmRemove,
    save,
    review,
    status,
    last,
    results,
    detail,
};

struct Binding {
    Action action = Action::none;
    size_t index = 0;
    VisitId visit{};

    bool matches(const Binding& other) const {
        return action == other.action && index == other.index && visit.matches(other.visit);
    }
};

struct Row {
    char left[27]{};
    char right[15]{};
};

struct Frame {
    char title[25]{};
    char hint[25]{};
    char titleRight[17]{};
    char leftKey[10]{};
    char rightKey[15]{};
    Row rows[4]{};
    size_t count = 0;
    int selected = -1;
    int column = 68;
    bool pointMenu = false;
    bool compact = false;
    bool scrollUp = false;
    bool scrollDown = false;
    // Four compact rows leave no gap at y = 49, so the menu draws its mark below the last row.
    int scrollDownY = 49;
    bool fileLink = false;
    Binding binding{};

    void row(const char* left, const char* right = "") {
        if(count < sizeof(rows) / sizeof(rows[0])) {
            snprintf(rows[count].left, sizeof(rows[count].left), "%s", left);
            snprintf(rows[count].right, sizeof(rows[count].right), "%s", right);
            ++count;
        }
    }

    bool matches(const Frame& other) const {
        return strcmp(title, other.title) == 0 && strcmp(hint, other.hint) == 0 &&
               memcmp(rows, other.rows, sizeof(rows)) == 0 && count == other.count &&
               selected == other.selected && column == other.column &&
               pointMenu == other.pointMenu && compact == other.compact &&
               scrollUp == other.scrollUp && scrollDown == other.scrollDown &&
               scrollDownY == other.scrollDownY && fileLink == other.fileLink &&
               strcmp(titleRight, other.titleRight) == 0 && strcmp(leftKey, other.leftKey) == 0 &&
               strcmp(rightKey, other.rightKey) == 0 && binding.matches(other.binding);
    }
};

struct Display {
    Frame frame;
    Binding drawn;
};

using flipper::FlashError;
using flipper::FlashManifest;
using flipper::FlashStage;

// The Flash board flow: idle, no image on the card, the active-image attestation, the flash in
// progress, a verified flash, or a failed flash.
enum class FlashPhase {
    idle,
    noImage,
    attest,
    flashing,
    done,
    failed
};

struct App {
    App(Storage* storageHandle, uint64_t nonce)
        : client(send, this, nonce)
        , store(storageHandle)
        , storage(storageHandle) {
    }

    static void send(void* context, const char* bytes, size_t count) {
        auto& app = *static_cast<App*>(context);
        furi_hal_serial_tx(app.serial, reinterpret_cast<const uint8_t*>(bytes), count);
    }

    flipper::SessionClient client;
    flipper::ProfileStorage store;
    Storage* storage = nullptr;
    ViewDispatcher* dispatcher = nullptr;
    View* view = nullptr;
    ByteInput* keyboard = nullptr;
    FuriHalSerialHandle* serial = nullptr;
    // Received bytes with the tick of their interrupt. A slow SD write can block the event loop
    // past the reply deadline, so the tick handler cannot tell when a byte arrived.
    FuriMessageQueue* rx = nullptr;
    std::atomic<bool> damaged{false};

    session::Profile profile;
    flipper::NetworkList networks;
    VisitId networkRun{};
    size_t networkFocus = 0;
    size_t placeholder = 0;
    // The mode of the next Start. It lasts until the app closes; the next launch uses Beacon.
    size_t captureMode = flipper::beaconModeIndex;
    bool modeNotAvailable = false;
    Page scanOrigin = Page::home;
    size_t scanOriginFocus = 0;
    bool scanStarted = false;
    bool scanCancel = false;
    bool selectionLimit = false;
    bool networkLogFailed = false;
    session::Target draft{{}, 1};
    RunReport latest;
    RunReport selectedReport;
    session::Profile submitted;
    VisitId submittedId{};
    session::Target legacyTarget{};
    VisitId legacyTargetId{};
    VisitId acknowledgedStop{};
    VisitId confirmedId{};
    bool restarted = false;
    bool commandUnconfirmed = false;
    bool opening = true;
    uint32_t editorGeneration = 0;

    Page page = Page::home;
    Page editOrigin = Page::points;
    Page resultOrigin = Page::last;
    size_t focus = 0;
    size_t point = 0;
    size_t resultPoint = 0;
    ProfileSave saveResult = ProfileSave::saved;
    Binding held{};

    FlashPhase flashPhase = FlashPhase::idle;
    FlashManifest flashManifest;
    FlashStage flashStage = FlashStage::idle;
    uint32_t flashDone = 0;
    uint32_t flashTotal = 0;
    FlashError flashError = FlashError::none;
    bool flashing = false;
#ifndef AIRROUNDS_EMULATOR
    FuriThread* flashThread = nullptr;
    File* flashImage = nullptr;
    std::atomic<bool> flashComplete{false};
#endif
};

const char* actionLabel(Action action) {
    switch(action) {
    case Action::scan:
        return "Scan";
    case Action::start:
        return "Start";
    case Action::stop:
        return "Stop";
    case Action::refresh:
        return "Refresh";
    case Action::save:
        return "Save";
    case Action::confirmRemove:
        return "Remove";
    case Action::results:
        return "Results";
    case Action::last:
        return "Last result";
    default:
        return nullptr;
    }
}

void title(Frame& frame, const char* name) {
    snprintf(frame.title, sizeof(frame.title), "%s", name);
}

void addressText(const session::Target& target, char (&text)[18]) {
    const auto* bssid = target.bssid;
    snprintf(
        text,
        sizeof(text),
        "%02X:%02X:%02X:%02X:%02X:%02X",
        static_cast<unsigned>(bssid[0]),
        static_cast<unsigned>(bssid[1]),
        static_cast<unsigned>(bssid[2]),
        static_cast<unsigned>(bssid[3]),
        static_cast<unsigned>(bssid[4]),
        static_cast<unsigned>(bssid[5]));
}

void pointText(size_t index, char (&text)[12]) {
    snprintf(text, sizeof(text), "Point %u", static_cast<unsigned>(index + 1));
}

void channelText(uint8_t channel, char (&text)[8]) {
    snprintf(text, sizeof(text), "Ch %u", static_cast<unsigned>(channel));
}

constexpr size_t visibleNetworkRows = 3;
constexpr size_t visibleMenuRows = 4;
constexpr const char*
    scanMenu[]{"Scan area", "Capture mode", "Flash board hardware", "Test board", "About"};
constexpr size_t scanMenuSize = sizeof(scanMenu) / sizeof(scanMenu[0]);
constexpr size_t captureModeItem = 1;
constexpr size_t flashBoardItem = 2;
constexpr size_t testBoardItem = 3;
constexpr size_t aboutItem = scanMenuSize - 1;

size_t topRow(size_t focus, size_t visible = visibleNetworkRows) {
    return focus < visible ? 0 : focus - visible + 1;
}

void networkName(const App& app, const session::Target& target, size_t slot, char (&name)[25]) {
    const auto row = app.networks.find(target);
    if(row < app.networks.size()) {
        app.networks.label(row, name);
    } else {
        snprintf(name, sizeof(name), "Point %u", static_cast<unsigned>(slot + 1));
    }
}

bool canStart(const App& app) {
    return app.serial != nullptr && app.client.canStart(app.profile.size());
}

// A scan-capable board starts rounds of selected beacons, so the manual profile does not decide
// whether the status screens call the board ready.
bool readyToStart(const App& app) {
    return app.client.supportsScan() ? app.client.canStart(1) : canStart(app);
}

// A board with the microSD feature blocks Scan and Capture when it reports no card. An older board
// never blocks, and the firmware round preflight is the backstop if the card is pulled later.
bool cardBlocks(const App& app) {
    return app.client.supportsSelfTest() && app.client.cardMissing();
}

const char* checkText(uart::CheckResult result) {
    switch(result) {
    case uart::CheckResult::pass:
        return "Pass";
    case uart::CheckResult::fail:
        return "Fail";
    case uart::CheckResult::blocked:
        return "Blocked";
    case uart::CheckResult::not_run:
        return "-";
    }
    return "-";
}

// The labels of a running or finished round come from the mode round_status_mode reports; until
// the first such reply names it, the chosen mode's labels apply, so an EAPOL round never shows
// Beacon labels while it waits.
const flipper::CaptureModeInfo& roundMode(const App& app) {
    // Use the board's reported mode only for a real round; an idle round_status_mode reply reports
    // mode 00, which must not override the chosen mode before a round starts.
    if(app.client.reportedModeKnown() && app.client.round().profile.size() != 0) {
        return flipper::modeOf(app.client.reportedMode());
    }
    return flipper::captureModes[app.captureMode];
}

// The capture modes the board lists in its `modes` reply, or none when the board has no bit 5.
flipper::BoardModes boardModes(const App& app) {
    flipper::BoardModes modes;
    if(app.client.supportsCaptureModes()) {
        modes.count = app.client.boardModeCount();
        const uint8_t* ids = app.client.boardModeIds();
        for(size_t index = 0; index < modes.count && index < sizeof(modes.ids); ++index) {
            modes.ids[index] = ids[index];
        }
    }
    return modes;
}

// A non-Beacon choice returns to Beacon, clearing the per-point results, once a successful modes
// reply no longer lists it or the board loses bit 5; while the list is still pending it waits.
void syncCaptureMode(App& app) {
    if(app.captureMode != flipper::beaconModeIndex && !app.client.modesPending() &&
       !flipper::modeAvailable(flipper::captureModes[app.captureMode], boardModes(app))) {
        app.captureMode = flipper::beaconModeIndex;
        app.networks.clearOutcomes();
    }
}

bool unconfirmedStart(const App& app) {
    return app.submittedId.session != 0 && !app.submittedId.matches(app.confirmedId);
}

RunReport currentRun(const App& app) {
    RunReport report;
    report.id = app.client.visitId();
    report.state = app.client.state();
    report.outcome = app.client.outcome();
    report.journal = app.client.journalState();
    report.round = app.client.round();
    if(report.round.profile.size() == 0 && report.id.session != 0 &&
       report.id.matches(app.legacyTargetId)) {
        report.round.profile.add(app.legacyTarget);
        report.round.results[0] = report.outcome;
    }
    return report;
}

void showOutcome(Frame& frame, const RunReport& report, const flipper::CaptureModeInfo& mode) {
    if(report.id.session == 0) {
        title(frame, "No result available");
        return;
    }
    title(frame, flipper::outcomeTitle(report));
    char count[32];
    snprintf(
        count,
        sizeof(count),
        "%u of %u captured",
        flipper::savedPoints(report, mode),
        static_cast<unsigned>(flipper::reportSize(report)));
    frame.row(count);
    if(report.outcome == Outcome::storage_error) {
        frame.row("Check board SD");
    }
    if(report.journal == uart::JournalState::failed) {
        frame.row("Session log failed");
    }
    frame.binding = {Action::results, 0, report.id};
}

const char* boardIssue(const App& app) {
    if(app.serial == nullptr) {
        return "UART unavailable";
    }
    if(app.client.link() == Link::incompatible ||
       (app.client.link() == Link::ready && app.profile.size() > 1 &&
        !app.client.supportsRounds())) {
        return "Update Devboard";
    }
    if(app.client.link() == Link::lost) {
        return "Devboard offline";
    }
    if(app.client.link() == Link::ready) {
        return "Board unavailable";
    }
    return "Checking status";
}

void showNetworkProgress(Frame& frame, const App& app) {
    const auto report = currentRun(app);
    const auto slot = report.round.next();
    const auto total = report.round.profile.size();
    title(frame, "Capture");
    const auto& mode = roundMode(app);
    snprintf(
        frame.titleRight,
        sizeof(frame.titleRight),
        mode.counter,
        flipper::savedPoints(report, mode),
        static_cast<unsigned>(total));
    char status[32];
    snprintf(
        status,
        sizeof(status),
        "%s - %s",
        mode.name,
        report.state == State::running ? mode.listening : mode.saving);
    frame.row(status);
    if(slot < total) {
        char name[25];
        networkName(app, report.round.profile[slot], slot, name);
        frame.row(name);
    }
    if(app.client.canStop() && !app.acknowledgedStop.matches(report.id)) {
        frame.binding = {Action::stop, 0, report.id};
    }
}

void showStatus(Frame& frame, const App& app) {
    const auto& client = app.client;
    if(app.serial == nullptr) {
        title(frame, "UART unavailable");
        frame.row("Close other UART apps");
    } else if(client.pending() == Request::start || client.pending() == Request::stop) {
        title(frame, client.pending() == Request::start ? "Starting..." : "Stopping...");
        frame.row("Waiting for board");
    } else if(client.link() == Link::lost) {
        const bool busy = flipper::sessionBusy(client.state()) || unconfirmedStart(app);
        title(frame, busy ? "Link lost" : "Devboard offline");
        if(busy) {
            frame.row("Session may run");
        }
        frame.row("Check cable");
    } else if(client.link() == Link::connecting) {
        title(frame, "Checking status");
        frame.row(app.commandUnconfirmed ? "Command unconfirmed" : "Waiting for board");
    } else if(client.link() == Link::incompatible) {
        title(frame, "Update Devboard");
        frame.row("AirRounds firmware");
    } else if(app.page == Page::board && !flipper::sessionBusy(client.state())) {
        title(frame, canStart(app) ? "Ready" : boardIssue(app));
        frame.row(
            client.supportsRounds() || app.profile.size() < 2 ? "Press Back for menu" :
                                                                "Use newer board firmware");
    } else if(app.restarted && !flipper::sessionBusy(client.state())) {
        title(frame, "Devboard restarted");
        frame.row("Check board SD");
        if(app.latest.id.session != 0) {
            frame.binding = {Action::last, 0, app.latest.id};
        }
    } else if(client.rejected() != Request::none && client.state() != State::error) {
        title(frame, client.rejected() == Request::start ? "Start rejected" : "Stop rejected");
        frame.row("Session status updated");
        frame.binding = {Action::refresh, 0, client.visitId()};
    } else if(flipper::sessionBusy(client.state()) && client.supportsScan()) {
        showNetworkProgress(frame, app);
    } else if(flipper::sessionBusy(client.state())) {
        const auto report = currentRun(app);
        const auto slot = report.round.next();
        const bool laterPoints = slot + 1 < report.round.profile.size();
        const bool canStop = client.canStop() && !app.acknowledgedStop.matches(report.id);
        const bool pointSaving = client.state() == State::finalizing && laterPoints &&
                                 !app.acknowledgedStop.matches(report.id);
        title(
            frame,
            client.state() == State::running ? "Observing" :
            pointSaving                      ? "Saving point" :
                                               "Reception stopped");
        if(client.state() == State::finalizing && !pointSaving) {
            frame.row("Saving to board SD");
        } else if(slot < report.round.profile.size()) {
            char text[27];
            snprintf(
                text,
                sizeof(text),
                "Point %u / Ch %u",
                static_cast<unsigned>(slot + 1),
                static_cast<unsigned>(report.round.profile[slot].channel));
            frame.row(text);
        } else {
            frame.row("Waiting for a beacon");
        }
        if(canStop) {
            frame.binding = {Action::stop, 0, report.id};
        }
    } else if(client.state() == State::idle) {
        title(frame, readyToStart(app) ? "Ready" : boardIssue(app));
        frame.row("Press Back for menu");
    } else {
        showOutcome(frame, currentRun(app), roundMode(app));
    }
}

void showHome(Frame& frame, const App& app) {
    title(frame, "AirRounds");
    if(app.client.supportsScan()) {
        frame.compact = true;
        const bool busy = flipper::sessionBusy(app.client.state()) || unconfirmedStart(app);
        const size_t first = topRow(app.focus, visibleMenuRows);
        for(size_t row = first; row < scanMenuSize && row < first + visibleMenuRows; ++row) {
            frame.row(row == 0 && busy ? "Session status" : scanMenu[row]);
        }
        frame.scrollUp = first > 0;
        frame.scrollDown = first + visibleMenuRows < scanMenuSize;
        frame.scrollDownY = 63;
        frame.selected = static_cast<int>(app.focus - first);
        Action action = Action::placeholder;
        if(app.focus == 0) {
            action = busy ? Action::status : Action::scan;
        } else if(app.focus == captureModeItem) {
            action = Action::captureMode;
        } else if(app.focus == flashBoardItem) {
            action = Action::flashBoard;
        } else if(app.focus == testBoardItem) {
            action = Action::testBoard;
        }
        frame.binding = {action, app.focus, {}};
        return;
    }
    const auto pending = app.client.pending();
    auto first =
        flipper::homeEntry(pending, app.client.state(), app.profile.size(), canStart(app));
    if(app.restarted || (unconfirmedStart(app) && app.client.link() != Link::ready)) {
        first = HomeEntry::status;
    }
    switch(first) {
    case HomeEntry::status:
        frame.row("Session status");
        break;
    case HomeEntry::setup:
        frame.row("Set up points");
        break;
    case HomeEntry::start:
        frame.row("Start session");
        break;
    case HomeEntry::board:
        frame.row("Board status");
        break;
    }
    frame.row("Access points");
    frame.row("Last result");
    frame.selected = static_cast<int>(app.focus);
    Action action = Action::last;
    if(app.focus == 0) {
        switch(first) {
        case HomeEntry::status:
            action = Action::status;
            break;
        case HomeEntry::setup:
            action = Action::points;
            break;
        case HomeEntry::start:
            action = Action::review;
            break;
        case HomeEntry::board:
            action = Action::board;
            break;
        }
    } else if(app.focus == 1) {
        action = Action::points;
    }
    frame.binding = {action, app.focus, {}};
}

void profileAction(Frame& frame, const App& app, const char* pageTitle, Action action) {
    if(app.saveResult == ProfileSave::changed) {
        title(frame, "Profile changed");
        snprintf(frame.hint, sizeof(frame.hint), "%s", "Reopen AirRounds");
    } else {
        title(frame, app.saveResult == ProfileSave::saved ? pageTitle : "Save not confirmed");
        frame.binding = {action, app.point, {}};
    }
}

void showScan(Frame& frame, const App& app) {
    if(app.scanStarted && app.client.scanState() == uart::ScanState::running) {
        title(frame, app.scanCancel ? "Canceling..." : "Scanning...");
        frame.row("Listening for networks");
        if(!app.scanCancel) {
            frame.row("Back to cancel");
        }
        return;
    }
    title(frame, app.scanCancel ? "Cancel failed" : "Scan failed");
    frame.row("Check Devboard");
    frame.binding = {Action::scan, 0, {}};
}

void showNetworks(Frame& frame, const App& app) {
    const auto& mode = roundMode(app);
    title(frame, mode.listTitle);
    frame.compact = true;
    const auto count = app.networks.size();
    const auto selected = static_cast<uint8_t>(app.networks.selection().size());
    snprintf(
        frame.titleRight,
        sizeof(frame.titleRight),
        "%u/5 selected",
        static_cast<unsigned>(selected));
    snprintf(frame.leftKey, sizeof(frame.leftKey), "Scan");
    // A non-Beacon choice waits for the board to confirm its mode list before Start appears.
    const bool modePending = app.captureMode != flipper::beaconModeIndex &&
                             app.client.modesPending();
    if(selected != 0 && !modePending && app.client.canStart(selected)) {
        snprintf(
            frame.rightKey, sizeof(frame.rightKey), "Start (%u)", static_cast<unsigned>(selected));
    }
    if(count == 0) {
        frame.row("No networks found");
        return;
    }
    const size_t first = topRow(app.focus);
    frame.scrollUp = first > 0;
    frame.scrollDown = first + visibleNetworkRows < count;
    frame.selected = static_cast<int>(app.focus - first);
    for(size_t row = first; row < count && row < first + visibleNetworkRows; ++row) {
        const auto outcome = app.networks.outcome(row);
        char name[25];
        app.networks.label(row, name);
        char label[27];
        const auto* marker = outcome == Outcome::saved  ? "+ " :
                             app.networks.selected(row) ? "[x] " :
                                                          "[ ] ";
        snprintf(label, sizeof(label), "%s%.20s", marker, name);
        const char* status = flipper::networkRowStatus(outcome, mode);
        frame.row(label, status);
    }
    if(app.selectionLimit) {
        snprintf(frame.titleRight, sizeof(frame.titleRight), "Maximum 5");
    } else if(app.networkLogFailed) {
        snprintf(frame.titleRight, sizeof(frame.titleRight), "Log failed");
    } else if(app.client.link() != Link::ready) {
        snprintf(frame.titleRight, sizeof(frame.titleRight), "Board offline");
    } else if(modePending) {
        snprintf(frame.titleRight, sizeof(frame.titleRight), "Mode pending");
    }
    frame.binding = {Action::selectNetwork, app.focus, {}};
}

const char* modeLock(const App& app) {
    const auto& client = app.client;
    flipper::ModeLockInput board;
    board.uartOpen = app.serial != nullptr;
    board.link = client.link();
    board.pending = client.pending();
    board.commandUnconfirmed = app.commandUnconfirmed;
    board.session = client.state();
    board.scan = client.scanState();
    return flipper::captureModeLock(board);
}

// fbt passes fap_version from application.fam as FAP_VERSION, so the app has one source of truth.
#ifndef FAP_VERSION
#error "FAP_VERSION is undefined; build the app with fbt or ufbt"
#endif

void showAbout(Frame& frame, const App& app) {
    title(frame, "About");
    frame.row("App", "v" FAP_VERSION);
    // "v", a version of up to 15 characters, and a terminator. Frame::row keeps the first 14
    // characters; a major.minor version stays far below that.
    char board[1 + sizeof(uart::Reply::firmware)];
    const auto firmware = app.client.boardFirmware();
    const auto link = app.client.link();
    if(link == Link::lost || link == Link::incompatible) {
        snprintf(board, sizeof(board), "Not connected");
    } else if(firmware == flipper::BoardFirmware::known) {
        snprintf(board, sizeof(board), "v%s", app.client.firmware());
    } else if(firmware == flipper::BoardFirmware::unknown) {
        snprintf(board, sizeof(board), "Unknown");
    } else {
        snprintf(board, sizeof(board), "Reading...");
    }
    frame.row("Board", board);
}

void showCaptureMode(Frame& frame, const App& app) {
    title(frame, "Capture mode");
    frame.compact = true;
    const auto board = boardModes(app);
    for(size_t index = 0; index < flipper::captureModeCount; ++index) {
        const auto& mode = flipper::captureModes[index];
        char label[sizeof(Row::left)];
        snprintf(label, sizeof(label), "%s%s", index == app.captureMode ? "* " : "", mode.name);
        frame.row(label, flipper::modeAvailable(mode, board) ? "" : "Soon");
    }
    frame.selected = static_cast<int>(app.focus);
    const char* hint = modeLock(app);
    if(hint == nullptr && app.modeNotAvailable) {
        hint = "Not available yet";
    }
    if(hint != nullptr) {
        snprintf(frame.hint, sizeof(frame.hint), "%s", hint);
    }
    frame.binding = {Action::chooseMode, app.focus, {}};
}

void go(App& app, Page page, size_t focus = 0);

// Read the release manifest from the card and confirm the image is present at the manifest size.
// valid is false when either file is missing or the image size does not match.
FlashManifest readFlashManifest(Storage* storage) {
    FlashManifest manifest;
    // The CI manifest is pretty-printed JSON with a 64-hex sha256 before the md5, so it runs past
    // 256 bytes; a short read truncates before md5 and parseManifest rejects it. 512 holds the
    // whole file with headroom for the build identity fields.
    char json[512];
    File* file = storage_file_alloc(storage);
    size_t count = 0;
    if(storage_file_open(
           file, APP_DATA_PATH("airrounds.manifest"), FSAM_READ, FSOM_OPEN_EXISTING)) {
        count = storage_file_read(file, reinterpret_cast<unsigned char*>(json), sizeof(json) - 1);
    }
    storage_file_close(file);
    storage_file_free(file);
    if(count == 0) {
        return manifest;
    }
    json[count] = '\0';
    manifest = flipper::parseManifest(json);
    File* image = storage_file_alloc(storage);
    const bool ready =
        storage_file_open(image, APP_DATA_PATH("airrounds.bin"), FSAM_READ, FSOM_OPEN_EXISTING) &&
        storage_file_size(image) == manifest.size;
    storage_file_close(image);
    storage_file_free(image);
    if(!ready) {
        manifest.valid = false;
    }
    return manifest;
}

const char* flashStageText(FlashStage stage) {
    switch(stage) {
    case FlashStage::enter:
        return "Entering loader";
    case FlashStage::sync:
        return "Syncing";
    case FlashStage::begin:
        return "Erasing";
    case FlashStage::write:
        return "Writing";
    case FlashStage::end:
        return "Finishing";
    case FlashStage::verify:
        return "Verifying";
    case FlashStage::reset:
        return "Resetting";
    case FlashStage::done:
        return "Done";
    case FlashStage::idle:
        break;
    }
    return "Starting";
}

const char* flashErrorText(FlashError error) {
    switch(error) {
    case FlashError::manifest:
        return "No image on card";
    case FlashError::sync:
        return "No board response";
    case FlashError::image:
        return "Image read failed";
    case FlashError::mismatch:
        return "Verify mismatch";
    case FlashError::begin:
    case FlashError::write:
    case FlashError::end:
        return "Write failed";
    case FlashError::io:
    case FlashError::overflow:
    case FlashError::none:
        break;
    }
    return "Flash failed";
}

#ifndef AIRROUNDS_EMULATOR
// The devboard routes the ESP IO0 (boot select) and EN (reset) to these Flipper GPIO. The bench
// confirms the mapping; if it is wrong, the operator holds BOOT and taps RESET and the sync retries
// still catch the ROM loader.
const GpioPin* const flashBootPin = &gpio_ext_pc0;
const GpioPin* const flashResetPin = &gpio_ext_pc1;

void flashWrite(void* context, const uint8_t* data, size_t size) {
    auto& app = *static_cast<App*>(context);
    furi_hal_serial_tx(app.serial, data, size);
}

bool flashReadByte(void* context, uint8_t* byte, uint32_t timeoutMs) {
    auto& app = *static_cast<App*>(context);
    uart::TimedByte received{};
    if(furi_message_queue_get(app.rx, &received, timeoutMs) != FuriStatusOk) {
        return false;
    }
    *byte = received.value;
    return true;
}

void flashBoot(void*, bool bootDown) {
    furi_hal_gpio_init(flashBootPin, GpioModeOutputPushPull, GpioPullNo, GpioSpeedLow);
    furi_hal_gpio_init(flashResetPin, GpioModeOutputPushPull, GpioPullNo, GpioSpeedLow);
    // A low boot line at the reset edge enters the ROM loader; a high one boots the application.
    furi_hal_gpio_write(flashBootPin, !bootDown);
    furi_hal_gpio_write(flashResetPin, false);
    furi_delay_ms(20);
    furi_hal_gpio_write(flashResetPin, true);
    furi_delay_ms(100);
    furi_hal_gpio_write(flashBootPin, true);
}

size_t flashReadImage(void* context, uint32_t offset, uint8_t* out, size_t size) {
    auto& app = *static_cast<App*>(context);
    if(!storage_file_seek(app.flashImage, offset, true)) {
        return 0;
    }
    return storage_file_read(app.flashImage, out, size);
}

void flashProgress(void* context, FlashStage stage, uint32_t done, uint32_t total) {
    auto& app = *static_cast<App*>(context);
    app.flashStage = stage;
    app.flashDone = done;
    app.flashTotal = total;
}

int32_t flashWorker(void* context) {
    auto& app = *static_cast<App*>(context);
    flipper::FlasherIo io;
    io.context = &app;
    io.write = flashWrite;
    io.readByte = flashReadByte;
    io.boot = flashBoot;
    io.readImage = flashReadImage;
    // The flasher holds the block, packet, and SLIP buffers, a few kilobytes, so it lives on the
    // heap rather than this worker's stack.
    auto* flasher = new(std::nothrow) flipper::EspFlasher(io);
    app.flashError = flasher == nullptr ? FlashError::overflow :
                                          flasher->flash(app.flashManifest, flashProgress, &app);
    delete flasher;
    app.flashComplete.store(true, std::memory_order_release);
    return 0;
}
#endif

void beginFlash(App& app) {
    app.flashPhase = FlashPhase::flashing;
    app.flashStage = FlashStage::enter;
    app.flashDone = 0;
    app.flashTotal = static_cast<uint32_t>(
        (app.flashManifest.size + flipper::EspFlasher::blockSize - 1) /
        flipper::EspFlasher::blockSize);
    app.flashError = FlashError::none;
#ifndef AIRROUNDS_EMULATOR
    // The flasher owns the UART and the receive queue while it runs; the status poll resumes after.
    app.flashing = true;
    app.flashComplete.store(false, std::memory_order_relaxed);
    app.flashImage = storage_file_alloc(app.storage);
    if(!storage_file_open(
           app.flashImage, APP_DATA_PATH("airrounds.bin"), FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_free(app.flashImage);
        app.flashImage = nullptr;
        app.flashing = false;
        app.flashError = FlashError::image;
        app.flashPhase = FlashPhase::failed;
        return;
    }
    app.flashThread = furi_thread_alloc_ex("airrounds-flash", 4096, flashWorker, &app);
    furi_thread_start(app.flashThread);
#endif
}

void pollFlash(App& app) {
    if(app.flashPhase != FlashPhase::flashing) {
        return;
    }
#ifndef AIRROUNDS_EMULATOR
    if(!app.flashComplete.load(std::memory_order_acquire)) {
        return;
    }
    furi_thread_join(app.flashThread);
    furi_thread_free(app.flashThread);
    app.flashThread = nullptr;
    if(app.flashImage != nullptr) {
        storage_file_close(app.flashImage);
        storage_file_free(app.flashImage);
        app.flashImage = nullptr;
    }
    app.flashing = false;
    app.flashPhase = app.flashError == FlashError::none ? FlashPhase::done : FlashPhase::failed;
    // The board rebooted into the new firmware, so read it again from the next hello.
    app.client.refresh(furi_get_tick());
#else
    if(emu::flashFinish) {
        emu::flashFinish = false;
        app.flashError = emu::flashSucceeds ? FlashError::none : FlashError::sync;
        app.flashPhase = emu::flashSucceeds ? FlashPhase::done : FlashPhase::failed;
    }
#endif
}

void startFlashFlow(App& app) {
    app.flashManifest = readFlashManifest(app.storage);
    if(!app.flashManifest.valid) {
        app.flashPhase = FlashPhase::noImage;
    } else if(app.flashManifest.active) {
        app.flashPhase = FlashPhase::attest;
    } else {
        beginFlash(app);
    }
    go(app, Page::flash);
}

void showFlash(Frame& frame, const App& app) {
    switch(app.flashPhase) {
    case FlashPhase::noImage:
        title(frame, "No image");
        frame.row("Install the release");
        frame.row("to apps_data/airrounds");
        return;
    case FlashPhase::attest:
        title(frame, "Active TX image");
        frame.row("Transmits deauth.");
        frame.row("Own network only.");
        snprintf(frame.hint, sizeof(frame.hint), "%s", "OK to flash");
        frame.binding = {Action::flashConfirm, 0, {}};
        return;
    case FlashPhase::flashing: {
        title(frame, "Flashing...");
        frame.row(flashStageText(app.flashStage));
        if(app.flashTotal != 0) {
            char progress[24];
            snprintf(
                progress,
                sizeof(progress),
                "Block %u / %u",
                static_cast<unsigned>(app.flashDone),
                static_cast<unsigned>(app.flashTotal));
            frame.row(progress);
        }
        return;
    }
    case FlashPhase::done:
        title(frame, "Board flashed");
        frame.row("Image verified.");
        return;
    case FlashPhase::failed:
        title(frame, "Flash failed");
        frame.row(flashErrorText(app.flashError));
        // The Flipper cannot drive the Devboard into the ROM loader on its own, so a sync failure
        // means the board never entered it. Prompt the manual entry the sync retries then catch.
        if(app.flashError == FlashError::sync) {
            frame.row("Hold BOOT, tap RESET");
        }
        snprintf(frame.hint, sizeof(frame.hint), "%s", "OK to retry");
        frame.binding = {Action::flashBoard, 0, {}};
        return;
    case FlashPhase::idle:
        break;
    }
    title(frame, "Flash board hardware");
}

void showTestBoard(Frame& frame, const App& app) {
    title(frame, "Test board");
    switch(app.client.selfTestRun()) {
    case flipper::SelfTestRun::unsupported:
        frame.row("Not supported");
        frame.row("Update board firmware");
        return;
    case flipper::SelfTestRun::running:
        frame.row("Testing...");
        return;
    case flipper::SelfTestRun::failed:
        frame.row("No response");
        snprintf(frame.hint, sizeof(frame.hint), "%s", "Press OK to retry");
        frame.binding = {Action::testBoard, 0, {}};
        return;
    case flipper::SelfTestRun::idle:
    case flipper::SelfTestRun::done:
        break;
    }
    const auto& result = app.client.selfTest();
    frame.compact = true;
    const char* const names[] = {"microSD", "Radio", "Receive", "Write"};
    const uart::CheckResult checks[] = {result.sd, result.radio, result.probe, result.write};
    for(size_t index = 0; index < 4; ++index) {
        frame.row(names[index], checkText(checks[index]));
    }
    snprintf(frame.hint, sizeof(frame.hint), "%s", "OK to run again");
    frame.binding = {Action::testBoard, 0, {}};
}

void showCard(Frame& frame) {
    title(frame, "Insert SD card");
    frame.row("The board needs a");
    frame.row("microSD card for");
    frame.row("Scan and Capture.");
}

Frame makeFrame(const App& app) {
    Frame frame;
    switch(app.page) {
    case Page::home:
        showHome(frame, app);
        break;
    case Page::scan:
        showScan(frame, app);
        break;
    case Page::networks:
        showNetworks(frame, app);
        break;
    case Page::captureMode:
        showCaptureMode(frame, app);
        break;
    case Page::placeholder:
        if(app.placeholder == aboutItem) {
            showAbout(frame, app);
        } else {
            title(frame, scanMenu[app.placeholder]);
        }
        break;
    case Page::points:
        title(frame, "Access points");
        for(size_t i = 0; i < app.profile.size(); ++i) {
            char name[12];
            char channel[8];
            pointText(i, name);
            channelText(app.profile[i].channel, channel);
            frame.row(name, channel);
        }
        if(app.profile.size() < session::Profile::legacyCapacity) {
            frame.row("Add point");
        }
        frame.selected = static_cast<int>(app.focus);
        frame.binding = {
            app.focus == app.profile.size() ? Action::add : Action::point, app.focus, {}};
        break;
    case Page::point: {
        char name[12];
        char address[18];
        char channel[8];
        pointText(app.point, name);
        addressText(app.profile[app.point], address);
        channelText(app.profile[app.point].channel, channel);
        title(frame, name);
        frame.row(address, channel);
        frame.column = 101;
        frame.row("Edit");
        frame.row("Remove");
        frame.pointMenu = true;
        frame.selected = static_cast<int>(app.focus + 1);
        frame.binding = {app.focus == 0 ? Action::edit : Action::remove, app.point, {}};
        break;
    }
    case Page::channel: {
        char address[18];
        char channel[19];
        addressText(app.draft, address);
        profileAction(frame, app, "Select channel", Action::save);
        frame.row(address);
        snprintf(
            channel, sizeof(channel), "< Channel %u >", static_cast<unsigned>(app.draft.channel));
        frame.row(channel);
        break;
    }
    case Page::remove: {
        char address[18];
        addressText(app.profile[app.point], address);
        profileAction(frame, app, "Remove point?", Action::confirmRemove);
        frame.row(address);
        break;
    }
    case Page::review:
        title(frame, "Review session");
        frame.column = 102;
        for(size_t i = 0; i < app.profile.size(); ++i) {
            char address[18];
            char line[22];
            char channel[8];
            addressText(app.profile[i], address);
            snprintf(line, sizeof(line), "%u %s", static_cast<unsigned>(i + 1), address);
            channelText(app.profile[i].channel, channel);
            frame.row(line, channel);
        }
        if(canStart(app)) {
            frame.binding = {Action::start, 0, app.client.visitId()};
        } else {
            snprintf(
                frame.hint,
                sizeof(frame.hint),
                "%s",
                flipper::sessionBusy(app.client.state()) ? "Session in progress" :
                                                           boardIssue(app));
        }
        break;
    case Page::board:
    case Page::status:
        showStatus(frame, app);
        break;
    case Page::testBoard:
        showTestBoard(frame, app);
        break;
    case Page::card:
        showCard(frame);
        break;
    case Page::flash:
        showFlash(frame, app);
        break;
    case Page::last:
        if(app.latest.id.session == 0 && app.client.link() == Link::connecting &&
           app.serial != nullptr) {
            title(frame, "Checking status");
            frame.row("Waiting for board");
        } else {
            showOutcome(frame, app.latest, roundMode(app));
        }
        break;
    case Page::results: {
        title(frame, "Results");
        const auto count = flipper::reportSize(app.selectedReport);
        const size_t first = topRow(app.focus);
        for(size_t i = first; i < count && i < first + visibleNetworkRows; ++i) {
            char name[25];
            if(app.client.supportsScan() && i < app.selectedReport.round.profile.size()) {
                networkName(app, app.selectedReport.round.profile[i], i, name);
            } else {
                snprintf(name, sizeof(name), "Point %u", static_cast<unsigned>(i + 1));
            }
            frame.row(
                name,
                flipper::pointResult(
                    flipper::pointOutcome(app.selectedReport, i),
                    app.selectedReport.state,
                    roundMode(app)));
        }
        frame.compact = app.client.supportsScan();
        frame.scrollUp = first > 0;
        frame.scrollDown = first + visibleNetworkRows < count;
        frame.selected = static_cast<int>(app.focus - first);
        frame.binding = {Action::detail, app.focus, app.selectedReport.id};
        break;
    }
    case Page::detail: {
        char name[12];
        pointText(app.resultPoint, name);
        title(frame, name);
        const auto outcome = flipper::pointOutcome(app.selectedReport, app.resultPoint);
        frame.row(flipper::pointDetail(outcome, app.selectedReport.state, roundMode(app)));
        if(app.resultPoint < app.selectedReport.round.profile.size()) {
            const auto& target = app.selectedReport.round.profile[app.resultPoint];
            if(app.client.supportsScan()) {
                char label[25];
                networkName(app, target, app.resultPoint, label);
                frame.row(label);
            } else {
                char address[18];
                char channel[8];
                addressText(target, address);
                channelText(target.channel, channel);
                frame.row(address);
                frame.row(channel);
            }
        } else {
            frame.row("Address unavailable");
        }
        frame.fileLink = outcome == Outcome::saved;
        break;
    }
    case Page::file: {
        title(frame, "Board SD file");
        const auto& report = app.selectedReport;
        const auto slot =
            flipper::reportSize(report) == 1 ? 0U : static_cast<unsigned>(app.resultPoint + 1);
        char path[56];
        if(uart::capturePath(path, sizeof(path), "", {report.id, slot})) {
            // The file is at card root. Read the three lines as one filename.
            frame.row("visit-");
            char boot[18];
            snprintf(boot, sizeof(boot), "%.17s", path + 7);
            frame.row(boot);
            frame.row(path + 24);
        }
        break;
    }
    case Page::address:
        break;
    }
    return frame;
}

void draw(Canvas* canvas, void* model) {
    auto& display = *static_cast<Display*>(model);
    const auto& frame = display.frame;
    display.drawn = frame.binding;
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 4, 11, frame.title);
    canvas_set_font(canvas, FontSecondary);
    if(frame.titleRight[0] != '\0') {
        canvas_draw_str(
            canvas, 124 - canvas_string_width(canvas, frame.titleRight), 11, frame.titleRight);
    }
    for(size_t i = 0; i < frame.count; ++i) {
        const bool selected = static_cast<int>(i) == frame.selected;
        const auto y = frame.compact       ? 22 + static_cast<int>(i) * 11 :
                       frame.pointMenu     ? 24 + static_cast<int>(i) * 14 :
                       frame.selected >= 0 ? 28 + static_cast<int>(i) * 16 :
                                             22 + static_cast<int>(i) * 11;
        if(selected) {
            elements_slightly_rounded_box(
                canvas,
                1,
                y - (frame.compact ? 9 : 10),
                frame.compact ? 124 : 125,
                frame.compact ? 11 : 14);
            canvas_set_color(canvas, ColorWhite);
        }
        if(frame.compact) {
            char clipped[sizeof(Row::left)];
            snprintf(clipped, sizeof(clipped), "%s", frame.rows[i].left);
            const auto rightWidth = canvas_string_width(canvas, frame.rows[i].right);
            const auto rightX = 121 - rightWidth;
            const auto limit = rightWidth == 0 ? 118 : rightX - 9;
            for(size_t length = strlen(clipped);
                length > 0 && canvas_string_width(canvas, clipped) > limit;
                --length) {
                clipped[length - 1] = '\0';
            }
            canvas_draw_str(canvas, 5, y, clipped);
            if(rightWidth != 0) {
                canvas_draw_str(canvas, rightX, y, frame.rows[i].right);
            }
        } else {
            canvas_draw_str(canvas, 5, y, frame.rows[i].left);
            canvas_draw_str(canvas, frame.column, y, frame.rows[i].right);
        }
        canvas_set_color(canvas, ColorBlack);
    }
    if(frame.hint[0] != '\0') {
        canvas_draw_str(canvas, 5, 61, frame.hint);
    }
    if(frame.selected < 0) {
        if(const char* label = actionLabel(frame.binding.action)) {
            elements_button_center(canvas, label);
        }
    }
    if(frame.scrollUp) {
        canvas_draw_str(canvas, 123, 25, "^");
    }
    if(frame.scrollDown) {
        canvas_draw_str(canvas, 123, frame.scrollDownY, "v");
    }
    if(frame.leftKey[0] != '\0') {
        elements_button_left(canvas, frame.leftKey);
    }
    if(frame.rightKey[0] != '\0') {
        elements_button_right(canvas, frame.rightKey);
    } else if(frame.fileLink) {
        elements_button_right(canvas, "File");
    }
}

void paint(App& app, bool reset = false) {
    if(app.page == Page::address) {
        return;
    }
    const auto frame = makeFrame(app);
    auto& display = *static_cast<Display*>(view_get_model(app.view));
    const bool changed = reset || !display.frame.matches(frame);
    if(changed) {
        display.frame = frame;
    }
    if(reset) {
        display.drawn = {};
    }
    view_commit_model(app.view, changed);
}

constexpr uint32_t mainView = 0;
constexpr uint32_t addressView = 1;

void go(App& app, Page page, size_t focus) {
    const bool switching = (app.page == Page::address) != (page == Page::address);
    app.page = page;
    app.focus = focus;
    app.held = {};
    if(page == Page::address) {
        app.editorGeneration = (app.editorGeneration + 1U) & 0x7fffffffU;
    }
    paint(app, true);
    if(switching) {
        view_dispatcher_switch_to_view(
            app.dispatcher, page == Page::address ? addressView : mainView);
    }
}

enum class EditorEvent : uint32_t {
    changed,
    confirmed
};

void sendEditorEvent(App& app, EditorEvent event) {
    view_dispatcher_send_custom_event(
        app.dispatcher, (app.editorGeneration << 1U) | static_cast<uint32_t>(event));
}

void confirmAddress(void* context) {
    sendEditorEvent(*static_cast<App*>(context), EditorEvent::confirmed);
}

void addressChanged(void* context) {
    sendEditorEvent(*static_cast<App*>(context), EditorEvent::changed);
}

void edit(App& app, size_t index) {
    app.point = index;
    app.editOrigin = app.page;
    app.draft = index < app.profile.size() ? app.profile[index] : session::Target{{}, 1};
    app.saveResult = ProfileSave::saved;
    byte_input_set_header_text(app.keyboard, "Enter BSSID");
    byte_input_set_result_callback(
        app.keyboard,
        confirmAddress,
        addressChanged,
        &app,
        app.draft.bssid,
        sizeof(app.draft.bssid));
    go(app, Page::address);
}

bool editorEvent(void* context, uint32_t event) {
    auto& app = *static_cast<App*>(context);
    // ByteInput calls handlers with its model locked. Update it from the event loop.
    if(app.page != Page::address || (event >> 1U) != app.editorGeneration) {
        return true;
    }
    if((event & 1U) == static_cast<uint32_t>(EditorEvent::changed)) {
        byte_input_set_header_text(app.keyboard, "Enter BSSID");
        return true;
    }
    if(!session::validTarget(app.draft)) {
        byte_input_set_header_text(app.keyboard, "Invalid BSSID");
        return true;
    }
    session::Profile next;
    if(!flipper::editProfile(app.profile, app.point, &app.draft, next)) {
        byte_input_set_header_text(app.keyboard, "Point already added");
        return true;
    }
    app.saveResult = ProfileSave::saved;
    go(app, Page::channel);
    return true;
}

void savePoint(App& app, const session::Target* target) {
    session::Profile updated;
    app.saveResult = flipper::editProfile(app.profile, app.point, target, updated) ?
                         app.store.save(app.profile, updated) :
                         ProfileSave::unavailable;
    if(app.saveResult != ProfileSave::saved) {
        paint(app);
        return;
    }
    app.profile = updated;
    const auto rows =
        updated.size() + static_cast<size_t>(updated.size() < session::Profile::legacyCapacity);
    go(app, Page::points, app.point < rows ? app.point : rows - 1);
}

void enterNetworks(App& app) {
    const auto count = app.networks.size();
    if(count == 0) {
        app.networkFocus = 0;
    } else if(app.networkFocus >= count) {
        app.networkFocus = count - 1;
    }
    go(app, Page::networks, app.networkFocus);
}

void scan(App& app, uint32_t now) {
    if(cardBlocks(app)) {
        go(app, Page::card);
        return;
    }
    if(app.page != Page::scan) {
        app.scanOrigin = app.page;
        app.scanOriginFocus = app.focus;
    }
    app.scanStarted = app.client.scan(now);
    app.scanCancel = false;
    go(app, Page::scan);
}

void leaveScan(App& app) {
    app.scanStarted = false;
    if(app.scanOrigin == Page::networks) {
        enterNetworks(app);
    } else {
        go(app, Page::home, app.scanOriginFocus);
    }
}

void startCapture(App& app, const session::Profile& profile, uint32_t now) {
    if(cardBlocks(app)) {
        go(app, Page::card);
        return;
    }
    // A non-Beacon choice cannot start until the board confirms its mode list, as the hidden Start
    // button on the list already shows. The Right gesture reaches here too, so gate it here as
    // well.
    if(app.captureMode != flipper::beaconModeIndex && app.client.modesPending()) {
        return;
    }
    if(!app.client.start(profile, now, flipper::captureModes[app.captureMode].wireId)) {
        return;
    }
    app.submitted = profile;
    app.submittedId = {app.client.visitId().boot, app.client.visitId().session + 1U};
    if(app.page == Page::networks) {
        app.networkRun = app.submittedId;
        app.networkLogFailed = false;
    }
    app.restarted = false;
    app.acknowledgedStop = {};
    go(app, Page::status);
}

void chooseMode(App& app, size_t index) {
    if(index >= flipper::captureModeCount || modeLock(app) != nullptr) {
        return;
    }
    if(!flipper::modeAvailable(flipper::captureModes[index], boardModes(app))) {
        app.modeNotAvailable = true;
        return;
    }
    if(index != app.captureMode) {
        // Results of another mode mean something else, so a change clears them.
        app.captureMode = index;
        app.networks.clearOutcomes();
    }
    go(app, Page::home, captureModeItem);
}

void perform(App& app, const Binding& binding) {
    const uint32_t now = furi_get_tick();
    switch(binding.action) {
    case Action::scan:
        scan(app, now);
        break;
    case Action::selectNetwork:
        app.selectionLimit = !app.networks.toggle(binding.index);
        break;
    case Action::captureMode:
        app.modeNotAvailable = false;
        go(app, Page::captureMode, app.captureMode);
        break;
    case Action::chooseMode:
        chooseMode(app, binding.index);
        break;
    case Action::placeholder:
        app.placeholder = binding.index;
        if(app.placeholder == aboutItem) {
            app.client.requestFirmware();
        }
        go(app, Page::placeholder);
        break;
    case Action::start:
        startCapture(app, app.profile, now);
        break;
    case Action::stop:
        app.client.stop(binding.visit, now);
        break;
    case Action::refresh:
        app.client.refresh(now);
        break;
    case Action::review:
        go(app, Page::review);
        break;
    case Action::board:
        go(app, Page::board);
        break;
    case Action::testBoard:
        app.client.runSelfTest();
        go(app, Page::testBoard);
        break;
    case Action::flashBoard:
        startFlashFlow(app);
        break;
    case Action::flashConfirm:
        beginFlash(app);
        paint(app);
        break;
    case Action::status:
        go(app, Page::status);
        break;
    case Action::points:
        go(app, Page::points);
        break;
    case Action::point:
        app.point = binding.index;
        go(app, Page::point);
        break;
    case Action::add:
    case Action::edit:
        edit(app, binding.index);
        break;
    case Action::remove:
        app.saveResult = ProfileSave::saved;
        go(app, Page::remove);
        break;
    case Action::save:
        savePoint(app, &app.draft);
        break;
    case Action::confirmRemove:
        savePoint(app, nullptr);
        break;
    case Action::last:
        go(app, Page::last);
        break;
    case Action::results:
        app.resultOrigin = app.page;
        app.selectedReport = app.page == Page::last ? app.latest : currentRun(app);
        go(app, Page::results);
        break;
    case Action::detail:
        app.resultPoint = binding.index;
        go(app, Page::detail);
        break;
    case Action::none:
        break;
    }
}

bool input(InputEvent* event, void* context) {
    auto& app = *static_cast<App*>(context);
    if(app.page == Page::captureMode && event->type == InputTypePress) {
        app.modeNotAvailable = false;
    }
    if(event->key == InputKeyOk) {
        if(event->type == InputTypePress) {
            app.opening = false;
            const auto& display = *static_cast<Display*>(view_get_model(app.view));
            app.held = display.drawn;
            view_commit_model(app.view, false);
        } else if(event->type == InputTypeShort) {
            const auto held = app.held;
            app.held = {};
            if(held.action != Action::none && held.matches(makeFrame(app).binding)) {
                perform(app, held);
            }
            paint(app);
        } else if(event->type == InputTypeRelease) {
            app.held = {};
        }
        return true;
    }
    if(event->type != InputTypeShort && event->type != InputTypeRepeat) {
        return false;
    }
    if(event->key == InputKeyUp || event->key == InputKeyDown) {
        const auto frame = makeFrame(app);
        if(frame.selected >= 0) {
            app.opening = false;
            app.held = {};
            size_t count = frame.pointMenu ? 2U : frame.count;
            if(app.page == Page::networks) {
                count = app.networks.size();
            } else if(app.page == Page::results) {
                count = flipper::reportSize(app.selectedReport);
            } else if(app.page == Page::home && app.client.supportsScan()) {
                count = scanMenuSize;
            }
            if(count != 0) {
                app.focus = (app.focus + (event->key == InputKeyDown ? 1U : count - 1U)) % count;
            }
            if(app.page == Page::networks) {
                app.networkFocus = app.focus;
                app.selectionLimit = false;
            }
            paint(app);
            return true;
        }
    }
    if(event->key == InputKeyLeft || event->key == InputKeyRight) {
        if(app.page == Page::networks && event->type == InputTypeShort) {
            app.held = {};
            const uint32_t now = furi_get_tick();
            if(event->key == InputKeyLeft) {
                scan(app, now);
            } else {
                startCapture(app, app.networks.selection(), now);
            }
            return true;
        }
        if(app.page == Page::channel) {
            app.held = {};
            const unsigned channel = app.draft.channel;
            app.draft.channel = static_cast<uint8_t>(
                (channel - 1U + (event->key == InputKeyRight ? 1U : 12U)) % 13U + 1U);
            paint(app);
            return true;
        }
        if(app.page == Page::detail && event->key == InputKeyRight &&
           flipper::pointOutcome(app.selectedReport, app.resultPoint) == Outcome::saved) {
            go(app, Page::file);
            return true;
        }
    }
    return false;
}

bool back(void* context) {
    auto& app = *static_cast<App*>(context);
    app.opening = false;
    switch(app.page) {
    case Page::home:
        return false;
    case Page::scan:
        if(app.scanStarted && app.client.cancelScan(furi_get_tick())) {
            app.scanCancel = true;
        } else {
            leaveScan(app);
        }
        break;
    case Page::captureMode:
        go(app, Page::home, captureModeItem);
        break;
    case Page::placeholder:
        go(app, Page::home, app.placeholder);
        break;
    case Page::networks:
        go(app, Page::home);
        break;
    case Page::point:
        go(app, Page::points, app.point);
        break;
    case Page::testBoard:
        go(app, Page::home, testBoardItem);
        break;
    case Page::card:
        go(app, Page::home);
        break;
    case Page::flash:
        if(app.flashing) {
            return true; // A flash in progress must not be interrupted.
        }
        go(app, Page::home, flashBoardItem);
        break;
    case Page::address:
        go(app, app.editOrigin, app.editOrigin == Page::points ? app.point : 0);
        break;
    case Page::channel:
        go(app, Page::address);
        break;
    case Page::remove:
        go(app, Page::point, 1);
        break;
    case Page::results:
        go(app, app.resultOrigin);
        break;
    case Page::detail:
        go(app, Page::results, app.resultPoint);
        break;
    case Page::file:
        go(app, Page::detail);
        break;
    case Page::status:
        if(app.networkRun.session != 0) {
            if(flipper::sessionBusy(app.client.state()) || unconfirmedStart(app)) {
                return true;
            }
            enterNetworks(app);
        } else if(
            app.client.supportsScan() && app.networks.scanned() &&
            !flipper::sessionBusy(app.client.state())) {
            enterNetworks(app);
        } else {
            app.restarted = false;
            go(app, Page::home);
        }
        break;
    default:
        go(app, Page::home);
    }
    return true;
}

void onRx(FuriHalSerialHandle* handle, FuriHalSerialRxEvent event, void* context) {
    auto& app = *static_cast<App*>(context);
    constexpr unsigned errors = FuriHalSerialRxEventFrameError | FuriHalSerialRxEventNoiseError |
                                FuriHalSerialRxEventOverrunError | FuriHalSerialRxEventParityError;
    bool damaged = (event & errors) != 0;
    if((event & FuriHalSerialRxEventData) != 0) {
        const uart::TimedByte byte{furi_hal_serial_async_rx(handle), furi_get_tick()};
        damaged = damaged || furi_message_queue_put(app.rx, &byte, 0) != FuriStatusOk;
    }
    if(damaged) {
        app.damaged.store(true, std::memory_order_relaxed);
    }
}

void updateRun(App& app, bool wasBusy) {
    const auto report = currentRun(app);
    if(app.confirmedId.boot != 0 && app.confirmedId.boot != report.id.boot) {
        app.restarted = wasBusy || unconfirmedStart(app);
        app.submittedId = {};
        app.legacyTargetId = {};
        app.acknowledgedStop = {};
        app.networkRun = {};
    }
    if(report.id.session != 0) {
        app.restarted = false;
    }
    app.confirmedId = report.id;
    app.submittedId = {};
    if(report.id.session != 0 && !flipper::sessionBusy(report.state)) {
        app.latest = report;
    }
    // A terminal Stop acknowledgment can arrive before the detailed status reply.
    const bool waitingForStopDetail =
        report.state == State::stopped && app.client.supportsRoundDetail() &&
        report.round.interruption == uart::VisitInterruption::unknown;
    if(app.networkRun.session != 0 && report.id.matches(app.networkRun) &&
       report.round.profile.matches(app.submitted) && !waitingForStopDetail) {
        const auto missed = app.networks.record(report.round, report.state);
        if(!flipper::sessionBusy(report.state)) {
            app.networkLogFailed = report.journal == uart::JournalState::failed;
            app.networkRun = {};
            if(report.state != State::stopped && missed < app.networks.size()) {
                app.networkFocus = missed;
            }
            if(app.page == Page::status) {
                enterNetworks(app);
            }
        }
    }
    if(app.networkRun.session != 0 && report.id.matches(app.networkRun) &&
       !report.round.profile.matches(app.submitted)) {
        app.networkRun = {};
    }
    // updateRun runs only after a status, Start or Stop reply with no command left in flight. A
    // lost Start is never resent, and UART keeps request order. Once the board applies the Start,
    // every reply of this boot reports that session until the round ends, and the block above
    // records its result. Any other session means the Start never ran or the board moved past it
    // unseen, so no later reply can match the run.
    if(app.networkRun.session != 0 && report.id.boot == app.networkRun.boot &&
       report.id.session != app.networkRun.session) {
        app.networkRun = {};
    }
    if(app.opening) {
        app.opening = false;
        if(flipper::sessionBusy(report.state)) {
            go(app, Page::status);
        }
    }
}

void tick(void* context) {
    auto& app = *static_cast<App*>(context);
    // While a flash runs, the ROM loader owns the UART and the receive queue; the status poll
    // resumes once the flash finishes.
    if(app.serial != nullptr && !app.flashing) {
        // Count the bytes before reading the clock, so no byte to handle is newer than now.
        const uint32_t count = furi_message_queue_get_count(app.rx);
        const uint32_t now = furi_get_tick();
        const bool wasBusy = flipper::sessionBusy(app.client.state());
        const auto pending = app.client.pending();
        const auto precedingLink = app.client.link();
        const bool damaged = app.damaged.exchange(false, std::memory_order_relaxed);
        if(damaged) {
            app.client.transportLost(now);
        }
        for(uint32_t i = 0; i < count; ++i) {
            uart::TimedByte byte{};
            if(furi_message_queue_get(app.rx, &byte, 0) == FuriStatusOk && !damaged) {
                app.client.receive(byte);
            }
        }
        if((pending == Request::status || pending == Request::start || pending == Request::stop) &&
           app.client.pending() == Request::none && app.client.link() == Link::ready) {
            app.commandUnconfirmed = false;
            if(app.client.rejected() == Request::none) {
                const auto id = app.client.visitId();
                if(pending == Request::stop) {
                    app.acknowledgedStop = id;
                } else if(
                    pending == Request::start && !app.client.supportsRounds() &&
                    id.matches(app.submittedId) && app.submitted.size() == 1) {
                    app.legacyTargetId = id;
                    app.legacyTarget = app.submitted[0];
                }
            }
            updateRun(app, wasBusy);
        }
        if(app.page == Page::scan && app.scanStarted) {
            if(app.client.scanState() == uart::ScanState::complete) {
                app.networks.replace(app.client.scanResults());
                app.scanStarted = false;
                app.selectionLimit = false;
                enterNetworks(app);
            } else if(app.client.scanState() == uart::ScanState::idle) {
                leaveScan(app);
            }
        }
        app.client.poll(now);
        syncCaptureMode(app);
        if((pending == Request::start || pending == Request::stop) &&
           app.client.link() == Link::lost && app.client.pending() != pending) {
            app.commandUnconfirmed = true;
        }
        if(precedingLink != app.client.link()) {
            app.held = {};
        }
    }
    pollFlash(app);
    paint(app);
}

} // namespace

extern "C" int32_t airrounds_app(void*) {
    uint64_t nonce = 0;
    furi_hal_random_fill_buf(reinterpret_cast<uint8_t*>(&nonce), sizeof(nonce));
    auto* storage = static_cast<Storage*>(furi_record_open(RECORD_STORAGE));
    auto* gui = static_cast<Gui*>(furi_record_open(RECORD_GUI));
    void* memory = malloc(sizeof(App));
    if(memory == nullptr) {
        furi_record_close(RECORD_GUI);
        furi_record_close(RECORD_STORAGE);
        return -1;
    }
    auto* app = new(memory) App(storage, nonce);
    app->profile = app->store.load();

    app->dispatcher = view_dispatcher_alloc();
    app->view = view_alloc();
    app->keyboard = byte_input_alloc();
    view_set_context(app->view, app);
    view_allocate_model(app->view, ViewModelTypeLocking, sizeof(Display));
    new(view_get_model(app->view)) Display{};
    view_commit_model(app->view, false);
    view_set_draw_callback(app->view, draw);
    view_set_input_callback(app->view, input);
    view_dispatcher_add_view(app->dispatcher, mainView, app->view);
    view_dispatcher_add_view(app->dispatcher, addressView, byte_input_get_view(app->keyboard));

    auto* expansion = static_cast<Expansion*>(furi_record_open(RECORD_EXPANSION));
    expansion_disable(expansion);
    app->serial = furi_hal_serial_control_acquire(FuriHalSerialIdUsart);
    if(app->serial != nullptr) {
        app->rx = furi_message_queue_alloc(512, sizeof(uart::TimedByte));
        furi_hal_serial_init(app->serial, 115200);
        furi_hal_serial_async_rx_start(app->serial, onRx, app, true);
    } else {
        expansion_enable(expansion);
    }
    view_dispatcher_attach_to_gui(app->dispatcher, gui, ViewDispatcherTypeFullscreen);
    view_dispatcher_set_event_callback_context(app->dispatcher, app);
    view_dispatcher_set_custom_event_callback(app->dispatcher, editorEvent);
    view_dispatcher_set_navigation_event_callback(app->dispatcher, back);
    view_dispatcher_set_tick_event_callback(app->dispatcher, tick, furi_ms_to_ticks(50));
    paint(*app, true);
    view_dispatcher_switch_to_view(app->dispatcher, mainView);
    view_dispatcher_run(app->dispatcher);

    if(app->serial != nullptr) {
        furi_hal_serial_async_rx_stop(app->serial);
        furi_hal_serial_deinit(app->serial);
        furi_hal_serial_control_release(app->serial);
        expansion_enable(expansion);
        furi_message_queue_free(app->rx);
    }
    furi_record_close(RECORD_EXPANSION);
    view_dispatcher_remove_view(app->dispatcher, addressView);
    view_dispatcher_remove_view(app->dispatcher, mainView);
    byte_input_free(app->keyboard);
    view_free(app->view);
    view_dispatcher_free(app->dispatcher);
    app->~App();
    free(memory);
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_STORAGE);
    return 0;
}
