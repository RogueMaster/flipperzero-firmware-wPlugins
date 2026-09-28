<!-- SPDX-FileCopyrightText: Mistial Dev -->
<!-- SPDX-License-Identifier: GPL-2.0-or-later -->

# TWIC canceled card lists

Enable `TINY_CRYPTO_ENABLE_TWIC_CCL=ON` and include
`<tiny_crypto/twic_ccl.h>`. The desktop resource profile enables it by default.
The reader has no heap or cryptographic dependencies.

The [TSA CCL feed](https://tsaenrollmentbyidemia.tsa.dhs.gov/canceled-card-lists)
identifies canceled or suspended credentials by FASC-N. Each CSV row contains
50 hexadecimal characters, a comma, and a `ddMmmYYYY` cancellation date.
The parser accepts uppercase or lowercase hexadecimal and English title-case
month abbreviations. Dates must be valid Gregorian dates with a nonzero year.
Records may end with CRLF or LF. Empty lists and unterminated final rows fail
validation. Quoting, headers and extra fields fail validation.

## Reading and lookup

`TC_TWIC_CCL_read` decodes a single row with its line ending removed. Its output
contains the 25-byte FASC-N and cancellation date. Output remains unchanged on
failure. The date describes the individual cancellation event.

`TC_TWIC_CCL_contains` scans a complete CSV buffer. Pass `chuid.fascn` from the
CHUID reader directly as the query and set a maximum record count. It validates
the complete buffer before writing the membership result. Duplicate records are
accepted and have the same membership effect as a single record. Each lookup
takes linear time in the input size.

For chunked input, use `TC_TWIC_CCL_stream_init`, `stream_update` and
`stream_finish`. Initialization takes maximum byte and record counts, a callback,
and its context. Zero limits permit zero work. Complete rows are parsed directly
from input chunks; a split row uses at most 61 bytes of retained input. The
callback receives a decoded record valid for that call. Store records in staging
storage when building a persistent list.

Stream failures persist until reinitialization. Earlier callback writes remain
in staging storage after an error. A successful `finish` confirms framing and
record validity. The application must also verify download completeness,
provenance and freshness before publishing the staged list. A download cut at a
record boundary can still be a syntactically valid CSV file.

Input chunks, parser state and outputs require disjoint storage. Keep input
bytes stable while a call is running. Callbacks must avoid modifying or
reentering the parser.

## Credential policy

An absent identifier reports membership in the supplied list. Credential
acceptance also requires the application's list freshness policy, card
authentication, expiration and access checks. Keep the selected list stable
throughout that decision. Preserve the previous active list when an update
fails.

The TSA download page also supplies an MD5 checksum. It can detect download
corruption; authenticated retrieval or trusted provisioning establishes the
list's origin. Track publication and retrieval metadata separately from the
per-record cancellation dates.

`TC_TWIC_CCL_check_freshness` checks trusted publication/retrieval timestamps
against an application policy in Unix seconds. Set `now`, the maximum permitted
publication age, and a persisted `minimum_publication` floor. The age limit is
inclusive; a zero limit requires publication at the current instant. A later
download of the same list retains the original publication age. Future receipt
times and receipt preceding publication return `TC_TWIC_CCL_INVALID`; expired
or older-than-permitted publications return `TC_TWIC_CCL_STALE`. Call this helper
at the application's chosen policy boundary and handle its result according to
the deployment's warning and access rules. Store operations perform no age check.

TSA's *TWIC NEXGEN & Legacy, Part 4, version 4*, §2.6
directs PACS implementations to retain the previous list and issue a warning
after an unavailable or empty download, verify the supplied message digest,
download daily, and warn when the list is older than three days.

For maritime checks governed by
[33 CFR 101.525](https://www.ecfr.gov/current/title-33/chapter-I/subchapter-H/part-101/subpart-E/section-101.525),
the information-age limits are seven days at MARSEC Level 1 and one day at
Levels 2 and 3. An increase in MARSEC level requires an update within twelve
hours. Card validity checks must use the most recently obtained list. Configure
age limits, update triggers and warning handling in the application for its
deployment. Publication timestamps supplied to the helper require their own
trusted provenance; CSV cancellation dates describe individual records.

The visual list, VCCL, uses the printed card identification number (CIN).
Electronic CCL queries use the full FASC-N. TWIC certificate revocation and
credential cancellation are separate checks; suspended credentials can appear
on the CCL while their certificates remain unrevoked. See the
[TWIC Reader Specification, Part 3, sections 4.4.3 and 4.4.4](https://www.tsa.gov/sites/default/files/5c.-twic-nexgen-legacy-part-3-reader-specification-v4.pdf).

## Indexed storage

For repeated checks, provision a packed array of 25-byte FASC-Ns sorted in
unsigned byte order. Retain every identifier from the validated CSV; duplicate
keys may be retained or deduplicated. Sorting can take place during provisioning
on the host. The resulting image uses 25 bytes per retained key and can reside
in external flash or a file.

`TC_TWIC_CCL_source` supplies a count, context and indexed read callback.
`TC_TWIC_CCL_index_prepare` scans the source once to validate key lengths and
ordering. It accepts a maximum record count and writes the index only on
success. Bind the prepared index to the completed import and its provenance
metadata before making it available to credential checks.

For an image already held in stable memory, `TC_TWIC_CCL_index_from_memory`
accepts a pointer to its `TC_bytes` descriptor and the maximum record count.
It checks record boundaries and ordering through the same preparation path;
indexed reads borrow keys directly from the image. Keep the descriptor and
bytes unchanged until all indexes and snapshots using them have been released.
Lookup output storage must be separate from that image and descriptor.

Host utilities can load a bounded packed file with `example_read_file` from
`examples/pki_input.h`, close the file, and prepare this memory-backed index.
The owned buffer remains stable across later file replacements. Provision the
image and its publication metadata together through a trusted channel.

`TC_TWIC_CCL_index_contains` performs exact binary search. It takes the same
FASC-N span as the CSV lookup, plus a maximum read count. An index of `n` keys
requires at most `floor(log2(n)) + 1` reads per query. A failed read produces
`TC_TWIC_CCL_SOURCE_ERROR`; a wrong-length key produces `TC_TWIC_CCL_INVALID`.
Exhausting the read budget produces `TC_TWIC_CCL_LIMIT`. All leave the membership
output unchanged.

The source may reuse one 25-byte read buffer. Preparation preserves the preceding
key while checking ordering, and lookup preserves the query across reads. Keep
key values and ordering unchanged for the index's lifetime. Protect shared read
buffers with application locking, or give concurrent readers separate buffers.

[twic_ccl_storage.c](../examples/twic_ccl_storage.c) adapts a byte-addressed
storage callback to this API. Its caller-owned `ExampleTwicCclStorage` retains
the read buffer. Set `context`, `read_at` and the provisioned image's `length`,
then call `example_twic_ccl_open`. A successful result supplies an index for
`TC_TWIC_CCL_index_contains`; pass the CHUID reader's `fascn` field as the query.
Keep the storage object and image alive until all checks using that index end.

## Snapshots

Keep two or more zero-initialized `TC_TWIC_CCL_snapshot` slots and a
`TC_TWIC_CCL_store`. After validating and persisting a provisioned index and its
metadata, call `TC_TWIC_CCL_store_prepare` on a free slot, then
`TC_TWIC_CCL_store_publish` with the expected store revision. A failed publish
preserves the current list. Discard a prepared slot to abandon that update.
Publication rejects an older publication timestamp than the current list.

Acquire the current slot with `TC_TWIC_CCL_store_acquire`, query it with
`TC_TWIC_CCL_snapshot_contains`, and release it with `TC_TWIC_CCL_store_release`.
A superseded slot remains allocated until its readers release it, but queries
return `TC_TWIC_CCL_STALE`. Acquire the new current slot and repeat the lookup.
Missing lists return `TC_TWIC_CCL_UNAVAILABLE`. Lookup outputs remain unchanged
on either result.

Serialize these operations and the final validity decision with application
locking. That boundary prevents an update from superseding a successful lookup
before the application consumes it. Source buffers need the same protection.
Apply warning/age policy explicitly using the held metadata. Persist rollback
state and the completed image before publishing; restore both after restart.

`example_check_twic_cancellation` in the storage example combines acquisition,
an explicitly supplied age policy, lookup and release. Its `listed` output is
the membership result; `age_warning` reports a separate caller-selected warning
threshold. The example treats `policy.max_age` as an acceptance limit. Both
outputs remain unchanged on failure, and every acquired snapshot is released.
Use it as the cancellation step of a credential workflow with separate card
authentication, expiration and access-right checks.

## Staged import example

[twic_ccl_import.c](../examples/twic_ccl_import.c) combines CSV parsing and MD5
verification. Enable both CCL and MD5 support. Initialize its caller-owned state
with the decoded expected checksum, byte/record limits, and a callback that
appends each parsed key to private staging storage. Feed the exact download
chunks through `example_twic_ccl_import_update`.

After a complete download, sort the staged keys and expose them as an immutable
`TC_TWIC_CCL_source`. Retain duplicates in this example: the source count must
equal the parsed record count. `example_twic_ccl_import_finish` verifies framing,
the checksum, record count and sorted index before preparing the proposed slot.
Wrong checksums return `TC_TWIC_CCL_CHECKSUM_MISMATCH`, including a download
truncated exactly at a valid row boundary.

On success, persist the completed image and its metadata, apply update policy,
and publish under the store lock. On failure, keep the active list, discard the
staged image and issue an application warning with the returned error. Clear the
import state before releasing its storage. Metadata and the expected checksum
must come from trusted retrieval or provisioning.

## Tests

Build `test_twic_ccl` and run `ctest --test-dir build -R '^test_twic_ccl$'`
after configuring the test build. Synthetic munit cases cover decoding,
membership, invalid rows, chunk boundaries, truncation, limits and sink failures.

To check an independently downloaded feed with the same C reader:

```sh
TC_TEST_TWIC_CCL=/path/to/CCL.CSV \
TC_TEST_TWIC_CCL_MD5=the_32_hex_digits_from_CCL.CSV.MD5 \
  ./build/test_twic_ccl
```

The external-file case skips when the file variable is unset. When a file is
provided, its expected MD5 is required and checked by the library. Unit tests run
without network access. The external-file case also builds a sorted key image
on the host, checks every identifier through the index, and compares mutated queries
with the C library's `bsearch`. The storage example is compiled and tested by
the synthetic suite.
