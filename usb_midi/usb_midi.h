#ifndef USB_MIDI_H
#define USB_MIDI_H

char* NOTES[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};

typedef struct {
    int note;
    int velocity;
} Note;

typedef struct {
    int running;
    FuriMessageQueue* events;
    FuriThread* reader;
    Gui* gui;
    ViewPort* view_port;
    FuriHalUsbInterface* usb_config_restore;
    MidiParser* parser;
    Note current_note;
} UsbMidiApp;

typedef enum {
    UsbMidiEventTypeExit,
    UsbMidiEventTypeNoteOn,
    UsbMidiEventTypeNoteOff,
} UsbMidiEventType;

typedef struct {
    UsbMidiEventType type;
    Note note;
} UsbMidiEvent;

#endif
