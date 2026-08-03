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
gh release create v0.2 --generate-notes dist/nfc_canary.fap
```

Attaching the `.fap` lets people install without a toolchain.

## Submitting to the Flipper Apps Catalog

The catalog hosts only a manifest pointing at this repo — never the source.

1. **Make this repository public.** The catalog accepts public GitHub repos
   only.
2. **Screenshots.** Already captured — but note there are two sets, kept
   deliberately:

   | Location | Size | Purpose |
   |---|---|---|
   | `docs/img/` | 512×256 RGB | README display (qFlipper's export, orange on black) |
   | `catalog/screenshots/` | 128×64 1-bit | Catalog submission (true panel resolution) |

   Recent qFlipper versions export at 4× with the orange UI color baked in,
   rather than the panel's native 128×64. Since that upscale is clean
   nearest-neighbour with exactly two colors, `catalog/screenshots/` was
   derived from it losslessly — verified pixel-for-pixel, zero mismatches.

   If the catalog reviewer prefers qFlipper's raw export, point the manifest at
   `docs/img/` instead; both are faithful, they differ only in scale and color.

   To recapture: launch the app, navigate to each screen on the device, and use
   qFlipper's screenshot button. The set is Status (armed), Status (mid-alarm —
   the inverted banner, and the shot that sells the app), Event log with
   entries, and Settings.
3. **Fill in `catalog/manifest.yml`** — set `commit_sha` to the released
   commit and confirm the screenshot paths.
4. **Fork** `flipperdevices/flipper-application-catalog`, add the manifest at
   `applications/NFC/nfc_canary/manifest.yml`, and open a PR.

Expect review in ~1–2 business days. If they request metadata or code changes
and get no response within 14 days, the app is removed — so watch the PR.

### Requirements worth re-checking

- Open source license permitting binary distribution (MIT — satisfied)
- Icon is 10×10px **1-bit** PNG (`nfc_canary.png` — satisfied). Verify with:
  ```bash
  python3 -c "import struct;d=open('nfc_canary.png','rb').read();\
  print(struct.unpack('>II',d[16:24]), 'depth',d[24], 'color',d[25])"
  # want: (10, 10) depth 1 color 0
  ```
  The ufbt scaffold generates an 8-bit RGB placeholder, which is the wrong
  format even though the dimensions look right.
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
