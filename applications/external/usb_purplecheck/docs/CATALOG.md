# Flipper application catalog submission

The source repository is https://github.com/RogerF5-Security/USB-PurpleCheck.
The listing is proposed to https://github.com/flipperdevices/flipper-application-catalog
at applications/USB/usb_purplecheck/manifest.yml. Lab lists accepted catalog entries;
uploading a FAP to a local device does not publish the application.

Before submitting:

- Review the official Contributing.md, Manifest.md and PR template at submission time.
- Require a passing build with the latest official release SDK, not only Momentum.
- Complete the pending physical USB acceptance cases in VALIDATION.md.
- Export unedited 512x256, orange/black PNGs using qFlipper for splash, menu and settings.
- Pin sourcecode.location.commit_sha to the full immutable source commit containing
  application.fam, assets, docs and screenshots. Do not pin a moving branch.
- Reference @docs/catalog-description.md and @docs/catalog-changelog.md in the manifest.
- Clone the catalog, place the manifest at the path above, then run:
  python tools/bundle.py --nolint applications/USB/usb_purplecheck/manifest.yml bundle.zip
- Submit one application per PR and complete the author checklist truthfully. Disclose
  AI-generated code under RogerF5's direction and the derived GPL-3.0 USB transport.
  Preserve the reviewer checklist without checking it yourself.

Do not claim publication or hardware acceptance while those steps remain pending.
The source PR's official-release workflow supplies a firmware-specific FAP artifact.
Any locally supplied Momentum FAP is labeled separately in dist/BUILD.json.

The Ukrainian UI deliberately uses UTF-8 literals plus original bitmap glyphs; it does
not rely on a firmware localization patch. English is the fresh-install default.
