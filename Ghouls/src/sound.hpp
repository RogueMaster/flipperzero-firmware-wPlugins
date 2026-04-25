#pragma once
#include "config.hpp"

typedef struct
{
    uint16_t leftFrequency;  // Frequency in Hz
    uint16_t rightFrequency; // Frequency in Hz (optional)
    uint32_t durationMs;     // Duration in milliseconds
} SoundNote;

typedef struct
{
    char name[32];          // Short name for command reference
    const SoundNote *notes; // Pointer to the song data
    uint16_t noteCount;     // Number of notes in the song
} SoundSong;

class Sound
{
public:
    Sound();
    ~Sound();
    void playNote(const SoundNote &note);                  // play a single note immediately
    void playPCMSample(const int16_t *samples, int count); // play raw PCM audio samples
    void playWAV(const char *path);                        // play a WAV file by path
    void stop();                                           // halt all WAV/audio playback
    void setSong(const SoundSong &song);                   // set song to play tick-by-tick
    void tick();                                           // update sound playback state
private:
    uint32_t time;
    SoundSong currentSong;
    uint16_t currentNoteIndex;
};