# Changelog


## [v1.4] – 30-03-2026

## Added

- **Structured Mode (NEW)**
  - UID modeling using segmented generation (Facility + Card)
  - Tail patterns for better bruteforce efficiency

- **Structured Randomization Option**
  - Switch between:
    - Sequential structured (default)
    - Randomized structured (anti pattern detection & wide coverage)

- **Engine Upgrade**
  - Prefix × Sequence × Tail generation
  - Better coverage of UID distributions
  - More efficient brute force lists

- General improvements & code cleanup


## [v1.3] – 01-02-2026

### Added
- **Reverse sequential generation**
  - Sequential IDs can now be generated in reverse order
  - Safe underflow handling to prevent early termination

- **Collision fuzzing**
  - Optional collision reuse during fuzzing
  - Configurable collision rate
  - Collision pool sampling for reader behavior and stress testing

- **Per-prefix sequential distribution**
  - Sequential generation can optionally distribute IDs evenly across enabled prefixes
  - Ensures balanced coverage when multiple prefixes are selected

- **Improved fuzzing controls**
  - Prefix preservation now works correctly with boundary and bit-flip mutations

### Changed
- Sequential generation logic refactored for correctness and stability
- Prefix handling unified across Random, Sequential, and Fuzz modes
- Output generation stabilized across all modes and configurations

### Fixed
- Fixed prefix overwrite issues during fuzzing
- Fixed edge cases producing empty or duplicated lists
- Improved generation stability





## [v1.2] – 26-01-2026

### Added
- **Multiple generation modes**
  - **Random** – Fully randomized UID generation
  - **Sequential** – Ordered UID generation with configurable start value and step size
  - **Fuzz** – Boundary and mutation based UID generation for reader testing

- **Fuzzing / bit mutation engine**
  - Boundary UID generation (all zero, all one, alternating bit patterns)
  - Random bit flip mutation
  - Adjustable number of flipped bits
  - Optional prefix preservation during fuzzing

- **Sequential generation controls**
  - Custom start value
  - Custom step size
  - Optional per-prefix independent counters

- **Enhanced prefix system**
    - Prefixes apply consistently across Random, Sequential, and Fuzz modes
    - Automatic fallback to pure random generation when no prefix is selected

- **UI & navigation**
    - Dedicated submenus for Sequential and Fuzz modes

### Changed
- Output filenames now include protocol, generation mode, and selected prefixes

### Fixed
- Improved generation stability and thread cleanup
- Prevented UI freezes during long generation runs



## [v1.1] - 24-01-2026
### Added
- Improved fast scrolling when holding navigation buttons
- Faster protocol selection without skipping entries
- Faster ID count adjustment when holding Left/Right



## [v1.0] - 27-12-2025
### Added
- Initial public release
