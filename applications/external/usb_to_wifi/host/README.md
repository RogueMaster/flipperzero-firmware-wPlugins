# Cross-platform host

The command-line host runs on Windows 10/11, Linux, and macOS. The native
SwiftUI menu bar application remains the recommended macOS experience.

## Install from source

Install Python 3.10 or newer, then run from the repository root:

```sh
python3 -m venv .host-venv
```

Linux/macOS:

```sh
. .host-venv/bin/activate
python -m pip install .
fib-bridge
```

Windows PowerShell:

```powershell
.host-venv\Scripts\Activate.ps1
python -m pip install .
fib-bridge
```

The helper detects Flipper serial ports. Use `fib-bridge --list-ports` and
`fib-bridge --port PORT` if automatic discovery cannot select the device.

The first HELLO displays a terminal permission prompt. An always-allow choice
stores only a SHA-256 permission identifier in the current user's configuration
directory. Run without administrator/root privileges; on Linux the user may
need normal serial-port membership (commonly the `dialout` group).

## Security

The host performs direct HTTPS requests with the operating system's trusted CA
store. It does not use browser cookies, proxy credentials, shell commands, or
the local filesystem. URL credentials and non-HTTPS schemes are rejected. DNS
answers are checked before connecting; localhost, private, link-local,
multicast, reserved, and unspecified addresses are blocked. Every redirect is
resolved and checked again, and the TLS socket connects to the validated IP
while retaining the original hostname for SNI and certificate validation.

## Prebuilt binaries

The `host-binaries.yml` GitHub Actions workflow builds one-file Windows and
Linux artifacts with PyInstaller. Release candidates are published on the
[GitHub Releases](https://github.com/mete888/flipper_usb_to_wifi/releases) page.

Windows PowerShell:

```powershell
.\fib-bridge-windows-x86_64.exe
```

Linux:

```sh
tar -xzf fib-bridge-linux-x86_64.tar.gz
cd fib-bridge-linux-x86_64-package
./install.sh
```

The Linux installer places the binary and icon under the current user's XDG
data directory and adds a **Flipper USB Internet Bridge** launcher to the
desktop application menu. It does not use `sudo`. Run `./uninstall.sh` from the
extracted package to remove those files. The Windows executable contains the
same icon as its native executable resource.

The binaries are currently unsigned. Windows SmartScreen or Linux desktop
policy may therefore ask for confirmation. The packaged host has automated
Windows/Linux tests and a POSIX virtual-serial integration test. Physical
Flipper USB validation on Windows and Linux has not yet been completed.
