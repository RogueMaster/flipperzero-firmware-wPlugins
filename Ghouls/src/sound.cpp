#include "sound.hpp"

#ifdef SOUND_INCLUDE
#include SOUND_INCLUDE
#endif

Sound::Sound()
{
#ifdef SOUND_INIT
    SOUND_INIT();
#endif
}

Sound::~Sound()
{
#ifdef SOUND_DEINIT
    SOUND_DEINIT();
#endif
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

void Sound::playWAV(const char *path)
{
#ifdef SOUND_PLAY_WAV
    SOUND_PLAY_WAV(path);
#else
    (void)path;
#endif
}

void Sound::stop()
{
#ifdef SOUND_STOP
    SOUND_STOP();
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