VaultFlipper keeps your Bitwarden and Vaultwarden logins on the Flipper Zero and types them over USB – fully offline.

The free companion CLI (https://github.com/its-sven/vaultflipper) fetches logins from one or more accounts – including shared organizations and single sign-on – encrypts them for your Flipper and your PIN, and copies them over USB.

On the Flipper:

- Unlock with a PIN made of 8–32 arrow presses. After 10 wrong PINs the vault is deleted.
- Home shows your accounts; open folders or search by name, username or URL. Every list shows the username next to the name. Thousands of logins fit – only the folder you open is decrypted.
- 2FA codes: all logins with a TOTP secret in one list with their current codes. Open one for a big code with countdown; OK types it.
- The entry card types for you: OK = login + Enter, Left = username, Right = password, Up = show password, Down = more.
- While unlocked the Flipper is a USB keyboard, like a security key. Pick the layout of the computer you type into; a German Mac layout, a typing test and a one-click Mac keyboard setup are included.
- Hold Back to lock. The app also locks after 2 minutes without input.
- English and German interface.

Setup: install the companion and run "vaultflipper setup". User guide and security notes: see the project page.
