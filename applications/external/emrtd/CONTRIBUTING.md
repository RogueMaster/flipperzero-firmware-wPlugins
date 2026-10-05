# Contributing

## Building

```bash
pip install --upgrade ufbt
ufbt update --channel=release      # firmware 1.4.3, API 87.1
ufbt                               # build dist/emrtd.fap
ufbt launch                        # build, upload and start on a connected Flipper
ufbt cli                           # the Flipper's serial console, for the logs
```

The application links against the firmware's own mbed TLS
(`fap_libs=["mbedtls"]`), so it is tied to the API version it was built for.
Build against the release channel unless you are deliberately testing another.
[docs/install.md](docs/install.md) has the SDK commands for Unleashed and
Momentum, and how to keep more than one deployed at a time.

## Tests

```bash
make -C tests/host test             # the suites, with ASan and UBSan
make -C tests/host config-drift     # the vendored mbed TLS config against the SDK
```

The first run clones mbed TLS 3.6.2 into `tests/host/.deps`. The host build
compiles `protocol/`, `crypto/`, `transport/` and `access/` natively with the
firmware's own `mbedtls_cfg.h`, so the host sees exactly the feature set the
device has: brainpool off, CMAC absent, `MBEDTLS_ECP_MAX_BITS` at 256.

Adding a suite takes three edits: a line in `tests/host/suites.h`, the file
name in `TEST_SRC` in `tests/host/Makefile`, and a
`test_suite_<name>(void)` in `tests/host/test_<name>.c`. `tests/host/test_mac.c`
is the shape to copy, and `tests/host/emrtd_test.h` has the macros.

Anything with a published test vector gets one. A pull request that changes
the cryptography or a parser without a vector or a crafted input to go with it
will be asked for one.

## Style

`.clang-format` at the root is the firmware's own, and the toolchain's
`clang-format` - version 18.1.8 - is what CI runs:

```bash
ufbt lint                          # check
ufbt format                        # fix
```

Beyond the formatter:

- every file opens with the SPDX identifier and the copyright line;
- comments say **why**, and cite the clause they come from, for example
  `ICAO 9303-11, 9.8.2`. A comment that restates the code is noise;
- identifiers, comments and anything the user sees are in English;
- no emoji, anywhere.

## The rules that come from the platform

[docs/platform.md](docs/platform.md) is a record of what was measured on this
firmware. Four of its entries will break a build or a read if they are
ignored:

- **never reference `mbedtls_mpi_exp_mod` or `mbedtls_mpi_core_exp_mod`.**
  They pull in a symbol that is not in the firmware's library, and the
  application fails to link. This is why PACE over MODP groups is out of
  scope;
- **no large buffers on the read path's stack.** The NFC worker thread has
  8 KB and the elliptic curve arithmetic runs there;
- **nothing holds a whole data group.** The heap is 186 KB, half of it
  taken by the application's own image, and DG2 can be forty; the export
  streams;
- **a large allocation is asked about before it is made.** `malloc` does not
  return NULL on this firmware. It reboots the device, so the only place a
  shortage can be handled is a check before the call. For one block, ask
  `memmgr_heap_get_max_free_block()`. For a whole phase such as a read, ask
  both that and `memmgr_get_free_heap()`, as `emrtd_scene_read_have_memory()`
  does. Nothing collects garbage either: whatever allocates a block frees it
  in the same scope. See items 17 and 22 of
  [docs/platform.md](docs/platform.md).

And two that come from the runtime: `furi_check()` and the `bit_buffer_*`
family abort the application on misuse rather than returning an error, so
sizes are validated before they are passed in - and every function that takes
a buffer takes its capacity with it.

In security code the review will look for: every return value checked, every
error path freeing what it allocated including mbed TLS contexts, a MAC
verified before anything is decrypted, a point checked to be on the curve
before it is multiplied, and key material wiped before it is freed.

## Artwork

The images the firmware compiles into the application are generated:

```bash
uv run --with pillow python assets/make_logo.py
uv run --with segno --with pillow --with zxing-cpp python assets/make_donate_qr.py
```

`images/emrtd_10px.png` must stay exactly 10x10 and one bit, because that is
what `fap_icon` accepts. Every other image in `images/` has to be one bit and
free of metadata as well, or `ufbt lint` rejects it - and `fbt` does not strip
an icon that nothing draws, so an image is added only when something uses it.
Both generators check the size and the bit depth of each file they write; the
QR code generator also checks for metadata, and reads the code back.
[docs/branding.md](docs/branding.md) says why the mark is shaped the way it is.

## Documents and data

Never commit anything read from a real document: no MRZ, no chip dump, no
export directory, no screenshot with a name or a number in it. The specimen
identity published in ICAO Doc 9303 - `L898902C`, ANNA MARIA ERIKSSON of
Utopia - is what the test fixtures use, and it is the only identity that
belongs in this repository. [docs/security.md](docs/security.md) has the rest.

## Pull requests

CI runs the host suites under ASan with leak detection, a formatting check,
a `ufbt` build against the release channel, and `ufbt lint`. Please run
`make -C tests/host test` and `ufbt lint` before opening one.

Say in the description what you tested it against. "Read a 2019 Czech
passport, PACE, brainpoolP256r1" or "Czech identity card, PACE over P-256,
opened with the CAN" is worth a great deal here: the country, the kind of
document, the year and the key say everything useful without saying whose
document it was.
