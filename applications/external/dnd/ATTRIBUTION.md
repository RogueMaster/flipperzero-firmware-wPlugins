# Rules attribution and implementation references

This work includes material from the System Reference Document 5.2.1 ("SRD 5.2.1") by Wizards of the Coast LLC, available at https://www.dndbeyond.com/srd. SRD 5.2.1 is licensed under the Creative Commons Attribution 4.0 International License: https://creativecommons.org/licenses/by/4.0/legalcode.

Dungeons & Dolphins provides compact tracking, calculations and reference data rather than reproducing the complete SRD. Expansion catalogs should contain public option names/metadata only unless the contributor has redistribution rights for additional text.

Automatic XP floors use the SRD/Basic Rules Character Advancement XP thresholds from 0 XP at level 1 through 355,000 XP at level 20.

## Flipper persistence references reviewed

The project's storage design was informed by common Flipper Zero application patterns, including:

- FlipperTasks: https://github.com/MadLadSquad/FlipperTasks
- FlipNote: https://github.com/morty517/flipnote
- Flipper Zero Note Application: https://github.com/AdrianN001/Flipper-Zero-Note-Application

The implementation in this project is original. Persistence policy differs by owner: DNDolphins retains character validation/backup behavior and uses bounded eight-record spell/item sidecar paging, DNDAdventure deliberately writes campaign progress/registry/index files directly to one canonical destination, and the standalone Journal/Initiative/Bestiary modules retain their own storage implementations.

The dice animation is original code-drawn behavior. The Flipper Zero Dice project (https://github.com/Ka3u6y6a/flipper-zero-dice) was reviewed as a behavioral reference; its GPL code and image assets were not copied.
