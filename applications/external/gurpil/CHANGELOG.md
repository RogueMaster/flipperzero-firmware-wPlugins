# Changelog

## [0.1.8](https://github.com/Endika/flipper-gurpil/compare/v0.1.7...v0.1.8) (2026-07-10)


### Features

* **sim:** ramp run speed 1x-&gt;3x by distance ([4d23ae2](https://github.com/Endika/flipper-gurpil/commit/4d23ae206a1c9fd48669c190df2dd5ba13074080))

## [0.1.7](https://github.com/Endika/flipper-gurpil/compare/v0.1.6...v0.1.7) (2026-07-09)


### Bug Fixes

* **game:** triple the pace and thin the time budget for a much harder, twitchier run ([ee35847](https://github.com/Endika/flipper-gurpil/commit/ee3584787e299b702b1af5df036d74afa90138c1))

## [0.1.6](https://github.com/Endika/flipper-gurpil/compare/v0.1.5...v0.1.6) (2026-07-09)


### Bug Fixes

* **ui:** size the checkpoint-flash buffer for the runtime bonus to satisfy -Werror=format-truncation ([951400e](https://github.com/Endika/flipper-gurpil/commit/951400eed49b63a2b4d91bfc195757aff7a303df))

## [0.1.5](https://github.com/Endika/flipper-gurpil/compare/v0.1.4...v0.1.5) (2026-07-09)


### Bug Fixes

* **endless:** decay the checkpoint time bonus with distance for a rising difficulty curve ([9dd3d4e](https://github.com/Endika/flipper-gurpil/commit/9dd3d4e0a821f91cf9a32642cacb90be57b5c576))

## [0.1.4](https://github.com/Endika/flipper-gurpil/compare/v0.1.3...v0.1.4) (2026-07-09)


### Bug Fixes

* **ui:** outline wheel shapes and draw Back as a button glyph for a clearer footer ([48682bd](https://github.com/Endika/flipper-gurpil/commit/48682bd52cb89b00c218eac9e30761e9bd923a3e))

## [0.1.3](https://github.com/Endika/flipper-gurpil/compare/v0.1.2...v0.1.3) (2026-07-09)


### Bug Fixes

* **ci:** build the release FAP against stable SDK channel not dev ([fe0e0ee](https://github.com/Endika/flipper-gurpil/commit/fe0e0ee10d0372acd284cc03f4178bd2f2f88477))

## [0.1.2](https://github.com/Endika/flipper-gurpil/compare/v0.1.1...v0.1.2) (2026-07-09)


### Bug Fixes

* **endless:** rebalance time economy so skilled play is sustainable ([595355b](https://github.com/Endika/flipper-gurpil/commit/595355b5037330920c8d9aef1a4bdf4afc0b75b1))

## [0.1.1](https://github.com/Endika/flipper-gurpil/compare/v0.1.0...v0.1.1) (2026-07-09)


### Features

* **app:** classic Flipper menu (Play/How to play/Credits) with opaque game-over panel and HUD units ([2cfaf77](https://github.com/Endika/flipper-gurpil/commit/2cfaf770c7b9fdda5439ccfe214ec9bca0b195d4))
* **app:** entry point and game main loop ([d79a33d](https://github.com/Endika/flipper-gurpil/commit/d79a33d50588320942115db2931523f514ee9e5d))
* **application:** game per-tick orchestrator ([4c76f15](https://github.com/Endika/flipper-gurpil/commit/4c76f153557421796c5dc4a987e058aa75c3bac9))
* **domain:** best-distance record codec ([721baaa](https://github.com/Endika/flipper-gurpil/commit/721baaafa84b9f1d913d323168dee51de4f9ad82))
* **domain:** endless checkpoint-timer state machine ([7058134](https://github.com/Endika/flipper-gurpil/commit/7058134715b34279bc19a1383eb2f36b92eb48ae))
* **domain:** seeded endless terrain generator ([100772d](https://github.com/Endika/flipper-gurpil/commit/100772d0a96dc80329a426cc3c755c53ace5f3d6))
* **domain:** shape catalog and terrain-kind enum ([587d6f1](https://github.com/Endika/flipper-gurpil/commit/587d6f1373fc3d27667a1201ac99fcaa1c6b84f6))
* **domain:** stable-hybrid fixed-point speed sim ([3d00b4f](https://github.com/Endika/flipper-gurpil/commit/3d00b4f659b7a538877d7bb740886c172d811cfc))
* **game:** tune endless timer and add speed bar, checkpoint flag/flash, tutorial hint and new-best reward ([47b45e6](https://github.com/Endika/flipper-gurpil/commit/47b45e69dd2c63312b769142a271938db05fc47f))
* **persistence:** best-distance storage ([5630d12](https://github.com/Endika/flipper-gurpil/commit/5630d120b82e960e76cd853d92d01dd1b20447bc))
* **platform:** hardware random seed wrapper ([d4ec379](https://github.com/Endika/flipper-gurpil/commit/d4ec3799d85455b82dd301de3e358aa7c512475e))
* **ui:** add in-play D-pad control legend with shape mapping and back-to-menu hint ([cdab621](https://github.com/Endika/flipper-gurpil/commit/cdab62108375d35699ece1a8deaa5c12f1e8421f))
* **ui:** scrolling render, shape wheel, HUD and input ([54d37c4](https://github.com/Endika/flipper-gurpil/commit/54d37c4189d905b70289f0bee5501aea308a3084))
* **ui:** start/controls screen, rider sprite and spinning wheel ([de66c5c](https://github.com/Endika/flipper-gurpil/commit/de66c5cc5571ce008ccd1ea105474ab1aaa01e06))


### Bug Fixes

* **ui:** lower and compress terrain so it clears the top HUD and speed bar ([45dd222](https://github.com/Endika/flipper-gurpil/commit/45dd22246e2fab150584e12fe61ee3e7c0d6ba8d))
* **ui:** lower terrain further for more top clearance ([6553c59](https://github.com/Endika/flipper-gurpil/commit/6553c594bac9ca7848e55beb799af140d4e1f930))
* **ui:** use CanvasDirectionBottomToTop for the triangle wheel ([db4f3ac](https://github.com/Endika/flipper-gurpil/commit/db4f3acb59faa878b60603aa2740a91a2eb4bbc7))
