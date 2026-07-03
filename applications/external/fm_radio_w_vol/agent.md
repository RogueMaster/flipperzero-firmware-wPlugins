# Repo notes (local AI / developer)

Flipper Zero external app for the **FReD FM** radio board (TEA5767 + PT2257/PT2259 + PAM8406 + RDS on `PA4`).

## Layout (post-refactor)

| Path | Role |
|------|------|
| [application.fam](application.fam) | Flipper app manifest (`appid` `fred_fm`) |
| [src/fred_fm/app/](src/fred_fm/app/) | Entry, alloc/free, view setup |
| [src/fred_fm/core/](src/fred_fm/core/) | Tune, presets, settings, seek |
| [src/fred_fm/ui/](src/fred_fm/ui/) | Views, input, config menu |
| [src/fred_fm/audio/](src/fred_fm/audio/) | PT22xx + PAM8406 output |
| [src/fred_fm/rds/](src/fred_fm/rds/) | RDS pipeline, capture, worker |
| [src/fred_fm/include/](src/fred_fm/include/) | `config.h`, `types.h` |
| [src/drivers/tea5767/](src/drivers/tea5767/) | TEA5767 I2C driver |
| [src/drivers/pt/](src/drivers/pt/) | PT2257 / PT2259 / facade |
| [src/drivers/pam/](src/drivers/pam/) | PAM8406 GPIO |
| [src/drivers/rds/](src/drivers/rds/) | Acquisition, DSP, decoder |

`backups/` is local-only (see `.gitignore`).

## Build / run

```bash
ufbt update && ufbt fap_fred_fm && ufbt launch
```

If `ufbt` is missing: `source .venv/bin/activate` or `pipx install ufbt`.

## Device data (SD)

- `/ext/apps_data/fred_fm/settings.fff`
- `/ext/apps_data/fred_fm/presets.fff`
- RDS debug: `rds_capture_*` under same folder when enabled

## Catalog / PR checklist (Flipper Apps Catalog)

- **Version:** stay on `0.11` until next feature release; update `commit_sha` in catalog manifest after push.
- **Hardware:** app requires FReD FM board (or partial TEA5767-only for FM without full RDS).
- **Tested:** v0.11 verified on retail **FReD FM PCB v1.1** (Tindie) — tune, seek, presets, volume, RDS PS, constellation.
- **Bundle validate:** `python3 tools/bundle.py --nolint applications/GPIO/fred_fm/manifest.yml bundle.zip` (catalog repo; needs `dataclass_wizard`).
- **AI disclosure (Partially AI assisted):** layout refactor (`src/`), module headers, driver comment pass, RDS FIR tap headers, and catalog prep were done with AI assistance under human review. Core RDS DSP algorithms, hardware bring-up, and on-device validation are author-owned. Flipper app entry and driver integration remain GPLv3; see [README.md](README.md) provenance.

## License

GPLv3 — see [LICENSE](LICENSE). Publish matching source for any distributed `.fap` build.
