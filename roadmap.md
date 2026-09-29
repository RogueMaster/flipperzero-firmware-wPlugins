# Cribbage Calc Roadmap

This roadmap prepares Cribbage Calc for a polished public release and eventual submission to the Flipper Apps Catalog.

## 1. Scoring Confidence

- [ ] Add tests for individual scoring categories: no-score hand, all fifteen combinations, pair royal, four of a kind, nobs, and Ace-low runs.
- [ ] Add tests for duplicated and triplicated run patterns, including double, triple, and double-double runs.
- [ ] Add flush tests for four- and five-card player-hand flushes and the crib's five-card-only flush rule.
- [ ] Add an integration test using a complete 13-card deal and assert the non-dealer, dealer, crib, and his-heels results.
- [ ] Run `make test` after each scoring change and test the canonical 29-point hand on the Flipper.

**Done when:** automated tests cover every scoring category and important multiplicity rule, and the known on-device test deal produces its expected results.

## 2. UI and Device Polish

- [ ] Improve card presentation so selected rank and suit are immediately readable on the 128×64 display.
- [ ] Make all result-screen actions discoverable, including breakdown navigation when his heels is displayed.
- [ ] Refine the results overview and breakdown layouts for readable labels, values, and navigation hints.
- [ ] Improve duplicate-card feedback by clearly identifying the conflicting card and slot.
- [ ] Test the complete workflow on-device: new deal, rank/suit selection, Back navigation, duplicate recovery, score details, and New Deal.
- [ ] Incorporate feedback from real cribbage use before declaring the UI stable.

**Done when:** a first-time user can enter a deal, recover from an accidental duplicate, understand every displayed score, and start another deal without outside instructions.

## 3. Apps Catalog Preparation

- [ ] Choose and add an OSI-approved open-source `LICENSE` that permits binary distribution by the catalog.
- [ ] Create a 10×10, 1-bit PNG app icon and reference it with `fap_icon` in `application.fam`.
- [ ] Capture unmodified qFlipper screenshots of the polished app; include a welcome/entry screen and results screen.
- [ ] Add `changelog.md` with an initial `v0.1` entry; keep it updated for every catalog submission.
- [ ] Finalize README content: app purpose, controls, scoring scope, build instructions, and source references.
- [ ] Review `application.fam` metadata: unique app ID, display name, category, version, description, author, and project URL.
- [ ] Verify a clean `ufbt` build and `ufbt launch` against current Release or Release Candidate firmware.
- [ ] Tag or otherwise record the exact release commit and update `fap_version` for the catalog release.
- [ ] Create the Apps Catalog `manifest.yml` using that commit SHA, add it under the correct catalog application path, validate it, and submit the catalog pull request.

**Done when:** the public repository contains all required assets and metadata, the release builds on the current SDK, and the Apps Catalog pull request is ready for review.

## Recommended Release Order

1. Complete scoring tests.
2. Complete and test UI polish on the device.
3. Add license, icon, screenshots, changelog, and final metadata.
4. Build and test the release candidate with `ufbt`.
5. Commit the release, create the catalog manifest, and open the Apps Catalog pull request.

## References

- [Flipper Apps Catalog contribution guide](https://github.com/flipperdevices/flipper-application-catalog/blob/main/documentation/Contributing.md)
- [Flipper App Development documentation](https://developer.flipper.net/flipperzero/doxygen/applications.html)
- [Bicycle Cribbage rules](https://bicyclecards.com/how-to-play/cribbage)
