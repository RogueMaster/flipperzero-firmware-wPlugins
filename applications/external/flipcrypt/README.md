# FlipCrypt

FlipCrypt is a Flipper Zero app that provides a collection of classic cipher algorithms, cryptographic hash functions, and some text encoding methods to explore and learn about. You can emulate the result using NFC, generate a QR code, or save it to a .txt file on the Flipper.

## Ciphers
- AES-128
- Affine
- Atbash
- Baconian
- Beaufort
- Caesar
- Playfair
- Polybius Square
- Porta
- Rail Fence
- RC4
- ROT-13
- Scytale
- Vigenère

## Hashing Algorithms
- BLAKE-2s
- FNV-1a
- MD2
- MD5
- MurmurHash3
- SHA-1
- SHA-224
- SHA-256
- SHA-384
- SHA-512
- SipHash
- XXHash64

## Other
- Base32
- Base58
- Base64

## Usage
Navigate through the app's menu on your Flipper Zero to:
- Choose a cipher to encrypt or decrypt text.
- Select a hashing algorithm to generate a hash from input data.
- Select an encoding method to encode / decode inputs.

Once on the output screen, you can choose to save the output to a .txt file located at ext/flip_crypt_saved/, emulate it using NFC (NTAG215) or generate and display a QR code of the data (if the output is not too long). Not all three options will be available on every output screen due to memory limitations.

Warning - Being connected to qFlipper does make a decent amount of QR generations run out of memory and crash that work when standalone. Also, I do not guarantee accuracy of any of these functions - everything is provided as-is, don't use for anything important.

## Licensing
v0.1 is licensed under the MIT license.
v0.2 and later is licensed under the GNU General Public License.

All previous versions can still be accessed through the Releases page.

Any licenses for the cipher / hash function implementations I included is at the top of their respective code files.

## Future Ideas
- Add chain encoding / hashing
- Auto-detect cipher decoding
- Add hash bruteforcing
- Add Polybius square key support
- Option to upload .txt file / read NFC as input instead of always typing.

## 
Feel free to leave a Github issue / PR with a feature you'd like to see.
##
