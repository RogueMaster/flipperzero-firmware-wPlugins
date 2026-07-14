#include "ToyPadEmu.h"

#include <dolphin/dolphin.h>

#include "usb/frame.h"
#include "bytes.h"
#include "minifigures.h"

#define TOKEN_DELAY_TIME 150 // delay time for the token to be placed on the pad in ms

ToyPadEmu* emulator;

bool quick_swap = false;

// Define box information (coordinates and dimensions) for each box (7 boxes total)
struct BoxInfo boxInfo[NUM_BOXES] = {
    {18, 26, false, -1}, // Selection 0 (box)
    {50, 20, false, -1}, // Selection 1 (circle)
    {85, 27, false, -1}, // Selection 2 (box)
    {16, 40, false, -1}, // Selection 3 (box)
    {35, 41, false, -1}, // Selection 4 (box)
    {70, 41, false, -1}, // Selection 5 (box)
    {86, 40, false, -1} // Selection 6 (box)
};

static unsigned char generate_checksum(const unsigned char* command, size_t len) {
    unsigned char result = 0;

    // Add bytes, wrapping naturally with unsigned char overflow
    for(size_t i = 0; i < len; ++i) {
        result += command[i];
    }

    return result;
}

void ToyPadEmu_clear() {
    // clear the tokens on the emulator and free the memory
    for(int i = 0; i < MAX_TOKENS; i++) {
        if(emulator->tokens[i] != NULL) {
            free(emulator->tokens[i]);
            emulator->tokens[i] = NULL;
        }
    }
    memset(emulator->tokens, 0, sizeof(emulator->tokens));

    // clear all box info
    for(int i = 0; i < NUM_BOXES; i++) {
        boxInfo[i].isFilled = false;
        boxInfo[i].index = -1; // Reset index
    }
}

Token* ToyPadEmu_get_token(int index) {
    if(index < 0 || index >= MAX_TOKENS) {
        return NULL; // Invalid index
    }
    return emulator->tokens[index];
}

Token* ToyPadEmu_find_token_by_index(int index) {
    for(int i = 0; i < MAX_TOKENS; i++) {
        if(emulator->tokens[i] != NULL && emulator->tokens[i]->index == index) {
            return emulator->tokens[i];
        }
    }
    return NULL;
}

void ToyPadEmu_remove_old_token(Token* token) {
    for(int i = 0; i < MAX_TOKENS; i++) {
        Token* t = emulator->tokens[i];
        if(!t) continue;

        if(memcmp(t->uid, token->uid, 7) != 0) continue;

        // Remove the old token
        if(ToyPadEmu_remove(i)) {
            for(int j = 0; j < NUM_BOXES; j++) {
                if(boxInfo[j].index == i) {
                    boxInfo[j].isFilled = false;
                    boxInfo[j].index = -1;
                    break;
                }
            }
            furi_delay_ms(TOKEN_DELAY_TIME);
        }
        break; // Stop after removing the old token
    }
}

typedef enum {
    PAD_CIRCLE = 1,
    PAD_2 = 2,
    PAD_3 = 3,
} Pad;

static void selectedBox_to_pad(Token* const token, int selectedBox) {
    // Map 7 boxes to 3 pads
    static const Pad box_to_pad[7] = {
        PAD_2, // 0
        PAD_CIRCLE, // 1
        PAD_3, // 2
        PAD_2, // 3
        PAD_2, // 4
        PAD_3, // 5
        PAD_3 // 6
    };

    // Defensive bounds check: negative or out-of-range values trigger crash
    if((unsigned)selectedBox >= COUNT_OF(box_to_pad)) {
        furi_crash("Selected pad is invalid"); // It should never reach this
    }

    // Assign the pad using a simple, fast lookup
    token->pad = box_to_pad[selectedBox];
}

static int find_empty_slot() {
    for(int i = 0; i < MAX_TOKENS; i++) {
        if(emulator->tokens[i] == NULL) {
            return i;
        }
    }
    return -1; // No empty slot available
}

static void send_token_command(Token* token, int index, bool placed) {
    unsigned char buffer[32] = {0};

    buffer[0] = FRAME_TYPE_REQUEST; // Magic number
    buffer[1] = 0x0b; // Size always 11
    buffer[2] = token->pad;
    buffer[3] = 0x00; // always 0
    buffer[4] = index;
    buffer[5] = placed ? 0x00 : 0x01; // 0 = placed, 1 = removed
    memcpy(&buffer[6], token->uid, 7);
    buffer[13] = generate_checksum(buffer, 13);

    usbd_ep_write(get_usb_device(), HID_EP_IN, buffer, sizeof(buffer));
}

bool ToyPadEmu_place(Token* token, int selectedBox) {
    ToyPadEmu_remove_old_token(token); // Remove old token if it exists by UID

    // Find an empty slot or use the next available index
    int new_index = find_empty_slot();
    if(new_index == -1) return false;

    token->index = new_index;

    selectedBox_to_pad(token, selectedBox);

    emulator->tokens[new_index] = token;
    boxInfo[selectedBox].index = new_index;
    boxInfo[selectedBox].isFilled = true;

    send_token_command(token, new_index, true);

    /* Award some XP to the dolphin after placing a minifigure/vehicle. This needs to
    * happen outside of the ISR context of the USB, so we place it here.
    */
    dolphin_deed(DolphinDeedNfcReadSuccess);

    return true;
}

bool ToyPadEmu_remove(int index) {
    if(index < 0 || index >= MAX_TOKENS || emulator->tokens[index] == NULL) {
        return false; // Invalid index or already removed
    }

    Token* token = emulator->tokens[index];

    send_token_command(token, index, false);

    // Free the token and clear the slot
    free(emulator->tokens[index]);
    emulator->tokens[index] = NULL;

    return true;
}

void ToyPadEmu_remove_all_tokens() {
    // Remove all tokens from the toypad by ToyPadEmu_remove with waiting between each removal
    for(int i = 0; i < MAX_TOKENS; i++) {
        if(emulator->tokens[i] != NULL) {
            if(ToyPadEmu_remove(i)) {
                furi_delay_ms(TOKEN_DELAY_TIME); // wait for the token to be removed
            }
        }
    }
    // Clear the box info
    ToyPadEmu_clear();
}

void ToyPadEmu_place_tokens(Token* tokens[MAX_TOKENS], BoxInfo boxes[NUM_BOXES]) {
    if(tokens == NULL || boxes == NULL) return;

    ToyPadEmu_remove_all_tokens();

    // Build a map from token index to box position
    BoxInfo* box_map[MAX_TOKENS] = {0};
    for(int j = 0; j < NUM_BOXES; j++) {
        if(boxes[j].index >= 0 && boxes[j].index < MAX_TOKENS) {
            box_map[boxes[j].index] = &boxes[j];
        }
    }

    for(int i = 0; i < MAX_TOKENS; i++) {
        Token* token = tokens[i];
        BoxInfo* box = box_map[i];

        if(token && box) {
            int box_position = box - boxes;
            if(box_position < 0 || box_position >= NUM_BOXES) {
                continue;
            }
            if(ToyPadEmu_place(token, box_position)) {
                furi_delay_ms(TOKEN_DELAY_TIME);
            }
        }
    }
}

uint8_t ToyPadEmu_get_token_count() {
    // Count the number of tokens currently placed on the toypad
    uint8_t count = 0;
    for(int i = 0; i < MAX_TOKENS; i++) {
        if(emulator->tokens[i] != NULL) {
            count++;
        }
    }
    return count;
}

int ToyPadEmu_get_token_count_by_id(unsigned int id) {
    // Get the number of tokens with the same ID

    if(quick_swap) {
        return 0;
    }

    int count = 0;

    Token** tokens = emulator->tokens;
    for(int i = 0; i < MAX_TOKENS; i++) {
        Token* t = tokens[i];
        if(t && t->id == id) count++;
    }

    return count;
}

static inline uint32_t uid_hash(uint32_t x) {
    x ^= x >> 16;
    x *= 0x7feb352d;
    x ^= x >> 15;
    x *= 0x846ca68b;
    x ^= x >> 16;
    return x;
}

static void create_uid(Token* token, int id) {
    token->uid[0] = 0x04; // first uid byte always 0x04
    token->uid[6] = 0x80; // last uid byte always 0x80

    if(is_vehicle(token)) {
        for(int i = 1; i <= 5; i++) {
            token->uid[i] = rand() % 256;
        }
        return;
    }

    const char* v = furi_hal_version_get_name_ptr();
    uint32_t vh = uid_hash(v[0] | (v[1] << 8) | (v[2] << 16) | (v[3] << 24));

    uint32_t h = uid_hash(id) ^ uid_hash(ToyPadEmu_get_token_count_by_id(id)) ^ vh;

    for(int i = 1; i <= 5; i++) {
        token->uid[i] = (h >> ((i - 1) * 8)) & 0xFF;
    }
}

Token* createCharacter(int id) {
    Token* token = (Token*)malloc(sizeof(Token)); // Allocate memory for the token

    token->id = id; // Set the ID

    create_uid(token, id); // Create the UID

    // convert the name to a string
    snprintf(token->name, sizeof(token->name), "%s", get_minifigure_name(id));

    return token; // Return the created token
}

Token* createVehicle(int id, uint32_t upgrades[2]) {
    Token* token = (Token*)malloc(sizeof(Token));

    // Initialize the token data to zero
    memset(token, 0, sizeof(Token));

    // Dont set an ID for vehicles, it is in the token buffer already, we can use this to see if it is an minfig or vehicle by checking if the ID set or not

    // Generate a random UID and store it
    create_uid(token, id);

    // Write the upgrades and ID to the token data
    writeUInt32LE(&token->token[0x23 * 4], upgrades[0], 0); // Upgrades[0]
    writeUInt16LE(&token->token[0x24 * 4], id, 0); // ID
    writeUInt32LE(&token->token[0x25 * 4], upgrades[1], 0); // Upgrades[1]
    writeUInt16BE(&token->token[0x26 * 4], 1, 0); // Constant value 1 (Big Endian)

    snprintf(token->name, sizeof(token->name), "%s", get_vehicle_name(id));

    return token;
}

void ToyPadEmu_free() {
    ToyPadEmu_clear();
    free(emulator);
}
