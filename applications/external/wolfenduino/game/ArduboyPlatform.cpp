#include "game/ArduboyPlatform.h"

extern Arduboy2Base arduboy;

ArduboyPlatform Platform;

void ArduboyPlatform::update() {
    uint8_t raw = 0;

    Arduboy2Base::InputContext* ctx = arduboy.inputContext();
    if(ctx && ctx->input_state) raw = *(ctx->input_state);

    inputState = 0;

    if(raw & INPUT_UP) inputState |= Input_Dpad_Up;
    if(raw & INPUT_RIGHT) inputState |= Input_Dpad_Right;
    if(raw & INPUT_DOWN) inputState |= Input_Dpad_Down;
    if(raw & INPUT_LEFT) inputState |= Input_Dpad_Left;

    if(raw & INPUT_A) inputState |= Input_Btn_A;
    if(raw & INPUT_B) inputState |= Input_Btn_B;
}
