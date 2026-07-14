# Space Travel Calculator - Development Guide

## Project Context

This is a Flipper Zero application for calculating interplanetary transfer orbits. The goal is absolute minimalism - no fluff, just the essential physics for planning transfers between planets.

## Core Requirements

### Mission Planning Focus
- **Real User Goal**: Planning actual interplanetary missions from Earth
- **Key Questions Answered**: 
  - "When's the next good launch window?"
  - "How much fuel (delta-v) will this mission need?"  
  - "How long until my crew gets there?"
  - "Is this launch date optimal or should I wait?"

### Dynamic Visualization (KSP-style)
- **Real-time orbital mechanics**: As user scrolls through calendar days, planets move in their orbits
- **Transfer ellipse updates**: Shape changes dynamically based on planetary alignment
- **Visual window feedback**: User can SEE good vs poor transfer windows
- **Burn direction updates**: Arrow rotates as optimal departure angle changes
- **Response time**: <50ms for all visual updates

### Display Philosophy
- NO loading animations, but DO animate orbital positions during date changes
- NO splash screens or unnecessary delays
- Use default Flipper system font only
- Visual feedback takes priority over pure minimalism

### Calculation Engine
- Account for orbital eccentricity (affects timing by weeks)
- Hohmann transfer calculations with Oberth effect
- Real planetary positions based on calendar date
- Epoch: January 1, 2024

### User Experience - Mission Planner
1. Boot directly to current Earth → Mars transfer window
2. **Scroll through time**: LEFT/RIGHT changes launch date, watch planets move
3. **Compare destinations**: UP/DOWN switches target planets
4. **Detailed analysis**: OK cycles through orbit/data/phase views
5. **Visual confirmation**: User can see optimal windows without reading numbers

## Key Design Decisions

### Visual Rendering Approach
- **Graphics Method**: Use Flipper's built-in graphics primitives only
- **Orbit Visualization**: Simple circles at different scales/fills/strokes
- **Screen Size Optimization**: Super simplified for small display
- **No Pre-calculation**: Real-time rendering using basic shapes

### Physics Accuracy Requirements
- **Orbital Mechanics**: Include Oberth effect for delta-v calculations (burn efficiency matters)
- **Planetary Orbits**: Account for orbital eccentricity (Mars 0.09 affects timing by weeks)
- **No Value Judgments**: Display raw data only - no GOOD/FAIR/POOR assessments
- **User Decision Making**: Let users interpret the numbers themselves

### Implementation Notes

#### Verified Hohmann Transfer Calculations
```c
// Standard gravitational parameter (m³/s²)
#define MU_SUN 1.32712440018e20

// Hohmann transfer delta-v calculation
double hohmann_delta_v(double r1, double r2) {
    // First burn: enter transfer orbit
    double v1 = sqrt(MU_SUN / r1);
    double vt1 = sqrt(MU_SUN / r1) * sqrt(2.0 * r2 / (r1 + r2));
    double delta_v1 = vt1 - v1;
    
    // Second burn: circularize at destination
    double v2 = sqrt(MU_SUN / r2);
    double vt2 = sqrt(MU_SUN / r2) * sqrt(2.0 * r1 / (r1 + r2));
    double delta_v2 = v2 - vt2;
    
    return fabs(delta_v1) + fabs(delta_v2);
}

// Transfer time (half period of elliptical orbit)
double transfer_time(double r1, double r2) {
    double a_transfer = (r1 + r2) / 2.0;
    return M_PI * sqrt(pow(a_transfer, 3) / MU_SUN);
}
```

#### Oberth Effect Implementation
```c
// Oberth effect efficiency gain
double oberth_gain(double v_initial, double delta_v) {
    // Energy change is proportional to initial velocity
    // More efficient burns at higher orbital speeds
    double kinetic_initial = 0.5 * pow(v_initial, 2);
    double kinetic_final = 0.5 * pow(v_initial + delta_v, 2);
    double energy_gain = kinetic_final - kinetic_initial;
    
    // Compare to energy gain at zero velocity
    double reference_gain = 0.5 * pow(delta_v, 2);
    
    return energy_gain / reference_gain;
}
```

#### Expanded Destination Database (NASA sources)
```c
typedef struct {
    const char* name;
    double period_days;
    double semi_major_au;       // Or km for moons
    double eccentricity;
    double start_angle_deg;
    int parent_body;           // -1 for planets, planet_id for moons
    bool is_moon;
} CelestialBody;

CelestialBody destinations[] = {
    // Planets
    {"MERCURY", 88,   0.387, 0.2056, 0,   -1, false},
    {"VENUS",   225,  0.723, 0.0068, 45,  -1, false},
    {"MARS",    687,  1.524, 0.0934, 30,  -1, false},
    {"JUPITER", 4333, 5.203, 0.0484, 120, -1, false},
    
    // Earth's Moon
    {"MOON",    27.3, 384400, 0.0549, 0,  2, true},  // km from Earth
    
    // Jupiter's Major Moons
    {"EUROPA",  3.55, 671034, 0.009,  90,  4, true}, // km from Jupiter
    {"TITAN",   15.9, 1221830, 0.0288, 120, 5, true}, // Actually Saturn's
    {"GANYMEDE", 7.15, 1070400, 0.0013, 45, 4, true},
    {"IO",      1.77, 421700, 0.0041, 0,   4, true},
    
    // Add more as needed: Enceladus, Callisto, etc.
};
```

#### Hierarchical Transfer Calculations
- **Planet-to-Planet**: Standard Hohmann transfers
- **Earth-to-Moon**: Lunar transfer orbits
- **Planet-to-Moon**: Two-stage transfers (planet intercept + moon capture)
- **Visual Hierarchy**: Show moon orbits around parent planets

### Dynamic Visualization Behaviors
- **Planetary motion**: As launch date changes, planets move to correct orbital positions
- **Transfer ellipse**: Shape updates in real-time based on planetary alignment
  - **Optimal windows**: Clean, efficient ellipse
  - **Poor windows**: Stretched, high-energy ellipse
  - **Impossible**: "NO PATH" when alignment is too poor
- **Burn direction**: Arrow (→ ↗ ↑ ↖ ← ↙ ↓ ↘) rotates as departure angle changes
- **Visual feedback precedence**: User should see good windows before reading numbers

### Example Visual Progression (scrolling through days)
```
Day 120: Mars far ahead  → Long curved ellipse, 8.2 km/s, 340d
Day 147: Optimal window  → Clean ellipse, 5.6 km/s, 259d  
Day 180: Mars closer     → Short ellipse, 6.1 km/s, 210d
Day 200: Poor alignment  → "NO PATH" or "IMPRACTICAL"
```

### Critical Behaviors
- **Date wrapping**: Day 366 → Day 1
- **Impossible transfers**: Show "NO PATH" when planets badly aligned
- **High delta-v** (>15 km/s): Show "IMPRACTICAL"
- **Visual update speed**: All orbital position changes <50ms
- **Angles**: Use arrow symbols (→ ↗ ↑ etc) for burn direction

### Display Format Rules
- **Delta-v**: Always one decimal (5.6 km/s)
- **Flight time**: No decimals (259d)
- **Phase angles**: No decimals (44°)
- **No quality judgments**: Raw data only, user interprets

## Testing Checkpoints

### Static Calculations
1. **Earth → Mars on day 147**: ~259 day flight time, ~5.6 km/s total delta-v
2. **Formula verification**: Match verified Hohmann transfer equations
3. **Planetary positions**: Accurate orbital mechanics for given date

### Dynamic Visualization Testing  
4. **Smooth planetary motion**: As date scrolls, planets move to correct positions
5. **Transfer ellipse updates**: Shape changes reflect actual orbital mechanics
6. **Visual window identification**: User can identify optimal windows by sight
7. **Burn direction updates**: Arrow rotates correctly with changing geometry

### Performance Requirements
8. **Response time**: All visual updates <50ms during date scrolling
9. **Memory stability**: No leaks during extended date scrolling
10. **Edge case handling**: Graceful "NO PATH" for impossible transfers

### User Experience Validation
11. **Mission planning workflow**: Can user find optimal launch windows visually?
12. **KSP-like experience**: Does it feel like Kerbal Space Program's transfer planner?
13. **Immediate understanding**: Visual feedback clear without reading numbers

## Code Style

- Single responsibility functions
- No global state except current view/selection
- Defensive programming for edge cases
- Comments only where physics isn't obvious

## File Structure
```
space_travel_calculator/
├── space_travel_calculator.c
├── calculations.h      // Orbital mechanics
├── display.h          // Screen rendering
└── manifest.yml       // Flipper app manifest
```

## Build Commands
```bash
# Build
./fbt fap_space_travel_calculator

# Deploy to Flipper
./fbt launch_app APPSRC=applications_user/space_travel_calculator
```

## Implementation Priority Order

### Phase 1: Core Calculations
1. Hohmann transfer delta-v and timing calculations
2. Planetary position calculations for any given day
3. Basic display framework with three views

### Phase 2: Static Visualization  
4. Draw basic orbit diagram (circles, dots, lines)
5. Show transfer ellipse for current date
6. Display burn direction arrow

### Phase 3: Dynamic Visualization (Critical)
7. **Real-time planetary motion** as date changes
8. **Transfer ellipse updates** with orbital geometry
9. **Visual optimization indicators** (ellipse shape quality)
10. **Performance optimization** for <50ms updates

## Common Pitfalls to Avoid

1. **Don't skip the dynamic visualization** - this is the key differentiator
2. **Don't animate unnecessarily** - only animate orbital positions during date changes  
3. **Don't optimize calculations prematurely** - focus on visual responsiveness first
4. **Don't add imperial units** - this is space, use metric
5. **Don't add sound effects** - space is silent
6. **Don't lose sight of mission planning focus** - user needs practical transfer windows

Remember: This tool replicates KSP's transfer window planner for real mission planning. The visual feedback is as important as the calculations.