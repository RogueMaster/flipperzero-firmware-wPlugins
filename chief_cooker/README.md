# Chief Cooker
Your ultimate Flipper Zero restaurant pager tool. Be a _real chief_ of all the restaurants on the food court!

This app supports receiving, decoding, editing and sending restaurant pager signals. 

**Developed & compatible with [Momentum firmware](https://github.com/Next-Flip/Momentum-Firmware).** Other firmwares are most likely not supported (but I've not tried).

## Video demo
[![Video 1](https://img.youtube.com/vi/iuQSyesS9-o/0.jpg)](https://youtube.com/shorts/iuQSyesS9-o)

More demos:
- [Video 2](https://youtube.com/shorts/KGDAGblbtFo)
- [Video 3](https://youtube.com/shorts/QqbfHF-yDiE)

## Disclaimer
I've built this app for research and learning purposes. But please, don't use it in a way that could hurt anyone or anything.

Use it responsibly, okay?

## [Usage instructions](instructions/instructions.md)
Please, read the [instructions](instructions/instructions.md) before using the app. It will definitely make your life easier!

## Features
- **Receive** signals from pager stations
- Automatically **decode** them and dsiplay station number, pager number and action (Ring/Mute/etc)
- Manually **change encoding in real-time** to look for the best one if automatically detected encoding is not working
- **Resend** captured message to specific pager to all at once to **make them all ring**!
- **Modify** captured signal, e.g. change pager number or action
- **Save** captured signals (and give each station a name)
- Create separate **categories** for each food court you are chief on
- Display signals from saved stations **by ther names** (instead of HEX code) or hide them from list
- **Send** signals from saved stations at any time, no need to capture it again
- Of course, suports working with **external CC1101 module** to cover the area of all the pagers on your food court! 

## Supported protocols
- Princeton
- SMC5326

## Supported pager encodings
- Retekess TD157
- Retekess TD165/T119
- Retekess TD174
- L8R / Retekess T111 (not tested)
- L8S / iBells ZJ-68 (check [source code](app/pager/decoder/L8SDecoder.hpp#L8) for description)

## Contributing
If you want to add any new pager encoding, please feel free to create PR with it!

Also you can open issue and share with me any captured data (and at least pager number) if you have any and maybe I'll try create a decoder for it

## Building
If you build the source code just with regular `ufbt` command, the app will probably crash due to out of memory error because your device will have less that 10 kb of free RAM on the "Scan" screen.

This is because after you compile the app with ufbt, the result executable will contain hundreds of sections with very long names like `.fast.rel.text._ZNSt17_Function_handlerIFvmEZN18PagerActionsScreenC4EP9AppConfigSt8functionIFP15StoredPagerDatavEEP12PagerDecoderP13PagerProtocolP12SubGhzModuleEUlmE_E9_M_invokeERKSt9_Any_dataOm`.
**These names stay in RAM during the execution and consume about 20kb of heap!**

The reason for it is [name mangling](https://en.wikipedia.org/wiki/Name_mangling). Perhaps, the gcc parameter `-fno-mangle` could disable it, but unfortunately, it is not possible to pass any arguments to gcc when you compile app with `ufbt`. 
Luckily the sections inside the compiled file can be renamed using gcc's `objcopy` tool with `--rename-section` parameter. To automate it, I built a small python script which renames them all and gives them short names like `_s1`, `_s2`, `_s228` etc... 

**Therfore you must use [scripts/build-and-clear.py](scripts/build-and-clear.py) script instead!** It will build, rename sections and upload the fap to flipper.

After building and cleaning your `.fap` with it, you'll get extra +20kb of free RAM which will make compiled app work stably.

The `.fap` files under the release tab are already cleared with this script, so if you just want to use this app without modifications, just forget about it and download the latest one from releases.

## Support & Donate

> [PayPal](https://paypal.me/denr01)

## Special Thanks
- [meoker/pagger](https://github.com/meoker/pagger) for Retekess pager encodings
- This [awesome repository](https://dev.xcjs.com/r0073dl053r/flipper-playground/-/tree/main/Sub-GHz/Restaurant_Pagers?ref_type=heads) for Retekess T111 and iBells ZJ-68 files
