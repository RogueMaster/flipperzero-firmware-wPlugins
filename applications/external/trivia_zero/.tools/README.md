# Trivia Zero — Data Pipeline

Off-Flipper Python tooling that builds the question packs the FAP consumes.

## Install (host, one-time)

```
cd tools
python3 -m venv .venv
source .venv/bin/activate
pip install -r requirements-dev.txt
```

## Build the pack

From the repo root:

```
make pack
```

This pulls Open Trivia DB (cached locally), applies the blacklist, maps categories, runs translations through the configured backend (stub by default), and writes:

- `data/trivia_es.tsv` + `data/trivia_es.idx`
- `data/trivia_en.tsv` + `data/trivia_en.idx`

## Translation backends

| `TZ_TRANSLATOR` | Behavior |
|-----------------|----------|
| unset / `stub` (default) | Deterministic stub — appends `[es]`/`[en]` markers. Useful for development; output is valid but not human-grade. |
| `anthropic` | Real translation via Anthropic Haiku. Requires `ANTHROPIC_API_KEY` env var. Cached on disk to `data/_cache/translations.json` so reruns are free. |

## Test

```
make py-test
```

## Lint, format, type-check

```
make py-lint
make py-format
make py-typecheck
```
