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

The database is large, about 8.9 MB. Installing or updating the firmware is much faster without this database. You can find the bin file [here](https://github.com/lrehmann/fcc-id-lookup-flipper/blob/main/files/fcc_freq_v2.bin) place it in `/ext/apps_assets/fcc_id_lookup` to install the database.
