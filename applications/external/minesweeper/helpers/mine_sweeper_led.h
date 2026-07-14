#ifndef MINESWEEPER_LED_H
#define MINESWEEPER_LED_H

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

void mine_sweeper_led_set_rgb(void* context, int red, int green, int blue);

void mine_sweeper_led_blink_red(void* context);

void mine_sweeper_led_blink_yellow(void* context);

void mine_sweeper_led_blink_magenta(void* context);

void mine_sweeper_led_blink_cyan(void* context);

void mine_sweeper_led_reset(void* context);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // MINESWEEPER_LED_H
