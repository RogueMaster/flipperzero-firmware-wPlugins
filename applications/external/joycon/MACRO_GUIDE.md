# Macro Recording Guide

## Challenge: Limited Buttons

The Flipper Zero only has 6 physical buttons (Up, Down, Left, Right, OK, Back), but the Nintendo Switch controller has many more inputs. Here are strategies for creating macros:

## Approach 1: Manual Controller Mode (Current Implementation)

The app currently maps Flipper buttons with three control modes:

### Control Modes
- **D-Pad Mode**: Up/Down/Left/Right → D-Pad movements
- **Left Stick Mode**: Up/Down/Left/Right → Left analog stick (full deflection)
- **Right Stick Mode**: Up/Down/Left/Right → Right analog stick (full deflection)

**Toggle modes**: Long press Back button

### Button Mapping
- **Directional Buttons**: Up/Down/Left/Right (controls D-Pad or sticks based on mode)
- **A Button**: OK button
- **B Button**: Back button (short press)
- **Mode Toggle**: Back button (long press)
- **Button Menu**: OK button (long press) to access X, Y, L, R, ZL, ZR, +, -, Home

### Recording Macros with Manual Mode

1. Before recording, set the control mode (D-Pad, Left Stick, or Right Stick)
2. Start recording
3. Use directional buttons for D-Pad or stick movements
4. Switch modes during recording if needed (long press Back)
5. Use button menu (long press OK) for additional buttons
6. Stop recording

**Example Use Cases:**
- **Menu navigation**: D-Pad Mode + A/B buttons
- **Character movement**: Left Stick Mode for 3D games
- **Camera control**: Right Stick Mode for camera movement
- **Repeated actions**: Any mode for farming, grinding
- **Complex combos**: Switch between modes during recording

## Approach 2: Create Macros via Text Files

For advanced users, you can manually create macro files:

### Macro File Format

Macro files (`.scm`) are binary files stored in `/ext/apps_data/switch_controller/`

**Structure:**
1. Magic number: `0x4D414353` (4 bytes)
2. Name: null-terminated string (32 bytes)
3. Event count: uint32_t (4 bytes)
4. Total duration: uint32_t (4 bytes)
5. Events: array of MacroEvent structures

**MacroEvent Structure:**
```c
typedef struct {
    MacroEventType type;     // 4 bytes
    uint32_t timestamp;       // 4 bytes (ms since start)
    union {
        struct { uint16_t button; } button;
        struct { uint8_t direction; } dpad;
        struct { uint16_t lx, ly, rx, ry; } stick;
        struct { uint32_t duration_ms; } delay;
    } data;                   // 8 bytes
} MacroEvent; // Total: 16 bytes
```

### Creating Macros Programmatically

You could create a Python script to generate macro files:

```python
import struct

def create_macro(name, events):
    magic = 0x4D414353

    # Calculate total duration
    total_duration = max([e['timestamp'] for e in events]) if events else 0

    # Write header
    data = struct.pack('<I', magic)  # Magic
    data += name.encode('utf-8').ljust(32, b'\0')  # Name
    data += struct.pack('<I', len(events))  # Event count
    data += struct.pack('<I', total_duration)  # Total duration

    # Write events
    for event in events:
        # Event type (0=ButtonPress, 1=ButtonRelease, 2=Delay, 3=DPad, 4=Stick)
        data += struct.pack('<I', event['type'])
        data += struct.pack('<I', event['timestamp'])

        # Event data (union, always 8 bytes)
        if event['type'] == 0 or event['type'] == 1:  # Button
            data += struct.pack('<H', event['button'])
            data += b'\x00' * 6
        elif event['type'] == 3:  # DPad
            data += struct.pack('<B', event['direction'])
            data += b'\x00' * 7
        elif event['type'] == 4:  # Stick
            data += struct.pack('<HHHH', event['lx'], event['ly'], event['rx'], event['ry'])
        else:  # Delay
            data += struct.pack('<I', event['duration'])
            data += b'\x00' * 4

    return data

# Example: Create a simple A button spam macro
events = []
for i in range(10):
    events.append({
        'type': 0,  # ButtonPress
        'timestamp': i * 200,
        'button': 0x04  # SWITCH_BTN_A
    })
    events.append({
        'type': 1,  # ButtonRelease
        'timestamp': i * 200 + 100,
        'button': 0x04
    })

macro_data = create_macro("a_spam", events)
with open('a_spam.scm', 'wb') as f:
    f.write(macro_data)
```

## Approach 3: Future Enhancements

Potential improvements for easier macro creation:

### Option A: Macro Script Language
Create a simple text-based script format:

```
# Example macro script
WAIT 1000
PRESS A
WAIT 100
RELEASE A
WAIT 500
DPAD DOWN
WAIT 100
DPAD NEUTRAL
PRESS A
WAIT 100
RELEASE A
```

### Option B: Record from Actual Controller
Use a real Switch Pro Controller connected to a computer, record inputs, then convert to Flipper macro format.

### Option C: Macro Editor App
Build a companion app (desktop or mobile) that allows:
- Visual macro editing
- Timing adjustment
- Button combination shortcuts
- Export to `.scm` format for Flipper

## Button Reference

### Switch Button Bit Flags
```c
SWITCH_BTN_Y       = 0x0001  // 1 << 0
SWITCH_BTN_B       = 0x0002  // 1 << 1
SWITCH_BTN_A       = 0x0004  // 1 << 2
SWITCH_BTN_X       = 0x0008  // 1 << 3
SWITCH_BTN_L       = 0x0010  // 1 << 4
SWITCH_BTN_R       = 0x0020  // 1 << 5
SWITCH_BTN_ZL      = 0x0040  // 1 << 6
SWITCH_BTN_ZR      = 0x0080  // 1 << 7
SWITCH_BTN_MINUS   = 0x0100  // 1 << 8
SWITCH_BTN_PLUS    = 0x0200  // 1 << 9
SWITCH_BTN_LSTICK  = 0x0400  // 1 << 10
SWITCH_BTN_RSTICK  = 0x0800  // 1 << 11
SWITCH_BTN_HOME    = 0x1000  // 1 << 12
SWITCH_BTN_CAPTURE = 0x2000  // 1 << 13
```

### D-Pad Directions
```c
SWITCH_HAT_UP          = 0x00
SWITCH_HAT_UP_RIGHT    = 0x01
SWITCH_HAT_RIGHT       = 0x02
SWITCH_HAT_DOWN_RIGHT  = 0x03
SWITCH_HAT_DOWN        = 0x04
SWITCH_HAT_DOWN_LEFT   = 0x05
SWITCH_HAT_LEFT        = 0x06
SWITCH_HAT_UP_LEFT     = 0x07
SWITCH_HAT_NEUTRAL     = 0x08
```

## Example Macros

### Simple A Button Spam
Records pressing A button 10 times with 200ms delay between presses.

### Menu Navigation
```
DOWN -> A -> DOWN -> A -> DOWN -> A
```

### Pokémon Egg Hatching (D-Pad Mode)
```
Loop:
  DPAD LEFT (hold 2s)
  DPAD NEUTRAL
  DPAD RIGHT (hold 2s)
  DPAD NEUTRAL
```

### Character Circling (Left Stick Mode)
```
Loop:
  LEFT STICK UP (0.5s)
  LEFT STICK RIGHT (0.5s)
  LEFT STICK DOWN (0.5s)
  LEFT STICK LEFT (0.5s)
  CENTER
```

## Tips

1. **Keep it simple**: Start with basic D-Pad and A/B button macros
2. **Test thoroughly**: Verify macros work as expected before relying on them
3. **Timing matters**: Some games are timing-sensitive; adjust delays as needed
4. **Loop wisely**: Use loop mode for repeated actions (farming, grinding)
5. **Safety first**: Always supervise macro playback to prevent unintended actions

## Limitations

- Analog stick movements are digital only (full deflection or centered, no partial movements)
- Maximum 1000 events per macro
- No real-time editing during recording
- Cannot record simultaneous button presses (they're recorded sequentially)
- Diagonal stick movements require switching rapidly between directions (not true analog)

## Future Ideas

- Add analog stick support
- Implement macro variables (random delays, conditional logic)
- Support for rumble/vibration patterns
- Macro chaining (play multiple macros in sequence)
- Cloud macro library (share macros with community)
