## 2.0.9
- Added support for loading a custom dictionary for the keyboard auto-complete functionality (stored in "apps_data/flip_social/dictionary.txt" with each word/phrase separated by a newline).
- Updated the Explore view to allow users to click on a user to either add them as a friend or send them a message.
- Updated the Profile view to allow users to click on their friend count to view a list of their friends and then click on a friend to optionally remove them.
- Updated the Profile view to allow users to click on their bio to edit it. 

## 2.0.8
- Updated auto-complete functionality to accept and return multi-word suggestions.
- Moved the keyboard class to its own folder for better organization.
- Added more suggestions to the auto-complete dictionary.
- Fixed some minor bugs in file saving/loading.

## 2.0.7
- Fixed crashes that occurred in menu navigation when trying to access an item that was out of bounds.
- Updated the keyboard input: pressing BACK now deletes the current character instead of exiting the keyboard (hold BACK to exit the keyboard).

## 2.0.6
- Updated the keyboard input handling to support long presses for capitalization.
- Updated the keyboard input to show a blinking text cursor
- Updated the keyboard input to allow users to change the text cursor position (scroll up until you reach the text box, then use left/right buttons to move the cursor)
- Added auto-complete functionality to the keyboard input.

## 2.0.5
- Updated the display of feed posts to show the comment count for each post.
- Added more detailed handling for registration errors to provide better feedback to users during the sign-up process.

## 2.0.4
- Updated to use the flipper_http folder as intended

## 2.0.3
- Fixed an issue with loading credentials from the "flipper_http" folder, ensuring that Wi-Fi and user credentials are correctly retrieved.
- Updated the feed to change to "Flip" and "UnFlip" after flipping or unflipping a post, respectively, to reflect the current state.

## 2.0.2
- Updated the save/load functions to use a specific folder ("flipper_http") for storing Wi-Fi and user credentials. This enhances organization and security of stored data.

## 2.0.1
- Fixed the auto-updating to use the official Flipper Catalog API as intended
- Fixed a memory leak
- Updated the Post view to create the pre_saved_messages.txt file if not yet created

## 2.0
- Switched to C++ for better performance and maintainability.
- Added support for comments on feed posts (press DOWN to view comments on the current post).
- Simplified login and registration processes.

## 1.1
- Restructured code.
- Added auto‑updating.
- Updated login and registration views to display errors.
- Reduced maximum feed posts from 20 to 12 per batch for faster parsing.
- Fixed an out‑of‑memory crash in the Friends view.
- Fixed several bugs.

## 1.0.4
- New server backend.

## 1.0.3
- Updated to ensure the flip_social data folder is created when saving settings.

## 1.0.2
- Fixed the Feed Type and Notifications toggles, in the User Settings, to work as intended. 
- Added flip status to each feed post.

## 1.0.1  
- Fixed a freeze that occurred when sending a direct message.

## 1.0 - Official Release
- Final memory improvements.
- Updated the New Message option in the Messages view to allow users to search for recipients to send messages to.
- Updated the display of the Feed to use a custom view and custom font, and to show how long ago each post was created.
- Organized the files saved/utilized in the apps_data folder.
- Fixed bugs in the Direct Messaging View.
- Fixed bugs in the Pre-Save View.
- Added home announcements and notifications.
- Added User Settings (Notifications and Feed Type). With the Feed Type option, users can choose between a private feed, which only consists of posts from their friends, and a global feed, which consists of posts from all Flippers. The Notifications settings allow users to turn home notifications on and off.
- Updated the Feed to be "endless." Once you reach the end of the feed, previous posts will load in sets of 20.

## 0.8 - New Features
- Added support for RPC_KEYBOARD (thanks to Derek Jamison).
- Introduced a bio feature in the Profile section: View and update your bio.
- Enhanced the Explore view: Clicking on a user now displays their bio and friend count. You can also search for users.
- Improved the Messages view: Users can now search for a contact to send messages.

## 0.7
- Improved memory allocation
- Increased the max explore users from 50 to 100
- Increased the max feed posts from 40 to 50.
- Added loading screens.
- Added updates from Derek Jamison

## 0.6
- Improved memory allocation
- Fixed bugs in Direct Messaging View
- Fixed bugs in Pre-Save View

## 0.5
- Improved memory allocation
- Improved Feed Page
- Raspberry Pi Pico W Support

## 0.4
- Added direct messaging.
- Updated app flow for a smoother user experience.
- Reduced recent feed posts from 128 to 50.
- Improved memory allocation.

## 0.3
- Added an Explore page to discover and add other users as friends.
- Added a Friends page to view and remove friends.
- Enhanced error handling.

## 0.2
- Stability improvements.

## 0.1
- Initial release.