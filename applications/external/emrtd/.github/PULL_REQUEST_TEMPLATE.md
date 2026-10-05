<!--
Thank you for the patch. The checklists below are the things that are easy to
miss on this platform and expensive to find later; tick what applies and
delete what does not.
-->

## What this changes

<!-- One or two sentences. If it fixes an issue, link it. -->

## Why

<!-- What was wrong, or what could not be done before. -->

## How it was tested

- [ ] `make -C tests/host test` passes
- [ ] New or changed behaviour has a test; anything cryptographic is pinned to
      a published vector (ICAO Doc 9303, RFC, NIST) or a crafted input
- [ ] Built with `ufbt` against the release channel and run on a device

Tested against a real document: <!-- e.g. "2019 Czech passport, PACE,
brainpoolP256r1, MRZ key" or "2023 Czech identity card, PACE over P-256,
CAN", or "no". Name the kind of document and which key opened it. Never
include a document number, a name or a date of birth. -->

## Platform checklist

- [ ] No reference to `mbedtls_mpi_exp_mod` or `mbedtls_mpi_core_exp_mod`
      (see docs/platform.md)
- [ ] No large buffers on the read path's stack; the NFC thread has 8 KB
- [ ] Nothing holds a whole data group in memory
- [ ] A large allocation is checked with `memmgr_heap_get_max_free_block()`
      first, and freed in the scope that allocated it
- [ ] Every return value checked, every error path frees what it allocated,
      mbed TLS contexts included
- [ ] Key material and credentials wiped before they are freed
- [ ] `ufbt lint` passes, and any new image in `images/` is one bit and used

## Documentation

- [ ] `docs/` updated if behaviour, screens or errors changed
- [ ] `.catalog/changelog.md` updated if this is user visible

## Data

- [ ] Nothing read from a real document is in this pull request - no MRZ, no
      dump, no export, no screenshot carrying an identity
