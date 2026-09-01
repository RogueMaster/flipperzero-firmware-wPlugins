# Compatibility

- Character files load best-effort by recognized field name. Unknown fields and unrelated malformed values are ignored when possible.
- Current Inventory, Spellbook, Feature and applied-grant sidecars remain authoritative under `/ext/apps_data/dndolphins/`; splitting Inventory and Spellbook into separate FAPs does not move or rename those live files.
- Companion character selection comes only from `custom_active_profile.txt` (`Active=<id>`); launch arguments do not override it and companions never scan for a replacement character. All seven FAPs use `dnd_profile_handoff.*` for this persisted active-ID/exact-profile contract. Inventory, Spellbook, Adventure and Journal require the exact persisted character to load; Initiative and Bestiary use ID `0` only when the metadata itself is absent or unreadable, and an existing stale `Active=<id>` is not replaced by 0.
- Historical SWD/SHD files are not used as live collection/character state.
- Legacy embedded Feature/Grant character fields are tolerated but ignored and are not migrated into sidecars.
- Journal, Adventure, Initiative and Bestiary keep independent storage ownership except for their explicit character-sidecar bridges.
- Companion main-screen Back-to-parent handoff is best-effort: Short Back launches DNDolphins only when `/ext/apps/Games/dndolphins.fap` exists and passes a small non-persistent focus hint so the corresponding DNDolphins home row is selected; otherwise the companion exits normally. Hold Back always exits to firmware and never launches the parent. The focus hint changes no save data and is not a character selector.
- Pack text is intentionally editable and has no checksum requirement.
- The default Dolphin/Capybara custom seed runs only when no user custom monster files exist, so upgrades do not replace an existing custom pack.
- Ghost Protocol uses the existing campaign schema and therefore requires no progress-file conversion.
- Save structure changes are avoided unless new information truly must be persisted. Campaign variables are added only when a future campaign feature genuinely requires new persisted state.
- Inventory grant markers remain backward compatible: `InitialInventory=1` is the normal granted state; `InitialInventory=2` means the optional one-time regrant override has already been consumed. Older sidecars with state `1` remain valid.
- Combat Ritual Adept uses existing Spellbook fields (`Known`, `Ritual`, `Level`, `SourceClass`) and adds no save-schema field or migration.
- Companion main-screen profile badges never render the internal `UINT32_MAX` sentinel as `[4294967295]`; the sentinel remains an internal lookup value only and valid character ID `0` remains supported.

- Existing Spellbook sidecars require no schema migration for ordering. When opened by Spellbook, valid `S|` records may be rewritten into level/name order while their fields and non-record metadata are preserved.
