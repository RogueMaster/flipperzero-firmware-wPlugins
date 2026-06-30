## 1.22
 - Create credential flow
 - Improve the look of the info screen, more like the read screen
 - Support to change FC during emulation for some formats
 - Add support for emulating NRMAC dumped cards without save
## 1.21
 - Removed wiegand plugin
 - Support for changing CN during emulation for some formats
## 1.20
 - Parse SIO
 - Move to GitHub
 - Auto NR-MAC read for iClass SE
## 1.19
 - Capture longer SIO
 - 'Save as Seader' for iClass SE Captured with NR-MAC
## 1.18
 - Added Save SR as legacy from saved menu
 - Fix write key 'retry' when presented with new card
## 1.17
 - CVE-2024-41566: When keys are unknown emulate with a dummy MAC and ignore reader MACs
## 1.16
 - Acknowledgements page
 - Elite VB6 RNG keygen attack
 - Bump plugin version
## 1.15
 - Add downgrade from iClass SR to iClass Legacy
## 1.14
 - Add plugin to parse some wiegand formats
 - Store unknown blocks in picopass file with '??'
## 1.13
 - Rework loclass writer with datetime lib
## 1.12
 - Add support for non-secure Picopass
 - Change Read to use all dictionaries
 - Improve saving of cards authenticated with NR-MAC
## 1.11
 - Update working with keys with new API
 - Display more tag information
 - Add additional keys to elite dict
 - Correct config card detection so it doesn't happen for SE cards (read using nr-mac partial read)
 - Have back button go to start menu instead of read retry
## 1.10
 - Fix missing folder in readme
 - Allow partial save for any read failure
## 1.9
 - Fix bug (#77) with loclass
 - Better loclass notes
 - Read card using nr-mac
 - Save as Seader format
## 1.8
 - Minimal changes for recent API updates
## 1.7
 - Rework application with new NFC API
## 1.6
 - Faster loclass response collection
 - Save as LF for all bit lengths
 - Removes unvalidated H10301 parsing
## 1.5
 - New random filename API
## 1.4
 - Optimize crypto speed to fix compatibliity with Signo and OmniKey readers
## 1.3
 - Show standard key instead of hex bytes when detected
## 1.2
 - Sentinel bit remove
## 1.1
 - Key dicts moved to app assets 
## 1.0
 - Initial release
