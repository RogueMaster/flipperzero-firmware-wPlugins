# Changelog

## [1.3.1](https://github.com/Endika/flipper-sub-dup/compare/v1.3.0...v1.3.1) (2026-11-03)


### Bug Fixes

* regroup by CRC32 and size after a delete instead of rereading every duplicate ([b3bc578](https://github.com/Endika/flipper-sub-dup/commit/b3bc5786cf8566733a398fae3d7431a3db66bd4c))
* scan groups by CRC32 and size and compare bytes only when deleting ([1fe4117](https://github.com/Endika/flipper-sub-dup/commit/1fe4117284646477666db4778d3307d071b3c446))
* verify duplicate contents before grouping and deletion ([344006e](https://github.com/Endika/flipper-sub-dup/commit/344006e4b954d1fae29e532bd692be42c8f8fcc1))

## [1.3.0](https://github.com/Endika/flipper-sub-dup/compare/v1.2.2...v1.3.0) (2026-09-25)


### Features

* choose the folder to scan and remember it ([1298d0c](https://github.com/Endika/flipper-sub-dup/commit/1298d0c4b0e4c206199ba0904010d6a858e414aa))


### Bug Fixes

* never offer unreadable or truncated files for deletion ([ba7aeef](https://github.com/Endika/flipper-sub-dup/commit/ba7aeef68f65a9c52d28ed2426cc17e22775ee75))

## [1.2.2](https://github.com/Endika/flipper-sub-dup/compare/v1.2.1...v1.2.2) (2026-09-05)


### Bug Fixes

* guard the app allocation so the null cannot flow ([b941a1a](https://github.com/Endika/flipper-sub-dup/commit/b941a1afbd6f4e7d04a1db8202888838b58bb7b3))

## [1.2.1](https://github.com/Endika/flipper-sub-dup/compare/v1.2.0...v1.2.1) (2026-08-04)


### Bug Fixes

* track fap_version with release-please so the version triad stops drifting ([19a19b6](https://github.com/Endika/flipper-sub-dup/commit/19a19b6d0e5a8262fae2fef96c22bbd83b38681a))

## [1.2.0](https://github.com/Endika/flipper-sub-dup/compare/v1.1.0...v1.2.0) (2026-03-23)


### Features

* add release-please ([b04c96f](https://github.com/Endika/flipper-sub-dup/commit/b04c96f62a0dd8dd11bbd5bea47954f6bdb26488))

## [1.1.0](https://github.com/Endika/flipper-sub-dup/compare/v1.0.0...v1.1.0) (2026-03-23)


### Features

* add automated releases with release-please and FAP upload ([451fd8c](https://github.com/Endika/flipper-sub-dup/commit/451fd8c8575a70829ef8279131511c33da26e49b))
