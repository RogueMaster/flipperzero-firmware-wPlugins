#pragma once
#include <stdint.h>

// 8x8, 8 кадров (0..7) — как в первом коде
static const uint8_t transitionSet[] = {
    8,
    8,
    // FRAME 00
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    // FRAME 01
    0x00,
    0x55,
    0x00,
    0x55,
    0x00,
    0x55,
    0x00,
    0x55,
    // FRAME 02
    0x00,
    0xFF,
    0x00,
    0xFF,
    0x00,
    0xFF,
    0x00,
    0xFF,
    // FRAME 03
    0xAA,
    0xFF,
    0xAA,
    0xFF,
    0xAA,
    0xFF,
    0xAA,
    0xFF,
    // FRAME 04
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    // FRAME 05
    0xFF,
    0xAA,
    0xFF,
    0xAA,
    0xFF,
    0xAA,
    0xFF,
    0xAA,
    // FRAME 06
    0xFF,
    0x00,
    0xFF,
    0x00,
    0xFF,
    0x00,
    0xFF,
    0x00,
    // FRAME 07
    0x55,
    0x00,
    0x55,
    0x00,
    0x55,
    0x00,
    0x55,
    0x00,
};

class Menu {
public:
    // Startup sequence: title screen, then the menu. The Credits item replays
    // that title screen from the menu, with the credit line under the logo.
    enum class SplashPhase : uint8_t {
        Title,
        Done,
        Credits
    };

    // main.cpp overlays the logo and the border frame while this is set
    bool ShowsTitleScreen() const {
        return m_splashPhase != SplashPhase::Done;
    }

    bool InCredits() const {
        return m_splashPhase == SplashPhase::Credits;
    }

    void CloseCredits();

    void Init();
    void Draw();
    void Tick();

    void ReadSave();
    void WriteSave();

    void TickEnteringLevel();
    void DrawEnteringLevel();
    void TransitionToLevel();

    void TickGameOver();
    void DrawGameOver();

    void ResetTimer();
    void FadeOut();

private:
    using TransitionNextFn = void (*)();
    static void RunTransition(Menu* menu, uint8_t& t, TransitionNextFn next);
    void DrawTransitionFrame(uint8_t frameIndex);

    void SetScore(uint16_t score);
    void PrintItem(uint8_t idx, uint8_t row);

    SplashPhase m_splashPhase = SplashPhase::Title;
    uint8_t m_splashTimer = 0;

    uint8_t m_selection = 0;
    uint8_t m_topIndex = 0;
    uint8_t m_cursorPos = 0;

    uint16_t m_score = 0;
    uint16_t m_high = 0;
    uint16_t m_storedHigh = 0;
    uint8_t m_save[9] = {0};

    union {
        uint16_t timer;
        uint16_t fizzleFade;
    };
};
