# Changelog

## 0.2 — 2026-11-11

### 🚀 Features

- *(deed)* Earn the dolphin xp for dictations and /clear (`c0d89e6`)
- *(reconnect)* Stop advertising after a minute with no link (`6be8b40`)
- *(keymap)* Switch the link by holding right (`3e283c5`)
- *(view)* Draw the usb boards and switch plate (`ac960a3`)
- *(usb)* Act as a usb keyboard over the cable (`d421d5b`)
- *(view)* Show the hold legend on no link (`a0cca23`)

### 🐛 Bug Fixes

- *(keymap)* Act on the screen a gesture began on (`34985ac`)

### 🚜 Refactor

- *(transport)* Split out the key router (`e1afed9`)
- *(asset)* Move the pictures under asset/ (`002c973`)

## 0.1 — 2026-11-07

### 🚀 Features

- *(app)* Show the remote's one screen (`08c2e9d`)
- *(keymap)* Map button events to actions (`c1444e5`)
- *(feedback)* Say how an action confirms itself (`6f6b64a`)
- *(keymap)* Translate firmware button events (`4c2bbec`)
- *(view)* Draw the voice screen and the flash (`a151bdc`)
- *(screen)* Keep what the display shows (`1fdca48`)
- *(app)* Act on the remote's buttons (`1119c46`)
- *(feedback)* Give each flash its duration (`d37aeec`)
- *(view)* Draw the redesigned screen (`8bd5d9a`)
- *(bluetooth)* Bring up the link and show its state (`aa7fb72`)
- *(effort)* Set the effort level from the device (`8603a38`)
- *(actions)* Send every button's keys (`a40dcc4`)
- *(keymap)* Release the voice key on a lost link (`e0f4c67`)
- *(screen)* Keep the backlight on (`a764474`)
- *(view)* Draw the recess around the buttons (`982cdb3`)

### 🐛 Bug Fixes

- *(view)* Center the voice screen (`1e72ec2`)
- *(release)* Look up the tag's commit by its full ref (`76ab21a`)

### 🚜 Refactor

- *(app)* Move the entry point to main.c (`f527dc1`)
- *(src)* Split sources by layer (`e4e6396`)

### 📚 Documentation

- *(test)* State the host test setup (`475bf59`)
- *(readme)* Lead with what the app does (`e7819c4`)
- Add the security policy (`f1eb753`)
- Move the development guide to CONTRIBUTING (`5a6c2ff`)
- *(claude)* End every session on a clean tree (`2f9cf15`)
- Adopt the template's planning and wip rules (`ce1d384`)
- Add the site with home, quick start and the guide (`a11f165`)
- Move usage from the readme to the site (`f3bf827`)
- Move contributing to the site (`7dafe9b`)
- Move development to the site (`4129eb7`)
- List development and contributing pages (`d084c0e`)
- Add the internals pages (`00bcbab`)
- List the internals pages (`cfd5fb1`)
- Show the app icon as the site logo (`3445f96`)
- Shorten the readme (`5c30f38`)
- Put each flash argument on its own line (`263d5b6`)
- Keep Dark Reader off the site (`7573923`)
- Keep the theme overrides inside docs (`a159663`)
- Retake the screenshot in qFlipper (`839a0f4`)

### 🏗️ Build & CI

- Replace the template workflow with ci.yml (`ed6aaa8`)
- Lint shell scripts with shellcheck (`44281fd`)
- *(release)* Cut and publish a release with one command (`9b46165`)
- *(release)* Prepare the Flipper Apps Catalog branch in the fork (`85f0735`)
- *(docs)* Build and publish the site (`0fb83fb`)
- *(release)* Draft the release and publish it last (`84443f4`)

### ⚙️ Miscellaneous Tasks

- Scaffold openspec project (`e13184c`)
- *(editorconfig)* Adopt flipper c style (`ab83c1e`)
- *(toolchain)* Bootstrap the flipper toolchain (`08359f0`)
- *(debug)* Add device debugging and ide tasks (`a38f566`)
- *(ide)* Track the idea files clion needs (`02a8714`)
- *(ide)* Add the test run configuration (`9893e8d`)
- *(icon)* Take the slash icon from the wireframe (`1b0162a`)
- *(app)* Fill in the manifest (`249c0c1`)
- License under GPL-3.0 (`ea29219`)
- *(changelog)* Generate the changelog with git-cliff (`20728d3`)
- *(prek)* Skip compile_commands.json in check-symlinks (`3d5f831`)
- *(mise)* Pin shellcheck 0.11.0 (`ac98c3a`)
- *(mise)* Pin zensical 0.1.0 (`f95d5d4`)
