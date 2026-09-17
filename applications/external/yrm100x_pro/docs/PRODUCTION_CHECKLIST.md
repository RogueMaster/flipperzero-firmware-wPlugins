# Production and catalog checklist

This checklist separates application readiness from the later Flipper
Application Catalog pull request.

## Already prepared in this repository

- Public GitHub source repository.
- MIT license permitting source and binary distribution.
- Root application.fam with app ID, display name, GPIO category, author,
  project URL, version, description, and icon.
- uFBT-compatible source and a GitHub Actions build/lint workflow.
- Detailed README covering hardware, every user-facing menu, cloning behavior,
  storage, and safety, plus a catalog-safe description using only the supported
  Markdown subset.
- Versioned CHANGELOG.md.
- Strict 10 x 10 pixel, 1-bit app.png icon.
- Catalog-ready short description in docs/CATALOG.md.
- Exact five-wire hardware guide with a vector/raster connection diagram and
  three photographs of the tested Flipper, protoboard, reader, and antenna.
- Screenshot capture instructions and stable filenames.
- Catalog manifest template in docs/catalog-manifest.example.yml.

## Hardware acceptance test

Version 1.7 has been tested successfully on physical Flipper Zero hardware by
the maintainer. Keep this list for regression testing future catalog updates.

- Start with the antenna connected; verify both connection-attempt text and the
  reader-information OK screen.
- Start without the antenna; verify Antenna Not Connected and blocked RF actions.
- Reconnect through Configure and query About Antenna.
- Exercise all five read modes and stop each active scan with both OK and Back.
- Let Read Forever complete several cycles; verify countdown, history, and X/Y.
- Save, inspect, rename, write, clone, and delete test dumps.
- Test normal Clone between compatible PC3000 tags.
- Test PC3000 to PC3400 and PC3400 to PC3000 conversion on authorized
  rewritable tags.
- Run Test UMI Auto on every tag model intended to be advertised as tested.
- Verify successful Clone and intentional failure cases with Clone Attempts set
  to 1 and 5.
- Check Get Size Bank and Check TAG Rewritable on expendable test tags.
- Verify Configure values persist after application and device restarts.
- Run the app while qFlipper Remote Control is connected and confirm sufficient
  free RAM after opening and leaving every large submenu.
- Leave the app running through an extended read/write session and confirm that
  free RAM stabilizes rather than decreasing on every repeated operation.

## Catalog screenshots

- Eight original 512 x 256 qFlipper PNG files are included using the stable
  names from screenshots/README.md.
- The files were copied without editing their resolution or format.
- Keep 01-main-menu.png first because the first manifest image is the catalog
  preview.
- Check that no screenshot exposes real customer EPCs, passwords, or private
  device information.
- The screenshots must remain in the same source commit that will be submitted.

## Release source preparation

- Confirm that application.fam, the About screen, README, CHANGELOG, and the FAP
  filename all show the same major.minor version.
- Run ufbt lint and a clean ufbt build against the latest supported official
  release SDK.
- Install the newly built FAP on physical hardware and repeat the acceptance
  smoke test.
- Review git diff for temporary diagnostics, generated files, credentials,
  absolute local paths, test dumps, and unrelated artifacts.
- Commit all source, documentation, icon, and screenshot files.
- Push that exact commit to the public default branch.
- Optionally create a matching GitHub tag and release with the versioned FAP and
  checksum. The catalog itself builds from source, so the release binary does
  not replace the required commit SHA.

## Catalog pull request

- Fork flipperdevices/flipper-application-catalog.
- Create a branch such as AlexeySmirnov74/yrm100x_pro_1.7.
- Copy docs/catalog-manifest.example.yml to
  applications/GPIO/yrm100x_pro/manifest.yml in the catalog fork.
- Replace REPLACE_WITH_FULL_RELEASE_COMMIT_SHA with the complete 40-character
  commit SHA from the public YRM100X_PRO repository.
- Remove any screenshot rows that were not captured; every referenced file must
  exist in the selected source commit.
- Validate the manifest using the catalog tools/bundle.py workflow.
- Open a pull request, complete its template, and monitor build and moderation
  feedback until merge.

Do not create the final catalog manifest before the source and screenshots are
committed: changing the source afterward changes the required commit SHA.
