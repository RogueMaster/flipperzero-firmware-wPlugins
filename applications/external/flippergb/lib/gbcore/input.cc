/* Cold code: size-optimized on the embedded target. Every KB of binary
 * is a KB of heap the ROM page cache loses (the FAP loads into RAM), and
 * nothing in this file runs per-instruction. */
#if defined(__arm__) && defined(__GNUC__)
#pragma GCC optimize("Os")
#endif

#include "input.h"

#include "bitwise.h"

void Input::button_pressed(GbButton button) {
    set_button(button, true);
}

void Input::button_released(GbButton button) {
    set_button(button, false);
}

void Input::set_button(GbButton button, bool set) {
    if (button == GbButton::Up) { up = set; }
    if (button == GbButton::Down) { down = set; }
    if (button == GbButton::Left) { left = set; }
    if (button == GbButton::Right) { right = set; }
    if (button == GbButton::A) { a = set; }
    if (button == GbButton::B) { b = set; }
    if (button == GbButton::Select) { select = set; }
    if (button == GbButton::Start) { start = set; }
}

void Input::write(u8 set) {
    using bitwise::check_bit;

    direction_switch = !check_bit(set, 4);
    button_switch = !check_bit(set, 5);
}

auto Input::get_input() const -> u8 {
    using bitwise::set_bit_to;

    /* with both matrix lines selected the nibbles combine (AND: pressed
     * pulls low); the old code let the button nibble overwrite the
     * direction nibble */
    u8 dir = 0b1111, btn = 0b1111;

    if (direction_switch) {
        /* opposite directions are mechanically impossible on the real
         * d-pad rocker: if a frontend ever feeds both, show neither
         * (games misbehave badly on impossible pad states) */
        bool r = right, l = left, u = up, d = down;
        if(r && l) r = l = false;
        if(u && d) u = d = false;
        dir = set_bit_to(dir, 0, !r);
        dir = set_bit_to(dir, 1, !l);
        dir = set_bit_to(dir, 2, !u);
        dir = set_bit_to(dir, 3, !d);
    }

    if (button_switch) {
        btn = set_bit_to(btn, 0, !a);
        btn = set_bit_to(btn, 1, !b);
        btn = set_bit_to(btn, 2, !select);
        btn = set_bit_to(btn, 3, !start);
    }

    u8 buttons = static_cast<u8>(dir & btn);

    buttons = set_bit_to(buttons, 4, !direction_switch);
    buttons = set_bit_to(buttons, 5, !button_switch);

    /* bits 6-7 are unwired and read as 1 on hardware */
    return static_cast<u8>(buttons | 0xC0);
}
