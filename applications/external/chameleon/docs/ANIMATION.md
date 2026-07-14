# Chameleon Flipper - Animações Contextuais

## 🎬 Sistema de Animações

O projeto implementa um sistema completo de animações contextuais onde o **Camaleão** (representando o Chameleon Ultra) e o **Golfinho** (mascote do Flipper Zero) interagem em diferentes situações.

## 🎭 Tipos de Animação Implementados

### 1. **Bar Scene** (ChameleonAnimationBar) - Padrão
A animação original que mostra **Chameleon** e **Dolphin** se encontrando no "CHAMELEON BAR".

### Characters

**Dolphin (Flipper Mascot)**
- Animated blinking eyes
- Waving fin gesture
- Characteristic smile
- Positioned on the left side of the bar

**Chameleon**
- Moving eye (characteristic of real chameleons)
- Animated color-changing crest
- Curled tail
- **Tongue that shoots out periodically**
- Triangular head shape
- Positioned on the right side of the bar

### Scene Elements

**Bar Environment:**
- "CHAMELEON BAR" sign at top
- Blinking light/chandelier
- Shelf with 6 decorative bottles
- Detailed counter/bar top
- Two bar stools (one for each character)
- Drinks with animated bubbles
- Decorative details

### Animation Sequence

The animation runs at 8 FPS for approximately 4 seconds (32 frames):

1. **Frames 0-7**: Dolphin greets with speech bubble "Olá!"
2. **Frames 8-15**: Chameleon responds "E ai!"
3. **Frames 16-23**: Both characters toast together with "Saúde!"
4. **Frames 24-31**: "Conectado!" message appears

### User Interaction

- **Skip**: Press any button to skip the animation
- **Auto-advance**: Animation automatically completes and returns to main menu
- **Duration**: ~4 seconds total

## Technical Implementation

### Files

```
views/
├── chameleon_animation_view.h    # Animation view interface
└── chameleon_animation_view.c    # Animation implementation
```

### Key Components

**ChameleonAnimationView**
- Custom view with canvas drawing
- Timer-based frame updates (8 FPS)
- Model-View pattern for state management
- Callback support for completion events

**Drawing Functions**
- `draw_dolphin()` - Renders Flipper mascot
- `draw_chameleon()` - Renders Chameleon character
- `draw_bar()` - Renders bar environment
- `draw_speech()` - Renders speech bubbles

### Integration Points

The animation is triggered in these scenes:
- `chameleon_scene_usb_connect.c` - After successful USB connection
- `chameleon_scene_ble_connect.c` - After successful Bluetooth connection

### Memory Usage

- View Model: ~8 bytes (frame counter + running flag)
- Stack: Minimal (uses canvas drawing)
- Timer: FuriTimer periodic at 125ms intervals

## Usage Example

```c
// Setup callback
chameleon_animation_view_set_callback(
    app->animation_view,
    my_callback_function,
    context);

// Start animation
view_dispatcher_switch_to_view(app->view_dispatcher, ChameleonViewAnimation);
chameleon_animation_view_start(app->animation_view);

// Animation will automatically call callback when done
```

## Customization

To modify the animation:

1. **Change duration**: Adjust `ANIMATION_FPS` or frame count check
2. **Add frames**: Modify frame counter limit in timer callback
3. **Change timing**: Adjust speech bubble frame ranges
4. **Modify characters**: Edit drawing functions in `chameleon_animation_view.c`

## Future Enhancements

Possible improvements:
- Sound effects for speech bubbles
- More complex character animations
- Different animations for USB vs Bluetooth
- Configurable animation speed
- Multiple animation variations
