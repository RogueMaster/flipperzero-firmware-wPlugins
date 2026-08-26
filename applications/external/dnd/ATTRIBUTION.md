# Rules attribution

This work includes material from the System Reference Document 5.2.1 ("SRD 5.2.1") by Wizards of the Coast LLC, available at https://www.dndbeyond.com/srd. The SRD 5.2.1 is licensed under the Creative Commons Attribution 4.0 International License, available at https://creativecommons.org/licenses/by/4.0/legalcode.

Dungeons & Dolphins implements compact tracking, dice calculations, encounter budgets, and 330 monster reference records from the SRD; it does not include the complete SRD text or a complete rules database. Ten additional bundled monsters are original Dungeons & Dolphins material.

Expansion catalogs contain public option names only. They do not reproduce spell descriptions, class/subclass/background mechanics, item stat blocks, or other proprietary rules text. Users should consult books they own and enter any needed mechanics in the app's notes and editable fields.

## Persistence references reviewed

The storage design was informed by established Flipper Zero application patterns, including:

- FlipperTasks: https://github.com/MadLadSquad/FlipperTasks
- FlipNote: https://github.com/morty517/flipnote
- Flipper Zero Note Application: https://github.com/AdrianN001/Flipper-Zero-Note-Application

Dungeons & Dolphins' save implementation is original to this project. It uses the native Flipper Storage API with a temporary file, backup rotation, schema version, payload validation, and checksum.

The dice animation is an original code-drawn sequence. The Flipper Zero Dice project (https://github.com/Ka3u6y6a/flipper-zero-dice) was reviewed as a behavioral reference; its GPL code and image assets were not copied.
