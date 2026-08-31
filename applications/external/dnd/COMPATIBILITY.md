# Compatibility

- Character files load best-effort by recognized field name. Unknown fields and unrelated malformed values are ignored when possible.
- Current spell/item sidecars remain authoritative; historical SWD/SHD files are not used as live collection/character state.
- Journal, Adventure, Initiative and Bestiary keep independent storage ownership.
- Pack text is intentionally editable and has no checksum requirement.
- The default Dolphin/Capybara custom seed runs only when no user custom monster files exist, so upgrades do not replace an existing custom pack.
- Ghost Protocol uses the existing campaign schema and therefore requires no progress-file conversion.
- Save structure changes are avoided unless new information truly must be persisted.
