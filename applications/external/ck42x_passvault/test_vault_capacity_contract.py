#!/usr/bin/env python3
"""Source-level regression checks for unbounded password vault storage."""
from pathlib import Path

source = Path("ck42x_passvault.c").read_text(encoding="utf-8")
store_header = Path("ck42x_vault_store.h").read_text(encoding="utf-8")
store_impl = Path("ck42x_vault_store.c").read_text(encoding="utf-8")
manifest = Path("application.fam").read_text(encoding="utf-8")

assert "#define CK_MAX_ENTRIES" not in source
assert "#define CK_MAX_ENTRIES" not in store_header
assert "Vault Full" not in source
assert "Max entries reached." not in source
assert "CkVaultStore vault;" in source
assert "ck_vault_store_append(&app->vault, &app->draft)" in source
assert "ck_vault_store_parse(&app->vault, buf)" in source
assert "ck_vault_store_serialize(&app->vault, out, out_len)" in source
assert "CkEventSavedBase = 0x1000" in source
assert "event >= CkEventSavedBase && event < CkFido2ServiceEventPresence" in source
assert "CK_MAX_VAULT_FILE (256U * 1024U)" in source
assert 'fap_version="0.4.6"' in manifest

assert (
    "bool ck_vault_store_reserve(CkVaultStore* store, size_t needed);" in store_header
)
assert (
    "bool ck_vault_store_append(CkVaultStore* store, const CkVaultEntry* entry);"
    in store_header
)
assert "store->capacity ? store->capacity : 8" in store_impl
assert "store->count * CK_LINE_MAX + 1" in store_impl

print("OK: unbounded password vault capacity contract checks passed")
