#include "sound.hpp"

#ifdef SOUND_INCLUDE
#include SOUND_INCLUDE
#endif

Sound::Sound()
{
}

Sound::~Sound()
{
}

void Sound::playNote(const SoundNote &note)
{
#ifdef SOUND_PLAY_STEREO_FREQUENCY
    SOUND_PLAY_STEREO_FREQUENCY(note.leftFrequency, note.rightFrequency, note.durationMs);
#elif defined(SOUND_PLAY_MONO_FREQUENCY)
    SOUND_PLAY_MONO_FREQUENCY(note.leftFrequency, note.durationMs);
#else
    (void)note;
#endif
}

void Sound::playPCMSample(const int16_t *samples, int count)
{
#ifdef SOUND_PLAY_PCM
    SOUND_PLAY_PCM(samples, count);
#else
    (void)samples;
    (void)count;
#endif
}

void Sound::setSong(const SoundSong &song)
{
    currentSong = song;
    currentNoteIndex = 0;
    time = 0;
}

void Sound::tick()
{
    time++;

    if (currentSong.notes == nullptr || currentNoteIndex >= currentSong.noteCount)
    {
        return; // No song or finished
    }

    // will come back here
}