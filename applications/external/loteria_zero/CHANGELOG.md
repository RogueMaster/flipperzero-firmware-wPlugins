# Changelog

All notable changes to **Lotería Zero** are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/).

## [Unreleased]

### Planned
- Flipper Apps Catalog merge for PR #1389

## [1.1.0] - 2026-11-08

### Fixed
- LEFT now walks back through every called card, not only the last one
- History screen lists the full called and pending sets
- OK/Right while browsing history returns to the live card without advancing (avoids “re-calling” a past card like El Gallo)
- Shuffle uniqueness guard so a card cannot appear twice in one game

## [1.0.0] - 2026-11-06

### Added
- First stable release candidate for Flipper Zero (FAP)
- Full game loop: new game, continue, pause, history browse
- History, statistics, settings, and auto-call modes (5 / 10 / 15 s)
- Welcome flow with dedication to APA (skippable)
- "Nuestra historia" menu section
- Persistent save via official Storage / saved_struct APIs
- Vertical card view during play (`ViewOrientationVertical`)
- uFBT build workflow template under `.github/workflows/`

## [0.5.0] - 2026-11-06

### Added
- Historial de cartas cantadas y pendientes
- Estadísticas persistentes
- Modo automático configurable
- Pantalla de fin de partida
- Configuración (omitir bienvenida, velocidad auto)

## [0.2.0] - 2026-11-06

### Added
- Catálogo completo de 54 cartas tradicionales en `cards.json`
- 54 rimas originales
- 54 ilustraciones monocromáticas 1-bit
- Generadores `tools/gen_cards.py` y `tools/gen_card_art.py`
- Pruebas de integridad del catálogo

## [0.1.0] - 2026-11-06

### Added
- MVP: menú principal, motor Fisher-Yates, navegación de escenas
- Welcome and dedication to APA
- `application.fam` y estructura modular del FAP

[Unreleased]: https://github.com/databrainsco/Loteria_Zero/compare/v1.1.0...HEAD
[1.1.0]: https://github.com/databrainsco/Loteria_Zero/compare/v1.0.0...HEAD
[1.0.0]: https://github.com/databrainsco/Loteria_Zero/releases/tag/v1.0.0
[0.5.0]: https://github.com/databrainsco/Loteria_Zero/releases/tag/v0.5.0
[0.2.0]: https://github.com/databrainsco/Loteria_Zero/releases/tag/v0.2.0
[0.1.0]: https://github.com/databrainsco/Loteria_Zero/releases/tag/v0.1.0
