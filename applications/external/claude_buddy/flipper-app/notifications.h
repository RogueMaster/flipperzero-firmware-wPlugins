#pragma once

#include <notification/notification.h>
#include <notification/notification_messages.h>

typedef enum {
    LedStateOff,      // no persistent LED — sequence cleans up after itself
    LedStateWorking,  // restore to blue solid after transient flash
} LedState;

typedef enum {
    SoundSuccess,        // Ascending C5-E5-G5 + green solid
    SoundError,          // Low double beep G4-G4 + red flash
    SoundAlert,          // Single E5 blip + cyan flash
    SoundVoiceRequest,   // G5 blip + red flash (immediate on button press, before host confirms)
    SoundVoiceStart,     // A5 ding + red blink (host confirmed dictation started)
    SoundVoiceStartLed,  // red blink only — no sound, no vibro (used by host when button feedback is local)
    SoundVoiceStop,      // C5 dong + vibro + red flash then LED off
    SoundVoiceStopQuiet, // LED reset only (no sound, no vibro)
    SoundEsc,            // Quick descending E5-C5 + yellow flash
    SoundEnter,          // Short confirm blip + cyan flash
    SoundCmd,            // C5 blip + vibro + cyan flash
    SoundPerm,           // Rising C5-E5 + magenta blink (persistent)
    SoundConnect,        // Startup chord C5-E5-G5-C6 + green solid (persistent)
    SoundDisconnect,     // Shutdown chord C6-G5-E5-C5 + red flash
    SoundLedWorking,     // Blue solid LED (no sound, persistent)
    SoundLedOff,         // LED off (no sound)
    SoundInterrupt,      // Descending E5-C5 + yellow solid (persistent)
    SoundSessionEnd,     // Descending C6-G5-E5-C5 + yellow flash
    SoundReady,          // Ascending C5-E5-G5 + cyan flash
    SoundMuteOn,         // Descending two-note blip (going quiet)
    SoundMuteOff,        // Ascending two-note blip (coming back)
    SoundLedCompact,     // Cyan blink (persistent — context compaction in progress)
    SoundCompactDone,    // Blink stop + short C5 ding + cyan flash (compaction finished)
    SoundLedFlash,       // Brief cyan LED flash only (no sound, no vibro)
} SoundType;

// Play sound sequence, then restore LED to `restore` state.
// Pass LedStateWorking when the working LED should persist after a transient flash.
// Pass LedStateOff for sounds that manage their own persistent LED (connect, perm, etc.).
void notify_play(NotificationApp* app, SoundType sound, LedState restore);
