# Catalog policy

Bundled catalogs are read-only app assets; user-owned/custom data lives in app data.

- Character option catalogs provide compact names/metadata suitable for offline selection.
- Owned spells/items are copied into character sidecars and then treated as character state.
- Bestiary bundled monsters, user custom monsters and installed monster packs remain separate sources.
- Default custom Dolphin/Capybara assets are seed material only. They are copied only for a fresh custom-monster state and are never merged over user data.
- Campaigns remain declarative scene/choice files. Ghost Protocol is fictional defensive-audit content and intentionally avoids real-world exploit instructions.
- Text pack formats stay editable; do not add checksum rejection.
- Do not duplicate the same owned catalog in multiple FAP asset trees unless runtime ownership requires it.
