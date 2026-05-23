# CK42X PassVault — Community Release Draft

## Best sharing path

### Phase 1 — GitHub + downloadable `.fap`

Create a public source repo, for example:

- `ck42x/flipper-ck42x-passvault`

Recommended repo contents:

- `application.fam`
- `ck42x_passvault.c`
- `ck42x_passvault.png`
- `ck42x_website_bee_crown.png`
- `README.md`
- `changelog.md`
- `LICENSE` — MIT recommended unless Cris wants a different open-source license
- `screenshots/` — qFlipper screenshots, unmodified resolution/format, before app-catalog submission
- GitHub Release asset: `ck42x_passvault.fap`

Suggested topics:

- `flipper-zero`
- `flipper-app`
- `ufbt`
- `fap`
- `password-generator`
- `badusb`
- `ck42x`

## Launch post draft

> I built a small CK42X-branded Flipper Zero external app called **CK42X PassVault**.
>
> It is a field password vault + memorable password generator for the Flipper. You can add an account/username/password, generate readable passwords with a few presets, save entries locally, and then explicitly confirm before the Flipper types only the selected password over USB HID.
>
> Repo / download: `<GitHub repo or release URL>`
>
> Website: https://ck42x.com
>
> Important security note: this is a v0 prototype. Entries are stored in app data as plaintext TSV for now, so do not put high-value real credentials in it yet. The next hardening gates are master unlock/PIN, encrypted storage, edit/delete UI, and better screenshots/docs.
>
> I’m looking for feedback on the UX, safer storage direction, and whether this belongs in the official/community catalog once hardened.

## Official Flipper App Catalog readiness checklist

The official catalog requires a public GitHub source repo; the catalog repo only hosts `manifest.yml` metadata, not source.

Before submitting to `flipperdevices/flipper-application-catalog`:

- [ ] Public GitHub repo exists.
- [ ] Open-source license chosen and committed.
- [ ] App builds with uFBT against current Release/RC firmware.
- [ ] `application.fam` is committed with unique `appid` and incremented `fap_version`.
- [ ] 10x10 1-bit PNG app icon exists.
- [ ] `README.md` explains usage and caveats.
- [ ] `changelog.md` exists.
- [ ] qFlipper screenshots exist and are unmodified.
- [ ] Security caveat is prominent: current v0 stores plaintext TSV.
- [ ] Source repo commit SHA is stable.
- [ ] Catalog `manifest.yml` is created under `applications/<category>/<appid>/`.
- [ ] Catalog manifest validation passes.

## Recommended next hardening before broad catalog push

1. Add delete/edit entries in-app.
2. Add master unlock/PIN gate.
3. Add encrypted vault storage.
4. Add screenshots/demo GIF.
5. Rename public-facing copy carefully so it does not overclaim as a hardened password manager.

## Current local artifact

Built FAP:

```text
/home/x3y5x/flipper-zero-apps/dist/ck42x_passvault.fap
```

Installed on connected HERM Flipper at:

```text
/ext/apps/Tools/ck42x_passvault.fap
```
