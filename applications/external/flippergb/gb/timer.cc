#include "timer.h"

#include "definitions.h"
#include "gameboy.h"
#include "cpu.h"
#include "bitwise.h"

const uint CLOCKS_PER_CYCLE = 4;

Timer::Timer(Gameboy& _gb) : gb(_gb) {}

void Timer::tick(uint cycles) {
    /* DIV increments at 16384 Hz = every 64 M-cycles (upstream incremented
     * it once per M-cycle: 64x too fast, breaking games that use DIV for
     * delays or randomness) */
    div_clocks += cycles;
    if(div_clocks >= 64) {
        divider.set(static_cast<u8>(divider.value() + (div_clocks >> 6)));
        div_clocks &= 63;
    }

    clocks += cycles * CLOCKS_PER_CYCLE;

    auto timer_is_on = timer_control.check_bit(2);
    if (timer_is_on == 0) { return; }

    auto clock_limit = clocks_needed_to_increment();

    if (clocks >= clock_limit) {
        clocks = clocks % clock_limit;

        u8 old_timer_counter = timer_counter.value();
        timer_counter.increment();

        if (timer_counter.value() < old_timer_counter) {
            gb.cpu.interrupt_flag.set_bit_to(2, true);
            timer_counter.set(timer_modulo.value());
        }
    }
}

auto Timer::get_divider() const -> u8 { return divider.value(); }

auto Timer::get_timer() const -> u8 { return timer_counter.value(); }

auto Timer::get_timer_modulo() const -> u8 { return timer_modulo.value(); }

// Only the bottom three bits of this register are usable
auto Timer::get_timer_control() const -> u8 { return timer_control.value() & 0x3; }

void Timer::reset_divider() {
    divider.set(0x0);
}

void Timer::set_timer(u8 value) {
    timer_counter.set(value);
}

void Timer::set_timer_modulo(u8 value) {
    timer_modulo.set(value);
}

void Timer::set_timer_control(u8 value) {
    timer_control.set(value);
}

uint Timer::clocks_needed_to_increment() {
    using bitwise::check_bit;

    switch (get_timer_control()) {
        case 0: return CLOCK_RATE / 4096;
        case 1: return CLOCK_RATE / 262144;
        case 2: return CLOCK_RATE / 65536;
        case 3: return CLOCK_RATE / 16384;
        default: return CLOCK_RATE / 4096; /* unreachable */
    }
}
