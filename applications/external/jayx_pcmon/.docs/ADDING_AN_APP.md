# Adding a new app

Each app lives in its own folder under `apps/`, with its own `application.fam` manifest. `ufbt` builds whichever app folder you're currently in — it does not build the whole repo at once.

## Scaffold

```sh
mkdir apps/<name>
cd apps/<name>
ufbt vscode_dist create APPID=<name>
```

This generates:

- `application.fam` — manifest (edit `appid`, `name`, `fap_category`, etc.)
- `<name>.c` — entry point
- `<name>.png` — 10x10 1-bit icon
- `images/` — additional icon assets
- `.vscode/`, `.clang-format`, `.clangd`, `.editorconfig` — editor config (gitignored, regenerate anytime with `ufbt vscode_dist`)

## Build / flash

Run from inside the app's folder:

```sh
ufbt              # build -> ./dist/<name>.fap
ufbt launch       # build + install to a connected Flipper
```

See [BUILDING.md](BUILDING.md) for more commands.

## CI

`.github/workflows/build.yml` builds every folder under `apps/` automatically — no changes needed when you add a new one.
