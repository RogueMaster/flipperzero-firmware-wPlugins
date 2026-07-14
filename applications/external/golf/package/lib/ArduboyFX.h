#ifndef ARDUBOYFX_H
#define ARDUBOYFX_H

#include <Arduboy2.h>

#ifdef CART_CS_RX
  #define FX_PORT PORTD
  #define FX_BIT PORTD2
#else
  #define FX_PORT PORTD
  #define FX_BIT PORTD1
#endif


// progam data and save data pages(set by PC manager tool)
constexpr uint16_t FX_VECTOR_KEY_VALUE  = 0x9518;        /* RETI instruction used as magic key */
constexpr uint16_t FX_DATA_VECTOR_KEY_POINTER  = 0x0014; /* reserved interrupt vector 5  area */
constexpr uint16_t FX_DATA_VECTOR_PAGE_POINTER = 0x0016;
constexpr uint16_t FX_SAVE_VECTOR_KEY_POINTER  = 0x0018; /* reserved interrupt vector 6  area */
constexpr uint16_t FX_SAVE_VECTOR_PAGE_POINTER = 0x001A;

// Serial Flash Commands
constexpr uint8_t SFC_JEDEC_ID          = 0x9F;
constexpr uint8_t SFC_READSTATUS1       = 0x05;
constexpr uint8_t SFC_READSTATUS2       = 0x35;
constexpr uint8_t SFC_READSTATUS3       = 0x15;
constexpr uint8_t SFC_READ              = 0x03;
constexpr uint8_t SFC_WRITE_ENABLE      = 0x06;
constexpr uint8_t SFC_WRITE             = 0x02;
constexpr uint8_t SFC_ERASE             = 0x20;
constexpr uint8_t SFC_RELEASE_POWERDOWN = 0xAB;
constexpr uint8_t SFC_POWERDOWN         = 0xB9;

// drawbitmap bit flags (used by modes below and internally)
constexpr uint8_t dbfWhiteBlack   = 0; // bitmap is used as mask
constexpr uint8_t dbfInvert       = 1; // bitmap is exclusive or-ed with display
constexpr uint8_t dbfBlack        = 2; // bitmap will be blackened
constexpr uint8_t dbfReverseBlack = 3; // reverses bitmap data
constexpr uint8_t dbfMasked       = 4; // bitmap contains mask data
constexpr uint8_t dbfExtraRow     = 7; // ignored (internal use)

// drawBitmap modes with same behaviour as Arduboy library drawBitmap modes
constexpr uint8_t dbmBlack   = _BV(dbfReverseBlack) |   // white pixels in bitmap will be drawn as black pixels on display
                               _BV(dbfBlack) |          // black pixels in bitmap will not change pixels on display
                               _BV(dbfWhiteBlack);      // (same as sprites drawErase)

constexpr uint8_t dbmWhite   = _BV(dbfWhiteBlack);      // white pixels in bitmap will be drawn as white pixels on display
                                                        // black pixels in bitmap will not change pixels on display
                                                        //(same as sprites drawSelfMasked)

constexpr uint8_t dbmInvert  = _BV(dbfInvert);          // when a pixel in bitmap has a different color than on display the
                                                        // pixel on display will be drawn as white. In all other cases the
                                                        // pixel will be drawn as black
// additional drawBitmap modes
constexpr uint8_t dbmNormal     = 0;                    // White pixels in bitmap will be drawn as white pixels on display
constexpr uint8_t dbmOverwrite  = 0;                    // Black pixels in bitmap will be drawn as black pixels on display
                                                        // (Same as sprites drawOverwrite)

constexpr uint8_t dbmReverse = _BV(dbfReverseBlack);    // White pixels in bitmap will be drawn as black pixels on display
                                                        // Black pixels in bitmap will be drawn as white pixels on display

constexpr uint8_t dbmMasked  = _BV(dbfMasked);          // The bitmap contains a mask that will determine which pixels are
                                                        // drawn and which pixels remain unchanged on display
                                                        // (same as sprites drawPlusMask)

// Note above modes may be combined like (dbmMasked | dbmReverse)

// drawChar bit flags (used by modes below)
constexpr uint8_t dcfWhiteBlack   = 0; // character is used as mask
constexpr uint8_t dcfInvert       = 1; // character is exclusive or-ed with display
constexpr uint8_t dcfBlack        = 2; // character will be blackened
constexpr uint8_t dcfReverseBlack = 3; // reverses character data
constexpr uint8_t dcfMasked       = 4; // character contains mask data
constexpr uint8_t dcfProportional = 5; // use fonts width table to mimic proportional characters

//draw Font character modes
constexpr uint8_t dcmBlack   = _BV(dcfReverseBlack) |   // white pixels in character will be drawn as black pixels on display
                               _BV(dcfBlack) |          // black pixels in character will not change pixels on display
                               _BV(dcfWhiteBlack);      // (same as sprites drawErase)

constexpr uint8_t dcmWhite   = _BV(dcfWhiteBlack);      // white pixels in character will be drawn as white pixels on display
                                                        // black pixels in character will not change pixels on display
                                                        //(same as sprites drawSelfMasked)

constexpr uint8_t dcmInvert  = _BV(dcfInvert);          // when a pixel in character has a different color than on display the
                                                        // pixel on display will be drawn as white. In all other cases the
                                                        // pixel will be drawn as black
// additional drawcharacter modes
constexpr uint8_t dcmNormal     = 0;                    // White pixels in character will be drawn as white pixels on display
constexpr uint8_t dcmOverwrite  = 0;                    // Black pixels in character will be drawn as black pixels on display
                                                        // (Same as sprites drawOverwrite)

constexpr uint8_t dcmReverse = _BV(dcfReverseBlack);    // White pixels in character will be drawn as black pixels on display
                                                        // Black pixels in character will be drawn as white pixels on display

constexpr uint8_t dcmMasked  = _BV(dcfMasked);          // The character contains a mask that will determine which pixels are

constexpr uint8_t dcmProportional = _BV(dcfProportional); // draw characters with variable spacing. When this mode is used a
                                                          // 256 byte width table must precede the font data

// Note above modes may be combined like (dcmMasked | dcmProportional)


using uint24_t = __uint24;

struct JedecID
{
  uint8_t manufacturer;
  uint8_t device;
  uint8_t size;
};

struct FXAddress
{
  uint16_t page;
  uint8_t  offset;
};

struct Font
{
  uint24_t address;
  uint8_t  mode;
  uint8_t  width;
  uint8_t  height;
};

struct Cursor
{
  int16_t x;
  int16_t y;
  int16_t left;
  int16_t wrap;
};

class FX
{
  public:
    static inline void enableOLED() __attribute__((always_inline)) // selects OLED display.
    {
      CS_PORT &= ~(1 << CS_BIT);
    };

    static inline void disableOLED() __attribute__((always_inline)) // deselects OLED display.
    {
      CS_PORT |=  (1 << CS_BIT);
    };

    static inline void enable() __attribute__((always_inline)) // selects external flash memory and allows new commands
    {
      FX_PORT  &= ~(1 << FX_BIT);
    };

    static inline void disable() __attribute__((always_inline)) // deselects external flash memory and ends the last command
    {
      FX_PORT  |=  (1 << FX_BIT);
    };

    static inline void wait() __attribute__((always_inline)) // wait for a pending flash transfer to complete
    {
      while ((SPSR & _BV(SPIF)) == 0);
    }

    static uint8_t writeByte(uint8_t data); // write a single byte to flash memory.

    static inline void writeByteBeforeWait(uint8_t data) __attribute__((always_inline))
    {
      SPDR = data;
      asm volatile("nop\n");
      wait();
    }

    static inline void writeByteAfterWait(uint8_t data) __attribute__((always_inline))
    {
      wait();
      SPDR = data;
    }

    static uint8_t readByte(); // read a single byte from flash memory

    static void displayPrefetch(uint24_t address, uint8_t* target, uint16_t len, bool clear);

    static void display(); // display screen buffer

    static void display(bool clear); // display screen buffer with clear

    static void begin(); // Initializes flash memory. Use only when program does not require data and save areas in flash memory

    static void begin(uint16_t programDataPage); // Initializes flash memory. Use when program depends on data in flash memory

    static void begin(uint16_t datapage, uint16_t savepage); // Initializes flash memory. Use when program depends on both data and save data in flash memory

    static void readJedecID(JedecID* id);

    static bool detect(); //detect presence of initialized flash memory

    static void noFXReboot(); // flash RGB LED red and wait for DOWN button to exit to bootloader when no initialized external flash memory is present

    static void writeCommand(uint8_t command); // write a single byte flash command

    static void wakeUp(); // Wake up flash memory from power down mode

    static void sleep(); // Put flash memory in power down mode for low power

    static void writeEnable();// Puts flash memory in write mode, required prior to any write command

    static void seekCommand(uint8_t command, uint24_t address);// Write command and selects flash memory address. Required by any read or write command

    static void seekData(uint24_t address); // selects flashaddress of program data area for reading and starts the first read

    static void seekDataArray(uint24_t address, uint8_t index, uint8_t offset, uint8_t elementSize);

    static void seekSave(uint24_t address); // selects flashaddress of program save area for reading and starts the first read

    static inline uint8_t readUnsafe() __attribute__((always_inline)) // read flash data without performing any checks and starts the next read.
    {
      uint8_t result = SPDR;
      SPDR = 0;
      return result;
    };

    static inline uint8_t readUnsafeEnd() __attribute__((always_inline))
    {
      uint8_t result = SPDR;
      disable();
      return result;
    };

    static uint8_t readPendingUInt8() __attribute__ ((noinline));    //read a prefetched byte from the current flash location

    static uint8_t readPendingLastUInt8() __attribute__ ((noinline));    //read a prefetched byte from the current flash location

    static uint16_t readPendingUInt16() __attribute__ ((noinline)); //read a partly prefetched 16-bit word from the current flash location

    static uint16_t readPendingLastUInt16() __attribute__ ((noinline)); //read a partly prefetched 16-bit word from the current flash location

    static uint24_t readPendingUInt24() ; //read a partly prefetched 24-bit word from the current flash location

    static uint24_t readPendingLastUInt24() ; //read a partly prefetched 24-bit word from the current flash location

    static uint32_t readPendingUInt32(); //read a partly prefetched a 32-bit word from the current flash location

    static uint32_t readPendingLastUInt32(); //read a partly prefetched a 32-bit word from the current flash location

    static void readBytes(uint8_t* buffer, size_t length);// read a number of bytes from the current flash location

    static void readBytesEnd(uint8_t* buffer, size_t length); // read a number of bytes from the current flash location and end the read command

    static uint8_t readEnd(); //read last pending byte and end read command

    static void readDataBytes(uint24_t address, uint8_t* buffer, size_t length);

    static void readSaveBytes(uint24_t address, uint8_t* buffer, size_t length);

    static void eraseSaveBlock(uint16_t page);

    static void writeSavePage(uint16_t page, uint8_t* buffer);
    
    static void waitWhileBusy(); // wait for outstanding erase or write to finish

    static void drawBitmap(int16_t x, int16_t y, uint24_t address, uint8_t frame, uint8_t mode) __attribute__((noinline));

    static void readDataArray(uint24_t address, uint8_t index, uint8_t offset, uint8_t elementSize, uint8_t* buffer, size_t length);

    static uint8_t readIndexedUInt8(uint24_t address, uint8_t index);

    static uint16_t readIndexedUInt16(uint24_t address, uint8_t index);

    static uint24_t readIndexedUInt24(uint24_t address, uint8_t index);

    static uint32_t readIndexedUInt32(uint24_t address, uint8_t index);

    /* Draw character functions */

    static void setFont(uint24_t address, uint8_t mode);

    static void setFontMode(uint8_t mode);

    static void setCursor(int16_t x, int16_t y);

    static void setCursorX(int16_t x);

    static void setCursorY(int16_t y);

    static void setCursorRange(int16_t left, int16_t wrap);

    static void setCursorLeft(int16_t x);

    static void setCursorWrap(int16_t y);

    static void drawChar(uint8_t c);

    static void drawString(const uint8_t* buffer);

    static void drawString(const char* str);

    static void drawString(uint24_t address);

    //static void drawNumber(int8_t n, int8_t digits = 0); // draw a signed 8-bit number
    //
    //static void drawNumber(unt8_t n, int8_t digits = 0); // draw a unsigned 8-bit number
    //
    static void drawNumber(int16_t n, int8_t digits = 0); // draw a signed 16-bit number

    static void drawNumber(uint16_t n, int8_t digits = 0); // draw a unsigned 16-bit number

    static void drawNumber(int32_t n, int8_t digits = 0);  // draw signed 32-bit number with a fixed number of digits
                                                           // digits == 0: No leading characters are added, all digits are drawn
                                                           // digits >  0: Leading zeros are added if the number has less than 'digit' digits
                                                           // digits <  0: Leading spaces are added if the number has less than 'digit' digits
                                                           // Note: Only 'digits' number of digits are drawn regardless if the number has more digits
                                                           //       The sign character is not counted as a digit.

    static void drawNumber(uint32_t n, int8_t digits = 0); // draw unsigned 32-bit number with a fixed number of digits
                                                           // digits == 0: No leading characters are added, all digits are drawn
                                                           // digits >  0: Leading zeros are added if the number has less than 'digit' digits
                                                           // digits <  0: Leading spaces are added if the number has less than 'digit' digits
                                                           // Note: Only 'digits' number of digits are drawn regardless if the number has more digits

    /* general optimized functions */

    static inline uint16_t multiplyUInt8 (uint8_t a, uint8_t b) __attribute__((always_inline))
    {
     #ifdef ARDUINO_ARCH_AVR
      uint16_t result;
      asm volatile(
        "mul    %[a], %[b]      \n"
        "movw   %A[result], r0  \n"
        "clr    r1              \n"
        : [result] "=&r" (result)
        : [a]      "r"   (a),
          [b]      "r"   (b)
        :
      );
      return result;
     #else
      return (a * b);
     #endif
    }

    static inline uint8_t bitShiftLeftUInt8(uint8_t bit) __attribute__((always_inline)) //fast (1 << (bit & 7))
    {
     #ifdef ARDUINO_ARCH_AVR
      uint8_t result;
      asm volatile(
        "ldi    %[result], 1    \n" // 0 = 000 => 0000 0001
        "sbrc   %[bit], 1       \n" // 1 = 001 => 0000 0010
        "ldi    %[result], 4    \n" // 2 = 010 => 0000 0100
        "sbrc   %[bit], 0       \n" // 3 = 011 => 0000 1000
        "lsl    %[result]       \n"
        "sbrc   %[bit], 2       \n" // 4 = 100 => 0001 0000
        "swap   %[result]       \n" // 5 = 101 => 0010 0000
        :[result] "=&d" (result)    // 6 = 110 => 0100 0000
        :[bit]    "r"   (bit)       // 7 = 111 => 1000 0000
        :
      );
      return result;
     #else
      return 1 << (bit & 7);
     #endif
    }

    static inline uint8_t bitShiftRightUInt8(uint8_t bit) __attribute__((always_inline)) //fast (0x80 >> (bit & 7))
    {
     #ifdef ARDUINO_ARCH_AVR
      uint8_t result;
      asm volatile(
        "ldi    %[result], 1    \n" // 0 = 000 => 1000 0000
        "sbrs   %[bit], 1       \n" // 1 = 001 => 0100 0000
        "ldi    %[result], 4    \n" // 2 = 010 => 0010 0000
        "sbrs   %[bit], 0       \n" // 3 = 011 => 0001 0000
        "lsl    %[result]       \n"
        "sbrs   %[bit], 2       \n" // 4 = 100 => 0000 1000
        "swap   %[result]       \n" // 5 = 101 => 0000 0100
        :[result] "=&d" (result)    // 6 = 110 => 0000 0010
        :[bit]    "r"   (bit)       // 7 = 111 => 0000 0001
        :
      );
      return result;
     #else
      return 0x80 >> (bit & 7);
     #endif
    }

    static inline uint8_t bitShiftLeftMaskUInt8(uint8_t bit) __attribute__((always_inline)) //fast (0xFF << (bit & 7) & 0xFF)
    {
     #ifdef ARDUINO_ARCH_AVR
      uint8_t result;
      asm volatile(
        "ldi    %[result], 1    \n" // 0 = 000 => 1111 1111 = -1
        "sbrc   %[bit], 1       \n" // 1 = 001 => 1111 1110 = -2
        "ldi    %[result], 4    \n" // 2 = 010 => 1111 1100 = -4
        "sbrc   %[bit], 0       \n" // 3 = 011 => 1111 1000 = -8
        "lsl    %[result]       \n"
        "sbrc   %[bit], 2       \n" // 4 = 100 => 1111 0000 = -16
        "swap   %[result]       \n" // 5 = 101 => 1110 0000 = -32
        "neg    %[result]       \n" // 6 = 110 => 1100 0000 = -64
        :[result] "=&d" (result)    // 7 = 111 => 1000 0000 = -128
        :[bit]    "r"   (bit)
        :
      );
      return result;
     #else
      return (0xFF << (bit & 7)) & 0xFF;
     #endif
    }

    static inline uint8_t bitShiftRightMaskUInt8(uint8_t bit) __attribute__((always_inline)) //fast (0xFF >> (bit & 7))
    {
     #ifdef ARDUINO_ARCH_AVR
      uint8_t result;
      asm volatile(
        "ldi    %[result], 2    \n" // 0 = 000 => 1111 1111 = 0x00 - 1
        "sbrs   %A[bit], 1      \n" // 1 = 001 => 0111 1111 = 0x80 - 1
        "ldi    %[result], 8    \n" // 2 = 010 => 0011 1111 = 0x40 - 1
        "sbrs   %A[bit], 2      \n" // 3 = 011 => 0001 1111 = 0x20 - 1
        "swap   %[result]       \n"
        "sbrs   %A[bit], 0      \n" // 4 = 100 => 0000 1111 = 0x10 - 1
        "lsl    %[result]       \n" // 5 = 101 => 0000 0111 = 0x08 - 1
        "dec    %[result]       \n" // 6 = 110 => 0000 0011 = 0x04 - 1
        :[result] "=&d" (result)    // 7 = 111 => 0000 0001 = 0x02 - 1
        :[bit]    "r"   (bit)
        :
      );
      return result;
     #else
      return 0xFF >> (bit & 7);
     #endif
    }

    static uint16_t programDataPage; // program read only data area in flash memory
    static uint16_t programSavePage; // program read and write data area in flash memory
    static Font     font;
    static Cursor   cursor;
};
#endif
