# Dilate - Development Guide

## Project Context

Time dilation calculator for Flipper Zero. This is a **velocity-only** time dilation calculator focused on the human experience of relativistic travel. Absolute minimalism is the goal.

## Scope Decision: Single Mode Only

**GRAVITY MODE REMOVED** - After user consultation, gravity-based time dilation was eliminated to keep the app focused on its core use case:

*"I'm traveling to Mars at 0.9c - how much younger will I be when I get there compared to my brother back on Earth?"*

This is about real human scenarios, not abstract physics demonstrations.

## Core Requirements

### Boot Behavior
- Opens directly to velocity calculation
- No splash screen, no mode switching
- Initial state: 0.90c, 1.0 years

### User-Centered Design Philosophy
- **Human Impact Focus**: "What will it FEEL like?" 
- **Rounding for Experience**: Display values humans can relate to
- **Personal Relativity**: Always YOU vs EARTH comparison

### Display Philosophy
- Numbers speak for themselves - no explanations
- Auto-scale units to be meaningful (what will humans feel?)
- Right-align numbers, left-align labels
- No mode switching - single purpose app

## Implementation Notes

### Verified Physics Formulas (Special Relativity)

```c
// Verified Lorentz Factor Formula: γ = 1/√(1 - v²/c²)
double lorentz_factor(double v_over_c) {
    // Prevent division by zero at light speed
    if (v_over_c >= 1.0) return INFINITY;
    return 1.0 / sqrt(1.0 - v_over_c * v_over_c);
}

// Time dilation: Δt' = γΔt (coordinate time = γ × proper time)
double earth_time(double traveler_time, double velocity_c) {
    return traveler_time * lorentz_factor(velocity_c);
}

// Age difference calculation
double age_difference(double traveler_time, double velocity_c) {
    double earth_elapsed = earth_time(traveler_time, velocity_c);
    return earth_elapsed - traveler_time;  // Always positive for v < c
}
```

### Experimental Verification References
- **Hafele-Keating (1971)**: Atomic clocks on aircraft confirmed time dilation
- **Frisch-Smith (1962)**: Cosmic ray muons showed 8.8±0.8 dilation factor at 0.995c
- **GPS satellites**: Daily corrections prevent 10km positioning errors
- **Test values**: 0.866c → γ = 2.0 exactly, 0.999c → γ ≈ 22.4

### User Interface Layout

```
SPEED: 0.90c
TIME:  1.0y
──────────────
YOU:   1.0y
EARTH: 2.3y
DIFF:  +1.3y
```

**Key Interface Decisions:**
- Always show "YOU vs EARTH" comparison
- Auto-scale difference to meaningful units (seconds → minutes → hours → days → years)
- Round values for human comprehension ("what will it feel like?")
- Use + prefix to show Earth experiences MORE time

### Display Precision Rules

1. **Velocity Display**:
   - Below 0.9c: Show as "0.85c"
   - Above 0.9c: Show as "0.95c"
   - Above 0.99c: Show as "0.999c"

2. **Time Difference Auto-scaling**:
   ```c
   if (diff_seconds < 60) show_as_seconds();
   else if (diff_seconds < 3600) show_as_minutes();
   else if (diff_seconds < 86400) show_as_hours();
   else if (diff_seconds < 31536000) show_as_days();
   else show_as_years();
   ```

3. **Always show + or - prefix on differences**

### Button Behavior Details

### Controls (Simplified for Single Mode)

**UP/DOWN - Velocity Adjustment**:
```c
if (current_v < 0.90) step = 0.05;
else if (current_v < 0.99) step = 0.01;
else step = 0.001;
```

**LEFT/RIGHT - Time Duration**:
- Single press: ±0.1 in current unit
- Hold detection: After 500ms, ±10/second
- Bounds: 0.1 to 999.9

**OK Button**: Cycle time units (h → d → y → h)
**BACK Button**: Exit application

### Edge Cases

- At exactly c (1.0): Display "UNDEFINED"
- Above 0.999c: Cap at 0.999c, display "MAX"
- Gravity differences <1s/year: Display "NEGLIGIBLE"
- Time >100 years: Use scientific notation (1.2e2y)

## File Structure
```
dilate/
├── dilate.c
├── relativity.h       // Physics calculations
├── display.h         // Screen rendering
└── manifest.yml      // Flipper app manifest
```

## Testing Checkpoints

Must match these exactly:
1. 0.866c for 1 year → Earth: 2.0 years
2. Mars 365 days → Difference: +59 seconds
3. 0.999c for 1 year → Earth: 22.4 years

## Code Style

- No floating point display - convert to strings with proper precision
- State machine for mode switching
- Separate calculation from display logic
- No dynamic memory allocation

## Common Pitfalls to Avoid

1. Don't explain the physics - users know or they don't care
2. Don't add more modes - two is exactly right
3. Don't smooth animations - instant updates only
4. Don't add history or logging - each calculation stands alone
5. Don't add "fun" locations - keep it scientific

Remember: Someone might use this to plan their relativistic journey. Make the math perfect.