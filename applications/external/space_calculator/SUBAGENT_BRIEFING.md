# Subagent Implementation Briefing

## Project Status: READY FOR IMPLEMENTATION

### Environment Status ✅
- **uFBT installed**: Working virtual environment
- **Both apps created**: space-travel-calculator, dilate  
- **Build tested**: Both apps compile successfully
- **Quality tools**: Linter and formatter ready

## Implementation Priorities

### Phase 1: Space Travel Calculator (Start Here)
**Why first**: More complex, needs dynamic visualization system

**Core Requirements**:
1. **Hohmann transfer calculations** (formulas verified)
2. **Real-time planetary position updates** as user scrolls dates
3. **KSP-style visual feedback** - ellipse shapes show window quality
4. **Extensive destination support** - planets + major moons
5. **<50ms response time** for all visual updates

**User Story**: Mission planner scrolls through calendar days, watches planets move, identifies optimal launch windows visually before reading numbers.

### Phase 2: Dilate Calculator  
**Why second**: Simpler, focused single-mode app

**Core Requirements**:
1. **Lorentz factor calculations** (formulas verified)
2. **Velocity-only time dilation** (gravity mode removed)
3. **Human-centered display** - "YOU vs EARTH" comparison
4. **Auto-scaling units** - display in most meaningful units

## Critical Implementation Notes

### Physics Accuracy Requirements
- **Space Travel**: Include Oberth effect, orbital eccentricity
- **Dilate**: Use exact Lorentz factor, handle edge cases at light speed
- **All formulas verified**: See FORMULA_VERIFICATION.md

### User Experience Mandates  
- **No splash screens**: Boot directly to functionality
- **No explanations**: Numbers speak for themselves
- **Visual precedence**: User sees good windows before reading data
- **Human-focused**: "What will it FEEL like?" approach

### Technical Constraints
- **Response time**: <50ms for all interactions
- **Memory**: 2KB stack, no dynamic allocation
- **Platform**: Flipper Zero ARM Cortex-M4
- **Graphics**: Built-in primitives only (circles, lines, dots)

## Development Commands Reference

```bash
# Environment setup
source flipper-dev-env/bin/activate

# Work on Space Travel Calculator
cd space-travel-calculator
ufbt                    # Build
ufbt lint_all          # Check code quality  
ufbt format_all        # Format code
ufbt launch            # Deploy to Flipper (if connected)

# Work on Dilate
cd ../dilate
ufbt                    # Build
ufbt lint_all          # Check code quality
ufbt format_all        # Format code
```

## File Structure for Implementation

### Space Travel Calculator
```c
// Main app file: space_travel_calculator.c
#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>

// Required modules:
// - orbital_mechanics.h     // Hohmann transfers, planetary positions
// - visualization.h         // Dynamic orbit diagrams  
// - destinations.h          // Planet/moon database
// - ui_manager.h           // View switching (orbit/data/phase)
```

### Dilate Calculator
```c
// Main app file: dilate.c  
#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>

// Required modules:
// - relativity.h           // Lorentz factor calculations
// - time_display.h         // Unit formatting and scaling
```

## Success Criteria

### Space Travel Calculator
- [ ] User can scroll through calendar and see planets move
- [ ] Transfer ellipse updates in real-time with planetary alignment
- [ ] Visual feedback clearly shows optimal vs poor windows
- [ ] All destinations work: Mars, Venus, Mercury, Jupiter, Moon, Europa, etc.
- [ ] Delta-v calculations include Oberth effect
- [ ] <50ms response time maintained during date scrolling

### Dilate Calculator
- [ ] Accurate time dilation at all relativistic velocities
- [ ] Clear "YOU vs EARTH" age difference display
- [ ] Auto-scaling units (seconds → minutes → hours → days → years)
- [ ] Edge cases handled: v = c shows "UNDEFINED"
- [ ] Human-friendly rounding for experience focus

## Documentation Available

- **CLAUDE.md**: Detailed technical specifications for each app
- **FORMULA_VERIFICATION.md**: All physics formulas verified against sources
- **DEV_WORKFLOW.md**: Mandatory development process
- **DESIGN_DECISIONS.md**: Record of all design choices and rationale

## Ready for Implementation

The development environment is set up, requirements are clearly defined, and both apps have been scoped for real human use cases. Physics is verified, user experience is designed, and quality tools are ready.

**Start with Space Travel Calculator** - it has the more complex requirements and will establish the patterns for the project.

---

*"Make it work, make it clean, make it fast. The universe doesn't approximate."*