#ifndef ARDUBOY_AUDIO_H
#define ARDUBOY_AUDIO_H

#include <Arduino.h>
#include <EEPROM.h>

class ArduboyAudioRem {
public:
    void static begin();
    void static on();
    void static off();
    void static saveOnOff();
    bool static enabled();

protected:
    bool static audio_enabled;
};

#endif
