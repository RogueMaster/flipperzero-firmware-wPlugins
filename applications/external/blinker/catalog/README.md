# Blinker

A Flipper Zero application that blinks LEDs with a decreasing frequency over time. Unlike traditional Pomodoro timers, it provides visual feedback through LED blinks that gradually slow down.

# Application Features

## Main screen
* **Menu:** available options
    * **Int.** - configuration of max and min intervals using the number picker screen
    * **Dur.** - configuration of duration of execution using the number picker screen
    * **Flash** - start blinking the LED light and move to execution screen

## Number picker screen
* **Header:** name of selected mode selected mode and unit.
    * Available modes are max interval, min interval and duration.
    * Units: Beats per minute or BPM for intervals, minutes for duration.
* **Number picker:** keyboard with buffer to choose a number in correct constrains. 1 - 200 for intervals and 1-60 for duration.

## Execution screen
* **Text:** showing current tempo of blinking in beats per minute.