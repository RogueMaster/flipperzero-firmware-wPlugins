<!-- Source: https://docs.flipper.net/zero/u2f -->
<!-- Mirrored: 2026-07-14 -->

# U2F (Universal 2nd Factor)

## Overview

Flipper Zero can function as a USB Universal 2nd Factor authentication token or security key for two-factor authentication on web accounts. "A security key is a small device that helps computers verify that it is you when signing in to an account."

## Setting Up Flipper Zero as a Security Key

To register your Flipper Zero as a security key:

1. Close qFlipper if running on your computer
2. Connect Flipper Zero via USB cable
3. Navigate to Main Menu > U2F and verify "connected" displays
4. In your web account, activate two-factor authentication following the website's instructions
5. Select the security key as your 2nd verification step
6. Press OK on Flipper Zero to confirm registration

Several major platforms support this feature, including Google, X, Facebook, and GitHub, though each has different procedures for adding security keys.

## Signing In With Your Flipper Zero

Once registered as a security key:

1. Close qFlipper if running
2. Connect Flipper Zero via USB
3. Go to Main Menu > U2F and confirm "connected" displays
4. During sign-in, complete the first verification step with your password
5. When prompted for the security key, press OK on Flipper Zero

For more information, visit the DongleAuth website or consult your specific service's documentation.
