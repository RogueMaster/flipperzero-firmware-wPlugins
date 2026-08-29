# Accessibility conventions

- Monochrome selection uses fill, text, symbols and numeric values rather than color alone.
- Important states never rely on sprites alone.
- Long labels scroll or truncate predictably; stored text remains longer than compact display rows where supported.
- Storage-backed character, **eight-record spell/item**, and Journal paging is transparent to the user: moving across a cache boundary preserves normal Up/Down list behavior and does not expose page numbers as part of the record identity.
- Dice results always retain numeric values and individual-result review; animation can be skipped with Back.
- Combat attack-roll mode is shown as text (`Normal`, `Advantage`, or `Disadvantage`) on the main Combat screen rather than being represented only by animation or an implicit key state.
- Long-press actions are paired with visible menu/detail context where practical.
- Each standalone FAP uses the same basic Up/Down/OK/Back interaction model to reduce mode switching.
- DNDInitiative uses short Back for previous turn during combat and hold Back to return to its menu.
- DNDJournal milestone continuation is explicit and does not silently alter Adventure state.

Physical legibility, hold timing, storage-page transitions and maximum-length text cases remain in `DEVICE_TEST_MATRIX.md` until verified on hardware.
