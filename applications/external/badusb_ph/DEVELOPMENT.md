# badusb_ph — fork structure & baseline

This repo is a **standalone fork** of RogueMaster firmware's BadUSB app, adding
`[placeholder]` substitution + per-payload configs, then made **firmware-agnostic** so
it builds as an external FAP for both official and Unleashed/RogueMaster firmware. For
what the app does, the feature breakdown by origin, and the build process, see
**[`README.md`](README.md)**.

## Branches

- **`main`** — the app: RogueMaster baseline + the badusb_ph changes + the portability
  work. Default branch; what the catalog points at.
- **`vendored-rm-baseline`** — a verbatim copy of RM's `applications/main/bad_usb/` at a
  pinned commit. **Never commit here.** (The firmware-shipped `resources/` asset tree —
  keyboard layouts, demo payloads — is omitted; it's deployed by the firmware, not the FAP.)

```sh
git diff vendored-rm-baseline..main        # exactly the badusb_ph changes vs stock RM BadUSB
git diff --stat vendored-rm-baseline..main
```

## Why this isn't a normal GitHub fork

RogueMaster's BadUSB has **no standalone upstream repo** — it lives only inside the
`flipperzero-firmware-wPlugins` firmware tree at `applications/main/bad_usb/`. So there's
no repo to click "Fork" on and no `upstream` git remote; the original is **vendored** onto
`vendored-rm-baseline`.

**Current baseline:** RogueMaster `flipperzero-firmware-wPlugins` commit **`be48837`**
(`RM0620-0008-0.420.0`, API 87.x).

## Updating the baseline from a newer RogueMaster release

Because there's no git upstream remote, "pull updates" means **re-vendoring** the folder
from the firmware repo onto `vendored-rm-baseline`, then replaying `main` on top:

```sh
# 1. Grab just bad_usb from the RM firmware at the new tag/commit (sparse, no full clone)
git clone --depth 1 --filter=blob:none --sparse \
    https://github.com/RogueMaster/flipperzero-firmware-wPlugins.git /tmp/rm-fw
cd /tmp/rm-fw && git sparse-checkout set applications/main/bad_usb && NEW=$(git rev-parse --short HEAD)

# 2. Refresh the baseline from it (drop the firmware-only resources/ tree)
cd /path/to/this/repo
git checkout vendored-rm-baseline
rsync -a --delete --exclude='.git/' --exclude='resources/' \
    /tmp/rm-fw/applications/main/bad_usb/ ./
git commit -am "Re-vendor RogueMaster bad_usb @ $NEW"
git push origin vendored-rm-baseline

# 3. Replay your changes on the new baseline
git checkout main
git rebase vendored-rm-baseline            # resolve conflicts (RM-API changes land here)
git push --force-with-lease origin main
```

Update the "Current baseline" commit above when you do this. Use
`git merge vendored-rm-baseline` instead of `rebase` if you'd rather keep a merge commit
and not force-push.

**Never commit on `vendored-rm-baseline`** — keeping it a verbatim RM snapshot is what
makes `git diff vendored-rm-baseline..main` an exact picture of the fork.

## Building both targets

See README.md. In short: `ufbt` builds against whatever SDK channel your `~/.ufbt` is set
to; build against the **official** release SDK (lowest API minor) for the universal binary
that also loads on Unleashed/RM. Both targets are verified to compile from this one source.
