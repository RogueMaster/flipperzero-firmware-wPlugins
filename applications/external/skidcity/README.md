# SKIDcity

*"Your flipper can't do that. and if it could, you'd be in federal prison."*


So you got a Flipper Zero. Maybe you saw it on TikTok. Maybe some guy on Discord told you it could hack traffic lights, clone any card, crash wifi networks, and steal car keys. Maybe you bought it specifically to do one of those things.

This app is for you.

not in a "gotcha" way. Genuinely — this app is for you, because those things get asked about **constantly** in every Flipper community, subreddit, and Discord server, and the people asking usually have no idea why the answer is always no. SKIDcity gives you the actual answer, every time, with a demo your Flipper *can* do instead.


## What it does

eleven menu items. Each one is something a script kiddie has asked about. You tap it, you get:

- a **demo** — either a real thing your Flipper can actually do (traffic light? your LED cycles red/yellow/green. IR blast? the blue LED fires like a real transmitter), or a BANNED screen that looks exactly like you'd expect
- a **"Why is this Illegal?"** page with the actual law cited, the actual reason the hardware can't do what you think it can, and a real path forward if you're genuinely curious about that area
- a **CFAA deep-dive** for the ones that are straight-up federal crimes

The banned screen is styled after the classic "your flipper is banned" meme for a reason. If you've been in the community for more than a week, you've seen it. It lands.


## The menu

| What you tap | What actually happens |
|---|---|
| Hack Traffic Lights | LED cycles red → yellow → green. UP/DOWN changes it. That's your "traffic control" |
| Crash WiFi Networks | CFAA screen. 18 U.S.C. §1030. up to 10 years. |
| Steal Car Keys | CFAA screen. rolling codes explained. your replay does nothing. |
| Clone Credit Cards | CFAA screen. EMV crypto explained. |
| Bypass Smart Locks | CFAA screen. AES-128 mutual auth explained. |
| Control Any TV | actual IR blink demo. blue LED fires. This one's legal, use it. |
| Crash Airplane Systems | FCC screen. 18 U.S.C. §32. 20 years. |
| Jam Cell/GPS Signals | FCC screen. §47 U.S.C. §333. $100k/day fine. |
| Jam Sub-GHz / RF | FCC screen. also explains why 10mW isn't a jammer anyway. |
| Spam BLE / Bluetooth | CFAA screen. also explains why spamming BLE near medical devices is genuinely dangerous. |
| Hack ATM / Bank | CFAA screen. §1029 + §1030 stacked. |

Every single one of them also has legal alternatives — actual things you can study, certifications worth getting, protocols worth learning. not just "don't do it." *Here's what to do instead.*


## The thing about BLE spam

This one gets its own callout because people treat it like it's harmless.

It's not. BLE spam — the fake Apple/Android/Samsung pairing popups — can interfere with Bluetooth medical devices. insulin pumps. hearing aids. CGM sensors. People walk around with these, and they don't have a choice. "But I was just doing it for fun" is not something you want to be saying to a federal judge while someone explains what happened to a diabetic patient nearby.

The app says this directly. It's in the about screen. Not sorry about it.


## Requirements

Requires a Flipper Zero. no WiFi dev board needed, no SD card files. completely self-contained.


## Why this exists

Every Flipper community moderator has typed the same four sentences ten thousand times. Traffic lights use NTCIP on wired networks. Rolling codes make replay attacks useless. The CFAA doesn't care that you were "just testing." Your 10mW transmitter cannot jam a cell tower.

SKIDcity types those sentences so they don't have to. pin it, link it, paste it. The app does the rest.

If someone learns something from it — actually looks up the NTCIP spec, actually starts studying for their ham ticket, actually reads ISO 14443 because now they're curious — then it worked.


## The actual spirit of the Flipper

The Flipper Zero is a legitimate research and learning tool. Sub-GHz protocol analysis, NFC/RFID research, IR database building, iButton cloning (your own), BadUSB scripting, GPIO and hardware tinkering. There is a ton you can do with it that is interesting, legal, and will actually teach you something.

None of that requires you to target someone else's property. All of it requires you to understand what you're doing. That's the difference between a researcher and a script kiddie, and it's the whole point of this app.



*SKIDcity — Flipper Community*
*Stay curious. Stay legal.*
