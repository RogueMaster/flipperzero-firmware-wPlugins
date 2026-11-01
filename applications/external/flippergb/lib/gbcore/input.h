#pragma once

#include "definitions.h"

enum class GbButton {
    Up,
    Down,
    Left,
    Right,
    A,
    B,
    Select,
    Start,
};

class Input {
public:
    void button_pressed(GbButton button);
    void button_released(GbButton button);
    void write(u8 set);

    auto get_input() const -> u8;

    /* is this button's matrix line currently selected via P1? (the joypad
     * interrupt only triggers for selected lines on hardware) */
    auto line_selected(GbButton b) const -> bool {
        bool is_dir = (b == GbButton::Up || b == GbButton::Down ||
                       b == GbButton::Left || b == GbButton::Right);
        return is_dir ? direction_switch : button_switch;
    }

private:
    void set_button(GbButton button, bool set);

    bool up = false;
    bool down = false;
    bool left = false;
    bool right = false;
    bool a = false;
    bool b = false;
    bool select = false;
    bool start = false;

    bool button_switch = false;
    bool direction_switch = false;
};
