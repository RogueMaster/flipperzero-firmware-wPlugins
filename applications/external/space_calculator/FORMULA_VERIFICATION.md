# Formula Verification Summary

This document confirms all physics and orbital mechanics formulas used in the Flipper Space Calculators have been verified against authoritative sources.

## Space Travel Calculator - Orbital Mechanics ✅

### Hohmann Transfer Delta-V Formula (Verified)
**Source**: Multiple academic sources, NASA planetary fact sheets, MIT orbital mechanics lectures

**Formula**: 
```
Δv_total = |√(μ/r₁) × (√(2r₂/(r₁+r₂)) - 1)| + |√(μ/r₂) × (1 - √(2r₁/(r₁+r₂)))|
```

**Components**:
- μ = Standard gravitational parameter of Sun = 1.32712440018×10²⁰ m³/s²
- r₁ = Initial orbital radius
- r₂ = Final orbital radius

### Transfer Time Formula (Verified)
**Formula**: `t = π√(a³/μ)` where `a = (r₁ + r₂)/2`

### Oberth Effect (Verified)
**Principle**: Burns are more efficient at higher orbital velocities
**Formula**: Energy gain = `½m(v + Δv)² - ½mv²` = `mvΔv + ½m(Δv)²`
**Key insight**: The `mvΔv` term makes high-speed burns more effective

### Planetary Orbital Data (NASA Verified)
| Planet  | Period (days) | Semi-major (AU) | Eccentricity | Source |
|---------|---------------|-----------------|--------------|---------|
| Mercury | 88           | 0.387           | 0.2056       | NASA    |
| Venus   | 225          | 0.723           | 0.0068       | NASA    |
| Earth   | 365          | 1.000           | 0.0167       | NASA    |
| Mars    | 687          | 1.524           | 0.0934       | NASA    |
| Jupiter | 4333         | 5.203           | 0.0484       | NASA    |

**Note**: Eccentricity values critical for accuracy - Mars's 0.093 can shift transfer windows by weeks.

## Dilate Calculator - Special Relativity ✅

### Lorentz Factor (Verified)
**Source**: Einstein's Special Relativity, experimentally verified multiple times

**Formula**: `γ = 1/√(1 - v²/c²)`

### Time Dilation Formula (Verified)
**Formula**: `Δt' = γΔt`
- Δt = Proper time (traveler experiences)
- Δt' = Coordinate time (Earth observer measures)

### Experimental Verification
- **Hafele-Keating (1971)**: Atomic clocks on commercial flights
- **Frisch-Smith (1962)**: Cosmic ray muons at 0.995c showed 8.8±0.8 dilation factor
- **GPS System**: Daily relativistic corrections prevent 10km positioning errors
- **Particle accelerators**: Routine confirmation at CERN, LHC

### Test Values (Verified)
| Velocity | Lorentz Factor | 1 year traveler → Earth time |
|----------|----------------|-------------------------------|
| 0.866c   | 2.0 (exact)    | 2.0 years                    |
| 0.90c    | 2.294          | 2.3 years                    |
| 0.999c   | 22.366         | 22.4 years                   |

## Edge Cases Handled
1. **Space Travel**: v = c → Show "UNDEFINED", impossible transfers → "NO PATH"
2. **Dilate**: v ≥ c → Show "UNDEFINED", cap at 0.999c for practical use

## Simplifications Applied
- **Circular orbits**: Real orbits are elliptical, but circular approximation is standard for mission planning
- **Patched conic**: Ignores gravitational influences between planets (acceptable for preliminary calculations)
- **No atmospheric effects**: Assumes vacuum space operations
- **Instantaneous burns**: Real burns take time, but approximation is valid for high-thrust engines

## Sources
- NASA Planetary Fact Sheets (nssdc.gsfc.nasa.gov)
- MIT OpenCourseWare: Orbital Mechanics
- Wikipedia: Hohmann Transfer, Time Dilation, Lorentz Factor
- Experimental papers: Hafele-Keating, Frisch-Smith, Bailey et al.
- Textbook references: "Orbital Mechanics for Engineering Students" approach

---

**Verification Status**: ✅ All formulas confirmed accurate for their intended use cases.
**Implementation Ready**: Both calculators use physics-correct, experimentally verified equations.