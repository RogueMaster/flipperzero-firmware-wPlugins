# Database sources

The generated Flipper and browser databases use these public upstream sources:

- `postcodes.csv`, `errormasks.csv`, and `oserrors.csv` from
  <https://github.com/XboxOneResearch/errorcodes>
- User-facing E100/E200 documentation from
  <https://github.com/TorusHyperV/XboxOne-EXXX-err-Codes>

The source snapshots were refreshed on 2026-07-23. Run
`python tools/generate_databases.py --site-dir <WebXboxPOSTTool>` after replacing
the files in this directory to regenerate both products.
