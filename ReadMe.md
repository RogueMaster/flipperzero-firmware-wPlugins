<h1 align="center"><a href='https://rogue-master.net'><img src='.github/assets/rmlogo.png' width="40%"></a>
<br><a href='https://discord.gg/gF2bBUzAFe' target='_blank'><img src='.github/assets/Discord.png' alt='Discord' title='Discord'></a>
&nbsp;<a href='https://github.com/RogueMaster/flipperzero-firmware-wPlugins/releases/latest' target='_blank'><img src='.github/assets/Github.png' alt='Firmware GitHub' title='Firmware GitHub'></a>
&nbsp;<a href='https://www.patreon.com/RogueMaster?filters[tag]=Latest%20Release' target='_blank'><img src='.github/assets/Patreon.png' alt='Latest PATREON Release' title='Latest PATREON Release'></a>
&nbsp;<a href='https://github.com/RogueMaster/awesome-flipperzero-withModules' target='_blank'><img src='.github/assets/Resources.png' alt='More Research / Assets' title='More Research / Assets'></a></h1>

# Install GitHub Version With [WEB INSTALLER](https://lab.flipper.net/?url=https%3A%2F%2Frogue-master.net%2F%3Ffile%3DRM0819-2255-b3dd8981.tgz&channel=RM0819-2255-b3dd8981&version=0.420.0&target=f7)

## Thank you so much to our RM Pro/SUPER Patreon supporters! Because of people like you, we are able to offer the best and most up-to-date Flipper Zero Firmware!

<table><tr><th><div align="left">
This firmware is a fork of all Flipper Zero community projects! We are NOT paywalled. I will keep RM Custom Firmware the most cutting-edge with active development and updates from all projects that can be found to be useful to the community. Although there are features exclusive to this firmware, there are also many updates by the rest of the Flipper Zero community. With the help of the entire community, we get the best firmware! See the RM <a href='https://github.com/RogueMaster/flipperzero-firmware-wPlugins#Games'>Games</a> and <a href='https://github.com/RogueMaster/flipperzero-firmware-wPlugins#Plugins'>Plugins</a> for a list of most of those projects.
<br><a href='https://discord.gg/gF2bBUzAFe' target="_blank">Discord</a> - <a href='https://github.com/RogueMaster/flipperzero-firmware-wPlugins#Build'>Build</a> - <a href='https://github.com/RogueMaster/flipperzero-firmware-wPlugins#install'>Install Instructions</a> - <a href='https://github.com/RogueMaster/flipperzero-firmware-wPlugins#howto'>More Links And How To</a> - <a href='https://github.com/RogueMaster/flipperzero-firmware-wPlugins#all'>All Changes from OFW</a> - <a href='https://github.com/RogueMaster/flipperzero-firmware-wPlugins#support'>Support RM Custom Firmware</a> - <a href='https://www.patreon.com/RogueMaster' target="_blank">Patreon</a>
</div></th><th style='width:50%;'>
This software is for experimental purposes only and is not meant for any illegal activity/purposes. We do not condone illegal activity and strongly encourage keeping transmissions to legal/valid uses allowed by law.
</th></tr></table>

<a name="support"></a>

- Keep Your Flipper Up To Date With [PATREON membership](https://www.patreon.com/RogueMaster/membership)
- [PATREON for frequent compiled releases](https://www.patreon.com/RogueMaster) for nearly ALL updates. GitHub releases will occur monthly.
- [Contact me on Discord/Patreon to RENAME your Flipper or for GIF conversion for Flipper Zero](https://www.patreon.com/RogueMaster/membership)
- [:fire: PATREON ASSET PACKS AVAILABLE](https://www.patreon.com/RogueMaster/membership): [ALL](https://www.patreon.com/posts/animation-all-v4-77935728) (For Preview Only), Android, 420, 420+18, Anime Only, Anime+420, Anime+420+18, BMO, CHERRY 18+, CyberPunk77, DBZ, Dolphin, GTA, HACKZ, MARIO, MARVEL, NoAnim, ONE PIECE, P0KEMON, Pwnagotchi, RM 18PLUS, RM SELECT, RM SELECT + 18PLUS, RM MINIMAL (default on GitHub), SAO, SCIENCE, SCIFI, SJUMP, SQUATCH, VIRUS, WATCHDOGS AND STOCK with assets updated to look like stock. Anime and RM Select builds are very large and can slow down F0.
- [VIDEO PREVIEW FOR ALL ANIMATIONS](https://www.patreon.com/RogueMaster/posts/august-2023-all-87925007)
- [Supporters also get SD Card Assets zip file with extra NFC Assets such as Infrared remotes, NFC files, SubGHz files and 86 RM Pro Trained Level 50 Sm@sh Amiib0 (By RogueMaster)](https://www.patreon.com/RogueMaster/membership)
- We are open to PRs, suggestions and advice if you feel you need anything that has been left out. [Contact me on Discord](https://discord.gg/gF2bBUzAFe)

<a name="latest"></a>

## Latest Updates - [PATREON: Latest Release RM0821-0305-6688b862-Patreon](https://www.patreon.com/RogueMaster?filters[tag]=Latest%20Release)

- Last Synced/Checked OFW, changes in [commits](https://github.com/flipperdevices/flipperzero-firmware/commits/dev): `2026-08-21 03:05 EST`
- Last Synced/Checked CFW, changes in [changelog](https://github.com/RogueMaster/flipperzero-firmware-wPlugins/blob/420/CHANGELOG.md): `2026-08-21 03:05 EST`
- Updated: [HTW AC Remote v1.2 (By sokogen)](https://github.com/sokogen/flipperzero-htw-ac-remote)
- ESP Flasher: [Bump Marauder v1.15.0 (By justcallmekoko)](https://github.com/justcallmekoko/ESP32Marauder/releases/tag/v1.15.0)
- NFC: [Protocol scenes moved into their own plugins, roughly halving the app's resident RAM (By mishamyte)](https://github.com/DarkFlippers/unleashed-firmware/pull/1073)
- NFC: [Fixed the EMV plugin, which never loaded (By mishamyte)](https://github.com/DarkFlippers/unleashed-firmware/pull/1073)
- NFC: [Removed unreachable EMV render code that called an undefined function (By mishamyte)](https://github.com/DarkFlippers/unleashed-firmware/pull/1085)
- NFC: [Ultralight - Amiibo/Xiaomi unlock no longer burns an AUTHLIM attempt when the password cannot be derived (By mishamyte)](https://github.com/DarkFlippers/unleashed-firmware/pull/1086)
- NFC: [Ultralight read result now says whether authentication failed or was never attempted (By mishamyte)](https://github.com/DarkFlippers/unleashed-firmware/pull/1090)
- NFC: [Ultralight - the default password is no longer tried when AUTHLIM cannot be read (By mishamyte)](https://github.com/DarkFlippers/unleashed-firmware/pull/1089)
- Updated: [FlipDeFlock v0.77 (By ReconGrunt)](https://github.com/ReconGrunt/FlipDeFlock) `Req: ESP32`
- NFC: [Social Moscow - fixed the parser rejecting most genuine cards (By mishamyte)](https://github.com/DarkFlippers/unleashed-firmware/pull/1092)
- NFC: [Social Moscow - a partially read card no longer shows an all-zero card number as if it were real (By mishamyte)](https://github.com/DarkFlippers/unleashed-firmware/pull/1097)
- NFC: [Mosgortrans - an unrecognised ticket layout is now named in the debug log, instead of the Metro/Ground section vanishing with no explanation (By mishamyte)](https://github.com/DarkFlippers/unleashed-firmware/pull/1097)
- NFC: [SmartRider, All-In-One and Banapass no longer log an error for every card that is not theirs (By mishamyte)](https://github.com/DarkFlippers/unleashed-firmware/pull/1097)

<a name="release"></a>

## Install Latest Release

- FLASH STOCK FIRST BEFORE UPDATING TO CUSTOM FIRMWARE!
- To avoid Application errors, delete /ext/apps before doing the any RM firmware update
- Get <a href='https://github.com/RogueMaster/flipperzero-firmware-wPlugins/releases/latest' target="_blank">Latest GitHub Release</a> or for PATREON members <a href='https://www.patreon.com/RogueMaster?filters[tag]=Latest%20Release' target="_blank">Latest Patreon Release</a>.
- See <a href='https://github.com/RogueMaster/flipperzero-firmware-wPlugins#install'>Install Instructions</a>

<a name="howto"></a>

## How To

- [How to install firmware](https://github.com/RogueMaster/flipperzero-firmware-wPlugins/blob/420/documentation/HowToInstall.md)
- [How to build firmware](https://github.com/RogueMaster/flipperzero-firmware-wPlugins/blob/420/documentation/HowToBuild.md)
- [How to change Flipper name](https://github.com/RogueMaster/flipperzero-firmware/blob/420/documentation/CustomFlipperName.md)
- [How to use: NRF24 plugins](https://github.com/RogueMaster/flipperzero-firmware-wPlugins/blob/420/documentation/NRF24.md)
- [How to use: SentrySafe plugin](https://github.com/RogueMaster/flipperzero-firmware-wPlugins/blob/420/documentation/SentrySafe.md)
- [Barcode Generator readme](https://github.com/RogueMaster/flipperzero-firmware-wPlugins/blob/420/documentation/BarcodeGenerator.md)
- [How to extend SubGHz frequency range](https://github.com/RogueMaster/flipperzero-firmware-wPlugins/blob/420/documentation/DangerousSettings.md)
- [How to add extra SubGHz frequencies](https://github.com/RogueMaster/flipperzero-firmware-wPlugins/blob/420/documentation/SubGHzSettings.md)
- [Configure Sub-GHz Remote App](https://github.com/RogueMaster/flipperzero-firmware/blob/420/documentation/SubGHzRemotePlugin.md)
- [GAMES ONLY MODE PASSWORD: UP UP DOWN DOWN LEFT RIGHT LEFT RIGHT IN CLOCK](https://github.com/RogueMaster/flipperzero-firmware-wPlugins/blob/420/GAMES_ONLY.md)
- [FLIPPER PROJECT WISH LIST](https://github.com/RogueMaster/flipperzero-firmware-wPlugins/blob/420/RoadMap.md)

## New on MNTM RM Branch!

- TLDR: More Assets, Game Mode, RM Passport & Many more apps! Extra NFC, RFID & SubGHz features!
- ESP Flasher: [Comes with Marauder v1.15.0 (By justcallmekoko)](https://github.com/justcallmekoko/ESP32Marauder/releases/tag/v1.15.0)
- Animations: [Asset Packs for RM HERE](https://github.com/RogueMaster/awesome-flipperzero-withModules/tree/rogue_main/RM_AssetPacks)
- Animations: [PREVIEW "RM Minimal" animations (build default)](https://www.patreon.com/posts/animation-rm-77441581) or [OTHER SETS](https://www.patreon.com/RogueMaster?filters[tag]=Animations)
- Archive: Open MP3 from File Browser, thanks to [MP3 Player v3.5 (By coolshrimp)](https://github.com/coolshrimp/flipperzero-mp3-player)
- Assets: Includes 3 Sample Beacon Spam Marauder Companion Scripts (Thanks to TalkingSasquach for the RM Beacon Spam)
- Assets: Includes a NFC Level 50 Z3lda Amiibo
- Assets: Includes a NFC link to TalkingSasquach on YouTube
- Assets: Includes a NFC Rick Roll link
- Assets: Includes NFC Assets: 86 RM Pro Trained Level 50 Sm@sh Amiib0 (By RogueMaster) It is recommended you randomize UID with this script. [Patreon Build Only](https://www.patreon.com/RogueMaster/membership)
- Assets: Includes NFC Assets: RogueMaster GitHub and RogueMaster Patreon
- Assets: Includes sample Infrared remotes and IR files for Roku and Vizio
- Assets: Includes sample Music Player tunes
- Assets: Includes Sonic Screw Driver sound for the Wav Player
- Assets: Includes SubGHz assets for Crosswalk, CVS/Walgreens CS Buttons [Thanks jimilinuxguy](https://github.com/jimilinuxguy/customer-assistance-buttons-sdr/tree/main/cvs/English), Handicap Doors, Honda Lock/Unlock, Lowes, Sextoys, Tesla Charge Port & Unitree Go1 Robot Dog.
- Assets: Includes SubGHz Gas Sign Captures by [0day](https://github.com/0dayCTF)
- Assets: Includes SubGHz Playlist config and SubGHz Remote config for CVS, Lowes & Walgreens.
- Assets: Includes SubGHz Remote config for Gas_Sign_Edit [from UberGuidoZ](https://github.com/UberGuidoZ/Flipper)
- Assets: Includes SubGHz Remote config for Riding_Dirty [from UberGuidoZ](https://github.com/UberGuidoZ/Flipper)
- Assets: Includes SubGHz SexToy files [from UberGuidoZ](https://github.com/UberGuidoZ/Flipper)
- Assets: Includes Tama P1 Rom Game file. [this rom](https://tinyurl.com/tamap1)
- Assets: Includes Wifi Marauder scripts for: [Create EAPOLpacketsSniffer.json #714 (By grugnoymeme)](https://github.com/RogueMaster/flipperzero-firmware-wPlugins/pull/714) & Wifi Scripts for Beacon Spam themed for Aperature Science, Funny and RM.
- BadUSB: [Assets for CVE-2024-1086 on LINUX targets (By hed0rah & ComradeOrion)](https://github.com/Notselwyn/CVE-2024-1086)
- BadUSB: Assets for Kiosk Evasion (By nocomp) and Wifi Stealer (By 7h30th3r0n3)
- BadUSB: Assets for RickRoll, RogueMaster GitHub and RogueMaster Patreon
- BadUSB: [feat: Add NFC BLE pairing emulation #1154 (By phra)](https://github.com/RogueMaster/flipperzero-firmware-wPlugins/pull/1154)
- Bluetooth Remote: [Add camera control mode to bluetooth remote app. #2336 (By LoganMD)](https://github.com/flipperdevices/flipperzero-firmware/pull/2336)
- Desktop: ['Game Mode' added to UP menu (By RogueMaster)]
- Dolphin: Assigned profile pic for levels 1-10 (Happy Lvl 1 Dolphin), 11-15 (Happy Lvl 2 Dolphin), 16-18 (Happy Lvl 3 Dolphin), 19-21 (Kid G0ku), 22-24 (Adult G0ku), 25-27 (SSJ G0ku) and 28-30 (SSJ3 G0ku)
- Dolphin: CFW Settings: Passport: Now has 15 selectable backgrounds options and 74 profile image options. (Changes By RogueMaster with thanks to ESurge) (Section By ESurge)
- Dolphin: Expanded max level from 3 to 30 using [Roll20](https://roll20.net/compendium/dnd5e/Monsters#h-Experience%20Points), Increased max deed XP per action type from 15 to 45 exp daily & updated animation manifest for max level 30 for all animations (By RogueMaster)
- Dolphin: Mood Stays Happy [(Thanks to biocage)](https://github.com/biocage/flipperzero-firmware), changed to Mood Stays Less Than 6 / Mood Stays High (but not in game mode) (By RogueMaster)
- Dolphin: Passport: Default Mario & AD Police Themed [(Thanks to Kuronons)](https://github.com/Kuronons/FZ_graphics/tree/main/Passport%20background)
- Dolphin: Passport: Many thanks to Kuronons for the many passport backgrounds and profile images available and included.
- Dolphin: Passport: Rename Dolphin with CFW Settings or Name Changer app under Applications=>Settings [(By ESurge)](https://github.com/RogueMaster/flipperzero-firmware-wPlugins/pull/488)
- Dolphin: Passport: Show EXP [(By Dabolus)](https://github.com/Dabolus/flipperzero-firmware-rpg/)
- Dolphin: Passport: Stored on SD with Multiple Pages, and Dynamic Icons/Images support [(By ESurge)](https://github.com/RogueMaster/flipperzero-firmware-wPlugins/pull/527)
- Dolphin: Changed daily MAX to 198 on all 7 Deed Types. Random Deed Selection used for MAX +3 EXP daily gain.
- Dolphin: Plugin Achivement +3 EXP for a total of up to 700 EXP daily. Plugin Achivements are:
	- Any Application is Opened
	- Dice First Roll for d20+ = sides on dice (i.e. Nat 20 on d20)
	- Dice First Roll for d20+ = sides on dice - 1 (+1 EXP) (i.e. 19 on d20)
	- Games Only Mode from Lock Menu
	- Getting 2048 in 2048
	- Mouse Jacker Ducky Run
	- NRFSniffer Found Address
	- Dab Timer @ Alert
	- Tetris, Snake, or Flappy Bird Score For EXP
	- Zombiez for every 20 Zombie kills
- Infrared: Assets for Toto and BioBidet bidets.
- NFC: [Added additional aid GeldKarte (for emv) (thanks to haisenteck) (From Korai-Labs / ankris812)](https://github.com/Korai-Labs/Korai)
- NFC: [Extra NFC Mifare Keys (By Korai-Labs / ankris812)](https://github.com/Korai-Labs/Korai)
- NFC: [FeliCa: Add card generator for FeliCa Lite-S #4371 (By dogtopus)](https://github.com/flipperdevices/flipperzero-firmware/pull/4371)
- NFC: [FeliCa: Add Standard card emulation support #4383 (By nikachu2012)](https://github.com/flipperdevices/flipperzero-firmware/pull/4383)
- NFC: [Parser for Bambu Lab filament spool (By mishamyte)](https://github.com/DarkFlippers/unleashed-firmware/pull/1012)
- NFC: [Parser for CTAS Andalucia (By Alejandro12120 & duhow)](https://github.com/DarkFlippers/unleashed-firmware/pull/808)
- NFC: [Parser for treA (By grugnoymeme)](https://github.com/Next-Flip/Momentum-Firmware/pull/441)
- Plugins: 2048, Arkanoid, Snake, and Tetris show score. Thanks to [whoamins](https://github.com/flipperdevices/flipperzero-firmware/commit/7feda832ede1ba8468eff2ca055fef3ddbdc16ac) and [DevMilanIan](https://github.com/RogueMaster/flipperzero-firmware-wPlugins/pull/188) With position changes by RogueMaster. Also all + Tic Tac Toe updated by [Unleashed/Eng1n33r](https://github.com/DarkFlippers/unleashed-firmware) for stability.
- Plugins: Icon for Clock [Thanks to Redlink](https://github.com/redlink2/flipperzero-firmware/tree/menuChanges)
- RFID: [2 new T5577 extra actions: Set/clear custom pw; write block - NO WIPE #678 (By plgcoder)](https://github.com/RogueMaster/flipperzero-firmware-wPlugins/pull/678)
- RFID: [Support of Hitag Micro chips (8265/8210/H5.5) (By mishamyte)](https://github.com/DarkFlippers/unleashed-firmware/pull/1002)
- RFID: [Wipe T5577 (reset to blank, with read-back verification) (By mishamyte)](https://github.com/DarkFlippers/unleashed-firmware/pull/1003)
- Settings: Battery Meter on Desktop [Thanks to McAzzaMan](https://github.com/McAzzaMan/flipperzero-firmware/tree/BatteryPercentageView)
- CFW Settings => Interface => General => [Game Mode (By RogueMaster)](https://github.com/RogueMaster/flipperzero-firmware-wPlugins/blob/420/GAMES_ONLY.md)
- - [UP UP DOWN DOWN LEFT RIGHT LEFT RIGHT FROM CLOCK to EXIT](https://github.com/RogueMaster/flipperzero-firmware-wPlugins/blob/420/GAMES_ONLY.md)
- Settings: Power: [About on Power Off (By LeeroysHub)](https://github.com/LeeroysHub/)
- SubGHz: [Add Cardin S508 rolling-code decoder (decode-only) (By half2me)](https://github.com/DarkFlippers/unleashed-firmware/pull/1004)
- SubGhz: [Add Telcoma/Cardin EDGE protocol (By half2me)](https://github.com/DarkFlippers/unleashed-firmware/pull/1001)
- SubGHz: [All Supported Protocols](https://github.com/RogueMaster/flipperzero-firmware-wPlugins/tree/420/documentation/SubGHzSupportedSystems.md)
- SubGHz: Region Locked -  To transmit to outside ranges (Unlock), use the CFW Settings app under Apps=>Settings.
- SubGHz: [Transmit Indicator before Out of Region Frequency Use (By RogueMaster)](https://github.com/RogueMaster/flipperzero-firmware-wPlugins/commit/e24d4a0689db555ae083fec23a471e878adb6cd4)
- SubGHz: USE CFW Settings to use unlock from SD flag from [(cloudbreakdaniel)](https://github.com/RogueMaster/flipperzero-firmware-wPlugins/commit/97db0dc91ee3dff812b4dec0618e3f198de14405).  Update `subghz/assets/extend_range.txt` with [this file](https://github.com/RogueMaster/flipperzero-firmware-wPlugins/blob/420/applications/main/subghz/resources/subghz/assets/extend_range.txt) on SD.  **UPDATE IGNORE FLAG TO TRUE TO UNLEASH YOUR FLIPPER!!**
- SubGHz: [X10 Protocol Decoder (By CodeAllNight/jamisonderek)](https://github.com/jamisonderek/flipper-zero-tutorials/tree/main/subghz/protocols/x10)

## Games

- [15 v1.2 (By x27)](https://github.com/x27/flipperzero-game15)
- [1D Pacman v0.1 (By easiwork)](https://github.com/easiwork/1dpacman)
- [2048 v1.1 (By OlegSchwann)](https://github.com/OlegSchwann/flipperzero-firmware/tree/hackaton/game_2048/applications/game-2048)
- [2048 v1.6 (By eugene-kirzhanov)](https://github.com/eugene-kirzhanov/flipper-zero-2048-game) (Titled 2048 (Improved))
- [4 In A Row v1.4 (By leo-need-more-coffee)](https://github.com/leo-need-more-coffee/flipperzero-4inrow)
- [99 Bottles (By bolknote)](https://github.com/bolknote/99-Flipper)
- [Air Arkanoid  v1.1 (By DrZlo13)](https://github.com/flipperdevices/flipperzero-good-faps/pull/141) `Req: Video Game Module By Flipper Devices`
- [Air Labyrinth v0.1 (By jamisonderek)](https://github.com/jamisonderek/flipper-zero-tutorials/tree/main/vgm/apps/air_labyrinth) `Req: Video Game Module By Flipper Devices`
- [Apple Grabber v0.1 (By d7d8)](https://github.com/d7d8/apple-grabber)
- [Ardudrivin v1.0.3 (By apfxtech)](https://github.com/apfxtech/FlipperDrivin)
- [ArduGolf v1.0 (By apfxtech)](https://github.com/apfxtech/FlipperGolf)
- [Arduventure v2.0 (By apfxtech)](https://github.com/apfxtech/FlipperArduventure)
- [Arkanoid v1.3 (By gotnull)](https://github.com/gotnull/flipperzero-firmware-wPlugins)
- [Asteroids v1.3 (By antirez)](https://github.com/antirez/flipper-asteroids)
- [Avocado Zero v0.1 (By Endika)](https://github.com/Endika/flipper-avocado-zero)
- [Banana v0.4 (By DrEverr)](https://github.com/DrEverr/FlipperApps)
- [Blackjack v0.1 (By RocketGod)](https://github.com/RocketGod-Git/Flipper-Zero-Blackjack)
- [BlackJack v1.1 (By teeebor)](https://github.com/teeebor/flipper_games)
- [Bomberduck v1.3 (By leo-need-more-coffee)](https://github.com/leo-need-more-coffee/flipperzero-bomberduck)
- [Bomberfox v1.0 (By ashleyhuxley)](https://github.com/ashleyhuxley/bomber)
- [Brainy v1.1 (By deya-eldeen)](https://github.com/deya-eldeen/flipperzero-brainy)
- [BzzBzz v0.3 (By ikiruneo)](https://github.com/ikiruneo/BzzBzz)
- [CarJacker v1.0 (By RocketGod-git)](https://github.com/RocketGod-git/Flipper-Zero-CarJacker)
- [Catacombs Of The Damned v2.2 (By apfxtech)](https://github.com/apfxtech/FlipperCatacombs)
- [Cell Lab v1.1 (By PilotOfAsuka)](https://github.com/PilotOfAsuka/cell_lab_fliperzero)
- [Checkers v1.0 (By H4W9)](https://github.com/H4W9/FlipCheckers)
- [Chess Clock (By ihatecsv)](https://github.com/ihatecsv/flipper_chess_clock)
- [Chess v1.12 (By xtruan)](https://github.com/xtruan/flipper-chess)
- [CIDR Challenge v1.0 (By AlastorApps)](https://github.com/AlastorApps/cidrchallengef0)
- [Cigarette v0.1.2 (By fuckmaz)](https://github.com/fuckmaz/flipper-cigarette)
- [City Bloxx (By Milk-Cool)](https://github.com/Milk-Cool/fz-citybloxx)
- [Cognizant Flipper v1.0 (By TunnelDog)](https://github.com/TunnelDog/Cognizant-Flipper)
- [Color Guess v1.6 (By leedave)](https://github.com/leedave/Leeds-Flipper-Zero-Applications)
- [Connect Wires v1.2 (By AlexTaran)](https://github.com/AlexTaran/flipperzero)
- [CountDown v1.0 (By sistemasorp)](https://github.com/sistemasorp/flipper-countdown)
- [Crossy Road v1.0 (By Mikael098)](https://github.com/Mikael098/CrossyRoad-FlipperZero-)
- [DeadZone v0.2 (By retrooper)](https://github.com/retrooper/deadzone)
- [Decision Maker v1.0 (By Gerijacki)](https://github.com/Gerijacki/random_decision_maker)
- [Devilliers Platformer v0.1 (By adevil5)](https://github.com/adevil5/flipper-platformer-game)
- [Dice (RM) Including SEX/WAR/8BALL/WEED/DRINK DICE v2.5 (By RogueMaster)](https://github.com/RogueMaster/flipperzero-dice)
- [Digital Kaleidoscope v0.1 (By JamesR555)](https://github.com/JamesR555/digital_kaleidoscope)
- [DnD Dice v1.3 (By Ka3u6y6a)](https://github.com/Ka3u6y6a/flipper-zero-dice)
- [Doom v1.6 (By p4nic4ttack)](https://github.com/p4nic4ttack/doom-flipper-zero)
- [DopeWars v0.7.9 (By lordbuffcloud)](https://github.com/lordbuffcloud/flipper-ck42x-dopeflipper)
- [Drifter v0.2 (By jean-edouard)](https://github.com/jean-edouard/drifter)
- [DVD Bounce (By schaene)](https://github.com/schaene/Flipper-DVD-Bounce)
- [DVD Screensaver v1.1 (By shantih19)](https://github.com/shantih19/flipper_dvd_screensaver)
- [Etch-A-Sketch (By SimplyMinimal)](https://github.com/SimplyMinimal/FlipperZero-Etch-A-Sketch)
- [Executor Keychain v0.1 (By EstebanFuentealba)](https://github.com/EstebanFuentealba/Flipper-Keyller)
- [Fart Sound Generator v0.2 (By fgreil)](https://github.com/fgreil/mitzi-windbreak)
- [Fighter Jet v0.1.1 (By Erbonator3000)](https://github.com/Erbonator3000/flipper-fighter-jet)
- [Five Nights at Flippers v1.3 (By sillygir1)](https://github.com/sillygir1/flipperzero-fnaf)
- [Flappy Bird v1.5 (By DroomOne)](https://github.com/DroomOne/flipperzero-firmware/tree/dev/applications/flappy_bird)
- [Flight Assault v0.4 (By evillero)](https://github.com/evillero/flight_assault)
- [Flipcraft v4.3 (By apfxtech)](https://github.com/apfxtech/Flipcraft)
- [Flipper Hero v1.4 (By mentoster)](https://github.com/mentoster/Flipper-hero)
- [Flippy Road v1.1 (By rkilpadi)](https://github.com/rkilpadi/flippy-road)
- [FlipRogue v1.2.1 (By Abzac)](https://github.com/Abzac/fliprogue)
- [FMatrix v0.3.5 (By misterwaztaken)](https://github.com/misterwaztaken/fmatrix)
- [Fortune Cookie v0.1 (By evillero)](https://github.com/evillero/fortune_cookie)
- [Fortune Spinner v1.0 (By xantopren)](https://github.com/xantopren/fortune-spinner)
- [Free Roam v0.6 (By jblanked)](https://github.com/jblanked/Free-Roam) `Req: ESP32` with [FliperHTTP Firmware](https://github.com/jblanked/FlipperHTTP)
- [Furious Birds v1.1 (By bmstr-ru)](https://github.com/bmstr-ru/furious-birds)
- [Game of Life v1.3 (By Mad-MO)](https://github.com/Mad-MO/FlipperGoL)
- [GameBoy Emu v1.0 (By D4C1-Labs & jgilchrist)](https://github.com/D4C1-Labs/Flipper-ARF/tree/0d598b854c868067bf39111ae14bc8c2e195c26c/applications/system/FlipperGB)
- [Gauntlet v1.0 (By at0m-b0mb)](https://github.com/at0m-b0mb/Gauntlet-FlipperZero)
- [Geometry Dash v0.1 (By goosedev72-projects)](https://github.com/goosedev72-projects/flipperzero-geometryflip)
- [Ghouls v0.6 (By jblanked)](https://github.com/jblanked/Ghouls-Flipper) `Req: ESP32` with [FliperHTTP Firmware](https://github.com/jblanked/FlipperHTTP)
- [Groks Adventure v0.15 (By DigiMancer3D)](https://github.com/DigiMancer3D/Groks_Adventure)
- [Guess The Number v1.3 (By kWAYTV)](https://github.com/kWAYTV/guess-the-number-fz)
- [Gurpil v0.1.8 (By Endika)](https://github.com/Endika/flipper-gurpil)
- [Hangman v1.5 (By Evgeny Stepanischev & Eva Stepanischeva)](https://github.com/bolknote/Flipper-Zero-Hangman-Game)
- [Hanoi Towers v1.0 (By AlexTaran)](https://github.com/AlexTaran/flipperzero)
- [Heap Defence v1.4 (By xMasterX)](https://github.com/RogueMaster/flipperzero-firmware-wPlugins/commit/fc776446de9fdd553b221c02668b925b689378d8)
- [Hirn - Crack Color Codes v0.2 (By fgreil)](https://github.com/fgreil/mitzi-hirn)
- [Hold em v1.1 (By code-phreak)](https://github.com/code-phreak/flipper-holdem)
- [Hotspot Arcade v1.8.0 (By tarikbc)](https://github.com/tarikbc/hotspot-arcade) `Req: ESP32`
- [Hunter Killer v1.0 (By josephburnett)](https://github.com/josephburnett/hunter-flipper)
- [Impostor v0.1.3 (By Endika)](https://github.com/Endika/flipper-impostor-game)
- [Infinite Tic Tac Toe v0.1 (By kdiller713)](https://github.com/kdiller713/public-FlipperApps/tree/main/infinite_tic_tac_toe)
- [Insults v1.0 (By ERSUCC)](https://github.com/ERSUCC/Flipper-Insults)
- [IRONWYRM v1.0 (By Atompunk78)](https://github.com/Atompunk78/IRONWYRM)
- [JetPack v1.1 (by timstrasser)](https://github.com/RogueMaster/flipperzero-firmware-wPlugins/commit/1164e4d214efa38847e08c6387bf53578bc0d366)
- [Jumping Pawns v0.1 (By Tyl3rA)](https://github.com/Tyl3rA/Jumping-Pawns)
- [KC Line v1.2 (By HappyAmos)](https://github.com/HappyAmos/Flipper_FAPS/tree/master/kcline)
- [Laser Tag v2.3 (By RocketGod & jamisonderek)](https://github.com/RocketGod-git/Flipper-Zero-Laser-Tag)
- [Lifecounter v1.0 (By antsy)](https://github.com/antsy/Lifecounter) (Multi Counter remake)
- [Lights Out Flipper Zero v1.1 (By DigiMancer3D)](https://github.com/user/DigiMancer3D)
- [Magic 8-ball v0.1 (By stevenquinn)](https://github.com/stevenquinn/flipper-8-ball)
- [Mancala v0.1 (By fgreil)](https://github.com/fgreil/mitzi-mancala)
- [Mandelbrot Set v1.4 (By Possibly-Matt)](https://github.com/Possibly-Matt/flipperzero-firmware-wPlugins)
- [Matagotchi v1.1 (By MrModd)](https://github.com/MrModd/Matagotchi)
- [Maze 3D v6.12 (By k20120509)](https://github.com/k20120509/flipper-release)
- [Mind Probe v0.1 (By Miksang)](https://github.com/Miksang/flipper_guessing_game)
- [Minesweeper Redux v1.7 (By squee72564)](https://github.com/squee72564/F0_Minesweeper_Fap)
- [Minesweeper v1.2 (By panki27)](https://github.com/panki27/minesweeper)
- [Monster Slayer v0.1 (By ratmanZorry)](https://github.com/ratmanZorry/monster_slayer_flipper_zero)
- [Monty Hall v1.3 (By DevMilanIan)](https://github.com/RogueMaster/flipperzero-firmware-wPlugins/pull/203)
- [Morse Master v1.1 (By w84death)](https://github.com/w84death/morse-master)
- [MTP WIP v1.0 (By Alex4386)](https://github.com/Alex4386/f0-mtp)
- [Multi Counter v1.1 (By JadePossible)](https://github.com/JadePossible/Flipper-Multi-Counter)
- [Mystic Balloon v2.0 (By apfxtech)](https://github.com/apfxtech/FlipperMysticBalloon)
- [Nah 2 da Nah Nah Nah v0.2 (By DigiMancer3D)](https://github.com/DigiMancer3D/nah2nah3)
- [Network Defender v1.1 (By w84death)](https://github.com/w84death/flipper-zero-apps)
- [Nu Pogodi! (Ну, погоди!) v1.3 (By sionyx)](https://github.com/sionyx/flipper_nupogodi)
- [Oregon Trail v0.1 (By jlaughter)](https://github.com/jlaughter/flipper_oregon_trail)
- [P1X Adventure v0.1 (By w84death)](https://github.com/w84death/flipper-zero-apps)
- [P1X Your Own Adventure v0.1 (By w84death)](https://github.com/w84death/flipper-zero-apps)
- [PacMan WIP v0.1 (By DanieleMaffi)](https://github.com/DanieleMaffi/flipper-pacman)
- [Paint v1.4 (By n-o-T-I-n-s-a-n-e)](https://github.com/n-o-T-I-n-s-a-n-e)
- [Panis - A Grumpy Bread v0.1 (By fgreil)](https://github.com/fgreil/mitzi-panis)
- [Paper Plane v1.3 (By Larry-the-Pig)](https://github.com/Larry-the-Pig/flipper-plane)
- [Partition Panic v1.0 (By slavik0329)](https://github.com/slavik0329/partition-panic-flipper)
- [Pinball0 v0.5.2 (By rdefeo)](https://github.com/rdefeo/pinball0)
- [Pocket Battle+ v2.0 (By HermeticCode)](https://github.com/HermeticCode/showdown-current)
- [Pong v1.4 (By nmrr)](https://github.com/nmrr/flipperzero-pong)
- [Prince Of Arabia v1.0.12 (By apfxtech)](https://github.com/apfxtech/FlipperPrinceOfArabia)
- [Puck Girl v0.3 (By fgreil)](https://github.com/fgreil/mitzi-puck)
- [Quadrastic v1.0.2 (By ivanbarsukov)](https://github.com/ivanbarsukov/flipperzero-quadrastic)
- [Questions v1.1 (By nikilark)](https://github.com/nikilark/flipper_questions)
- [Race Game v1.0 (By mrc19056)](https://github.com/mrc19056/flipper-race-game)
- [Race v1.3 (By zyuhel)](https://github.com/zyuhel/flipperzero-racegame)
- [Reaction Test v1.3 (By Milk-Cool)](https://github.com/Milk-Cool/fz-reaction-game)
- [Reaction Time (By ihatecsv)](https://github.com/ihatecsv/flipper_reaction_time)
- [Reversi v1.4 (By dimat)](https://github.com/dimat/flipperzero-reversi)
- [Rock Paper Scissors v1.5 (By CodeAllNight/jamisonderek)](https://github.com/jamisonderek/flipper-zero-tutorials/tree/main/subghz/plugins/rock_paper_scissors)
- [Rock Paper Scissors 2 v1.1 (By benwoo1110)](https://github.com/benwoo1110/rps-FlipperZero)
- [Rock Paper Scissors 3 v1.0 (By xantopren)](https://github.com/xantopren/rock-paper-scissors)
- [Root of Life v1.4 (By Xorboo)](https://github.com/Xorboo/root-of-life)
- [Rubiks Cube Scrambler v1.4 (By RaZeSloth)](https://github.com/RaZeSloth/flipperzero-rubiks-cube-scrambler)
- [Rush Hour v0.1.6 (By Endika)](https://github.com/Endika/flipper-tutu)
- [Scorched Tanks v1.4 (By jasniec)](https://github.com/jasniec/flipper-scorched-tanks-game)
- [Secret Toggle (By nostrumuva)](https://github.com/nostrumuva/secret_toggle)
- [Simon Says v0.2 (By SimplyMinimal)](https://github.com/SimplyMinimal/FlipperZero-SimonSays)
- [Slot Machine v1.4 (By Daniel-dev-s)](https://github.com/Daniel-dev-s/flipperzero-slots)
- [Slots v1.0 (By vh8t)](https://github.com/vh8t/flipper-slots)
- [Snake 2.0 v2.4 (By Willzvul)](https://github.com/Willzvul/Snake_2.0/)
- [Snake v1.7 (By OlegSchwann)-OFW](https://github.com/flipperdevices/flipperzero-firmware/pull/829)
- [Sokoban v1.6 (By Racso)](https://github.com/Racso/fzero-apps)
- [Solitaire v2.0.2 (By doofy-dev)](https://github.com/doofy-dev/flipper_solitaire)
- [Space Impact II v1.0.2 (By Erbonator3000 & VoidXH)](https://github.com/Erbonator3000/Flipper-Zero-Space-Impact-II)
- [Space Impact WIP v0.2 (By Ka3u6y6a)](https://github.com/Ka3u6y6a/flipper-zero-space-impact)
- [Space Invaders v0.1 (By PavelZurek)](https://github.com/PavelZurek/flipperzero-space-invaders)
- [Stack v1.0 (By bergr22)](https://github.com/bergr22/Stack)
- [Stratagem Hero v1.0 (By maximkulkin)](https://github.com/maximkulkin/flipper-zero-stratagem-hero)
- [Strategem Zero v0.1 (By nymda)](https://github.com/nymda/StratagemZero)
- [Sudoku v1.2 (By profelis)](https://github.com/profelis/fz-sudoku)
- [SUPER-CHIP Emulator (By Milk-Cool)](https://github.com/Milk-Cool/fz-schip)
- [Swimmy Fish v2.5 (By Invizabel)](https://github.com/Invizabel/fish)
- [T-Rex Runner v1.5 (By Rrycbarm)](https://github.com/Rrycbarm/t-rex-runner)
- [TAMA P1: Save State & Layout Update (By DroomOne)](https://github.com/DroomOne/flipperzero-tamagotch-p1)
- [Tanks v1.4 (By Alexgr13)](https://github.com/alexgr13/flipperzero-firmware/tree/fork/dev/applications/tanks-game)
- [Tarot v0.2 (By pionaiki & tihyltew)](https://github.com/pionaiki/fz-tarot)
- [Tetris Modern v1.5 (By Alloy-Lou)](https://github.com/Alloy-Lou/all-the-plugins/tree/dev/base_pack/tetris_modern_game)
- [Tetris v1.6 (By jeffplang)](https://github.com/jeffplang/flipperzero-firmware/tree/tetris_game/applications/tetris_game)
- [Tic Tac Toe v1.3 (By gotnull)](https://github.com/gotnull/flipperzero-firmware-wPlugins)
- [Tic-Tac-Toe Multi v1.1 (By RouNNdeL)](https://github.com/RouNNdeL/flipper-tictactoe-multi)
- [Trivia Zero v0.1.12 (By Endika)](https://github.com/Endika/flipper-trivia-zero)
- [UART Pong v1.0 (By Olejka)](https://github.com/TheSainEyereg/flipper-uart-pong)
- [Ultimate Tic-Tac-Toe v1.1 (By Racso)](https://github.com/Racso/fzero-apps)
- [Umpire Indicator v0.1 (By RocketGod)](https://github.com/RocketGod-Git/Flipper-Zero-Umpire-Indicator)
- [Vexed v1.0.1 (By dlvoy)](https://github.com/dlvoy/flipper-zero-vexed)
- [Video Game Module Tool v1.3 (By gsurkov)](https://github.com/flipperdevices/flipperzero-good-faps/pull/127) `Req: Video Game Module By Flipper Devices`
- [Video Poker v1.5 (By PixlEmly)](https://github.com/PixlEmly/flipperzero-firmware-testing/blob/420/applications/VideoPoker/poker.c)
- [VL6180X Pong v0.1 (By kdiller713)](https://github.com/kdiller713/public-FlipperApps/tree/main/vl6180x_pong) `Req: Adafruit VL6180X`
- [VolorSavanna v1.0 (By Invizabel)](https://github.com/Invizabel/VolorSavanna)
- [Wave v0.2 (By Sladkisnovraper)](https://github.com/Sladkisnovraper/Wave-game-for-Flipper-Zero)
- [Wolfenduino v1.0 (By apfxtech)](https://github.com/apfxtech/FlipperWolfenstein)
- [Yappy Invaders (By dagnazty)](https://github.com/dagnazty/Yappy_Invaders)
- [Yatzee v1.2 (By emfleak)](https://github.com/emfleak/flipperzero-yatzee)
- [Zero v1.4 (By Racso)](https://github.com/Racso/fzero-apps)
- [Zombiez v1.3 (Reworked By DevMilanIan)](https://github.com/RogueMaster/flipperzero-firmware-wPlugins/pull/240)

## Apps / Plugins

- [24Cxx Programmer v2.1 (By kamylwnb)](https://github.com/kamylwnb/24cxxprog)
- [Access Audit v1.13 (By matthewkayne)](https://github.com/matthewkayne/flipper-access-audit)
- [Agentic Remote USB/BT v0.28 (By Wet-wr-Labs)](https://github.com/Wet-wr-Labs/claupper)
- [Air Level v1.0 (By jamisonderek)](https://github.com/jamisonderek/flipper-zero-tutorials/tree/main/vgm/air_level) `Req: Video Game Module By Flipper Devices`
- [Air Stats v1.0 (By thevan4)](https://github.com/thevan4/flipper-air-stats) Req: [See App Readme For Supported Modules](https://github.com/RogueMaster/flipperzero-firmware-wPlugins/blob/420/applications/external/air_stats/README.md)
- [Allstar Firefly v0.1 (By jlaughter)](https://github.com/jlaughter/flipper-allstar-firefly)
- [AmiiTool v0.3 (By Firefox2100)](https://github.com/Firefox2100/ami_tool) `Req: key_retail.bin on SD card /nfc/assets`
- [Amusement IC v0.1 (By object-Object)](https://github.com/object-Object/flip_aic)
- [Analog Clock v1.2 (By scrolltex)](https://github.com/scrolltex/flipper_analog_clock)
- [Analog Output v0.1 (By jamisonderek)](https://github.com/jamisonderek/flipper-zero-tutorials/tree/main/gpio/dac)
- [Android KB Bridge v0.5.8 (By andybeg)](https://github.com/andybeg/AndroidFlipperZeroKBD)
- [Anim Previewer v1.0 (By H4W9)](https://github.com/H4W9/AnimationPreviewerFlipperApp)
- [Animation Switcher v1.1 (By lsalik2)](https://github.com/lsalik2/FlipperAnimationSwitcher)
- [Anki Remote v1.3 (By Blue5GD)](https://github.com/Blue5GD/Anki-Remote)
- [Answer Book v0.1 (By redbson)](https://github.com/redbson/The-Book-of-Answers)
- [APRS TX v1.3.24 (By yo3gnd)](https://github.com/yo3gnd/flipper-zero-aprs-tx)
- [Argus v1.0 (By at0m-b0mb)](https://github.com/at0m-b0mb/Argus-FlipperZero) `Req: ESP32`
- [ASCII Table v1.0 (By x10102)](https://github.com/x10102/flipper_ascii)
- [Atari SIO Emulator v0.5 (By cepetr)](https://github.com/cepetr/sio2flip)
- [ATM Player v1.0.8 (By apfxtech)](https://github.com/apfxtech/FlipperATM)
- [Atomic Dice Roller v1.1 (By nmrr)](https://github.com/nmrr/flipperzero-atomicdiceroller)
- [Authenticator/TOTP v5.19.0 (By akopachov)](https://github.com/akopachov/flipper-zero_authenticator)
- [AVR Flasher v1.5 (By Skorpionm)](https://github.com/flipperdevices/flipperzero-firmware/pull/2475)
- [AZERTY Mouse Jacker v1.2 (By mothball187)](https://github.com/mothball187/flipperzero-nrf24/tree/main/mousejacker) `Req: NRF24` (Uses AZERTY keyboard - Source Unknown)
- [Bad Duck3 v1.1 (By dutchpatriot)](https://github.com/dutchpatriot/all-the-plugins/tree/833335354aa46fe605b494c1b3bf11afb5b362f9/non_catalog_apps/bad_duck3)
- [Bad KB (By WillyJL)](https://github.com/RogueMaster/flipperzero-firmware-wPlugins/pull/739) (Part of BadUSB)
- [Badge Audit v1.0 (By ViperDroid)](https://github.com/ViperDroid/flipper-badge-audit)
- [BadUSB Pro v1.0 (By barkandbite)](https://github.com/barkandbite/flipper_suite)
- [BadUSB wPlaceholders v1.0 (By whitewhidow)](https://github.com/whitewhidow/flipper-badusb-ph)
- [Bambu RID v0.2 (By DanTheMan827)](https://github.com/DanTheMan827/bambu-rfid-flipper)
- [Barcode App v1.5 (By Kingal1337)](https://github.com/Kingal1337/flipper-barcode-generator)
- [BarCode Scanner Emulator (By polarikus)](https://github.com/polarikus/flipper-zero_bc_scanner_emulator) `Uses: COM-port`
- [Battery Checker v0.2 (By Programistich)](https://github.com/Programistich/battery_checker)
- [Big Clock v1.3 (By Eris-Margeta)](https://github.com/Eris-Margeta/flipper-apps)
- [BlackJack Counter v1.0 (By grugnoymeme)](https://github.com/grugnoymeme/flipperzero-blackjack_counter-fap)
- [BLE Clicker v1.0 (By random1st)](https://github.com/random1st/flipper-ble-clicker)
- [BLE Killer v1.1 (By H4lo)](https://github.com/H4lo/flipper_zero_ble_killer) `Req: ESP32`
- [BLE Scanner v1.0 (By barkandbite)](https://github.com/barkandbite/flipper_suite) `Req: ESP32`
- [BLE Spam v6.6 (By WillyJL & ECTO-1A & Spooks4576 with research from xMasterX; OFW API thanks to noproto)](https://github.com/Next-Flip/Momentum-Apps/tree/dev/ble_spam)
- [Bluetooth Bridge v0.1 (By excitoon)](https://github.com/excitoon/zero-bluetooth-bridge)
- [Bluetooth/USB Remote v1.8 (By Cutch)-OFW](https://github.com/flipperdevices/flipperzero-firmware/pull/1330)
- [BME280 Vario v1.1 (By xumbax)](https://github.com/xumbax/flipper-vario) `Req: BME280`
- [BMI Air Mouse v1.4 (By ginkage)](https://github.com/ginkage/FlippAirMouse/) `Req: BMI160 or GY-521`
- [BPM Tapper v1.3 (By panki27)](https://github.com/panki27/bpm-tapper)
- [Brainfuck v1.5 (By nymda)](https://github.com/nymda/FlipperZeroBrainfuck)
- [BreachMap v1.0 (By Gerijacki)](https://github.com/Gerijacki/BreachMap)
- [BT Audio v1.2 (By FatherDivine)](https://github.com/FatherDivine/flipperzero-mayhem/tree/bt_audio/flipper_companion_apps/bt_audio) `Req: ESP32 w BT` & [This FW](https://github.com/RogueMaster/flipperzero-firmware-wPlugins/tree/420/applications/external/bt_audio/.esp32_firmware)
- [BT Remote for Kodi v1.2 (Remixed By coded-with-claws)](https://github.com/coded-with-claws/flipperzero-tools/tree/main/applications_user/bt_hid_kodi)
- [BT Trigger v1.5 (By Nem0oo)](https://github.com/Nem0oo/flipper-zero_ios-bluetooth-trigger)
- [BTHome v0.1 (By ghedo)](https://github.com/ghedo/flipper-bthome)
- [BunnyConnect v1.0 (By C0d3-5t3w)](https://github.com/C0d3-5t3w/flipper-bunnyconnect) `Req: BashBunny`
- [C5 Lab v0.41 (By C5Lab / Lab Team)](https://github.com/C5Lab/projectZero)
- [Caesar Cipher v1.3 (By panki27)](https://github.com/panki27/caesar-cipher)
- [Calculator v1.4 (By n-o-T-I-n-s-a-n-e)](https://github.com/n-o-T-I-n-s-a-n-e)
- [Calendar (By Adiras)](https://github.com/Adiras/flipperzero-calendar)
- [Calendar of weeks v0.1 (By fgreil)](https://github.com/fgreil/mitzi-cal-weeks)
- [Calendar TECHART v1.1 (By techartdev)](https://github.com/techartdev/Calendar)
- [Camera (By Z4urce)](https://github.com/Z4urce/flipper-camera) `Req: ESP32-CAM`
- [CAN Commander v2.3.1 (By MatthewKuKanich)](https://github.com/MatthewKuKanich/CAN_Commander) `Req: CAN Commander Board`
- [CAN Tools v1.1 (By MatthewKuKanich)](https://github.com/MatthewKuKanich/FlipperCANTools)
- [CAN Transceiver v0.1 (By Chainmanner)](https://github.com/Chainmanner/Flipper-CAN-Transceiver)
- [CANBUS ATTACK v1.0 (By jorgepnaranjo25)](https://github.com/jorgepnaranjo25/CAN-Bus-Attack) `Req: CANBUS By ElectronicCats`
- [CANBUS v1.1.6.2 (By ElectronicCats)](https://github.com/ElectronicCats/flipper-MCP2515-CANBUS) `Req: CANBUS By ElectronicCats`
- [Canon Intervalometer v1.0 (By TravisCG)](https://github.com/TravisCG/canonivm)
- [CCID Emu v1.0 (By kidbomb)](https://github.com/kidbomb/flipper-ccid-app)
- [CCID Emulator v1.1 (By barkandbite)](https://github.com/barkandbite/flipper_suite)
- [Cerberus v1.1 (By at0m-b0mb)](https://github.com/at0m-b0mb/flipper-cerberus)
- [CFS Tag Writer v1.0 (By pwschattenberg)](https://github.com/pwschattenberg/cfs-tag-writer)
- [CFW Settings v2.1 (By WillyJL)](https://github.com/Next-Flip/Momentum-Firmware/tree/dev/applications/main/momentum_app)
- [Chameleon Ultra v1.0 (By muylder)](https://github.com/muylder/Chameleon_Flipper)
- [ChaosID v0.6 (By cybersantana83)](https://github.com/cybersantana83/ChaosID)
- [Chief Cooker v1.1 (By denr01)](https://github.com/denr01/FZ-ChiefCooker)
- [Chronometer (By nmrr)](https://github.com/nmrr/flipperzero-chronometer)
- [CK42X Passvault v0.4.6 (By lordbuffcloud)](https://github.com/lordbuffcloud/flipper-ck42x-passvault)
- [CK42X Wakeup v2.29 (By lordbuffcloud)](https://github.com/lordbuffcloud/flipper-ck42x-wakeup)
- [Claude Buddy v0.6 (By jxw1102)](https://github.com/jxw1102/flipper-claude-buddy)
- [ClayLoop v1.0 (By b0bby225)](https://github.com/b0bby225/ClayLoop)
- [Click Recorder v1.2 (By 0x78f1935)](https://github.com/0x78f1935/Click-Recorder)
- [Clippy v0.1 (By mtormento)](https://github.com/mtormento/clippy)
- [Clock v1.2 (By kowalski7cc)](https://github.com/kowalski7cc/flipperzero-firmware/tree/clock-v1)
- [CO2 Logger v0.4 (By harryob2)](https://github.com/harryob2/co2_logger) `Req: MH-Z19`
- [Cocktail Book v0.4 (By resu95)](https://github.com/resu95/flipper_cocktail_book)
- [Coffee-EEPROM (By wh00hw)](https://github.com/wh00hw/Coffee-EEPROM-FAP) For ethical pentest only.
- [COGS MyKey v1.1 (By lupettohf)](https://github.com/lupettohf/mykey)
- [Combo Cracker v0.5 (By CharlesTheGreat77)](https://github.com/CharlesTheGreat77/ComboCracker-FZ)
- [Continuity Tester (By benallen-dev)](https://github.com/benallen-dev/flipper-zero-apps/tree/main/continuity)
- [Cookie Clicker v1.1 (By jamisonderek)](https://github.com/jamisonderek/flipper-zero-tutorials/tree/main/hid/hid_cookie)
- [Count Down Timer v1.5 (By 0w0mewo)](https://github.com/0w0mewo/fpz_cntdown_timer)
- [Counter v1.5 (By Krulknul & TEXploder)](https://github.com/TEXploder/TEX-Counter)
- [Cross Remote v3.4 (By leedave)](https://github.com/leedave/flipper-zero-cross-remote)
- [Crypto Dictionary v0.1 (By armixz)](https://github.com/armixz/Flipper-Zero-Crypto-Dictionary)
- [Cyborg Detector v1.1 (By RocketGod-Git)](https://github.com/RocketGod-Git/Flipper-Zero-Cyborg-Detector)
- [Dab Timer v2.2 (By RogueMaster)](https://github.com/RogueMaster/flipperzero-dabtimer)
- [Daikin64 AC Remote v0.1 (By mrcalifornium)](https://github.com/mrcalifornium/flipperzero-daikin64-ac-remote)
- [Dallas Tester v0.1 (By mishamyte)](https://github.com/mishamyte/flipper-dallas-tester)
- [DAP Link v1.4 (By DrZlo13)-OFW](https://github.com/flipperdevices/flipperzero-firmware/pull/1897)
- [DCF77 Clock Spoof v1.4 (By mdaskalov & molodos)](https://github.com/mdaskalov/dcf77-clock-sync)
- [DCF77 Transmitter v2.0 (By arha)](https://github.com/arha/flipper-dcf77)
- [Deauther PWNDTOOLS V2.6.0 (By HEX0DAYS)](https://github.com/HEX0DAYS/FlipperZero-PWNDTOOLS) `Req: ESP8266` [Original](https://github.com/SpacehuhnTech/esp8266_deauther)
- [DelfiRTL v0.1 (By gorebrau)](https://github.com/gorebrau/delfyRTL) `Req: RTL8720DN`
- [Device Info (By hedger)](https://github.com/hedger/flipperdevinfo)
- [Digimon F-Com v1.5 (By TylerWilley)](https://github.com/TylerWilley/flipper-f-com)
- [Disn3y Toolbox v0.1 (By Nathaniel.Belles)](https://gitlab.com/Nathaniel.Belles/flipper-disney-toolbox)
- [Distance Sensor v1.1 (By Sanqui)](https://github.com/Sanqui/flipperzero-firmware/tree/hc_sr04) `Req: HC-SR04`
- [Distance Sensor v1.2 (By privet971)](https://github.com/privet971/FlipperZeroApps/tree/main/usping) `Req: Parallax (3 wires) SKU 28015`
- [Doc Viewer Lite v0.1 (By C0d3-5t3w)](https://github.com/C0d3-5t3w/docviewlite)
- [DroidBeacon v0.1 (By spandox2)](https://github.com/spandox2/droidbeacon)
- [DSTIKE Deauther v1.1 (By SequoiaSan)](https://github.com/SequoiaSan/FlipperZero-Wifi-ESP8266-Deauther-Module/tree/FlipperZero-Module-v2/FlipperZeroModule/FlipperZero-ESP8266-Deauth-App) `Req: ESP8266`
- [DTMF Dolphin v1.5 (By litui)](https://github.com/litui/dtmf_dolphin)
- [E220 LoRa Configurator v0.1 (By crackerjacques)](https://github.com/crackerjacques/Flipper_E220_Lora_Module_Configurator) `Req: E220-900T22S(JP)`
- [Ear Trainer v1.1 (By barismert98)](https://github.com/barismert98/flipper-ear-trainer)
- [EM4100 Key Generator v1.1 (By Milk-Cool)](https://github.com/Milk-Cool/fz-em4100-generator)
- [EMV Reader v0.1 (By AmsaOne)](https://github.com/AmsaOne/Flipper-EMV-Reader)
- [Encoder Reader v0.1 (By Engineegor)](https://github.com/Engineegor/Enc_reader)
- [Enhanced Sub-Ghz Chat v1.3 (By twisted-pear)](https://github.com/twisted-pear/esubghz_chat)
- [Enigma v1.1 (By xtruan)](https://github.com/xtruan/flipper-enigma)
- [ESP Flasher v1.9 (By 0xchocolate)](https://github.com/0xchocolate/flipperzero-esp-flasher)
- [ESP WiFi Scanner v1.1 (By SequoiaSan)](https://github.com/SequoiaSan/FlipperZero-WiFi-Scanner_Module-ESP8266) `Req: ESP8266 or ESP32`
- [ESP32 Camera Suite v1.9 (By CodyTolene)](https://github.com/CodyTolene/Flipper-Zero-Camera-Suite)
- [ESP32 Gravity v0.6.0 (By chris-bc)](https://github.com/chris-bc/Flipper-Gravity) `Req: ESP32`
- [ESP32 WIFI Mapping v0.2 (By carvilsi)](https://github.com/carvilsi/flipper0-wifi-map) `Req: ESP32` & [ESP32 FW](https://github.com/carvilsi/esp32-wifi-map)
- [ESP32CAM Camera v1.0.0 (By Z4urce & eried)](https://github.com/eried/flipperzero-mayhem) `Req: ESP32-CAM (Mayhem)`
- [ESP32CAM Marauder v0.7.5 (By 0xchocolate & eried)](https://github.com/eried/flipperzero-mayhem) `Req: ESP32-CAM (Mayhem)`
- [ESP32CAM Morse Flasher v0.1 (By eried)](https://github.com/eried/flipperzero-mayhem) `Req: ESP32-CAM (Mayhem)`
- [ESP32CAM Motion Detection v0.1 (By eried)](https://github.com/eried/flipperzero-mayhem) `Req: ESP32-CAM (Mayhem)`
- [ESP32CAM Nanny Cam v0.1 (By eried)](https://github.com/eried/flipperzero-mayhem) `Req: ESP32-CAM (Mayhem)`
- [ESP32CAM QR Code v0.1 (By eried)](https://github.com/eried/flipperzero-mayhem) `Req: ESP32-CAM (Mayhem)`
- [Ethernet ENC28J60 v2.0 (By ElectronicCats)](https://github.com/ElectronicCats/flipper-ENC28J60-ethernet/tree/f3b79ffc670f3f7b61dafe98a933f49e5d344e65)
- [Ethernet W5500 v1.3 (By karasevia)](https://github.com/karasevia/finik_eth) `Req: W5500`
- [Evil BLE v1.0 (By barkandbite)](https://github.com/barkandbite/flipper_suite) `Req: ESP32`
- [Evil BW16 Controller v1.0 (By dagnazty)](https://github.com/dagnazty/Flipper_Apps)
- [Evil Portal v0.6 (By bigbrodude6119)](https://github.com/bigbrodude6119/flipper-zero-evil-portal)
- [Eye Saver v0.1 (By paul-sopin)](https://github.com/paul-sopin/flipper-eye-saver)
- [Fake Chip Detector v0.7 (By hleserg)](https://github.com/hleserg/flipper-fake-chip-detector)
- [FAP Boilerplate v1.3 (By leedave)](https://github.com/leedave/flipper-zero-fap-boilerplate)
- [Faraday v1.2 (By at0m-b0mb)](https://github.com/at0m-b0mb/Faraday-FlipperZero)
- [FCC ID Lookup v0.1 (By lrehmann)](https://github.com/lrehmann/fcc-id-lookup-flipper)
- [FDX-B Maker v1.0 (By snowsign)](https://github.com/snowsign/fdxb-maker)
- [Fencing Test Box v0.1 (By aarjaneiro)](https://www.github.com/aarjaneiro/fencing_testbox)
- [Financial Calculator v1.0 (By schaene)](https://github.com/schaene/Flipper-Financial-Calculator)
- [FindMy Flipper v3.5.2 (By MatthewKuKanich)](https://github.com/MatthewKuKanich/FindMyFlipper)
- [Fire String v2.0 (By RyanAboueljoud)](https://github.com/RyanAboueljoud/Fire-String)
- [Flame RNG v0.3 (By OrionW06)](https://github.com/OrionW06/Flame-RNG)
- [Flashlight v1.3 (By xMasterX)](https://github.com/xMasterX/flipper-flashlight)
- [Flight Monitor v1.0 (By kamylwnb)](https://github.com/kamylwnb/flight_monitor)
- [Flipagotchi v2.1.1 (By Matt-London)](https://github.com/Matt-London/pwnagotchi-flipper)
- [FlipBIP Crypto Wallet v1.18 (By xtruan)](https://github.com/xtruan/FlipBIP)
- [FlipBoard Blinky v3.6 (By jamisonderek)](https://github.com/jamisonderek/flipboard)
- [FlipBoard Keyboard v3.9 (By jamisonderek)](https://github.com/jamisonderek/flipboard)
- [FlipBoard Signal v3.12 (By jamisonderek)](https://github.com/jamisonderek/flipboard)
- [FlipBoard Simon v1.10 (By jamisonderek)](https://github.com/jamisonderek/flipboard)
- [FlipCrypt v0.6 (By TAxelAnderson)](https://github.com/TAxelAnderson/FlipCrypt)
- [FlipDeck v3.0 (By ismail-oe)](https://github.com/ismail-oe/flipdeck)
- [FlipDeFlock v0.77 (By ReconGrunt)](https://github.com/ReconGrunt/FlipDeFlock) `Req: ESP32`
- [FlipDownloader v1.3.5 (By jblanked)](https://github.com/jblanked/FlipStore) `Req: ESP32` with [FliperHTTP Firmware](https://github.com/jblanked/FlipperHTTP)
- [Flipfetch v0.1 (By alexroses47)](https://github.com/alexroses47/flipper-flipfetch)
- [FlipFlash v1.0 (By Mushi0)](https://github.com/Mushi0/FlippFlash)
- [FlipGemini v1.0 (By jblanked)](https://github.com/jblanked/FlipGemini) `Req: ESP32` with [FliperHTTP Firmware](https://github.com/jblanked/FlipperHTTP)
- [FlipIRFreq v0.2 (By jsammarco)](https://github.com/jsammarco/FlipIRFreq)
- [FlipLibrary v1.5 (By jblanked)](https://github.com/jblanked/FlipLibrary) `Req: ESP32` with [FliperHTTP Firmware](https://github.com/jblanked/FlipperHTTP)
- [FlipMap v0.2.2 (By jblanked)](https://github.com/jblanked/FlipMap) `Req: ESP32` with [FliperHTTP Firmware](https://github.com/jblanked/FlipperHTTP)
- [FlipNote v1.0 (By morty517)](https://github.com/morty517/flipnote)
- [FlipPar v0.1 (By jsammarco)](https://github.com/jsammarco/FlipPar)
- [FlipPass v2.0 (By Arckanos)](https://github.com/Arckanos/FlipPass)
- [Flipper Aid v1.0 (By spaghety)](https://github.com/spaghety/flipper-aid)
- [Flipper Blackhat v0.4.1 (By o7-machinehum)](https://github.com/o7-machinehum/flipper-blackhat-app) `Req: Blackhat Board` with [Blackhat OS](https://github.com/o7-machinehum/flipper-blackhat-os)
- [Flipper Printer v1.0 (By alxcrt)](https://github.com/alxcrt/flipper-zero-thermal-printer) `Req: T7-US thermal printer module`
- [Flipper Share iButton v0.1 (By lomalkin)](https://github.com/lomalkin/flipper-zero-apps/blob/-/flipper_share_ibutton)
- [Flipper Share IR v1.5 (By lomalkin)](https://github.com/lomalkin/flipper-zero-apps/tree/dev/flipper_share_ir)
- [Flipper Share NFC v1.5 (By lomalkin)](https://github.com/lomalkin/flipper-zero-apps/blob/-/flipper_share_nfc)
- [Flipper Share RFID v0.1 (By lomalkin)](https://github.com/lomalkin/flipper-zero-apps/blob/-/flipper_share_rfid)
- [Flipper Share v1.5 (By lomalkin)](https://github.com/lomalkin/flipper-zero-apps/tree/dev/flipper_share)
- [Flipper Tag v1.0 (By thejudge156)](https://github.com/thejudge156/flippertag)
- [Flipper Wedge v1.1 (By DangerousThings)](https://github.com/DangerousThings/flipper-wedge)
- [Flipper95 v1.1 (By CookiePLMonster)](https://github.com/CookiePLMonster/flipper-bakery)
- [FlipperPwn v1.7 (By barkandbite)](https://github.com/barkandbite/flipper_suite)
- [FlipperTrack v1.0 (By pinkpwningclub)](https://github.com/pinkpwningclub/flipperTrack)
- [FlipperZero Clock v1.3 (By mdaskalov)](https://github.com/mdaskalov/flipperzero-clock)
- [Flippy Temp WIP v0.1 (By n0ciple)](https://github.com/n0ciple/flippy_sensors) `Req: Flippy Sensor Board`
- [FlipRPI v1.0 (By jblanked)](https://github.com/jblanked/FlipRPI) `Req: ESP32` with [FliperHTTP Firmware](https://github.com/jblanked/FlipperHTTP)
- [FlipSocial v2.0.9 (By jblanked)](https://github.com/jblanked/FlipSocial) `Req: ESP32` with [FliperHTTP Firmware](https://github.com/jblanked/FlipperHTTP)
- [FlipTDI v1.1 (By Skorpionm)](https://github.com/flipperdevices/flipperzero-good-faps/pull/234)
- [FlipTelegram v1.0.1 (By jblanked)](https://github.com/jblanked/FlipTelegram) `Req: ESP32` with [FliperHTTP Firmware](https://github.com/jblanked/FlipperHTTP)
- [FlipTok Live v1.0 (By kamylwnb)](https://github.com/kamylwnb/FlipTok_Live_Chat)
- [FlipTrader v1.2 (By jblanked)](https://github.com/jblanked/FlipTrader) `Req: ESP32` with [FliperHTTP Firmware](https://github.com/jblanked/FlipperHTTP)
- [FlipWeather v1.4 (By jblanked)](https://github.com/jblanked/FlipWeather) `Req: ESP32` with [FliperHTTP Firmware](https://github.com/jblanked/FlipperHTTP)
- [FlipWifi v1.6.3 (By jblanked)](https://github.com/jblanked/FlipWiFi) `Req: ESP32` with [FliperHTTP Firmware](https://github.com/jblanked/FlipperHTTP)
- [FlipWorld v1.0.4 (By jblanked)](https://github.com/jblanked/FlipWorld) `Req: ESP32` with [FliperHTTP Firmware](https://github.com/jblanked/FlipperHTTP)
- [Flizzer Tracker v0.11 (By LTVA1)](https://github.com/LTVA1/flizzer_tracker)
- [Flo Tracker v1.0 (By harimwoo)](https://github.com/harimwoo/flipper-flo-tracker)
- [FM Radio v2.2 (By coolshrimp)](https://github.com/coolshrimp/flipperzero-fm-radio) `Req: TEA5767/Si4703`
- [FM Radio w Vol v0.10 (By coolshrimp & pchmielewski1)](https://github.com/pchmielewski1/tea5767-pt2257-fm-radio-with-volume-control) `Req: TEA5767+PT2257`
- [FM Transmitter KT0803 v1.0 (By goosedev72-projects)](https://github.com/goosedev72-projects/flipperzero-kt0803-transmitter) `Req: KT0803`
- [FM TX v0.1.0 (By yo3gnd)](https://github.com/yo3gnd/fmtx)
- [FMF2USBMIDI v0.1 (By crackerjacques)](https://github.com/crackerjacques/Flipper_Zero_FMF2USBMIDI)
- [Ford Radio Codes v0.1 (By DavidB445)](https://github.com/DavidB445/fz_fordradiocodes)
- [FreeStyle Libre CGM v0.1 (By darendarrow)](https://github.com/darendarrow/flipnCGM)
- [Frequency Analyzer (External) v1.2 (By xMasterX)](https://github.com/xMasterX/all-the-plugins/tree/dev/non_catalog_apps/freq_analyzer_ext)
- [FRSScan v0.2 (By thecybermind)](https://github.com/thecybermind/frsscan)
- [FZ nRF24 Jammer v1.4 (By W0rthlessS0ul)](https://github.com/W0rthlessS0ul/FZ_nRF24_jammer)
- [Gas Sensor v0.1 (By iancohee)](https://github.com/iancohee/gas_sensor_fap) `Req: BME680 / SGP30`
- [Gatekeeper v1.1 (By enexis1337)](https://github.com/enexis1337/Gatekeeper)
- [Gauge Tool v0.1 (By skotopes)](https://github.com/skotopes/flipperzero_gauge_tool) (Used To Downgrade From 10/07/2024 Update)
- [GB Printer v0.5 (By kbembedded)](https://github.com/kbembedded/flipper-gb-printer) `Req: GB Link Cable / Malveke`
- [Gemini IA v1.1 (By d4rks1d33 & jamisonderek)](https://github.com/d4rks1d33/Gemini-Flipper)
- [Genie Recorder v3.1 (By jamisonderek)](https://github.com/jamisonderek/flipper-zero-tutorials/tree/main/subghz/apps/genie-recorder)
- [Ghost ESP v1.8.2 (By jaylikesbunda & Spooks4576)](https://github.com/Spooks4576/ghost_esp_app) `Req: ESP32` with [Ghost ESP Firmware](https://github.com/Spooks4576/Ghost_ESP)
- [GhostBook v0.6 (By digitard)](https://github.com/digitard/ghostbook)
- [GhostTag v1.0 (By at0m-b0mb)](https://github.com/at0m-b0mb/GhostTag-FlipperZero) `Req: ESP32`
- [GlitchTrigger v1.3 (By at0m-b0mb)](https://github.com/at0m-b0mb/GlitchTrigger-FlipperZero)
- [GnomishTool v1.0 (By Andreeved88)](https://github.com/Andreeved88/GnomishTool)
- [Govee Control v0.1 (By devdotbo)](https://github.com/devdotbo/GlowFin)
- [GPIO 7-Segment Output v1.3 (By jamisonderek)](https://github.com/jamisonderek/flipper-zero-tutorials/tree/main/gpio)
- [GPIO Badge v1.0 (By jamisonderek)](https://github.com/jamisonderek/flipper-zero-tutorials/tree/main/gpio/gpio_badge)
- [GPIO Controller (By Lokno)](https://github.com/Lokno/gpio_controller)
- [GPIO Explorer v1.0 (By EvgeniGenchev07)](https://github.com/EvgeniGenchev07/gpio_explorer)
- [GPIO Reader (biotinker) (By biotinker)](https://github.com/biotinker/flipperzero-gpioreader)
- [GPIO Reader v1.2 (Aurelilc) (By aureli1c)](https://github.com/aureli1c/flipperzero_GPIO_read)
- [GPIO w I2C (By gornekich)-OFW] with [I²C-Scanner #1431 (By GitChris3004)](https://github.com/flipperdevices/flipperzero-firmware/pull/1431) and [read EEPROM of SFP Modules using I2C (By marcusju)](https://github.com/RogueMaster/flipperzero-firmware-wPlugins/pull/198)
- [GPS Track v1.0 (By xumbax)](https://github.com/xumbax/GPS-Track) `Req: NMEA GPS Module`
- [GPS v1.5 (By ezod)](https://github.com/ezod/flipperzero-gps) `Req: NMEA 0183`
- [GS1 Parser v0.2 (By kdiller713)](https://github.com/kdiller713/public-FlipperApps/tree/main/gs1_rfid_parser) `Req: U107 or YRM100`
- [Guido Music Score Reader v0.1 (By fgreil)](https://github.com/fgreil/mitzi-guido)
- [H1030L Writer v1.3 (By Zacvr)](https://github.com/Zacvr/h10301_writer)
- [Habit Flow v0.1.3 (By Endika)](https://github.com/Endika/flipper-habit-flow)
- [Ham Scanner v1.1 (By Clawzman)](https://github.com/Clawzman/Flipper-HAM-Scanner)
- [Hard Hat Brigade IR v0.1 (By Anomalous68)](https://github.com/Anomalous68/HHB-Flipper-App) `Req: Hard Hat Brigade Hat`
- [HC-11 Modem v1.3 (By Giraut)](https://github.com/Giraut/flipper_zero_hc11_wireless_modem) `Req: HC-11`
- [Hermes v1.3 (By at0m-b0mb)](https://github.com/at0m-b0mb/Hermes-FlipperZero)
- [HEX Editor v1.4 (By dunaevai135)](https://github.com/dunaevai135/flipper-zero-hex_editor)
- [HEX Viewer v2.0 (By QtRoS)](https://github.com/QtRoS/flipperzero-firmware)
- [HID Exfil v1.0 (By barkandbite)](https://github.com/barkandbite/flipper_suite)
- [HID File Transfer v1.0 (By Kavakuo)](https://github.com/Kavakuo/HID-File-Transfer)
- [Hitachi AC Remote v1.1 (By dogtopus)](https://github.com/dogtopus/flipperzero-hitachi-ac-remote)
- [HTW AC Remote v1.2 (By sokogen)](https://github.com/sokogen/flipperzero-htw-ac-remote)
- [HyperFocus Calc v0.1.2 (By Endika)](https://github.com/Endika/flipper-hyper-focus-calc)
- [I2C BMS Reader v0.4 (By gazirov)](https://github.com/gazirov/battery_reader) `Req: TI BQ30/BQ40 controllers`
- [I2C Explorer v0.9 (By 4mb3rz)](https://github.com/4mb3rz/Flipper-I2C-Explorer)
- [I2C Tools Cli v1.0 (By Tristus1er)](https://github.com/Tristus1er/flipperzero-i2c-tools-cli)
- [i2c Tools v1.2 (By NaejEL)](https://github.com/NaejEL/flipperzero-i2ctools)
- [iButton Converter v1.1 (By Leptopt1los)](https://github.com/Leptopt1los/ibutton_converter)
- [iButton Fuzzer v1.8 (By gid9798)](https://github.com/DarkFlippers/unleashed-firmware/pull/507)
- [IconEdit v0.7.1 (By rdefeo)](https://github.com/rdefeo/iconedit)
- [ID Card v3.0 (By evillero)](https://github.com/evillero/ID_card_v2)
- [IFTTT Button (By Ferrazzi)](https://github.com/Ferrazzi/FlipperZero_IFTTT_Virtual_Button) `Req: ESP8266 w/ IFTTT FW Flashed`
- [Image Scroller v0.1 (By fgreil)](https://github.com/fgreil/mitzi-scroller)
- [Image Viewer v0.1 (By polioan)](https://github.com/polioan/flipper-zero-image-viewer)
- [INA Meter v0.8 (By cepetr)](https://github.com/cepetr/flipper-tina)
- [InfraFi v1.2 (By amd989)](https://github.com/amd989/InfraFi)
- [Infrared Playback v0.2 (By kdiller713)](https://github.com/kdiller713/public-FlipperApps)
- [Internet Time Screen v0.2 (By sureserverman)](https://github.com/sureserverman/internet-time-screen)
- [Intervalometer v1.2 (By theageoflove)](https://github.com/theageoflove/flipperzero-zeitraffer)
- [IR Blaster v1.1 (By mateusz-szafraniec)](https://github.com/mateusz-szafraniec/flipperzero-IR_Blaster)
- [IR Decoder v1.0 (By prplecake)](https://github.com/prplecake/f0/tree/master/infrared/apps/ir_decoder)
- [IR Intervalometer v2.2 (By Nitepone)](https://github.com/Nitepone/flipper-intervalometer)
- [IR Remote v1.1 (By Hong5489)](https://github.com/Hong5489/ir_remote)
- [IR Scope v1.4 (By kallanreed)](https://github.com/kallanreed/unleashed-firmware/tree/dev/applications/external/ir_scope)
- [IR Signal Generator v1.0 (By temboohms68)](https://github.com/temboohms68/Flipper-Zero-IR-Signal-Generator)
- [IR Transfer WIP v0.1 (By WBP20)](https://github.com/WBP20/ir_transfer)
- [IR Xbox Controller v1.5 (By gebeto)](https://github.com/gebeto/flipper-xbox-controller)
- [ISO15693-3 NFC Writer v1.0 (By ch4istO)](https://github.com/ch4istO/unleashed-firmware/tree/dev/applications_user/iso15693_nfc_writer)
- [J305 Geiger Counter v1.5 (By nmrr)](https://github.com/nmrr/flipperzero-geigercounter)
- [JAYX PC Monitor v0.4 (By contactjayclatty)](https://github.com/contactjayclatty/JAYX)
- [Joycon v1.0 (By ccyyturralde)](https://github.com/ccyyturralde/Flipper-Zero-Joycon)
- [Karl Eido v0.1 (By fgreil)](https://github.com/fgreil/mitzi-karl-eido)
- [Key Copier v1.5 (By zinongli)](https://github.com/zinongli/KeyCopier)
- [KneeFlip Rehab v0.1 (By daryalok)](https://github.com/daryalok/KneeFlip-Rehab)
- [KyberWrite v0.1 (By spandox2)](https://github.com/spandox2/KyberWrite)
- [Lab401/DigiLab v1.1.1 (By lab-401 & tixlegeek)](https://github.com/lab-401/fzDigiLab)
- [Lab401/Light MESSENGER v1.4.1 (By tixlegeek)](https://github.com/lab-401/fzLightMessenger/tree/main/401lightMessengerApp)
- [LAN Tester v2.10 (By dok2d)](https://github.com/dok2d/fz-W5500-lan-analyse)
- [LaserCat v1.1 (By DroWez)](https://github.com/DroWez/LaserCat)
- [Lasko 2519 Timer v1.0 (By LN4CY)](https://github.com/LN4CY/lasko-2519-timer)
- [LD2410 Human Detector v1.0 (By sistemasorp)](https://github.com/sistemasorp/flipper-ld2410_human_detector)
- [LED Blinker v1.0 (By Cupprum)](https://github.com/Cupprum/Blinker)
- [LED Pulsator (By leedave)](https://github.com/leedave/Leeds-Flipper-Zero-Applications)
- [LED Remote v1.0 (By AugustinKrb)](https://github.com/AugustinKrb/flipper-led-remote)
- [LED Ring & Matrix Studio v0.1 (By xfwluna)](https://github.com/xfwluna/led_ring_matrix_studio) `Req: WS2812B / SK6812 NeoPixel`
- [LEGO Dimensions Toy Pad v0.5 (By SegerEnd)](https://github.com/SegerEnd/Flipper-Zero-LD-Toypad-Emulator)
- [Letter Beacon v1.0 (By nmrr)](https://github.com/nmrr/flipperzero-rfidbeacon)
- [LGT ISP (SWD) v0.7 (By mkrasem / SimonE)](https://github.com/mkrasem/flipper_lgt_isp) `Req: LGT8F328P`
- [LIDAR Emulator v0.2 (By regaly)](https://github.com/regaly/flipperzero-lidar_emulator)
- [Lightmeter v1.3 (By oleksiikutuzov)](https://github.com/oleksiikutuzov/flipperzero-lightmeter) `Req: BH1750/MAX44009`
- [Lightning Distance v0.1 (By HyperMuffin12)](https://github.com/HyperMuffin12/Flipper-Lightning-Distance)
- [Lishi HU66 v0.3 (By evillero)](https://github.com/evillero/lishi_hu66)
- [ListEM v1.4 (By Clawzman)](https://github.com/Clawzman/Flipper_ListEM)
- [Logic Analyzer v1.1 (By g3gg0)](https://github.com/g3gg0/flipper-logic_analyzer)
- [Longwave Clock v0.1 (By m7i-org)](https://github.com/m7i-org/flipper_longwave_clock)
- [LoRa Relay v1.1.3 (By ElectronicCats)](https://github.com/ElectronicCats/flipper-SX1262-LoRa) `Req: SX1262 By ElectronicCats`
- [LoRa Sample v1.1.3 (By ElectronicCats)](https://github.com/ElectronicCats/flipper-SX1262-LoRa) `Req: SX1262 By ElectronicCats`
- [LORA Terminal (By aafksab)](https://github.com/aafksab/LORA-Term)
- [LoRadar v0.1 (By baptleduc)](https://github.com/baptleduc/flipper-loradar-app)
- [MagicBand Plus Lights v3.0 (By Haw8411)](https://github.com/Haw8411/magic-band-plus-lights)
- [MagSpoof v0.10 (By zacharyweiss)](https://github.com/zacharyweiss/magspoof_flipper)
- [MALVEKE GAME BOY ADVANCE Cartridge v2.2 (By EstebanFuentealba)](https://github.com/EstebanFuentealba/MALVEKE-Flipper-Zero/tree/main/flipper_companion_apps/applications/external/malveke_gba_cartridge) `Req: Malveke`
- [MALVEKE GAME BOY Cartridge (GB/GBC) v2.2 (By EstebanFuentealba)](https://github.com/EstebanFuentealba/MALVEKE-Flipper-Zero/tree/main/flipper_companion_apps/applications/external/malveke_gb_cartridge) `Req: Malveke`
- [MALVEKE GAME BOY Emulator v2.1 (By EstebanFuentealba)](https://github.com/EstebanFuentealba/MALVEKE-Flipper-Zero/tree/main/flipper_companion_apps/applications/external/malveke_gb_emulator) `Req: Malveke`
- [MALVEKE GAME BOY Link-Camera v2.1 (By EstebanFuentealba)](https://github.com/EstebanFuentealba/MALVEKE-Flipper-Zero/tree/main/flipper_companion_apps/applications/external/malveke_gb_link_camera) `Req: Malveke`
- [MALVEKE GAME BOY Live Camera v2.1 (By EstebanFuentealba)](https://github.com/EstebanFuentealba/MALVEKE-Flipper-Zero/tree/main/flipper_companion_apps/applications/external/malveke_gb_live_camera) `Req: Malveke`
- [MALVEKE GB Photo v2.3 (By EstebanFuentealba)](https://github.com/EstebanFuentealba/MALVEKE-Flipper-Zero/tree/main/flipper_companion_apps/applications/external/malveke_gb_photo) `Req: Malveke`
- [MALVEKE PIN Test v2.2 (By EstebanFuentealba)](https://github.com/EstebanFuentealba/MALVEKE-Flipper-Zero/tree/main/flipper_companion_apps/applications/external/malveke_pin_test) `Req: Malveke`
- [Marmalade v0.1.1 (By RocketGod)](https://github.com/RocketGod-git/flipper-zero-rf-jammer) (Exercise Caution and Follow Your Local Laws)
- [Math Wiz v1.1 (By ImPapaGhost)](https://github.com/ImPapaGhost/math_wiz)
- [MAX31855 (By skotopes)](https://github.com/skotopes/flipperzero_max31855)
- [Mayan Decoder v1.1 (By RogerF5-Security)](https://github.com/RogerF5-Security/Mayan_decoder)
- [Meshcore Config v0.1 (By hleserg)](https://github.com/hleserg/flipperMeshCoreConfig/tree/main)
- [Metroflip v2.0.3 (By luu176)](https://github.com/luu176/Metroflip)
- [Metronome v1.3 (By panki27)](https://github.com/panki27/Metronome)
- [MFC Editor v1.0 (By TollyH)](https://github.com/TollyH/flipper-apps)
- [MFDesfire Auth v1.0 (By Gatorixx)](https://github.com/Gatorixx/flipper-zero-legacy-authentication)
- [Mfkey v4.1 (By noproto)](https://github.com/noproto/FlipperMfkey)
- [MFP Reader v1.1 (By Defensor7)](https://github.com/Defensor7/flipperzero-mfp-reader)
- [MH-Z19 CO2 Sensor v0.2 (By meshchaninov)](https://github.com/meshchaninov/flipper-zero-mh-z19)
- [MH-Z19 UART (By skotopes)](https://github.com/skotopes/flipperzero_mhz19_uart)
- [Mi Band NFC Writer v1.0 (By LucaTomei)](https://github.com/LucaTomei/flipper-miband-nfc-writer)
- [Midea AC Remote v1.0 (By xakep666)](https://github.com/xakep666/flipperzero-midea-ac-remote)
- [MIDI Ocarina v0.1 (By crackerjacques)](https://github.com/crackerjacques/Flipper_Zero_MIDI_Ocarina)
- [MIDI Player v0.1 (By Aspenini)](https://github.com/Aspenini/Flipper-Zero-MIDI-Player)
- [MIDI RX v0.1 (By crackerjacques)](https://github.com/crackerjacques/flipper_zero_midi_rx)
- [Miele Scout RX2 v1.0 (By kallevaravas)](https://github.com/kallevaravas/flipper-miele-scout)
- [Mifare Fuzzer v1.5 (By spheeere98)](https://github.com/spheeere98/mifare_fuzzer)
- [Mitsubishi AC Remote v1.0 (By achistyakov)](https://github.com/achistyakov/flipperzero-mitsubishi-ac-remote)
- [Mitzi Type Aid Demo v0.1 (By fgreil)](https://github.com/fgreil/mitzi-tyaid)
- [MiZip Balance Editor v1.0 (By teohumeau)](https://github.com/teohumeau/MiZipBalanceEditor)
- [MODBUS v1.1.3.1 (By ElectronicCats)](https://github.com/ElectronicCats/flipper-rs485modbus) `Req: Modbus By ElectronicCats`
- [Moisture Sensor v1.0 (By smallwat3r)](https://github.com/smallwat3r/flipperzero-moisture-sensor)
- [Moon Phases v1.0 (By w84death)](https://github.com/w84death/flipper-zero-apps)
- [Morse Code v1.3 (By wh00hw)](https://github.com/wh00hw/MorseCodeFAP)
- [Morse Flipper v0.1.78 (By yo3gnd)](https://github.com/yo3gnd/morse-flipper)
- [Mouse Jacker for MS Mouse v1.2 (Remixed By coded-with-claws)](https://github.com/coded-with-claws/flipperzero-tools/tree/main/applications_user/mousejacker_ms) `Req: NRF24`
- [Mouse Jacker v1.2 (By mothball187)](https://github.com/mothball187/flipperzero-nrf24/tree/main/mousejacker) [Pin Out from nocomp/Frog/UberGuidoZ](https://github.com/RogueMaster/flipperzero-firmware-wPlugins/tree/420/applications/mousejacker) `Req: NRF24`
- [Mouse Jiggler v1.2 (By Jacob-Tate & ozgunawesome)](https://github.com/Jacob-Tate/flipperzero-firmware/blob/dev/applications/mouse_jiggler/mouse_jiggler.c)
- [MP3 Player v3.5 (By coolshrimp)](https://github.com/coolshrimp/flipperzero-mp3-player)
- [MQ-3 Alcometer v1.0 (By serjantlk)](https://github.com/serjantlk/MQ-3-Alcometer) `Req: MQ-3 Breathalyzer` [Sold Here](https://flipperaddons.com/product/mq-3-alkotester)
- [MR60FDA1 Radar v0.1 (By adamaratski)](https://github.com/adamaratski/flipperzero-r60afd1) `Req: MR60FDA1/MR60FDA2`
- [Multi Converter v1.3 (By theisolinearchip)](https://github.com/theisolinearchip)
- [Multi Tally v1.0 (By aseypalich1)](https://github.com/aseypalich1/flipper-multi-counter)
- [MultiTimer v1.1 (By C0d3-5t3w)](https://github.com/C0d3-5t3w/flipper-multitimer)
- [Music Beeper (By DrZlo13)](https://github.com/flipperdevices/flipperzero-firmware/pull/1189)
- [Music Player v1.4 (By DrZlo13)-OFW](https://github.com/flipperdevices/flipperzero-firmware/pull/1189)
- [Music to Sub-GHz Radio v1.2 (By jamisonderek)](https://github.com/jamisonderek/flipper-zero-tutorials/tree/main/subghz/fmf_to_sub)
- [Music Tracker v1.5 (By DrZlo13)](https://github.com/DrZlo13/flipper-zero-music-tracker)
- [Mx2125 Dual-Axis Accelerometer (By jamisonderek)](https://github.com/jamisonderek/flipper-zero-tutorials/tree/main/gpio)
- [Mx2125 Step Counter v1.0 (By 47lecoste a.k.a. grugnoymeme)](https://github.com/grugnoymeme/flipperzero-stepcounter-fap)
- [Name Changer (By ESurge)](https://github.com/RogueMaster/flipperzero-firmware-wPlugins/pull/488)
- [Nearby Files v1.7 (By Stichoza)](https://github.com/Stichoza/flipper-nearby-files)
- [NestBridge v1.0 (By Bibfortuna5057)](https://github.com/Bibfortuna5057/NestBridge-Flipper) `Req: Nest Gen 2 Thermostat`
- [Netflix TV Remote v0.1 (By ablaran)](https://github.com/ablaran/NetflixTvRemote)
- [Network Calculator v1.0 (By WolfRorDev)](https://github.com/WolfRorDev/Net-Calculator-for-Flipper-Zero)
- [Network Subnet v0.1 (By aaumkar)](https://github.com/aaumkar/network_subnet)
- [Networking NFC + QR v1.0 (By DonJulve)](https://github.com/DonJulve/Networking_NFC_QR_Flipper_Zero)
- [NFC APDU Runner v0.3.5 (By SpenserCai)](https://github.com/SpenserCai/nfc_apdu_runner)
- [NFC Canary v0.2 (By antitree)](https://github.com/antitree/nfc_canary)
- [NFC Comparator v2.6 (By acegoal07)](https://github.com/acegoal07/FlipperZero_NFC_Comparator)
- [NFC Dict Manager v1.0 (By grugnoymeme)](https://github.com/grugnoymeme/nfc_dicts_manager)
- [NFC E-Ink Tags v1.0.4 (By RebornedBrain)](https://github.com/flipperdevices/flipperzero-good-faps/pull/245)
- [NFC Fuzzer v1.0 (By barkandbite)](https://github.com/barkandbite/flipper_suite)
- [NFC Keyboard v1.0 (By Patrick762)](https://github.com/Patrick762/flipper-nfc-keyboard)
- [NFC Login v1.1.2 (By Play2BReal)](https://github.com/Play2BReal/NFC-Login)
- [NFC Magic v2.1 (By gornekich)](https://github.com/flipperdevices/flipperzero-firmware/pull/1966)
- [NFC Maker v2.1 (by WillyJL)](https://github.com/RogueMaster/flipperzero-firmware-wPlugins/commit/343cb39d08f60b75b537e1a31100707935cf6e9d)
- [NFC Playlist v3.5 (By acegoal07)](https://github.com/acegoal07/FlipperZero_NFC_Playlist/tree/main)
- [NFC QR Presenter v1.0 (By RogerF5-Security)](https://github.com/RogerF5-Security/NFC_QR_Presenter)
- [NFC Relay v0.2 (By leommxj)](https://github.com/leommxj/nfc_relay)
- [NFC Seader v4.2 (By bettse)](https://github.com/bettse/seader)
- [NFC Sniffer v0.2 (By kdiller713)](https://github.com/kdiller713/public-FlipperApps/tree/main/nfc_sniffer)
- [NFC Stock v0.1.5 (By Endika)](https://github.com/Endika/flipper-nfc-stock)
- [NFC Tools v1.3 (By wakdev)](https://github.com/wakdev/nfctools-fz)
- [NFC URL v1.0 (By y-dejong)](https://github.com/y-dejong/nfcurl)
- [NFC/RFID Detector v1.4 (By Skorpionm)](https://github.com/flipperdevices/flipperzero-firmware/pull/2795)
- [Nickname Generator v0.2 (By disaxq)](https://github.com/disaxq/Nickname-Generator)
- [Nightstand Clock v1.3 (By nymda)](https://github.com/nymda/FlipperNightStand)
- [Noptel LRF Sampler v2.4 (By Giraut)](https://github.com/Giraut/flipper_zero_noptel_lrf_sampler)
- [Notes for FZ WIP (By AdrianN001)](https://github.com/AdrianN001/Flipper-Zero-Note-Application)
- [NRF Sniff v1.1 (By mothball187)](https://github.com/mothball187/flipperzero-nrf24/tree/main/nrfsniff) [Pin Out from nocomp/Frog/UberGuidoZ](https://github.com/RogueMaster/flipperzero-firmware-wPlugins/tree/420/applications/nrfsniff) `Req: NRF24`
- [NRF Sniffer for MS Mouse (Remixed By coded-with-claws)](https://github.com/coded-with-claws/flipperzero-tools/tree/main/applications_user/nrfsniff_ms) `Req: NRF24`
- [NRF24 Batch v2.0 (By vad7)](https://github.com/vad7/nRF24-Batch)
- [NRF24 Channel Scanner v1.4 (by htotoo)](https://github.com/htotoo/NRF24ChannelScanner)
- [NRF24 Jammer v1.0 (By huuck)](https://github.com/huuck/FlipperZeroNRFJammer) (Exercise Caution and Follow Your Local Laws)
- [NRF24 Monitor v1.0 (By CyberDemon73)](https://github.com/CyberDemon73/flipperzero-nrf24monitor)
- [NRF24 Scanner v2.3 (By vad7)](https://github.com/vad7/nrf24scan)
- [Nyx v1.1 (By at0m-b0mb)](https://github.com/at0m-b0mb/Nyx-FlipperZero)
- [Ocarina v1.3 (By invalidna-me)](https://github.com/invalidna-me/flipperzero-ocarina) [Here are the LOTZ Songs](https://www.zeldadungeon.net/wiki/Ocarina_of_Time_Songs)
- [OpenPrintTag v1.0 (By Houzvicka)](https://github.com/Houzvicka/FlipperPrintTag)
- [OpenShock v0.1 (By OpenShock Team)](https://github.com/OpenShock/FlipperZero)
- [Orgasmotron v1.1 (By leedave)](https://github.com/leedave/Leeds-Flipper-Zero-Applications)
- [Oscilloscope v0.4 (By anfractuosity)](https://github.com/anfractuosity/flipperscope)
- [OSM Logger GPS v0.15 (By simongrossi)](https://github.com/simongrossi/flipperzero-osm-logger-gps)
- [Paranoia WIP v0.2 (By C0d3-5t3w)](https://github.com/C0d3-5t3w/flipper-paranoia)
- [Password Generator v1.4 (By anakod)](https://github.com/anakod/flipper_passgen)
- [Password Keyboard v0.1 (By evgeniyraev)](https://github.com/evgeniyraev/password_keyboard)
- [Password Manager v1.2 (By Rrycbarm)](https://github.com/Rrycbarm/flipperZeroPasswordManager)
- [Passy v1.6 (By bettse)](https://github.com/bettse/passy)
- [Pause Timer v1.0 (By Matt-London)](https://github.com/Matt-London/pause_timer)
- [PC Monitor USB v1.0 (By DonJulve & TheSainEyereg)](https://github.com/TheSainEyereg/flipper-pc-monitor) `Req: [Backend](https://github.com/DonJulve/Flipper-Zero-PC-Monitor-USB-Backend)`
- [PC Monitor v1.1.3 (By TheSainEyereg)](https://github.com/TheSainEyereg/flipper-pc-monitor) `Req: [Backend](https://github.com/TheSainEyereg/flipper-pc-monitor-backend)`
- [Period Tracker v1.0 (By gorshunovr)](https://github.com/gorshunovr/period_tracker)
- [Pi Terminal v1.0 (By dagnazty & InfoSecREDD)](https://github.com/InfoSecREDD/Flip-pi)
- [Piano v1.0 (By bergr22)](https://github.com/bergr22/FlipPiano)
- [PicoPass (iClass) v1.22 (By Bettse)](https://gitlab.com/bettse/picopass)
- [PINGEQUA RF Lab v0.5.4 (By pingequalab)](https://github.com/pingequalab/rf-lab)
- [PIR HC-SR501 Alarm v1.0 (By ku6a-cj)](https://github.com/ku6a-cj/-PIR-HC-SR501-Proximity-alarm-Flipper-zero) `Req: PIR HC-SR501`
- [Plantower PMSx003 sensor reader v1.4 (By 3cky)](https://github.com/3cky/flipperzero-airmon) `Req: Plantower PMS5003/PMS7003/PMS9003`
- [Plugin Demo v1.0 (By csBlueChip)](https://github.com/csBlueChip/FlipperZero_plugin_howto)
- [Pocket Chords v0.1 (By rvonsch)](https://github.com/rvonsch/flipper-pocket-chords)
- [Pocket CVSS v0.3 (By vavkamil)](https://github.com/vavkamil/pocket-cvss)
- [PocketLab v1.6 (By PerfectoWeb)](https://github.com/PerfectoWeb/flipper-pocketlab)
- [POCSAG Pager v1.4 (By XMasterx & Shmuma)](https://github.com/xMasterX/flipper-pager)
- [Pokemon Trading v2.3 (By EstebanFuentealba & kbembedded)](https://github.com/EstebanFuentealba/Flipper-Zero-Game-Boy-Trading-Pokemons)
- [Polk MagnifiMini Remote v1.0 (By varunyellina)](https://github.com/varunyellina/flipper-ir-remote-polkmagnifi)
- [Pomodoro 2 v2.4.1 (By Th3Un1q3)](https://github.com/Th3Un1q3/flipp_pomodoro)
- [Pomodoro Timer v1.5 (By sbrin)](https://github.com/sbrin/flipperzero_pomodoro)
- [Portal of Flipper v1.3 (By bettse)](https://gitlab.com/bettse/portal_of_flipper)
- [Postman v0.1 (By MassivDash)](https://github.com/MassivDash/flipper-postman) `Req: ESP32` with [Postman Firmware](https://github.com/MassivDash/flipper-postman-esp32s2)
- [Programmer Calculator v0.9.2 (By armixz)](https://github.com/armixz/Flipper-Zero-Programmer-Calculator)
- [ProtoPirate v3.2 (By RocketGod-git)](https://protopirate.net/ProtoPirate/ProtoPirate)
- [ProtoView v1.3 (By antirez)](https://github.com/antirez/protoview)
- [QR Code v2.1.4 (By bmatcuk)](https://github.com/bmatcuk/flipperzero-qrcode)
- [QRCode Generator v0.1 (By qw3rtty)](https://github.com/qw3rtty/flipperzero-qrcode-generator)
- [Quac! v0.9.1 (By rdefeo)](https://github.com/rdefeo/quac)
- [Quadratic Solver v0.1 (By paul-sopin)](https://github.com/paul-sopin/flipper-quadratic-solver)
- [R.O.B. Control v1.0.1 (By riney)](https://github.com/riney/flip_rob)
- [Radar Scanner v2.0 (By MatthewKuKanich)](https://github.com/MatthewKuKanich/flipper-radar) `Req: RCWL-0516`
- [Radiation Sensor v1.0 (By sionyx)](https://github.com/sionyx/flipper_radsens) `Req: RadSens`
- [Radio Scanner v0.3 (By RocketGod)](https://github.com/RocketGod-Git/Flipper-Zero-Radio-Scanner)
- [Ray Hunter Client v1.0 (By barkandbite)](https://github.com/barkandbite/flipper_suite) `Req: ESP32`
- [RC2014 ColecoVision (By ezod)](https://github.com/ezod/flipperzero-rc2014-coleco)
- [RCWL-516 Motion Sensor (By ahmedafe1)](https://github.com/ahmedafe1/rcwl_0516-Flipperzero)
- [Reality Clock v4.1 (By Eris-Margeta)](https://github.com/Eris-Margeta/flipper-apps)
- [Remote for CaiXianlin v1.0 (By anty-ce)](https://github.com/anty-ce/flipper_zero_caixianlin_remote)[Remote for CaiXianlin v1.0 (By anty-ce)](https://github.com/anty-ce/flipper_zero_caixianlin_remote)
- [Remote Maker (By gid9798)](https://github.com/DarkFlippers/unleashed-firmware/pull/487)
- [Resistance Calculator v1.4 (By instantiator)](https://github.com/instantiator/flipper-zero-experimental-apps)
- [Restaurant Pager Trigger v1.7 (By leedave)](https://github.com/leedave/flipper-zero-meal-pager)
- [RFID Fuzzer v1.8 (By gid9798)](https://github.com/DarkFlippers/unleashed-firmware/pull/507)
- [RFID Metal Detector v1.0 (By hnsvrdmn)](https://github.com/hnsvrdmn/flipper-rfid-metal-detector)
- [RGB LED (By flyandi)](https://github.com/flyandi/flipper_zero_rgb_led)
- [Rogue AP Detector v1.0 (By barkandbite)](https://github.com/barkandbite/flipper_suite) `Req: ESP32`
- [RollCall v1.1 (By at0m-b0mb)](https://github.com/at0m-b0mb/RollCall-FlipperZero)
- [Roman Decoder v0.2 (By evillero)](https://github.com/evillero/roman_decoder)
- [Rosetta v1.0 (By at0m-b0mb)](https://github.com/at0m-b0mb/Rosetta-FlipperZero)
- [ROT13 v0.1 (By nothingbutlucas)](https://codeberg.org/nothingbutlucas/flipperzero_rot13)
- [RS41 Tracker v1.1 (By yeckel)](https://github.com/yeckel/RS41flipper)
- [Rubiks Cube Timer v0.0.2 (By KHOPAN)](https://github.com/KHOPAN/Cuber-Zero)
- [SaFlip v0.1 (By aaronjamt)](https://github.com/aaronjamt/saflip)
- [SAM (By Unknown)][Original?](https://github.com/ctoth/SAM)
- [Samsung AC Remote v1.0 (By dappermint)](https://github.com/dappermint/samsung-ac-remote-flipper-app)
- [San Morse v1.0.1 (By sanjorgek)](https://github.com/sanjorgek/san-morse-flipper)
- [SD Info v0.1 (By Sladkisnovraper)](https://github.com/Sladkisnovraper/SD-Info-For-Flipper-Zero)
- [SD SPI v0.5 (By Gl1tchub)](https://github.com/Gl1tchub/Flipperzero-SD-SPI) `Req: External SD Board`
- [Segment Clock v1.2 (By Sladkisnovraper)](https://github.com/Sladkisnovraper/Segment-Clock-For-Flipper-Zero)
- [Sentry Safe v2.0 (By H4ckd4ddy)](https://github.com/H4ckd4ddy/flipperzero-sentry-safe-plugin) ([Pin Out](https://github.com/RogueMaster/flipperzero-firmware-wPlugins/tree/420/applications/sentry_safe) from [UberGuidoZ](https://github.com/UberGuidoZ/)
- [Seos compatible v1.3 (By bettse)](https://github.com/bettse/seos_compatible)
- [ServoTester (2) v0.2 (By mhasbini)](https://github.com/mhasbini/ServoTesterApp)
- [Servotester v1.2 (By spin7ion)](https://github.com/spin7ion/flipper-servotester)
- [ShapShup v1.2 (By derskythe)](https://github.com/derskythe/flipperzero-shapshup)
- [SI4713 Tuner v0.1 (By rullinoiz)](https://github.com/rullinoiz/flipper-si4713) `Req: SI4713`
- [Sig Diary WIP v0.1 (By C0d3-5t3w)](https://github.com/C0d3-5t3w/flipper-sigdiary)
- [Signal Generator v1.2 (By nminaylov)-OFW](https://github.com/flipperdevices/flipperzero-firmware/pull/1793)
- [Simultanious UHF RFID Reader v1.1.2 (By RileyHaffner)](https://github.com/haffnerriley/Simultaneous-UHF-RFID-FlipperZero) `Req: M6e Nano UHF RFID Reader & Raspberry Pi Zero`
- [Skeleton App v1.1 (By jamisonderek)](https://github.com/jamisonderek/flipper-zero-tutorials/tree/main/ui/skeleton_app)
- [SKIDcity v1.2 (By H4W9)](https://github.com/H4W9/SKIDcity_App)
- [Sleep Counter v2.0 (By silverwolf2r)](https://github.com/silverwolf2r/flipper-sleep-counter) `Req: RCWL-0516`
- [SLI Writer v1.4 (By Julienbxl)](https://github.com/Julienbxl/SLI-Writer)
- [Smartra VIN2PIN v2.0 (By evillero)](https://github.com/evillero/smartra_vin2pin)
- [Smoke Free v0.1 (By kamylwnb)](https://github.com/kamylwnb/smoke_free)
- [Snowflake v0.1 (By fgreil)](https://github.com/fgreil/mitzi-snowflake)
- [Sonic Head ID v1.0 (By flipper_zero)](https://git.mbirth.uk/flipper_zero/sonicare)
- [Space PLayground v0.1 (By alanfortlink)](https://github.com/alanfortlink/fzspground)
- [Space Travel Calculator v1.0 (By ejfox)](https://github.com/ejfox/flipper-space-calculators)
- [Specter v2.7 (By at0m-b0mb)](https://github.com/at0m-b0mb/Specter-FlipperZero)
- [Spectrum Analyzer v1.5 (By jolcese)](https://github.com/jolcese/flipperzero-firmware/tree/spectrum/applications/spectrum_analyzer)
- [SPI Flash Dump v1.0 (By barkandbite)](https://github.com/barkandbite/flipper_suite)
- [SPI Mem Manager v1.5 (By drunkbatya)-OFW](https://github.com/flipperdevices/flipperzero-good-faps/tree/dev/spi_mem_manager)
- [SPI Terminal v0.4 (By janwiesemann)](https://github.com/janwiesemann/flipper-spi-terminal)
- [Spindle Calculator v1.0 (By Minto97)](https://github.com/Minto97/spindle-calc-flipper)
- [SSD1306 Test v1.1 (By martinbogo)](https://github.com/martinbogo/ssd1306_test) `Req: SSD1306`
- [Stopwatch v1.0 (By bergr22)](https://github.com/bergr22/Stopwatch-Flipper)
- [StroboMeter v1.1 (By LeanderJDev)](https://github.com/LeanderJDev/FlipperStroboMeterApp)
- [Sub Analyzer v1.0 (By RocketGod)](https://github.com/RocketGod-git/Flipper-Zero-SUB-Analyzer)
- [Sub Duplicate Finder v1.2.1 (By Endika)](https://github.com/Endika/flipper-sub-dup)
- [Sub-GHz Bruteforcer v4.0 (By Ganapati/xMasterX/derskythe)](https://github.com/derskythe/flipperzero-subbrute/tree/master)
- [Sub-GHz Jammer Detect v1.0 (By barkandbite)](https://github.com/barkandbite/flipper_suite)
- [Sub-GHz Keystore Decryptor v1.1 (By Lechnio)](https://github.com/Lechnio/KeeLoq-Keystore-Decryptor)
- [Sub-GHz Playlist Creator v0.2 (By coolerUA)](https://github.com/coolerUA/Sub-Ghz-Playlist-Creator)
- [Sub-GHz Playlist v1.0.1 (By darmiel)](https://github.com/darmiel/flipper-playlist)
- [Sub-GHz RAW Edit v1.7 (By Lechnio)](https://github.com/Lechnio/SubGHz-RAW-Edit)
- [Sub-GHz Remote v1.8 (By gid9798)](https://github.com/DarkFlippers/SubGHz_Remote)
- [Sub-GHz Rolling Flaws v1.5 (By jamisonderek)](https://github.com/jamisonderek/flipper-zero-tutorials)
- [Sub-GHz Scheduler v3.0 (By shalebridge)](https://github.com/shalebridge/flipper-subghz-scheduler)
- [Sub-GHz Spectrum v1.0 (By barkandbite)](https://github.com/barkandbite/flipper_suite)
- [Sub-GHz Toolkit 1808 v1.0 (By RocketGod-git)](https://github.com/RocketGod-git/RocketGods-SubGHz-Toolkit)
- [Subhound v1.2 (By maxwalks)](https://github.com/maxwalks/subhound)
- [Survival Manual v1.0 (By Radiomanrf)](https://github.com/Radiomanrf/Survival_manual)
- [SWD Probe v1.4 (By g3gg0)](https://github.com/g3gg0/flipper-swd_probe)
- [T-Union Master (China) v0.1 (By SocialSisterYi)](https://github.com/SocialSisterYi/T-Union_Master)
- [T5577 Multiwriter v0.2 (By Leptopt1los)](https://github.com/Leptopt1los/t5577_multiwriter)
- [T5577 Raw Writer v1.1 (By zinongli)](https://github.com/zinongli/T5577_Raw_Writer)
- [TagTinker v2.1 (By i12bp8)](https://github.com/i12bp8/TagTinker)
- [Tasks v1.3 (By MadLadSquad)](https://github.com/MadLadSquad/FlipperTasks)
- [Temp Sensors Reader 2.1 (By quen0n)](https://github.com/quen0n/Unitemp-Flipper-Zero-Plugin) Req: [See App Readme For Supported Modules](https://github.com/RogueMaster/flipperzero-firmware-wPlugins/blob/420/applications/external/unitemp/README.md)
- [Tesla Mod v2.16b26 (By hypery11)](https://github.com/hypery11/flipper-tesla-fsd)
- [Text To SAM v1.5 (By RoundPi)](https://github.com/Round-Pi/flipperzero-text2sam)
- [Text Viewer v1.7 (By WillyJL)](https://github.com/RogueMaster/flipperzero-firmware-wPlugins/commit/ae4fc4a54febccb44b010c4025a99c2edef9a7f0)
- [The C Prog. Language v0.2 (By armixz)](https://github.com/armixz/Flipper-Zero-The-C-Programming-Language)
- [Theme Manager v1.6 (By Hoasker)](https://github.com/Hoasker/flipper-theme-manager)
- [Time Dilation Calculator v1.0 (By ejfox)](https://github.com/ejfox/flipper-space-calculators)
- [Time Tracker v1.0.3 (By MassivDash)](https://github.com/MassivDash/flipper-tracker)
- [Timed Remote v0.1 (By anders-dc)](https://github.com/anders-dc/timed-remote)
- [Timer v0.1 (By nagamine-git)](https://github.com/nagamine-git/flipperzero_timer_app)
- [Tone Generator v1.0 (By GEMISIS)](https://github.com/GEMISIS/tone_gen)
- [TonUINO Writer v2.0 (By Bastelsaal)](https://github.com/Bastelsaal/tonuino-flipper-zero)
- [TPMS Bridge Renault v1.0 (By VishovVladimir)](https://github.com/VishovVladimir/flipper-zero-tpms)
- [TPMS Reader v1.0 (By wosk)](https://github.com/wosk/flipperzero-tpms)
- [Tree Identification v0.0.5 (By fgreil)](https://github.com/fgreil/mitzi-tree-ident)
- [Trident v1.1 (By at0m-b0mb)](https://github.com/at0m-b0mb/Trident-FlipperZero) `Req: 3-in-1 ESP32 + NRF24 + CC1101`
- [TuLlave v0.1 (By zqu4rtz)](https://github.com/zqu4rtz/FlipperApp-TuLlave)
- [Tuning Fork v2.0 (By besya)](https://github.com/besya/flipperzero-tuning-fork)
- [TV Remote v1.2 (By jmanion0139)](https://github.com/jmanion0139/flipper_tv_remote)
- [u-blox GPS v0.5 (By liamhays)](https://github.com/liamhays/ublox)
- [UART Echo (By DrZlo13)-OFW](https://github.com/flipperdevices/flipperzero-firmware/pull/831)
- [UART Sniff v1.0 (By barkandbite)](https://github.com/barkandbite/flipper_suite)
- [UART Terminal v1.7 (By cool4uma)](https://github.com/cool4uma/UART_Terminal)
- [UDECard v1.0.4 (By hahnworks](https://github.com/hahnworks/UDECard)
- [UHF Expansion v1.0 (By mtoolstec)](https://github.com/mtoolstec/fz-uhf-expansion)
- [UHF RFID WIP v1.0 (By frux-c)](https://github.com/frux-c/uhf_rfid) `Req: M100 or QM100`
- [UID Brute Smarter v1.1 (By fbettag)](https://github.com/fbettag/uid_brute_smarter)
- [UL-C Bruteforce Optimized v1.0 (By noproto)](https://github.com/zc-public/breakme-resources/tree/main/flipper_apps)
- [UL-C Relay v1.0 (By noproto)](https://github.com/zc-public/breakme-resources/tree/main/flipper_apps)
- [ULCFKey Next v1.0 (By noproto)](https://github.com/zc-public/breakme-resources/tree/main/flipper_apps)
- [ULCFKey v1.0 (By noproto)](https://github.com/zc-public/breakme-resources/tree/main/flipper_apps)
- [Unbank BTCLN QR Generator v1.0 (By shanzy39)](https://github.com/shanzy39/Unbank-BTCLN-QR-Generator)
- [uPython v1.8 (By ofabel)](https://github.com/ofabel/mp-flipper)
- [USB Consumer Control (By WithSecureLabs)](https://github.com/WithSecureLabs/usb-consumer-control)
- [USB HID Autofire v1.1 (By pbek)](https://github.com/pbek/usb_hid_autofire)
- [USB Mass Storage v1.5 (By nminaylov)](https://github.com/flipperdevices/flipperzero-good-faps/tree/nm/usb_mass_storage_app/mass_storage)
- [USB Midi Capturing v0.1 (By fgreil)](https://github.com/fgreil/mitzi-midi)
- [USB Midi v1.0 (By DrZlo13 & kribesk)](https://github.com/kribesk/flipper-zero-usb-midi) [Instructions by ESurge](https://github.com/RogueMaster/flipperzero-firmware-wPlugins/blob/420/applications/plugins/usb_midi/ReadMe.md)
- [UV Meter v1.0 (By michaelbaisch)](https://github.com/michaelbaisch/uv_meter) `Req: AS7331`
- [VB Migration Assistant v1.3 (By GMMan)](https://github.com/GMMan/flipperzero-vb-migrate)
- [VEML7700 Lux Meter v1.1 (By kamylwnb)](https://github.com/kamylwnb/Flipper-zero-app-VEML7700)
- [VGM Air Mouse v1.4 (By nminaylov)](https://github.com/flipperdevices/flipperzero-good-faps/tree/nm/motion_mouse_app) `Req: ICM42688, or Video Game Module By Flipper Devices`
- [VGM Game Remote v1.1 (By jblanked)](https://github.com/jblanked/VGM-Library) `Req: Video Game Module By Flipper Devices`
- [Video Game Module Console v1.0 (By jblanked)](https://github.com/jblanked/Video-Game-Module-Console)
- [Video Player v0.4 (By LTVA1)](https://github.com/LTVA1/flipper-zero-video-player)
- [VIN Decoder v0.2 (By evillero)](https://github.com/evillero/vin_decoder)
- [VK Thermo v1.1 (By VivoKey)](https://github.com/VivoKey/vkthermo-flipper)
- [VL6180X Distance Sensor I2C v0.1 (By kdiller713)](https://github.com/kdiller713/public-FlipperApps/tree/main/vl6180x) `Req: Adafruit VL6180X`
- [Voltage Calculator v1.0 (By HappyAmos)](https://github.com/HappyAmos/Flipper_FAPS)
- [Voyah Password v0.2 (By bolknote)](https://github.com/bolknote/Flipper-Zero-Voyah-Password)
- [Walkie Talkie v1.2 (By coolshrimp)](https://github.com/coolshrimp/flipperzero-walkie-talkie)
- [Walkman v1.0 (By rynosec)](https://github.com/rynosec/flipper-walkman) `Req:  GD3300D / HW-311 / YX5300-family board`
- [Warden v1.1 (By at0m-b0mb)](https://github.com/at0m-b0mb/Warden-FlipperZero)
- [Wardriver GPS v1.0 (By Sil333033)](https://github.com/Next-Flip/Momentum-Apps/tree/dev/wardriver)
- [Water Sensor Reader v1.0 (By MatveyStrelov)](https://github.com/MatveyStrelov/flipper-water-sensor)
- [WAV Player v1.4 (By DrZlo13)](https://github.com/flipperdevices/flipperzero-firmware/tree/zlo/wav-player)
- [WAV Recorder v0.2 (By scrolltex)](https://github.com/scrolltex/flipper-wav-recorder)
- [Weather Station v2.0.1 (By Skorpionm)-OFW](https://github.com/flipperdevices/flipperzero-good-faps/tree/dev/weather_station)
- [Web Crawler v1.0.1 (By jblanked)](https://github.com/jblanked/WebCrawler-FlipperZero) `Req: ESP32` with [FliperHTTP Firmware](https://github.com/jblanked/FlipperHTTP)
- [Weebo v0.5.1 (By bettse)](https://github.com/bettse/weebo) `Req: key_retail.bin on SD card /nfc/assets` (Amiibo Remixer)
- [Wendigo Bluetooth Tracker WIP v0.5.1 (By chris-bc)](https://github.com/chris-bc/wendigo) `WIP Req: ESP32`
- [WHC SWIO Flasher v1.1 (By sukvojte)](https://github.com/sukvojte/wch_swio_flasher)
- [Wiegand Reader v1.4 (By jamisonderek)](https://github.com/jamisonderek/flipper-zero-tutorials/tree/main/gpio)
- [WiFi (Deauther) V2 (By Timmotools)](https://github.com/Timmotools/flipperzero_esp8266_deautherv2) `Req: ESP8266`
- [WiFi (Marauder) v0.7.9 (By 0xchocolate & tcpassos)](https://github.com/0xchocolate/flipperzero-firmware-with-wifi-marauder-companion) `Req: ESP32` with [ESP32 MARAUDER Firmware](https://github.com/justcallmekoko/ESP32Marauder/releases/latest)
- [Wifi Internet Watch v1.1 (By DruzhininPavel)](https://github.com/DruzhininPavel/wifi-internet-watch-flipper) `Req: ESP32`
- [Wifi Mapping (By carvilsi)](https://github.com/carvilsi/flipper0-wifi-map/tree/main) `Req: ESP32`
- [Wii EC Analyzer v1.1 (By csBlueChip)](https://github.com/csBlueChip/FlipperZero_WiiEC)
- [WikiFlip v1.3 (By RogerF5-Security)](https://github.com/RogerF5-Security/WikiFlip)
- [Wire Tester v1.1 (By unixispower)](https://gitlab.com/unixispower/flipper-wire-tester)
- [wM-Buster v1.1.1 (By i12bp8)](https://github.com/i12bp8/wmbuster)
- [WoL Flipper v1.1 (By keetsta)](https://github.com/keetsta/wol-flipper) `Req: ESP32`
- [WS2812B LED Tester v1.9 (By jamisonderek)](https://github.com/jamisonderek/flipper-zero-tutorials/tree/main/gpio/ws2812b_tester)
- [Xbox Post Code Reader v0.14 (By coolshrimp)](https://github.com/coolshrimp/flipperzero-xbox-post-code-reader)
- [XBox360 USB Game Controller v0.2 (By expected-ingot)](https://github.com/expected-ingot/flipper-xinput)
- [Xiaomi Filter Reset v1.0 (By khmm12)](https://github.com/khmm12/flipper-xiaomi-filter-reset)
- [XRemote v1.4 (By kala13x)](https://github.com/kala13x/flipper-xremote)
- [YuriCable Pro Max v0.4 (By arag0re)](https://github.com/arag0re/fz-yuricable-pro-max)
- [ZeroFIDO v0.8 (By MinorGlitch)](https://github.com/MinorGlitch/zerofido)
- [ZeroMesh v2.1 (By SAMS0N1TE)](https://github.com/SAMS0N1TE/ZeroMesh) `Req: Meshtastic`

# Install GitHub Version With [WEB INSTALLER](https://lab.flipper.net/?url=https%3A%2F%2Frogue-master.net%2F%3Ffile%3DRM0819-2255-b3dd8981.tgz&channel=RM0819-2255-b3dd8981&version=0.420.0&target=f7)

- Have a new app? Submit GitHub link to me on Discord for a special reward (if I add it to RM Firmware).

<br>
<h2 align="center">Modus Operandi</h2>

The goal of this firmware is to constantly push the bounds of what is possible with Flipper Zero, driving the innovation of many new groundbreaking features, while maintaining the easiest and most customizable user experience of any firmware. Fixing bugs promptly and ensuring a stable and compatible system is also of our utmost importance.

- <h4>Feature-rich: We include all third-party features and apps as long as they fulfill a useful purpose and they work correctly, aswell as implement ourselves many new exciting functionalities.</h4>

- <h4>Stable: We ensure the most stable experience possible by having an actual understanding of what's going on, and proactively making all tweaks and additions backwards-, and inter-, compatible.</h4>

- <h4>Customizable: You can tweak just about everything you see: add/remove apps from the menu, change the animations, replace icon graphics, change your Flipper's name, change how the main menu looks, setup different keybinds like never before, and so much more. All on-device, with no complicated configuration.</h4>

<br>

Note that mentioned below are only a few of our staple additions to the firmware. For a full list check [down here](https://github.com/RogueMaster/flipperzero-firmware-wPlugins#List-of-Changes).

<br>
<h2 align="center">CFW Settings</h2>

We offer a powerful and easy-to-use application tailor-made for our firmware, that lets you configure everything you could dream of, and more:

- <ins><b>Interface:</b></ins> Tweak every part of your Flipper, from the desktop animations, to the main menu, lockscreen behavior, file browser, etc.

- <ins><b>Protocols:</b></ins> Configure SubGhz settings, add/remove custom frequencies, extend SubGhz frequencies to 281-361, 378-481, 749-962 MHz and setup which GPIO pins are used by different external modules.

- <ins><b>Misc:</b></ins> Everything else that doesn't fit the previous categories. Change your Flipper's name, XP level, screen options, and configure the <a href="https://github.com/Z3BRO/Flipper-Zero-RGB-Backlight">RGB backlight</a>.

<br>

<br>

<h2 align="center">Animations / Asset Packs</h2>

We created our own improved Animation / Asset system that lets you create and cycle through your own `Asset Packs` with only a few button presses, allowing you to easily load custom Animations, Icons and Fonts like never before. Think of it as a Theme system that's never been easier.

<img src=".github/assets/packs-folder.png" align="left" width="200px"/>
You can easily create your own pack, or find <a href="https://github.com/RogueMaster/awesome-flipperzero-withModules/tree/rogue_main/RM_AssetPacks">RM Asset Packs HERE</a>, or find some community-made ones on <b><a href="https://momentum-fw.dev/asset-packs">the MNTM website</a> or on Discord</b>. Check <a href="https://github.com/RogueMaster/flipperzero-firmware-wPlugins/blob/dev/documentation/file_formats/AssetPacks.md">here</a> for a tutorial on creating your own. Essentially, each <code>Asset Pack</code> can configure its own <code>Anims</code>, <code>Icons</code> & <code>Fonts</code>.

<br clear="left"/>

<br>

<img src=".github/assets/packs-select.png" align="left" width="200px"/>
Once you have some asset packs, upload them to your Flipper in <code>SD/asset_packs</code> (if you did this right you should see <code>SD/asset_packs/PackName/Anims</code> and/or <code>SD/asset_packs/PackName/Icons</code>). Alternatively, install directly using the website.


<br clear="left"/>

<br>

<img src=".github/assets/packs-done.png" align="left" width="200px"/>
After installing the packs to Flipper, hit the <code>Arrow Up</code> button on the main menu and go to <code>CFW Settings > Interface > Graphics</code>. Here choose which asset pack you want and tweak the other settings how you prefer, then exit the app to reboot and enjoy your fully customized Flipper!

<br clear="left"/>

<br>

<h2 align="center">Bad Keyboard</h2>

<img src=".github/assets/badkb.png" align="left" width="250px"/>
BadUSB is a great app, but it lacks a lot of options. Bad-KB allows you to customize all USB and Bluetooth parameters for your attacks.

In Bluetooth mode it allows you to spoof the display name and MAC address of the device to whatever you want. Showing up as a portable speaker or a wireless keyboard is easily doable, allowing you to get the attention of your target without needing a cable at hand.

In USB mode it also enables additional functionality to spoof the manufacturer and product names, as well as vendor and product IDs (VID/PID).

<br>

<h2 align="center">List of changes from MNTM</h2>

There are too many to name them all, this is a **non-comprehensive** list of the **most notable from an end-user perspective**. For a more detailed list, you can read through the [**changelogs**](https://github.com/RogueMaster/flipperzero-firmware-wPlugins/releases) and commits/code. 

Note that this repo is always updated with the great work from [Unleashed](https://github.com/DarkFlippers/unleashed-firmware), [Momentum](https://github.com/Next-Flip/Momentum-Firmware) and the latest changes from [OFW](https://github.com/flipperdevices/flipperzero-firmware). Below are mentioned only MNTM changes that they can actually be credited for, so make sure to check their fantastic additions aswell. And a huge thank you to both teams!

```txt
[Added]

- CFW Settings App (Easy configuration of features and behavior of the firmware)
- Asset Packs (Unparalleled theming and customization)
- More UI customization, redesigns and optimizations
- Bad-Keyboard App
- BLE Spam App
- FindMy Flipper App
- NFC Maker App
- Wardriver App
- File Search across SD Card
- Additional NFC parsers and protocols
- NFC Type 4 protocol and NTAG4xx support
- Subdriving (saving GPS coordinates for Sub-GHz)
- Easy spoofing (Name, MAC address, Serial number)
- Video Game Module color configuration right from Flipper
- Enhanced RGB Backlight modes (Full customization & Rainbow mode)
- File management on device (Cut, Copy, Paste, Show, New Dir, etc.)
- Remember Infrared GPIO settings and add IR Blaster support in apps
- Advanced Security measures (Lock on Boot, reset on false pins, etc.)
- Disk Image management (Mount and view image contents, open in Mass Storage)
- Extended JavaScript API (Support for UsbDisk/Mass Storage, File operations)
```
```txt
[Updated]

- Enhanced WiFi support for easiest setup ever
- Extended keyboard with cursor movement and symbols
- File Browser with Sorting, More supported File Types
- Advanced and optimized Level System (Up to 30 levels)
- Desktop Keybind system for full key and press/hold remapping
- Storage backend with instant rename and virtual mounting for disk images
- Expanded Sub-GHz App (Duplicate detection & Ignore, Autosave, History improvements)
- Improved Error Messages (Showing source file paths)
```
```txt
[Removed]

- Unused Dummy Mode
- Broken or Superfluous apps
```

<br>
<a name="install"></a>

<h2 align="center">Install</h2>

There are 4 methods to install RM, we recommend you use the **Web Updater**, but choose whichever one you prefer:

> <details><summary><code>Web Updater (Chrome)</code></summary><ul>
>   <li>Make sure qFlipper is closed</li>
>   <li>Open the Web Updater link from <a href="https://github.com/RogueMaster/flipperzero-firmware-wPlugins/releases/latest">latest release page</a></li>
>   <li>Click <code>Connect</code> and select your Flipper from the list</li>
>   <li>Select which update <code>Channel</code> you prefer from the dropdown</li>
>   <li>Click <code>Install</code> and wait for the update to complete</li>
> </ul></details>

> <details><summary><code>Flipper Lab/App (chrome/mobile)</code></summary><ul>
>   <li>(Desktop) Make sure qFlipper is closed</li>
>   <li>(Mobile) Make sure you have the <a href="https://docs.flipper.net/mobile-app">Flipper Mobile App</a> installed and paired</li>
>   <li>Open the <a href="https://github.com/RogueMaster/flipperzero-firmware-wPlugins/releases/latest">latest release page</a></li>
>   <li>Click the <code>☁️ Flipper Lab/App (chrome/mobile)</code> link</li>
>   <li>(Desktop) Click <code>Connect</code> and select your Flipper from the list</li>
>   <li>(Desktop) Click <code>Install</code> and wait for the update to complete</li>
>   <li>(Mobile) Accept the prompt to open the link in the Flipper Mobile App</li>
>   <li>(Mobile) Confirm to proceed with the install and wait for the update to complete</li>
> </ul></details>

> <details><summary><code>qFlipper Package (.tgz)</code></summary><ul>
>   <li>Download the qFlipper package (.tgz) from the <a href="https://github.com/RogueMaster/flipperzero-firmware-wPlugins/releases/latest">latest release page</a></li>
>   <li>Make sure the <code>WebUpdater</code> and <code>lab.flipper.net</code> are closed</li>
>   <li>Open <a href="https://flipperzero.one/update">qFlipper</a> and connect your Flipper</li>
>   <li>Click <code>Install from file</code></li>
>   <li>Select the .tgz you downloaded and wait for the update to complete</li>
> </ul></details>

> <details><summary><code>Zipped Archive (.zip)</code></summary><ul>
>   <li>Download the zipped archive (.zip) from the <a href="https://github.com/RogueMaster/flipperzero-firmware-wPlugins/releases/latest">latest release page</a></li>
>   <li>Extract the archive. This is now your new Firmware folder</li>
>   <li>Open <a href="https://flipperzero.one/update">qFlipper</a>, head to <code>SD/update</code> and simply move the firmware folder there</li>
>   <li>On the Flipper, hit the <code>Arrow Down</code> button, this will get you to the file menu. In there simply search for your updates folder</li>
>   <li>Inside that folder, select the Firmware you just moved onto it, and run the file thats simply called <code>Update</code></li>
> </ul></details>

<br>

<h2 align="center">Build it yourself</h2>

```bash
To download the repository:
$ git clone --recursive --jobs 8 https://github.com/RogueMaster/flipperzero-firmware-wPlugins.git
$ cd flipperzero-firmware-wPlugins/

To flash directly to the Flipper (Needs to be connected via USB, qFlipper closed)
$ ./fbt flash_usb_full

To compile a TGZ package
$ ./fbt updater_package

To build and launch a single app:
$ ./fbt launch APPSRC=your_appid

If building animation assets:
$ ./fbt dolphin_ext
```
