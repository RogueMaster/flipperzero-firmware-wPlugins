#ifndef ARDUBOY2_H
#define ARDUBOY2_H

#include "ArduboyCoreRem.h"
#include <Print.h>
#include <limits.h>

// Library version.
// For a version number in the form of x.y.z the value of the define will be
// ((x * 10000) + (y * 100) + (z)) as a decimal number.
// So, it will read as xxxyyzz, with no leading zeros on x.
#define ARDUBOY_LIB_VER 20100

//#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"
#include "ArduboyAudioRem.h"
//#pragma GCC diagnostic pop

#define PIXEL_SAFE_MODE

// pixel colors
#define GRAY  2 //< alternate lit/unlit
#define WHITE 1 //< lit pixel
#define BLACK 0 //< unlit pixel

// compare Vcc to 1.1 bandgap
#define ADC_VOLTAGE (_BV(REFS0) | _BV(MUX4) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1))
// compare temperature to 2.5 internal reference and _BV(MUX5)
#define ADC_TEMP    (_BV(REFS0) | _BV(REFS1) | _BV(MUX2) | _BV(MUX1) | _BV(MUX0))

/// Rectangle for collision functions
struct Rect {
    int16_t x;
    int16_t y;
    uint8_t width;
    uint8_t height;
};

/// Point for collision functions
struct Point {
    int16_t x;
    int16_t y;
};

//==================================
//========== ArduboyBaseRem ==========
//==================================

class ArduboyBaseRem : public ArduboyCoreRem {
public:
    ArduboyBaseRem();

    ArduboyAudioRem audio;

    /// Flashlight mode
    /**
   * Turn the RGB LED and screen fully on. Called in the begin() function.
   *
   * Hold the UP button when booting to enable. Press the DOWN button to exit
   * or simply turn off your Arduboy. It goes into a tight loop until the
   * DOWN button is pressed.
   */
    void flashlight();

    /// Handle buttons held on startup for system control
    /**
   * Hold the B button when booting to enter system control mode.
   * The B button must be held continuously to remain in this mode.
   * Pressing other buttons will perform system control functions:
   *
   * UP: Set "sound enabled" in EEPROM
   * DOWN: Set "sound disabled" (mute) in EEPROM
   */
    void systemButtons();

    /// Clears display.
    void clear();

    /// Copies the contents of the screen buffer to the screen.
    /**
   * X and Y positions on the display are from the top left corner, thus a Y of 64
   * is the bottom of the screen and an X of 128 is the right side of the screen.
   * "Color" or "value" means choosing whether a pixel is lit or not - if color is
   * 0, the pixel is off (black), if color is 1, the pixel is on (white).
   */
    void display();

    /// Sets a single pixel on the screen buffer to white or black.
    void drawPixel(int16_t x, int16_t y, uint8_t color = WHITE) const;

    /// Fast opaque unclipped: draw a full vertical byte (8 pixels) without any checks
    /// Warning: 'y' is not a pixel coordinate: it represents (y / 8), always aligned on a full byte.
    void inline drawByte(uint8_t x, uint8_t y, uint8_t color) const {
        sBuffer[(y * WIDTH) + x] = color;
    }

    /// Returns the state of the given pixel in the screen buffer.
    /**
   * Return value will be 1 if the pixel is on (white) or 0 if the pixel is off (black).
   */
    uint8_t getPixel(uint8_t x, uint8_t y) const;

    /// Draw a circle of a defined radius.
    /**
   * Draws a circle in white or black. X and Y are the center point of the circle.
   */
    void drawCircle(int16_t x0, int16_t y0, uint8_t r, uint8_t color = WHITE);

    /// Draws one or more "corners" of a circle.
    void drawCircleHelper(
        int16_t x0,
        int16_t y0,
        uint8_t r,
        uint8_t cornername,
        uint8_t color = WHITE);

    /// Draws a filled-in circle.
    void fillCircle(int16_t x0, int16_t y0, uint8_t r, uint8_t color = WHITE);

    /// Draws one or both vertical halves of a filled-in circle.
    void fillCircleHelper(
        int16_t x0,
        int16_t y0,
        uint8_t r,
        uint8_t cornername,
        int16_t delta,
        uint8_t color = WHITE);

    /// Draws a line between two points.
    /**
   * Uses Bresenham's algorithm.
   */
    void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint8_t color = WHITE);

    /// Draws a rectangle of a width and height.
    void drawRect(int16_t x, int16_t y, uint8_t w, uint8_t h, uint8_t color = WHITE);

    /// Draws vertical line.
    void drawFastVLine(int16_t x, int16_t y, uint8_t h, uint8_t color = WHITE);

    /// Draws a horizontal line.
    void drawFastHLine(int16_t x, uint8_t y, int16_t w, uint8_t color = WHITE);

    /// Draws a filled-in rectangle.
    void fillRect(int16_t x, int16_t y, uint8_t w, uint8_t h, uint8_t color = WHITE);

    /// Fills the screen buffer with white or black.
    void fillScreen(uint8_t color = WHITE);

    /// Draws a rectangle with rounded edges.
    void
        drawRoundRect(int16_t x, int16_t y, uint8_t w, uint8_t h, uint8_t r, uint8_t color = WHITE);

    /// Draws a filled-in rectangle with rounded edges.
    void
        fillRoundRect(int16_t x, int16_t y, uint8_t w, uint8_t h, uint8_t r, uint8_t color = WHITE);

    /// Draws the outline of a triangle.
    void drawTriangle(
        int16_t x0,
        int16_t y0,
        int16_t x1,
        int16_t y1,
        int16_t x2,
        int16_t y2,
        uint8_t color = WHITE);

    /// Draws a filled-in triangle.
    void fillTriangle(
        int16_t x0,
        int16_t y0,
        int16_t x1,
        int16_t y1,
        int16_t x2,
        int16_t y2,
        uint8_t color = WHITE);

    /// Draws a bitmap from program memory to a specific X/Y
    void drawBitmap(int16_t x, int16_t y, const uint8_t* bitmap, uint8_t w, uint8_t h);

    /// Draws a mask bitmap from program memory to a specific X/Y
    void drawMaskBitmap(int8_t x, int8_t y, const uint8_t* bitmap, uint8_t hflip = 0);

    /// Draws a 2-bit grayscale bitmap from program memory to a specific X/Y
    void drawGrayBitmap(int16_t x, int16_t y, const uint8_t* bitmap, uint8_t w, uint8_t h);

    /// Draws a bitmap from program memory to a specific X/Y
    /// Restriction: always 'y' aligned on a byte, and always a multiple of 8 in height, and always opaque
    void drawTurboBitmap(int16_t x, int16_t y, const uint8_t* bitmap, uint8_t w, uint8_t h);

    /// Draws images that are bit-oriented horizontally.
    /**
   * This requires a lot of additional CPU power and will draw images slower
   * than drawBitmap, where the images are stored in a format that
   * allows them to be directly written to the screen. It is
   * recommended you use drawBitmap when possible.
   */
    void drawSlowXYBitmap(
        int16_t x,
        int16_t y,
        const uint8_t* bitmap,
        uint8_t w,
        uint8_t h,
        uint8_t color = WHITE);

    /// Draw a compressed bitmap
    /**
   * Draws a bitmap starting at the given coordinates that has been compressed
   * using an algorthm implemented by Team A.R.G.
   * For more information see:
   * https://github.com/TEAMarg/drawCompressed
   * https://github.com/TEAMarg/Cabi
   */
    void drawCompressed(int16_t sx, int16_t sy, const uint8_t* bitmap, uint8_t color = WHITE);

    /// Get a pointer to the display buffer.
    unsigned char* getBuffer();

    /// Seeds the random number generator with entropy from the temperature, voltage reading, and microseconds since boot.
    /**
   * This method is still most effective when called semi-randomly such
   * as after a user hits a button to start a game or other semi-random
   * events
   */
    void initRandomSeed();

    /// Swap the references of two pointers.
    void swap(int16_t& a, int16_t& b);

    /// Sets the frame rate in frames per second. The parameter needs to be (1000000/fps)
    void setFrameRate(uint16_t rate);

    /// Returns "true" if it's time for the next frame.
    void nextFrame();

    /// Returns the load on the CPU as a percentage.
    /**
   * This is based on how much of the time your app is spending rendering
   * frames.  This number can be higher than 100 if your app is rendering
   * really slowly.
   */
    int cpuLoad();

    /// useful for getting raw approximate voltage values
    uint16_t rawADC(uint8_t adc_bits);

    /// Test if the specified buttons are pressed
    /**
   * Returns "true" if all the buttons in the provided mask are currently pressed.
   * (Can be a single button)
   * E.g. `if (pressed(LEFT_BUTTON + A_BUTTON))`
   */
    bool pressed(uint8_t buttons);

    /// Test if the specified buttons are not pressed
    /**
   * Returns "true" if all the buttons in the provided mask are currently released.
   * (Can be a single button)
   * E.g. `if (notPressed(LEFT_BUTTON))`
   */
    bool notPressed(uint8_t buttons);

    /// Poll the hardware buttons and track state over time.
    /**
   * Should be called either in the main `loop()` or as part of the
   * frame system at the start of rendering a frame.
   *
   * The `justPressed()` and `justReleased()` functions rely on this function.
   */
    void pollButtons();

    /// Check if a button has just been pressed
    /**
   * Will return "true" if the given button was pressed between the latest
   * call to `pollButtons()` and previous call to `pollButtons()`.
   * If the button has been held down over multiple polls this function will
   * return "false". This function should only be used to test a single button.
   */
    bool justPressed(uint8_t button);

    /// Check if a button has just been released
    /**
   * Will return "true" if the given button was released between the latest
   * call to `pollButtons()` and previous call to `pollButtons()`.
   * If the button has been released over multiple polls this function will
   * return "false". This function should only be used to test a single button.
   */
    bool justReleased(uint8_t button);

    /// Test if a point is within a rectangle
    /**
   * Returns "true" if the point is within the rectangle.
   */
    bool collide(Point point, Rect rect);

    /// Test if a rectangle is intersecting with another rectangle.
    /**
   * Returns "true" if the first rectangle is intersecting the second.
   */
    bool collide(Rect rect1, Rect rect2);

public:
    // helper function for sound enable/disable system control
    void sysCtrlSound(uint8_t buttons, uint8_t led, uint8_t eeVal);

    // Screen buffer with one row of 8 pixels of waste at the bottom for faster bitmap clipping
    static uint8_t sBuffer[((HEIGHT + 8) * WIDTH) / 8];

    // For button handling
    uint8_t currentButtonState;
    uint8_t previousButtonState;

    // For frame funcions
    uint16_t eachFrameMicros;
    uint16_t nextFrameStart;
};

//==============================
//========== ArduboyRem ==========
//==============================

class ArduboyRem : public Print, public ArduboyBaseRem {
public:
    ArduboyRem();

    /// Writes a single ASCII character to the screen.
    virtual size_t write(uint8_t);

    /// Draws an ASCII character at a point.
    void drawChar(uint8_t x, uint8_t y, unsigned char c) const;
    void drawChar(uint8_t x, uint8_t y, unsigned char c, uint8_t color, uint8_t bg, uint8_t size)
        const {
        drawChar(x, y, c);
    }

    /// Print a byte number padded with zero (i.e.: '00', '07', '99'..)
    void printBytePadded(uint8_t x, uint8_t y, byte num) const;

    /// Sets the location of the text cursor.
    void setCursor(uint8_t x, uint8_t y);

    /// Get the text cursor X position
    uint8_t getCursorX() const;

    /// Get the text cursor Y position
    uint8_t getCursorY() const;

    /// Sets the text foreground color
    void setTextColor(uint8_t color);

    /// Sets the text background color
    void setTextBackground(uint8_t bg);

    /// Set the text size
    /**
   * Individual ASCII characters are 6x8 pixels
   * (5x7 with spacing on two edges). The size is a pixel multiplier,
   * so a size of 2 means each character will be 12x16, etc.
   */
    void setTextSize(uint8_t s);

    /// Sets whether text will wrap at screen edges.
    void setTextWrap(bool w);

    /// Clears the display and sets the cursor to 0, 0
    void clear();

protected:
    uint8_t cursor_x;
    uint8_t cursor_y;
    uint8_t textColor;
    uint8_t textBackground;
    uint8_t textSize;
    bool textWrap;
};

#endif
