## Setup Build environment

### Build Instruction

1. Install `ufbt`:
   ```bash
   pip3 install ufbt
   ```
2. Clone this repository and enter the repository root.
3. Run `ufbt update` to update the SDK for your flipper
   - If you are using custom firmware, You should switch SDK. Here is the example for `unleashed` firmware:
     ```bash
     ufbt update --index-url=https://up.unleashedflip.com/directory.json
     ```
   - If you want to use different release channel, You can run update to that channel too. Here is the example for `dev` channel (`dev`, `rc`, `release` are supported):
     ```bash
     ufbt update --channel=dev
     ```
4. Run `ufbt` in the repository root:
   ```bash
   ufbt
   ```
5. Compiled binary is now available at `./dist/` directory.

### Setup Visual Studio Code

> [!WARNING]
> This command will overwrite your `.vscode` directory and `.gitignore` on your root directory.
> **Make sure to backup your changes before running this command.**

1. Suppose your build environment is ready.
2. Run `ufbt vscode_dist` to generate Visual Studio Code config.
