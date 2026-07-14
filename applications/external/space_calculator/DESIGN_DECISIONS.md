# Design Decisions Log

This document captures all key design decisions made during the planning phase of the Flipper Space Calculators project.

## Project Scope

### Initial Vision vs Final Scope
- **Original Brief**: Two apps with complex feature sets including gravity modes, multiple planet locations, assessment systems
- **Final Decision**: Simplified, user-centered approach focusing on core human experiences

## Space Travel Calculator Decisions

### Visual Rendering
- **Decision**: Use Flipper's built-in graphics primitives only
- **Rationale**: Simple circles, lines, and dots are sufficient for the small screen
- **Alternative Rejected**: Pre-calculated pixel arrays for smoother rendering
- **Impact**: Faster development, real-time updates, smaller memory footprint

### Physics Accuracy
- **Decision**: Include Oberth effect in delta-v calculations
- **Rationale**: Burn efficiency significantly affects mission planning accuracy
- **Alternative Rejected**: Simplified constant-thrust approximation
- **Impact**: More complex calculations but essential for real-world accuracy

- **Decision**: Account for orbital eccentricity
- **Rationale**: Mars eccentricity (0.09) can affect transfer windows by weeks
- **Alternative Rejected**: Perfectly circular orbits for simplicity
- **Impact**: More accurate timing, slightly more complex orbital mechanics

### User Interface Philosophy
- **Decision**: Display raw data only, no quality assessments
- **Rationale**: User quote: "just relay the data with no value judgement, the user can figure it out"
- **Alternative Rejected**: GOOD/FAIR/POOR window quality indicators
- **Impact**: Cleaner interface, trusts user expertise

## Dilate Calculator Decisions

### Mode Elimination
- **Decision**: Remove gravity mode entirely, keep velocity-only
- **Rationale**: User feedback emphasized human experience over abstract physics
- **Original Plan**: Two modes (velocity + gravity)
- **Final Plan**: Single velocity-based time dilation calculator
- **User Quote**: "kill gravity mode, keep it simple, velocity / age calculator only plz"

### User-Centered Design
- **Decision**: Always compare "YOU vs EARTH"
- **Rationale**: Addresses real human scenarios like "will my brother be alive when I return?"
- **Alternative Rejected**: Abstract physics demonstrations between arbitrary locations
- **Impact**: More intuitive, personally relevant calculations

### Display Precision
- **Decision**: Round values for human comprehension
- **Rationale**: User quote: "its mostly for humans and their experience right, so rounding is fine, 'whats it gonna FEEL LIKE'"
- **Alternative Rejected**: Precise scientific notation
- **Impact**: More accessible, focuses on experiential impact

## Cross-App Design Principles

### Minimalism Philosophy
- **Consistent Decision**: No animations, explanations, or unnecessary features
- **Response Time**: <50ms for all interactions
- **Display**: Default system font, no fancy typography
- **Units**: Metric only - "this is space"

### User Trust
- **Decision**: Present data without interpretation
- **Rationale**: Users are capable of making their own decisions
- **Impact**: Cleaner interfaces, respect for user intelligence

## Implementation Guidelines

### Graphics Approach
- Built-in primitives only
- Real-time rendering
- Super simplified for small screens

### Physics Requirements
- Include Oberth effect (Space Travel Calculator)
- Account for orbital eccentricity
- Use proper relativistic formulas (Dilate)
- No approximations where accuracy matters

### User Experience
- Direct boot to functionality
- Instant response times
- Human-centered value display
- No value judgments or assessments

---

*These decisions prioritize user needs over technical showcasing, human experience over abstract physics, and practical utility over feature complexity.*