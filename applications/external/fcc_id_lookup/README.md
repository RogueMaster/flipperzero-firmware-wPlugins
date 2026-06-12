# FCC ID Lookup

Offline FCC ID applicant and wireless frequency lookup for Flipper Zero.

Enter a full FCC ID or any non-empty prefix. Exact matches open directly; prefix
matches show a paginated list. The detail page shows the FCC ID, applicant,
supported frequency ranges, and a direct FCC ID source URL.

Data is sourced from FCCID.io and displayed in-app as:

```text
Data Source:
https://fcc.id/{FCC_ID}
```

Example source page: https://fcc.id/2A2V6-FZ

## RogueMaster build notes

This RogueMaster integration bundles the searchable database as FAP file assets
with `fap_file_assets="files"` and reads it with `APP_ASSETS_PATH(...)`. There is
no local deploy script, no app-catalog `manifest.yml`, and no separate app-data
cache/extraction step in this folder.

The database is large, about 8.9 MB. Installing or updating the firmware package
can take longer while the app asset is copied to the SD card, and the first app
launch after install may take time while Flipper prepares bundled assets. After
the asset is present, searches read the database directly.
