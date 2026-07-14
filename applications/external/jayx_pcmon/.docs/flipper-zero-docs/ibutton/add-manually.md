<!-- Source: https://docs.flipper.net/zero/ibutton/add-manually -->
<!-- Mirrored: 2026-07-14 -->

# Adding iButton Keys Manually

## Overview

You can create virtual iButton keys on your Flipper Zero to emulate physical iButton devices. This page explains how to create and manage virtual keys.

## Creating Virtual iButton Keys

To create a virtual iButton key, follow these steps:

1. Go to **Main Menu > iButton > Add Manually**
2. Select the iButton protocol from the list:
   - Dallas DS1990
   - Dallas DS1992
   - Dallas DS1996
   - Dallas DS1971
   - Dallas (non-specific) — any iButton device using the Dallas protocol
   - Cyfral
   - Metakom
3. Enter the UID of the original key in hexadecimal format
4. Press save, then enter a new name for the key and press save again

After adding a new virtual iButton key, you can emulate it. "Manually added Dallas keys can be written to a blank or a key of the same type."

## Editing Saved iButton Keys

To edit the UID of a saved key:

1. Go to **Main Menu > iButton > Saved**
2. Select the key you want to edit and choose **Edit**
3. Enter the desired value in hexadecimal format
4. Press save, then enter a new name for the key and press save again
