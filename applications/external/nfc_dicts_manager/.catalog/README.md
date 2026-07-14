# Flipper Zero Dictionary Manager

A powerful application for your Flipper Zero that lets you manage, optimize, and organize your NFC dictionaries with ease. Handle mf_classic_dict.nfc and mf_classic_dict_user.nfc files, without connecting to your PC.

## Key Features
The app provides a main menu with six distinct options, each designed to simplify your dictionary management workflow.
| Feature | Description |
|---|---|
| Backup dicts | Create a one-click backup of your active dictionaries. |
| Select dict | Set any dictionary from your SD card as the system or user dictionary. |
| Merge dicts | Combine two dictionaries into a single, comprehensive file. |
| Optimize dict | Clean and refine your dictionaries by removing duplicates, invalid keys, and more. |
| Edit dict | (WIP) Modify, add, or remove single keys within a dictionary. |
| About | View credits and information about the application. |

### 1. Backup Dictionaries
Backing up your dictionaries is crucial, and this feature makes it effortless. With a single click, the app creates a backup of your active dictionaries (mf_classic_dict.nfc and mf_classic_dict_user.nfc).
 * Backup Location: Files are saved in /nfc/dictionaries/backup/.
 * Naming Convention: Each backup is timestamped with the date and time, and clearly marked as either "system" or "user".
>  💡 Pro Tip: For targeted use cases, it's a good practice to create small, specific dictionaries with a limited set of keys.

### 2. Select Dictionary
This feature gives you full control over which dictionaries are in use. You can browse the /nfc/dictionaries/ folder and assign any file as either:
 * The System Dictionary (mf_classic_dict.nfc)
 * The User Dictionary (mf_classic_dict_user.nfc)
### 3. Merge Dictionaries
Combine the contents of any two dictionaries into a single, new file. This is perfect for building a larger, more comprehensive dictionary from multiple sources.
### 4. Optimize Dictionary
This is one of the most powerful features, ensuring your dictionaries are clean and efficient. It can be run on a single dictionary or on a newly merged one. Optimize dict performs the following actions in one go:
 * Removes Duplicates: Eliminates identical keys. (On very large dictionaries, with more than 3000 keys, it might not remove 100% of duplicates due to memory limitations, but it will handle at least 95%.)
 * Enforces Key Length: Deletes keys that are not 12 bits long.
 * Validates Characters: Removes keys containing non-hexadecimal characters (anything other than 0-9 and A-F).
 * Standardizes Case: Converts all keys to uppercase for consistency.
### 5. Edit Dictionary (WIP)
This feature is a work in progress and will soon include the following functionalities:
 * Key Management: Add, delete, or modify individual keys within a dictionary.
 * Search: Find a specific key inside a dictionary.

## Installation & Usage
 * Copy the application's .fap file into the apps folder on your Flipper Zero's SD card.
 * Launch the app from the main menu.
 * If you don't have an /nfc/dictionaries/ folder on your SD card to use all the app's features, the fap'll create it once you run it for the first time.

> If you have any idea or suggestion please don't hesitate to open an issue or a PR.
I would like to implement a better way to check for duplicates (in big dictionary) please let me know how... because i coudln't find anything better than this to not to run out of memory... (You can also contact me on Telegram
