#pragma once
#include <vector>

#define INVALID_ID -1

typedef enum {
    ALL,
    ANY
} SignalType;

typedef struct SignalData {
    int id;
    void* ctx;
    bool triggered;
} SignalData;

class SignalManager {
public:
    SignalManager() = default;

    void register_signal(int id, void* ctx);
    void register_slot(int id, void* ctx);

    void send(void* ctx);
    void reset(int id);

    bool validate(char* err, std::size_t err_size);

    // all id + ctx pairs must have triggered before we send
    std::vector<SignalData> signals;
    std::vector<SignalData> slots;
};
