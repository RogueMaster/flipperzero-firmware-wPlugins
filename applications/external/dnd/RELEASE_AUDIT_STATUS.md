# Dungeons & Dolphins 4.20.3 audit status

Audit date: 2026-10-06 UTC. All sixteen manifests use release label `4.20.3` and numeric `(4, 20)` metadata: eleven FAPs and five versioned, non-embedded FALs. All authorized source changes are implemented within the DND app family. Current ARM and device acceptance remain outstanding.

## Completed

- Preserved the corrected editor-release prototypes and actual-mode Hub/Combat/Grants ownership from 4.19.1.
- Retained Character Sheet, Journal, Monster Turn and spell damage FALs with descriptor validation, borrowed-data ownership, explicit return/loading contracts and callback-safe cleanup.
- Added unbiased random selection across fifteen preserved 128×64 images: the original plus fourteen supplied images, including Monk, Sorcerer, Warlock and Barbarian. Raw assets are packed as nonresident file data in both Hub and loading FAL. One shared heap object contains a 1,024-byte bitmap and four bytes of metadata; overlapping owners reuse it without another read or image allocation.
- Verified all choices, exact PNG/bitmap hashes, draw paths without I/O/allocation, partial/corrupt/missing file fallbacks, allocation failures and shared lifetime across real FAL unloads. Native failed File opens are closed before retry. The supplied asset extractor streams files to SD with a 512-byte copy buffer. First extraction/update still writes all bundled files to SD.
- Retained Hold OK Bag Mover and immediate per-item Container moves, preserving fields and transactional rollback.
- Replaced the firmware-dependent loading implementation with DND-owned handoff records and loading API version 2. Public direct drawing keeps the splash/hourglass across outgoing teardown; the incoming app releases it after its view is ready and its destination path matches. No firmware overlay, private GUI API, service or SDK implementation change is included or required.
- Native failure/cancellation/incoming-exit events and a ten-second timeout restore drawing. One inactive loading cache can remain until the next DND readiness/handoff safely unsubscribes, drains the timer and unmaps it. Callbacks never unload their own executing module.
- Added public synchronous Loader barriers before DND app-side module map/free, separating those operations from Loader startup/unload work without SDK patches.
- Passed strict `-Wall -Wextra -Werror -Wstrict-prototypes -Wredundant-decls` compilation under all sixteen actual manifest source sets at `-O1` and `-Os`, eleven entry-point lifecycle smokes, five shared-module links and twenty ASan/UBSan regression executables.
- Verified zero-view handoffs, wrong-destination readiness, missing/version/allocation fallback, failure-cache recovery, overlapping independent loading modules sharing one bitmap and threaded animation cleanup. Native animation free deletes and flushes the timer queue before callback contexts/views are released.
- Passed the supplied native manifest parser, public direct-draw/Loader/pubsub/timer source contracts and normalized host FAL import audit against API **88.7**, including native print wrappers. ARM import/relocation checking remains a separate gate.
- Regenerated 32-bit layout and comparable host size/frame evidence. Common Hub state remains 3,416 B; Bestiary is 1,536 B; lazy Combat runtime is 256 B. Handoff context/transfer proxies are 36 B/148 B, local loading is 12 B and Hub splash is 24 B. The one shared bitmap object is 1,028 B. These exclude native framework/ELF allocations. Host figures are not ARM residency or peak-memory measurements.

## Installation and remaining checks

Build and install the eleven FAPs plus all eight FAL destinations in [FAL_INTEGRATION.md](FAL_INTEGRATION.md). Install the version-2 loading FAL at `/ext/apps_data/dndolphins/plugins/dnd_loading.fal` together with the matching FAPs. Source changes belong only under `applications/external/dnd/`; no firmware flash is required by this release.

No current ARM compiler/complete matching SDK or connected Flipper is available here. Current ARM links/relocations, rendered startup/handoff timing, held-input routing, heap/stack high-water and device storage/power-loss behavior remain unverified. Loading before DND app entry remains controlled by stock firmware. The retained SDK logs are historical. Use [tests/host/VALIDATION.md](tests/host/VALIDATION.md) and [DEVICE_TEST_MATRIX.md](DEVICE_TEST_MATRIX.md) to distinguish passing source/host gates from required device acceptance. Source ZIPs contain no `dist/`, FAP/FAL binaries, firmware overlay or firmware image.
