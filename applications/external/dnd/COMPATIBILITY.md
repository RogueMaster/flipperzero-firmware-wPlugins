# Compatibility

- Character files load best-effort by recognized field name. Unknown fields and unrelated malformed values are ignored when possible.
- Inventory, Spellbook, Feature and applied-grant sidecars are authoritative under `/ext/apps_data/dndolphins/`; splitting Inventory and Spellbook into separate FAPs does not move or rename those live files.
- Companion character selection comes only from `custom_active_profile.txt` (`Active=<id>`); launch arguments do not override it and companions never scan for a replacement character. All seven FAPs use `dnd_profile_handoff.*` for this persisted active-ID/exact-profile contract. Inventory, Spellbook, Adventure and Journal require the exact persisted character to load; Initiative and Bestiary use ID `0` only when the metadata itself is absent or unreadable, and an existing stale `Active=<id>` is not replaced by 0.
- Historical SWD/SHD files are not used as live collection/character state.
- Legacy embedded Feature/Grant character fields are tolerated but ignored and are not migrated into sidecars.
- Journal, Adventure, Initiative and Bestiary keep independent storage ownership except for explicit character-sidecar bridges. Inventory, Spellbook and Adventure use narrow streamed profile projections; this changes only resident access, not the canonical character format or compatibility contract.
- Companion main-screen Back-to-parent handoff is best-effort: Short Back launches DNDolphins only when `/ext/apps/Games/dndolphins.fap` exists and passes a small non-persistent focus hint so the corresponding DNDolphins home row is selected; otherwise the companion exits normally. Hold Back always exits to firmware and never launches the parent. The focus hint changes no save data and is not a character selector.
- Pack text is editable and has no checksum requirement.
- The default Dolphin/Capybara custom seed runs only when no user custom monster files exist, so upgrades do not replace an existing custom pack.
- Ghost Protocol uses the existing campaign schema and therefore requires no progress-file conversion.
- Save structure changes are avoided unless new information truly must be persisted. Campaign variables are added only when a future campaign feature genuinely requires new persisted state.
- Inventory grant markers use: `InitialInventory=1` is the normal granted state; `InitialInventory=2` means the optional one-time regrant override has already been consumed. State `1` is valid for existing sidecars.
- Combat Ritual Adept derives from Spellbook fields (`Known`, `Ritual`, `Level`, `SourceClass`).
- Companion main-screen profile badges never render the internal `UINT32_MAX` sentinel as `[4294967295]`; the sentinel is internal and character ID `0` is valid.

- Spellbook ordering uses the current record schema. Valid `S|` records may be rewritten into level/name order while preserving fields and non-record metadata.

## Initiative history and Inventory compatibility

Completed Initiative history is additive app-owned data; existing Initiative live-combat files remain compatible. Inventory keeps the existing Item record schema: Stack Quantity reuses `quantity`, ammunition reuses existing ammo fields, and container deletion remaps the existing logical `container_index` during transactional rewrite. No migration is required.
