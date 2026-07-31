#!/usr/bin/env python3
"""Source-level regression contract for editing saved vault entries."""
from pathlib import Path

source = Path("ck42x_passvault.c").read_text(encoding="utf-8")

required = (
    "CkEventChooseKeepExisting",
    "CkEventWidgetEdit",
    "bool editing_entry;",
    "static void ck_begin_edit(CkApp* app)",
    'GuiButtonTypeCenter, "Edit"',
    '"Keep Existing"',
    '"Confirm Changes?"',
    "app->draft = app->entries[app->selected];",
    "CkVaultEntry original = app->entries[app->selected];",
    "app->entries[app->selected] = app->draft;",
    "app->entries[app->selected] = original;",
    'ck_show_text_input(app, CkInputAccount, "Name", app->draft.account)',
    'ck_show_text_input(app, CkInputUsername, "Username", app->draft.username)',
    'CkInputCustomPassword, "Custom Password", app->draft.password',
)
for contract in required:
    assert contract in source, contract

# Editing must not wipe the copied username/password when the account field completes.
account_start = source.index("if(app->input_stage == CkInputAccount)")
account_end = source.index("} else if(app->input_stage == CkInputUsername)", account_start)
account_flow = source[account_start:account_end]
assert "memset(&app->draft" not in account_flow

# The entry edit affordance must be routed from the center button, separate from Inject.
widget_start = source.index("static void ck_widget_button_callback")
widget_end = source.index("static void ck_dialog_callback", widget_start)
widget_callback = source[widget_start:widget_end]
assert "GuiButtonTypeCenter" in widget_callback
assert "CkEventWidgetEdit" in widget_callback

# Cancelling a save while editing returns to the original entry rather than saving the draft.
assert "if(app->editing_entry)" in source
assert "ck_show_entry_widget(app);" in source

print("OK: saved-entry editing contract checks passed")
