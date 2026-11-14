# Rights and distribution gate

Status: **unresolved; no Store submission authorized by this checklist alone**.

Paid distribution and open source are compatible in principle. GPL requires
preserving recipients' rights and corresponding source when distributing covered
work. Being allowed to charge does not establish compatibility with every Store
agreement, DRM setting or end-user license restriction. This is a release gate
for qualified rights/legal review, not a claim that a paid GPL listing is cleared.

Inventory:

| Component | Current treatment |
| --- | --- |
| Existing BLE Clipboard Mac/Flipper source | Repository GPL-3.0-only; retain notices |
| Mac adapter, library, compiler and UI | Existing repository license; no relicensing |
| Independent v3 transfer, validator, executor and test harness | Project-owned source under the existing repository license |
| Apple frameworks | System frameworks, not copied into the source tree |
| Flipper SDK | Separately licensed build dependency, not in the Mac bundle |
| Existing artwork | Existing generated master and ICNS; provenance in docs/ARTWORK.md |

Version 0.3.0 removes the prior prototype's external typing engine and its
submodule/overlay/test dependency. This inventory describes current shipping
source, not historical experiments. Removing that dependency does not itself
resolve the existing project-license/Store distribution review.

Before submission, record the reviewer, date, exact Store agreements and chosen
distribution terms, confirm all contributor rights, and ensure the paid binary's
source is available at a permanent versioned URL with build instructions. No
assumption is made that the repository owner can change every contributor's
license. Do not remove GPL terms or add a more restrictive EULA automatically.

If incompatible restrictions remain, resolve them with appropriate permissions
or a properly licensed alternative before uploading. Keep the direct-download
channel available separately while this is unresolved. Homebrew builds remain
free source/direct-download distribution; a paid Store listing must not imply
exclusive access to freely licensed source.

References for review:

- [GPL v3 text](https://www.gnu.org/licenses/gpl-3.0.html)
- [GNU GPL FAQ](https://www.gnu.org/licenses/gpl-faq.html)
- [Apple agreements](https://developer.apple.com/support/terms/)

Review record: **not completed**. No legal opinion or permission is manufactured
by generating this file.
