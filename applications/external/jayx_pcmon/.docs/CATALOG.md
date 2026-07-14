# Submitting JAYX to the Flipper Apps Catalog

Based on the official [Contribution Guide](https://github.com/flipperdevices/flipper-application-catalog/blob/main/documentation/Contributing.md).

## Checklist (source repo — this project)

| Requirement | Status |
| --- | --- |
| Public GitHub repo | https://github.com/contactjayclatty/JAYX |
| Builds with **ufbt** | `apps/jayx` |
| Open source **LICENSE** | MIT at repo root |
| 10×10 1-bit icon | `apps/jayx/jayx.png` |
| Screenshots (qFlipper) | `apps/jayx/screenshots/` |
| `README.md` in app dir | `apps/jayx/README.md` |
| `changelog.md` | `apps/jayx/changelog.md` |
| `application.fam` metadata | appid, name, category USB, version, author, weburl |
| Catalog `manifest.yml` draft | `docs/catalog/manifest.yml` |

## Steps to open the catalog PR

1. Ensure latest catalog-ready commit is on `main` (see `commit_sha` in `docs/catalog/manifest.yml`).
2. Fork [flipperdevices/flipper-application-catalog](https://github.com/flipperdevices/flipper-application-catalog).
3. Branch: `contactjayclatty/jayx_0.3`
4. Add file:

   `applications/USB/jayx/manifest.yml`

   Copy from `docs/catalog/manifest.yml` in this repo (with real `commit_sha`).

5. Validate (from catalog fork checkout):

   ```powershell
   python -m venv venv
   .\venv\Scripts\Activate.ps1
   pip install -r tools\requirements.txt
   python tools\bundle.py --nolint applications\USB\jayx\manifest.yml bundle.zip
   ```

6. Open a pull request using their template.
7. After merge, app appears in mobile apps and [Flipper Lab](https://lab.flipper.net/apps) within minutes.

## Notes for reviewers / users

- JAYX needs a **Windows companion agent** (`pc_agent/monitor.py --usb`). State this in the PR description.
- **Bluetooth is WIP** — catalog build uses USB path only for real use.
- App source lives in monorepo **subdir** `apps/jayx`.

## Updating after a new release

1. Bump `fap_version` in `apps/jayx/application.fam`
2. Add a section to `apps/jayx/changelog.md`
3. Commit & push
4. PR an updated `manifest.yml` with new `commit_sha` and version (must increase)
