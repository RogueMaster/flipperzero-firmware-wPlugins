#pragma once

#include <cstdio>

#include "shared/uart_reply.h"

namespace airrounds {
namespace flipper {

class NetworkList {
public:
    size_t size() const {
        return size_;
    }
    bool scanned() const {
        return scanned_;
    }
    uart::Outcome outcome(size_t index) const {
        return rows_[index].outcome;
    }
    bool selected(size_t index) const {
        return rows_[index].selected;
    }

    void replace(const uart::ScanList& scan) {
        uart::Outcome previousOutcomes[uart::ScanList::capacity]{};
        bool previousSelections[uart::ScanList::capacity]{};
        for(size_t index = 0; index < scan.size(); ++index) {
            const size_t old = find(scan[index].target);
            if(old < size_) {
                previousSelections[index] = rows_[old].selected;
                previousOutcomes[index] = rows_[old].outcome;
            }
        }
        size_ = scan.size();
        scanned_ = true;
        for(size_t index = 0; index < size_; ++index) {
            rows_[index].entry = scan[index];
            rows_[index].outcome = previousOutcomes[index];
            rows_[index].selected = previousSelections[index];
        }
    }

    size_t find(const session::Target& target) const {
        for(size_t index = 0; index < size_; ++index) {
            if(session::sameAddress(rows_[index].entry.target, target)) {
                return index;
            }
        }
        return size_;
    }

    void clearOutcomes() {
        for(size_t index = 0; index < size_; ++index) {
            rows_[index].outcome = uart::Outcome::none;
        }
    }

    bool toggle(size_t index) {
        if(index >= size_ || outcome(index) == uart::Outcome::saved) {
            return true;
        }
        if(!selected(index) && selection().size() == session::Profile::capacity) {
            return false;
        }
        rows_[index].selected = !rows_[index].selected;
        return true;
    }

    session::Profile selection() const {
        session::Profile profile;
        for(size_t index = 0; index < size_; ++index) {
            if(selected(index)) {
                profile.add(rows_[index].entry.target);
            }
        }
        return profile;
    }

    size_t record(const uart::RoundStatus& round, uart::State state) {
        size_t firstMissed = size_;
        const size_t finished = round.next();
        // Older boards cannot identify an interrupted visit in their terminal reply.
        const bool skipLastMiss = state == uart::State::stopped &&
                                  round.interruption != uart::VisitInterruption::no;
        for(size_t slot = 0; slot < finished; ++slot) {
            const auto result = round.results[slot];
            if(skipLastMiss && slot + 1 == finished && result == uart::Outcome::not_observed) {
                continue;
            }
            const auto index = find(round.profile[slot]);
            if(index == size_) {
                continue;
            }
            rows_[index].outcome = result;
            rows_[index].selected = false;
            if(result == uart::Outcome::not_observed && firstMissed == size_) {
                firstMissed = index;
            }
        }
        return firstMissed;
    }

    void label(size_t index, char (&text)[25]) const {
        char name[17];
        displayName(rows_[index].entry, name);
        size_t ordinal = 0;
        size_t duplicates = 0;
        for(size_t row = 0; row < size_; ++row) {
            char candidate[17];
            displayName(rows_[row].entry, candidate);
            if(strcmp(name, candidate) == 0) {
                ++duplicates;
                if(row == index) {
                    ordinal = duplicates;
                }
            }
        }
        if(duplicates > 1) {
            snprintf(text, sizeof(text), "%u. %s", static_cast<unsigned>(ordinal), name);
        } else {
            snprintf(text, sizeof(text), "%s", name);
        }
    }

private:
    // Limit the profile labels before passing them to the GUI. The original SSID stays on the wire.
    static void displayName(const uart::ScanEntry& entry, char (&text)[17]) {
        const auto count = entry.nameSize < sizeof(text) - 1 ? entry.nameSize : sizeof(text) - 1;
        bool named = false;
        for(size_t index = 0; index < count; ++index) {
            const auto byte = entry.name[index];
            named = named || (byte != 0 && byte != ' ');
            text[index] = byte >= ' ' && byte <= '~' ? static_cast<char>(byte) : '?';
        }
        text[count] = '\0';
        if(!named) {
            snprintf(text, sizeof(text), "Hidden network");
        }
    }

    struct Row {
        uart::ScanEntry entry{};
        uart::Outcome outcome = uart::Outcome::none;
        bool selected = false;
    };

    Row rows_[uart::ScanList::capacity]{};
    size_t size_ = 0;
    bool scanned_ = false;
};

} // namespace flipper
} // namespace airrounds
