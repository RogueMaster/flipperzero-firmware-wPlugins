#pragma GCC optimize("O3")

#ifndef ARDUBOY_CORE_H
#define ARDUBOY_CORE_H

#include <Arduino.h>
#include <avr/power.h>
#include <SPI.h>
#include <avr/sleep.h>
#include <limits.h>

const uint8_t PROGMEM yMask[] = {0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80};

// main hardware compile flags

#if !defined(ARDUBOY_10) && !defined(AB_DEVKIT)
/// defaults to Arduboy Release 1.0 if not using a boards.txt file
/**
 * we default to Arduboy Release 1.0 if a compile flag has not been
 * passed to us from a boards.txt file
 *
 * if you wish to compile for the devkit without using a boards.txt
 * file simply comment out the ARDUBOY_10 define and uncomment
 * the AB_DEVKIT define like this:
 *
 *     // #define ARDUBOY_10
 *     #define AB_DEVKIT
 */
#define ARDUBOY_10 //< compile for the production Arduboy v1.0
// #define AB_DEVKIT    //< compile for the official dev kit
#endif

#ifdef AB_DEVKIT
#define SAFE_MODE //< include safe mode (44 bytes)
#endif

#define RGB_ON  LOW // for digitially setting an RGB LED on
#define RGB_OFF HIGH // for digitially setting an RGB LED off

#ifdef ARDUBOY_10

#define CS  12
#define DC  4
#define RST 6

#define RED_LED   10
#define GREEN_LED 11
#define BLUE_LED  9
#define TX_LED    30
#define RX_LED    17

// pin values for buttons, probably shouldn't use these
#define PIN_LEFT_BUTTON  A2
#define PIN_RIGHT_BUTTON A1
#define PIN_UP_BUTTON    A0
#define PIN_DOWN_BUTTON  A3
#define PIN_A_BUTTON     7
#define PIN_B_BUTTON     8

// bit values for button states
#define LEFT_BUTTON  _BV(5)
#define RIGHT_BUTTON _BV(6)
#define UP_BUTTON    _BV(7)
#define DOWN_BUTTON  _BV(4)
#define A_BUTTON     _BV(3)
#define B_BUTTON     _BV(2)

#define PIN_SPEAKER_1 5
#define PIN_SPEAKER_2 13

#define PIN_SPEAKER_1_PORT &PORTC
#define PIN_SPEAKER_2_PORT &PORTC

#define PIN_SPEAKER_1_BITMASK _BV(6)
#define PIN_SPEAKER_2_BITMASK _BV(7)

#elif defined(AB_DEVKIT)

#define CS               6
#define DC               4
#define RST              12

// map all LEDs to the single TX LED on DEVKIT
#define RED_LED          17
#define GREEN_LED        17
#define BLUE_LED         17
#define TX_LED           17
#define RX_LED           17

// pin values for buttons, probably shouldn't use these
#define PIN_LEFT_BUTTON  9
#define PIN_RIGHT_BUTTON 5
#define PIN_UP_BUTTON    8
#define PIN_DOWN_BUTTON  10
#define PIN_A_BUTTON     A0
#define PIN_B_BUTTON     A1

// bit values for button states
#define LEFT_BUTTON      _BV(5)
#define RIGHT_BUTTON     _BV(2)
#define UP_BUTTON        _BV(4)
#define DOWN_BUTTON      _BV(6)
#define A_BUTTON         _BV(1)
#define B_BUTTON         _BV(0)

#define PIN_SPEAKER_1         A2
#define PIN_SPEAKER_1_PORT    &PORTF
#define PIN_SPEAKER_1_BITMASK _BV(5)
// SPEAKER_2 is purposely not defined for DEVKIT as it could potentially
// be dangerous and fry your hardware (because of the devkit wiring).
//
// Reference: https://github.com/Arduboy/Arduboy/issues/108

#endif

// OLED hardware (SSD1306)

#define OLED_PIXELS_INVERTED 0xA7 // All pixels inverted
#define OLED_PIXELS_NORMAL   0xA6 // All pixels normal

#define OLED_ALL_PIXELS_ON   0xA5 // all pixels on
#define OLED_PIXELS_FROM_RAM 0xA4 // pixels mapped to display RAM contents

#define OLED_VERTICAL_FLIPPED 0xC0 // reversed COM scan direction
#define OLED_VERTICAL_NORMAL  0xC8 // normal COM scan direction

#define OLED_HORIZ_FLIPPED 0xA0 // reversed segment re-map
#define OLED_HORIZ_NORMAL  0xA1 // normal segment re-map

// -----

#define WIDTH  128
#define HEIGHT 64

#define COLUMN_ADDRESS_END (WIDTH - 1) & 127 // 128 pixels wide
#define PAGE_ADDRESS_END   ((HEIGHT / 8) - 1) & 7 // 8 pages high

class ArduboyCoreRem {
public:
    ArduboyCoreRem();

    /// allows the CPU to idle between frames
    /**
     * This puts the CPU in "Idle" sleep mode.  You should call this as often
     * as you can for the best power savings.  The timer 0 overflow interrupt
     * will wake up the chip every 1ms - so even at 60 FPS a well written
     * app should be able to sleep maybe half the time in between rendering
     * it's own frames.
     *
     * See the Arduboy class nextFrame() for an example of how to use idle()
     * in a frame loop.
     */
    void static idle();

    void static LCDDataMode(); //< put the display in data mode

    /// put the display in command mode
    /**
     * See SSD1306 documents for available commands and command sequences.
     *
     * Links:
     * - https://www.adafruit.com/datasheets/SSD1306.pdf
     * - http://www.eimodule.com/download/SSD1306-OLED-Controller.pdf
     */
    void static LCDCommandMode();

    uint8_t static width(); //< return display width
    uint8_t static height(); // < return display height

    /// get current state of all buttons (bitmask)
    /**
     * Bit mask that is returned:
     *
     *           Hi   Low
     *  DevKit   00000000    - reserved
     *           -DLU-RAB    D down
     *                       U up
     *  1.0      00000000    L left
     *           URLDAB--    R right
     *
     * Of course you shouldn't worry about bits (they may change with future
     * hardware revisions) and should instead use the button defines:
     * LEFT_BUTTON, A_BUTTON, UP_BUTTON, etc.
     */

    uint8_t static buttonsState();

    // paints 8 pixels (vertically) from a single byte
    //  - 1 is lit, 0 is unlit
    //
    // NOTE: You probably wouldn't actually use this, you'd build something
    // higher level that does it's own calls to SPI.transfer().  It's
    // included for completeness since it seems there should be some very
    // rudimentary low-level draw function in the core that supports the
    // minimum unit that the hardware allows (which is a strip of 8 pixels)
    //
    // This routine starts in the top left and then across the screen.
    // After each "page" (row) of 8 pixels is drawn it will shift down
    // to start drawing the next page.  To paint the full screen you call
    // this function 1,024 times.
    //
    // Example:
    //
    // X = painted pixels, . = unpainted
    //
    // blank()                      paint8Pixels() 0xFF, 0, 0x0F, 0, 0xF0
    // v TOP LEFT corner (8x9)      v TOP LEFT corner
    // ........ (page 1)            X...X... (page 1)
    // ........                     X...X...
    // ........                     X...X...
    // ........                     X...X...
    // ........                     X.X.....
    // ........                     X.X.....
    // ........                     X.X.....
    // ........ (end of page 1)     X.X..... (end of page 1)
    // ........ (page 2)            ........ (page 2)
    void static paint8Pixels(uint8_t pixels);

    /// paints an entire image directly to hardware (from PROGMEM)
    /*
     * Each byte will be 8 vertical pixels, painted in the same order as
     * explained above in paint8Pixels.
     */
    void static paintScreen(const uint8_t* image);

    /// paints an entire image directly to hardware (from RAM)
    /*
     * Each byte will be 8 vertical pixels, painted in the same order as
     * explained above in paint8Pixels.
     */
    void static paintScreen(uint8_t image[]);

    /// paints a blank (black) screen to hardware
    void static blank();

    /// invert the display or set to normal
    /**
     * when inverted, a pixel set to 0 will be on
     */
    void static invert(bool inverse);

    /// turn all display pixels on, or display the buffer contents
    /**
     * when set to all pixels on, the display buffer will be
     * ignored but not altered
     */
    void static allPixelsOn(bool on);

    /// flip the display vertically or set to normal
    void static flipVertical(bool flipped);

    /// flip the display horizontally or set to normal
    void static flipHorizontal(bool flipped);

    /// send a single byte command to the OLED
    void static sendLCDCommand(uint8_t command);

    /// set the light output of the RGB LED
    /**
     * The brightness of each LED can be set to a value from
     * 0 (fully off) to 255 (fully on).
     */
    void static setRGBled(uint8_t red, uint8_t green, uint8_t blue);

    /// set the RGB LEDs digitally, to either fully on or fully off
    /**
     * Use value RGB_ON or RGB_OFF for each color of LED.
     */
    void static digitalWriteRGB(uint8_t red, uint8_t green, uint8_t blue);

    /// boots the hardware
    /**
     * - sets input/output/pullup mode for pins
     * - powers up the OLED screen and initializes it properly
     * - sets up power saving
     * - kicks CPU down to 8Mhz if needed
     * - allows Safe mode to be entered
     */
    void static boot();

    // flicker 1/0 for simulating gray
    static byte flicker;

protected:
    /// Safe mode
    /**
     * Safe Mode is engaged by holding down both the LEFT button and UP button
     * when plugging the device into USB.  It puts your device into a tight
     * loop and allows it to be reprogrammed even if you have uploaded a very
     * broken sketch that interferes with the normal USB triggered auto-reboot
     * functionality of the device.
     *
     * This is most useful on Devkits because they lack a built-in reset
     * button.
     */
    void static inline safeMode() __attribute__((always_inline));

    // internals
    void static inline setCPUSpeed8MHz() __attribute__((always_inline));
    void static inline bootOLED() __attribute__((always_inline));
    void static inline bootPins() __attribute__((always_inline));
    void static inline bootPowerSaving() __attribute__((always_inline));

private:
    volatile static uint8_t *csport, *dcport;
    uint8_t static cspinmask, dcpinmask;
};

#endif
