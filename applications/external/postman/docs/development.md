# Development Notes

## Setup

### Build

For building and executing the application build you will need to install https://github.com/flipperdevices/flipperzero-ufbt, this is a separate python application that will bundle the application without the need to build the whole firmware.

\*for mac you can use pipx to install python virtual applications

### Vscode Setup

In order to get function return and type completions for higher flipper firmware modules and functions like `<gui/models/dialog.h>` you will need to git clone the firmware project and tie ts location to vscode settings

```
{
  "cortex-debug.enableTelemetry": false,
  "cortex-debug.variableUseNaturalFormat": true,
  "cortex-debug.armToolchainPath": "/<your firmware location>/flipperzero-firmware/toolchain/current/bin",
  "cortex-debug.openocdPath": "/<your firmware location>/flipperzero-firmware/toolchain/current/bin/openocd",
  "cortex-debug.gdbPath": "/<your firmware location>/flipperzero-firmware/toolchain/current/bin/arm-none-eabi-gdb-py3",
  "editor.formatOnSave": true,
  "files.associations": {
    "*.scons": "python",
    "SConscript": "python",
    "SConstruct": "python",
    "*.fam": "python",
    "gui.h": "c",
    "dolphin.h": "c",
    "stdlib.h": "c",
    "input.h": "c"
  },
  "clangd.path": "<your firmware location>/flipperzero-firmware/toolchain/current/bin/clangd",
  "clangd.arguments": [
    "--query-driver=**/arm-none-eabi-*",
    "--compile-commands-dir=${workspaceFolder}/.vscode/clang_compile_commands/",
    "--clang-tidy",
    "--header-insertion=never"
  ],
  "clangd.onConfigChanged": "restart",
  "clangd.inactiveRegions.useBackgroundHighlight": true
}
```

## Vs Code run tasks

Project contains vscode run tasks json that you can you to build and deploy your app to the flipperzero. There is a mismatch in the build compile commands commands on mac, there is a special syncmcd.sh that alters the commands so the "wrong flag -mword-relocations" seen on the cs code ide can be adjusted.

## Firmware links and other development info

- Firmware (docs, c functions and modules) https://developer.flipper.net/flipperzero/doxygen/index.html

- GUI modules visual guide https://brodan.biz/blog/a-visual-guide-to-flipper-zero-gui-components/

- Tips and advanced concepts https://github.com/jamisonderek/flipper-zero-tutorials/wiki/User-Interface

\*Official firmware repo has application/examples folders you could follow as well
