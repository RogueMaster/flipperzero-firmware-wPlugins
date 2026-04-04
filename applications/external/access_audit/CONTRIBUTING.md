# Contributing

## Adding a new card type

Adding support for a new card type touches six files in a fixed order. Follow every step; missing one will leave a silent gap (the type will exist but not score, display, or advise correctly).

### 1. `core/observation.h`: declare the enum value

Add the new `CardType` constant in the appropriate family block, with a comment describing what makes it distinct:

```c
/* MIFARE DESFire sub-types */
CardTypeMifareDesfireEV3,    /* AES + enhanced security */
CardTypeMifareDesfireLight,  /* lightweight DESFire variant */
CardTypeMyNewCard,           /* short description */
```

Keep families grouped. The enum value itself carries no numeric meaning; ordering is for readability only.

---

### 2. `core/observation_provider.c` (or the relevant provider): detect and classify it

Find the function that maps a scanned protocol to a `CardType`. For NFC cards this is `protocol_to_card_type()` or the poller callback that sets `obs->card_type`. For RFID it is `lfrfid_protocol_to_card_type()` in `rfid_provider.c`.

Add a case that sets `obs->card_type = CardTypeMyNewCard` when the card is identified. Also set `obs->metadata_complete`, `obs->uid_present`, and `obs->user_memory_present` as accurately as the available protocol data allows.

```c
case NfcProtocolMyProtocol:
    obs->card_type = CardTypeMyNewCard;
    obs->metadata_complete = true;
    break;
```

For a new protocol that needs its own poller, model it after `iclass_provider.c`; implement `alloc / free / start / stop / poll` and borrow the `Nfc*` from `ObservationProvider` via `observation_provider_get_nfc()`.

---

### 3. `core/rules.c`: classify its risk

Decide which rules should fire for the new type and add it to the relevant function bodies:

**`rule_legacy_family`**: add if the card has no effective crypto:
```c
if(obs->card_type == CardTypeMyNewCard) return true;
```

**`rule_modern_crypto`**: add if the card uses AES or comparable modern crypto:
```c
case CardTypeMyNewCard:
    return true;
```

Do not add a card to both; `legacy_family` and `modern_crypto` are mutually exclusive in the current ruleset.

If neither rule applies (e.g. the card is an unclassified ISO generic), leave it out of both; `uid_no_memory` will still fire if appropriate.

---

### 4. `core/scoring.c`: add a display label

Add a `case` to `card_type_to_string()`:

```c
case CardTypeMyNewCard:
    return "My New Card";
```

The string appears on the result screen and in reports. Keep it short enough to fit the Flipper display (~20 characters).

---

### 5. `core/report.c`: add advice text

Add a `case` to `report_advice()`:

```c
case CardTypeMyNewCard:
    return "Plain-English advice about this card type.";
```

Advice is written to every saved report entry. It should be actionable: what should the operator do? Example: "Replace with DESFire EV2+ using AES auth and key diversification."

If the card type genuinely needs no advice (e.g. it is a strong modern card correctly configured), return `NULL`; the advice line is omitted from the report.

Also add a `case` to `memory_capacity()` if the card has a known user memory size:

```c
case CardTypeMyNewCard:
    return "512 B";
```

---

### 6. `docs/card-types.md`: document it

Add a row to the appropriate family table with:
- Risk label
- Common deployment context
- Recommended action

Update `docs/rules.md` if the new card type changes any rule's trigger list.

---

## Adding a new rule

Rules are pure functions in `core/rules.h` / `core/rules.c`. They take a `const AccessObservation*` and return `bool`.

1. Declare the function in `rules.h`
2. Implement it in `rules.c`
3. Wire it into `score_observation()` in `scoring.c`; choose the severity tier and add/subtract points
4. Add it to the `checks[]` array in `write_card_entry()` in `report.c` so it appears in reports
5. Document it in `docs/rules.md`

Rules must be side-effect free and must handle `obs == NULL` by returning `false` (or `true` for conservative defaults like `rule_incomplete_evidence`).

---

## Code style

- C99, no dynamic allocation in hot paths
- All public functions documented in the header with a one-line description
- Struct zero-init with `(Type){0}` before setting fields
- Mutex acquire/release pairs must be balanced in all code paths
- No `furi_assert` in provider callbacks (NFC worker thread; panics are unrecoverable)
- Prefer early returns over deep nesting

---

## Building

```sh
ufbt          # build FAP
ufbt launch   # build and deploy to connected Flipper (requires USB)
```

CI builds against official release, official dev, Momentum release, and Momentum dev SDKs on every push. All four must pass before merging.

When tagging a release, `release.yml` checks that `fap_version` in `application.fam` matches the tag's MAJOR.MINOR before building. Always bump `fap_version=(X, Y)` to match the intended tag.
