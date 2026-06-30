# Picopass

See readme: [https://lab.flipper.net/apps/picopass/](https://lab.flipper.net/apps/picopass/)

## Build

```bash
git clone https://github.com/bettse/picopass.git
cd picopass
git submodule init && git submodule update
ufbt
```

### To Build ASN1 (if you change sio.asn1)

 * Install git version of [asnc1](https://github.com/vlm/asn1c) (`brew install asn1c --head` on macos)
 * Run `asn1c -D ./lib/asn1 -no-gen-example -no-gen-OER -no-gen-PER -pdu=all sio.asn1` in in root to generate asn1c files


