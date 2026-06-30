# FlipSocial
The first social media app for Flipper Zero. Connect with other users directly on your device through WiFi.

The highlight of this app is customizable pre-saves, which, as explained below, aim to address the challenges of typing with the directional pad.

## Requirements
- Wi-Fi Developer Board, BW16, Raspberry Pi, or ESP32 device flashed with FlipperHTTP v2.0 or higher: https://github.com/jblanked/FlipperHTTP
- 2.4 GHz or 5 GHz Wi-Fi access point

## Connect Online
- Discord: https://discord.gg/5aN9qwkEc6
- YouTube: https://www.youtube.com/@jblanked
- Instagram: https://www.instagram.com/jblanked
- Other: https://www.jblanked.com/social/

## Features
- Login/Logout
- Registration
- Feed (with comments, flipping, and posting)
- Profile
- Customizable Pre-Saves
- AutoComplete keyboard
- Explore 
- Friends 
- Direct Messaging 

## Feed/Comments Controls
- Left/Right: Navigate through posts
- Down: Scroll through comments/ post a comment

**Login/Logout:** Log in to your account to view and post on the Feed. You can also change your password and log out when needed.

**Registration:** Create an account with just a username and password—no email or personal information required or collected.

**Feed:** View the latest posts, create your own posts, and "Flip" a post—FlipSocial’s version of liking or favoriting a post.

**Customizable Pre-Saves:** The biggest challenge with a social media app on the Flipper Zero is using only the directional pad for input. To address this, I implemented a pre-saved text system. The pre-saves are stored in a pre_saved_messages.txt file on your SD card. You can edit the pre-saves by opening qFlipper, downloading the file from the /apps_data/flip_social/ folder, adding your pre-saves (separated by new lines), and then copying it back to your SD card. You can also create pre-saves directly within the app.

**Explore:** Discover other users and add them as friends.

**Friends:** View and remove friends.

**Direct Messaging:** Send direct messages to other Flipper users and view your conversations.