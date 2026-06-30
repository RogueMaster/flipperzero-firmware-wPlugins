#include <furi.h>
#include "objects.h"
#include "signals.h"

void SignalManager::register_signal(int id, void* ctx) {
    // FURI_LOG_I("SIGNAL", "Registered signal, id = %d", id);
    signals.push_back({id, ctx, false});
}

void SignalManager::register_slot(int id, void* ctx) {
    // FURI_LOG_I("SIGNAL", "Registered slot, id = %d", id);
    slots.push_back({id, ctx, false});
}

// Send signal 'id' and account for type ALL and ANY
void SignalManager::send(void* ctx) {
    FixedObject* obj = (FixedObject*)ctx;
    int id = obj->tx_id;

    if(id == INVALID_ID) {
        return;
    }
    // FURI_LOG_I("SIGNAL", "Attempting to send signal %d", id);
    bool signal = true;
    for(auto& s : signals) {
        if(s.id == id) {
            if(s.ctx == ctx) {
                s.triggered = true;
                if(((FixedObject*)(ctx))->tx_type == SignalType::ANY) {
                    signal = true;
                    break;
                }
            }
            signal = signal & s.triggered;
        }
    }
    if(signal) {
        // Send the signal to all objects who want it
        // FURI_LOG_I(
        //     "SIGNAL",
        //     "Signals for id=%d, and type=%s",
        //     id,
        //     ((FixedObject*)(ctx))->tx_type == SignalType::ALL ? "ALL" : "ANY");
        for(auto& s : slots) {
            if(s.id == id) {
                ((FixedObject*)(s.ctx))->signal_receive();
            }
        }
        // Clear our internal state of what triggered (used for type ALL)
        reset(id);
        // Let the signal initiating objects know that we have sent the signal
        for(auto& s : signals) {
            if(s.id == id) {
                ((FixedObject*)(s.ctx))->signal_send();
            }
        }
    }
}

void SignalManager::reset(int id) {
    for(auto& s : signals) {
        if(s.id == id) {
            s.triggered = false;
        }
    }
}

// better data structures would make this function more efficient
bool SignalManager::validate(char* err, std::size_t err_size) {
    // Verify that there is at least one slot for every signal
    for(const auto& signal : signals) {
        bool found = false;
        for(const auto& slot : slots) {
            if(signal.id == slot.id) {
                found = true;
                break;
            }
        }
        if(!found) {
            FURI_LOG_E("PB0 SIGNAL", "Signal %d has no slots!", signal.id);
            snprintf(err, err_size, "Signal %d\nhas no\nslots!", signal.id);
            return false;
        }
    }
    // Verify that there is at least one signal for every slot
    for(const auto& slot : slots) {
        bool found = false;
        for(const auto& signal : signals) {
            if(slot.id == signal.id) {
                found = true;
                break;
            }
        }
        if(!found) {
            FURI_LOG_E("PB0 SIGNAL", "Slot %d has no signals!", slot.id);
            snprintf(err, err_size, "Slot %d\nhas no\nsignals!", slot.id);
            return false;
        }
    }
    // Verify that all objects with the same signal id have the same trigger type
    bool valid_types = true;
    for(const auto& signal : signals) {
        const SignalType signal_type = ((FixedObject*)signal.ctx)->tx_type;
        for(const auto& s : signals) {
            FixedObject* s_obj = (FixedObject*)signal.ctx;
            if(signal.id == s.id && signal_type != s_obj->tx_type) {
                valid_types = false;
                FURI_LOG_E("PB0 SIGNAL", "Signal %d has differing type!", s.id);
                snprintf(err, err_size, "Signal %d\nhas diff\ntype!", s.id);
                break;
            }
        }
        if(!valid_types) {
            break;
        }
    }
    return valid_types;
}
