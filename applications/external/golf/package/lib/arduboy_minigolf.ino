#include "game.hpp"

#include <SPI.h>
#include <EEPROM.h>
#include <Arduboy2.h>
#include "lib/ArduboyTones.h"
#include "lib/Arduino.h"

#if ARDUGOLF_FX
#include "fx_courses.hpp"
#endif

#define FPS_SET 1
#define SHOW_FPS 0

#if SHOW_FPS
static Arduboy2 a;
#else
static Arduboy2Base a;
#endif
uint8_t* const buf = Arduboy2Base::sBuffer;

ArduboyTones sound(a.audio.enabled);

void save_audio_on_off()
{
    a.audio.saveOnOff();
}

void toggle_audio()
{
    a.audio.toggle();
}

bool audio_enabled()
{
    return a.audio.enabled();
}

uint16_t time_ms()
{
    return (uint16_t)millis();
}

uint8_t poll_btns()
{
    return Arduboy2::buttonsState();
}

#define REMOVE_USB 0
#if REMOVE_USB
int main()
#else
void setup()
#endif
{
    Arduboy2Base::boot();
#if !REMOVE_USB
    if(Arduboy2Core::buttonsState() & UP_BUTTON)
    {
        Arduboy2Core::sendLCDCommand(OLED_ALL_PIXELS_ON);
        Arduboy2Core::digitalWriteRGB(RGB_ON, RGB_ON, RGB_ON);
        Arduboy2Core::digitalWriteRGB(RED_LED, RGB_ON);
        power_timer0_disable();
        for(;;) Arduboy2Core::idle();
    }
#endif
    a.audio.begin();

#if ARDUGOLF_FX
    FX::begin(FX_DATA_PAGE, FX_SAVE_PAGE);
#endif

    game_setup();

    //a.setFrameRate(30);
    uint16_t pt = time_ms();
#if SHOW_FPS
    uint16_t dt = 0;
    uint16_t fps = 0;
    uint16_t nf = 0;
#endif
    for(;;)
    {
        if (WDTCSR & _BV(WDE))
        {
            // disable ints and set magic key
            cli();
            *(volatile uint8_t*)0x800 = 0x77;
            *(volatile uint8_t*)0x801 = 0x77;
            for(;;);
        }

        //while(!a.nextFrame())
        //    ;

        // the above call increases code size by 1 KB.
        // reimplement logic below:

#if FPS_SET
        for(;;)
        {
            uint8_t t = (uint8_t)time_ms();
            uint8_t dt = t - (uint8_t)pt;
            if(dt < 33)
            {
                if(++dt < 33) Arduboy2Base::idle();
                continue;
            }
            break;
        }
#endif

#if SHOW_FPS
        uint16_t t = time_ms();
        uint16_t tdt = t - pt;
        pt = t;
        dt += tdt;
        ++nf;
        if(dt >= 250)
        {
            fps = uint32_t(1000) * nf / dt;
            nf = 0;
            dt = 0;
        }
#else
        pt = time_ms();
#endif

        game_loop();

#if SHOW_FPS
        a.setCursor(0, 0);
        uint16_t f = fps;
        a.write('0' + f / 100);
        f %= 100;
        a.write('0' + f /  10);
        f %=  10;
        a.write('0' + f);
#endif

#if ARDUGOLF_FX
    #ifdef OLED_SH1106
        FX::display(CLEAR_BUFFER);
        FX::readDataBytes(get_hole_fx_addr(leveli), &buf[0], 1024);
    #else
        FX::displayPrefetch(get_hole_fx_addr(leveli), &buf[0], 1024, false);
    #endif
        levelext = fxlevel.ext;
#else
        Arduboy2Base::display(true);
#endif
    }

#if REMOVE_USB
    return 0;
#endif
}
