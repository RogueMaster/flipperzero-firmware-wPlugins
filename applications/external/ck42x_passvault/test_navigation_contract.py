#!/usr/bin/env python3
"""Source-level regression checks for the PassVault navigation contract."""
from pathlib import Path

source = Path("ck42x_passvault.c").read_text(encoding="utf-8")
manifest = Path("application.fam").read_text(encoding="utf-8")

required = (
    'submenu_set_header(app->submenu, "PāSSVΛŭLƬ")',
    'ck_show_text_input(app, CkInputUnlockPin, "Enter Master PIN", NULL)',
    'dialog_ex_set_right_button_text(app->dialog, "Enter")',
    "app->selected = saved_index;\n        ck_show_entry_widget(app);",
    "if(ok)\n                ck_show_entry_widget(app);",
)
for contract in required:
    assert contract in source, contract

assert 'dialog_ex_set_right_button_text(app->dialog, "Save")' not in source
assert 'fap_version="0.4.5"' in manifest
print("OK: PassVault navigation contract checks passed")
