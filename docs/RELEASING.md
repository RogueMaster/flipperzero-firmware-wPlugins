# Releasing

## Before any release

- [ ] `ufbt` builds clean locally (pinned Unleashed SDK, API 87.8)
- [ ] CI green — this is the one that matters, since it builds against
      **official firmware**. Local builds use a fork SDK and will not catch a
      fork-only API.
- [ ] Deployed and exercised on hardware: alarm fires from a phone in reader
      mode, patterns are distinct, settings persist across a reboot
- [ ] `fap_version` bumped in `application.fam`
- [ ] `changelog.md` entry added, newest first
- [ ] README accurate — in particular, no feature described as working that
      isn't wired up

## Tagging

```bash
git tag -a v0.2 -m "v0.2: tiering, configurable alerts, event log"
git push origin v0.2
gh release create v0.2 --generate-notes dist/nfc_alerter.fap
```

Attaching the `.fap` lets people install without a toolchain.

## Submitting to the Flipper Apps Catalog

The catalog hosts only a manifest pointing at this repo — never the source.

1. **Make this repository public.** The catalog accepts public GitHub repos
   only.
2. **Take screenshots with qFlipper** (Screenshot button). Do not resize or
   re-encode; the catalog rejects modified images. Save to
   `catalog/screenshots/`. Suggested set:
   - Status screen, armed and idle
   - Status screen mid-alarm (the inverted banner)
   - Event log with a few entries
   - Settings
3. **Fill in `catalog/manifest.yml`** — set `commit_sha` to the released
   commit and confirm the screenshot paths.
4. **Fork** `flipperdevices/flipper-application-catalog`, add the manifest at
   `applications/NFC/nfc_alerter/manifest.yml`, and open a PR.

Expect review in ~1–2 business days. If they request metadata or code changes
and get no response within 14 days, the app is removed — so watch the PR.

### Requirements worth re-checking

- Open source license permitting binary distribution (MIT — satisfied)
- Icon is 10×10px 1-bit PNG (`nfc_alerter.png` — satisfied)
- Builds against the latest Release or Release Candidate firmware (CI)
- Unique, incremented `version` for every submission
- No code that bypasses the Flipper's intentional limits

### On content policy

This app is defensive: it tells you that *you* are being scanned. It does not
capture, store, or replay credentials, and the event log deliberately records
only that an interrogation occurred (time, tier, protocol, command) — never
card contents. Keep it that way. The moment it logs what a reader extracts, it
stops being a personal-safety tool.

The same line applies to Decoy mode when it lands: emulating a card to learn
which command a reader issued is diagnostic; harvesting what the reader would
have read is not.
