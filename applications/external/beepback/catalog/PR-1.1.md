# Application Submission

BEEPBACK v1.1. Every C source file is unchanged from v1.0, so the app behaves
exactly as the version you already accepted.

The short description was 118 characters, which the mobile app cut off
mid-sentence, so it is now 54 and fits the way the neighbouring entries do.
The long description was the README with nothing removed; it is now about half
as long.

Manifest change is the commit SHA. Version raised from 1.0 to 1.1.

# Extra Requirements

None. No hardware add-ons, no external files, nothing written outside
/ext/apps_data/beepback/.

# Author Checklist (Fill this out)

- [x] I've read the [contribution guidelines](../blob/HEAD/documentation/Contributing.md) and my PR follows them
- [x] I own the code I'm submitting or have code owner's permission to submit it
- [x] I have performed a self-review of my own code
- [x] I have commented my code, particularly in hard-to-understand areas
- [x] I [have validated](../blob/HEAD/documentation/Contributing.md#validating-manifest) the manifest file(s) with `python3 tools/bundle.py --nolint applications/CATEGORY/APPID/manifest.yml bundle.zip`

Validated with the full `tools/bundle.py` rather than `--nolint`, so the
`ufbt lint` stage ran too. Screenshots are unchanged — the same five qFlipper
captures accepted for v1.0.

# AI usage disclosure (Fill this out)

- Fully AI generated - explain what all the generated code does in moderate detail.

Same as v1.0: the game is mine and the C port was written by Claude under my
direction. Nothing compiled into the app changed in this release. The diff is
the two description files, the version number in `application.fam`, some stale
comments deleted from the reference browser build, and a stray `.pyc` removed
from the tree. The 254-check host suite in `test/` still passes, including the
browser-parity stage that drives the original web build and holds it to the
same constants and tables.

# Reviewer Checklist (Don't fill this out, and don't remove it from the template)

- [ ] Bundle is valid
- [ ] There are no obvious issues with the source code
- [ ] I've ran this application and verified its functionality
