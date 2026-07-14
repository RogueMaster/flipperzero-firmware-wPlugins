# Architecture

## Overview

HTW AC Remote follows a modular architecture with clear separation of concerns:

```
┌─────────────────────────────────────────────────────────┐
│                    htw_ac_remote.c                      │
│                   (Application Core)                    │
│  - Lifecycle management                                 │
│  - View dispatcher coordination                         │
│  - IR transmission                                      │
│  - Callbacks handling                                   │
└─────────────────────┬───────────────────────────────────┘
                      │
        ┌─────────────┼─────────────┐
        │             │             │
        ▼             ▼             ▼
┌───────────┐  ┌───────────┐  ┌───────────┐
│ Main View │  │Timer View │  │Setup View │
│           │  │           │  │           │
│ htw_main  │  │ htw_timer │  │ htw_setup │
│ _view.c   │  │ _view.c   │  │ _view.c   │
└─────┬─────┘  └─────┬─────┘  └─────┬─────┘
      │              │              │
      └──────────────┼──────────────┘
                     │
                     ▼
          ┌─────────────────────┐
          │     htw_state.c     │
          │  (State Management) │
          │  - Current settings │
          │  - Persistence      │
          └──────────┬──────────┘
                     │
                     ▼
          ┌─────────────────────┐
          │  htw_ir_protocol.c  │
          │   (IR Encoding)     │
          │  - Frame building   │
          │  - Timing generation│
          └─────────────────────┘
```

## Components

### Application Core (`htw_ac_remote.c/h`)

Main application module responsible for:

- **Initialization**: Allocating resources, loading state, setting up views
- **View Dispatcher**: Managing navigation between views
- **IR Transmission**: Coordinating with InfraredWorker for sending signals
- **Animation**: Timer-based sending animation
- **Cleanup**: Saving state and freeing resources on exit

```c
typedef struct {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    NotificationApp* notifications;

    HtwMainView* main_view;
    HtwTimerView* timer_view;
    HtwSetupView* setup_view;

    HtwState* state;

    InfraredWorker* ir_worker;
    uint32_t ir_timings[HTW_IR_MAX_TIMINGS];
    size_t ir_timings_count;

    FuriTimer* send_timer;
    bool is_sending;
    int anim_frame;

    HtwViewId current_view;
} HtwApp;
```

### State Management (`htw_state.c/h`)

Manages application state and persistence:

- **State Structure**: All AC parameters (mode, fan, temp, toggles, timers)
- **Persistence**: Save/load to SD card (`/ext/apps_data/htw_ac_remote/state.txt`)
- **Validation**: Ensures loaded data is within valid ranges
- **Cycling Functions**: Helper functions for incrementing/decrementing values

```c
typedef struct {
    HtwMode mode;           // Current mode
    HtwFan fan;             // Fan speed
    uint8_t temp;           // Temperature 17-30

    bool swing;             // Toggle states (virtual)
    bool turbo;
    bool fresh;
    bool led;
    bool clean;

    uint8_t timer_on_step;  // Timer ON (0=off, 1-34=time)
    uint8_t timer_off_step; // Timer OFF

    bool save_state;        // Persistence enabled
} HtwState;
```

### IR Protocol (`htw_ir_protocol.c/h`)

Handles IR signal encoding:

- **Timing Constants**: Carrier frequency, mark/space durations
- **Frame Building**: Constructs 6-byte frames with checksums
- **Double Transmission**: Each command sent twice for reliability
- **Command Types**:
  - State commands (mode/temp/fan)
  - Toggle commands (power off, swing, LED, turbo, fresh, clean)
  - Timer commands (ON/OFF scheduling)

### Views

#### Main View (`views/htw_main_view.c/h`)

Primary user interface:
- Mode/Fan selectors with labels
- Temperature display with +/- buttons
- Toggle buttons grid (Swing, Turbo, Fresh, LED, Clean)
- Navigation to Timer and Setup
- HTW logo with sending animation
- Debounce mechanism for carousel parameters (800ms delay)

#### Timer View (`views/htw_timer_view.c/h`)

Timer configuration interface:
- Auto ON timer selector (0.5h - 24h)
- Auto OFF timer selector (0.5h - 24h)
- Direct sending with OK button
- 34 time steps

#### Setup View (`views/htw_setup_view.c/h`)

Settings interface:
- Save state toggle switch
- Visual toggle control

## Data Flow

### Sending a Command

```
User Input → View Input Handler → State Update → Debounce Timer
                                                       │
                                                       ▼ (after 800ms)
                                              Send Callback
                                                       │
                                                       ▼
                                              IR Protocol Encode
                                                       │
                                                       ▼
                                              InfraredWorker TX
                                                       │
                                                       ▼
                                              Animation Start
```

### Toggle Commands (Immediate)

```
User Input → View Input Handler → State Toggle → Send Callback (immediate)
                                                       │
                                                       ▼
                                              IR Protocol Encode
                                                       │
                                                       ▼
                                              InfraredWorker TX
```

## Memory Management

- All allocations in `htw_app_alloc()`, freed in `htw_app_free()`
- View models use `ViewModelTypeLocking` for thread safety
- IR timings buffer pre-allocated (max 500 timings)
- State saved on app exit

## Threading Model

- Main thread: UI and user input
- FuriTimer: Animation updates (60ms period)
- InfraredWorker: IR transmission (internal threading)
- View model access: Protected by `with_view_model()` macro
