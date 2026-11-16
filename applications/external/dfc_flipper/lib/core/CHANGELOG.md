# Changelog

## 2.0.0 (2026-10-25)

- Add allocation-free AN10922 key diversification for AES-128, two-key TDEA,
  and three-key TDEA, including DESFire key-version preservation (#19).
- Add allocation-free typed decoders for GetVersion, application and file
  identifiers, file settings, key settings, and key versions (#20).
- Add allocation-free verification for authenticated ASCII SDM messages,
  including encrypted PICC and file data from captured card responses (#21).
- Add structured Transaction MAC receipt verification with helpers for the
  supported transaction inputs and Reader ID handling (#22).
- Stream Transaction MAC input through a bounded accumulator so transactions at
  the configured file-data limit remain verifiable (#159).
- Expose the new diversification, decoder, SDM, and Transaction MAC APIs through
  the Python, Rust, Java, Android, and .NET bindings.
- Document every public binding API with enforced Python, Rust, Java, Android,
  and .NET documentation builds.
- Split Python native declarations by responsibility, name every FFI parameter,
  and verify the complete signature registry against the public C header.
- Replace closed-domain binding integers with typed enums and make inactive
  snapshot and session fields explicit in the 2.0 source APIs (#199).
- Validate Python replay challenges before random generation or native and
  transport calls, preventing undersized EV2 buffers from reaching fixed-length
  native reads.
- Reject invalid ISO authentication references, algorithm and key combinations,
  replay challenge lengths, and EV2 capability inputs in Java before transport.
- Define SDM offsets and field-specific length units consistently across the
  native API and bindings, including absent-field validation.
- Add installable native and binding packages with one verified release
  inventory and package-consumer smoke tests (#156).
- Publish complete dependency metadata, configure automated dependency updates,
  and document private security reporting (#155, #157, #158).
- Add Java and Android JNI bindings and an Android library.
- Preserve JNI exception delivery in minified Android consumers (#141).
- Avoid undefined behavior in parameterized tests with empty parameter sets
  (#160).
- Harden build gates by installing the fuzz sanitizer runtime, compiling
  response FFI helpers only for enabled roles, treating parser initialization
  warnings as errors, and keeping the Rust transceive example unambiguous
  (#161, #162, #163, #164).
- Refresh the Android, .NET, Java, Python, and release-automation toolchains.
- Retry incomplete Android NDK downloads before archive and binding builds
  (#166).
- Remove dormant hosted formatting code from target common builds (#167).
- Use one reserved ISO file-ID policy throughout DER decoding (#168).
- Route WriteData through the canonical secure-messaging unwrap path while
  keeping already-clear payloads zero-copy (#170).
- Remove completed authentication challenge state from each emulator instance
  (#171).
- Calculate SDM MACs after the final mirrored response bytes are rendered
  (#169).
- Reuse the command response buffer while creating delegated applications
  (#172).
- Give emulator objects, response buffers, command chains, and transaction
  snapshots distinct allocation tags for static-storage ports (#175).
- Compact DER field indexes to reduce peak credential-decoder stack use
  (#173).
- Compact DER field indexing further and emit each constructed body once,
  reducing decoder stack and encoder work without changing canonical output
  (#217, #218).
- Consolidate reader authentication cleanup paths to reduce target code size
  while keeping secret wipes structurally enforced (#177).
- Pool key-set metadata only for applications that use it, reducing the full
  target credential by more than six KiB without reducing model capacity
  (#174).
- Store file-type-specific state in a tagged union and pack SDM presence flags,
  reducing each full target file slot by 52 bytes (#178).
- Journal only transactional files in the selected application and capture
  rollback data lazily, avoiding whole-pool snapshots and growth copies (#176).
- Keep minified Android instrumentation builds complete when AndroidX exposes
  Error Prone annotation metadata (#146).
- Add blocking Python bindings and safe Rust bindings for hosted and `no_std`
  target builds, with ABI-layout checks and caller-owned target storage.
- Use ABI 8 to verify the complete generated native layout and semantic-constant
  fingerprint before hosted bindings construct handles (#138, #140).
- Preserve Limited Credit Value when credentials cross the FFI and hosted
  binding models (#145).
- Keep session secrets out of ordinary binding snapshots, with a separate
  wipeable diagnostic boundary for conformance checks (#140).
- Support separately disabling PICC and delegated-application formatting, with
  protected configuration commands and scoped delegated formatting (#130).
- Add runnable transceive examples for .NET, Java, Android, Python, and Rust.
- Make semantic application-selection helpers consistently accept the
  human-readable application identifier order in every binding.
- Verify application identifier order end to end across command encoding,
  response decoding, card selection, and exported snapshots.
- Correct Java communication-mode values and Python reader FFI declarations,
  validate key-set storage widths at the FFI boundary, and enforce Rust 1.85
  compatibility in CI.
- Check formatting and documentation links across C and every binding.
- Bound encrypted clear-header lengths before reader state changes and cover the
  full byte-sized input domain across authenticated session families (#116).
- Validate binding integers before narrowing them to native field widths (#117).
- Stop Rust reader exchanges on terminal native statuses, preserve card status
  in Python and Rust, retry capacity failures, and verify both bindings' reader
  status constants against the C header (#136, #143).
- Make every credential decoder leave an empty, securely wiped credential after
  any failure, and enforce decoder coverage with a registry check and fuzz
  invariant (#115).
- Document and verify the 2.0 source-release process and artifacts.
- Generate target and Rust static libraries with the toolchain's native code
  model, and reserve position-independent objects for shared and JNI libraries.
- Add a reproducible Cortex-M4 footprint gate for code, static data, stack
  frames, model structures, and configured storage capacities.
- Refactor credential validation and serialization around explicit model invariants.
- Reject invalid communication settings, key references, transaction-MAC key
  types, and reserved access-right fields across credential codecs (#135).
- Require Full protection for key-gated ChangeFileSettings commands while
  retaining plaintext operation for free change access (#122).
- Support Full-protected ChangeKeySettings commands across D40, EV1, and EV2
  sessions (#125).
- Reject standard ISO file operations for Transaction-MAC applications and
  report EV1 short reads with the EOF warning (#120).
- Accept zero-initialized credentials with empty file and key pools.
- Make credential and file-pool updates transactional.
- Reuse prepared cipher contexts and stream segmented AES, DES, and TDEA MAC input,
  removing message-sized secure-messaging scratch buffers.
- Enforce explicit output capacities and initialization contracts across secure
  messaging operations (#124).
- Propagate cipher and MAC failures, reject overflowing or malformed inputs,
  and consistently wipe staged output, retired ISO-DEP requests, file-pool
  bytes, replay buffers, and secret scratch storage.
- Make reader, native, ISO, ISO-DEP, EV1, and EV2 retries state-atomic across
  authentication, response-capacity, chaining, and transport failures.
- Preserve the transaction identifier and establish a fresh command counter
  when EV2 NonFirst authentication changes the active key (#118).
- Distinguish valid zero-length D40 reads from authentication failure, reject
  MAC-only responses when a read requested data, and reject missing,
  truncated, or incorrect response MACs (#119).
- Report required JNI response lengths, wipe invalidated ISO-DEP responses,
  reset expired activations, and support ISO-DEP record commands.
- Roll back failed SDM, delegated-application, transaction-MAC, CommitReaderID,
  value, file-creation, and key-update operations.
- Preserve transactions across current-key changes while aborting them on
  application changes and terminal command failures.
- Correct transaction snapshots, rollback, Transaction-MAC lifecycle, and ISO
  update publication.
- Enforce file access rights and supported communication modes for ISO binary
  and record operations, including authenticated read CMACs (#120).
- Protect EV1 responses centrally, retain command-specific plain success
  responses, keep GetVersion continuation frames plain until the authenticated
  terminal response, and keep failures that retire authentication plain (#121).
- Apply the effective communication mode after access-right evaluation.
- Validate SDM counter keys, metadata-mirroring applicability, reset layouts,
  and reserved access-rights values, and reset the SDM read counter when its
  configuration changes.
- Reuse authenticated-session allocations and keep pending authentication identity
  separate from active authorization state.
- Reduce each secure-messaging and reader-session object by 2,256 bytes by
  eliminating message-sized scratch storage.
- Share mutually exclusive reader keys, pending authentication state, and
  command/response buffers, reducing persistent and per-exchange RAM while
  preserving configured capacities (#216, #219, #220).
- Reuse wiped exchange scratch across the Python, Rust, Java/JNI, and .NET
  bindings, and enforce one authentication boundary contract across them
  (#224, #225).
- Generate JNI packed-result layouts, split the flat FFI and JNI layers by API
  family, and enforce exact export ownership in the build (#223, #227, #228).
- Export the complete public FFI from every build role, with hygienic
  unsupported stubs and linked-symbol validation (#230).
- Report authentication in hosted and target PICC snapshots only while the
  corresponding authorization is active (#231).
- Consolidate AES wire-MAC, access-right, authentication, and little-endian
  helpers to reduce duplicate code and keep reduced profiles linkable
  (#226, #229).
- Enable function and data sections for target objects so firmware links can
  discard unreachable code (#221).
- Harden Python and .NET native-handle ownership across construction,
  closed-object access, callback reentry, disposal, finalization, and concurrent
  calls.
- Use constant-time authentication-secret comparisons and expand teardown and
  JNI secret-wipe verification.
- Wipe reader command storage at terminal exits and consume copied FFI response
  buffers promptly (#123).
- Standardize replay-random exhaustion across the hosted bindings.
- Make random providers fallible in ABI 5 and fail random-dependent operations
  atomically, distinguish retryable exhaustion from terminal backend failures,
  and propagate both consistently through every binding (#131).
- Propagate every native virtual-PICC status through the .NET binding (#139).
- Verify SDM random-padding failures leave counters, cached renders, and card
  sessions unchanged across native and ISO reads (#134).
- Enforce GetFileCounters file, access-key, command-MAC, and Full-response
  rules across D40, EV1, and EV2 sessions (#132).
- Make credential factory and blank initialization, and virtual-card
  anticollision, report random-source failures through boolean return values.
- Abort rejected ISO authentication handshakes and require a fresh challenge.
- Expand fault-injection, sanitizer, reduced-profile, teardown-wipe, capacity,
  long-transfer, and cross-binding corpus coverage.

## 1.2.1 (2026-10-01)

- Adapt the tiny_crypto_c backend to its split mode sources and buffer-based API.
- Document EV2 feature coverage.

## 1.2.0 (2026-09-25)

- Add a native reader, flat C interface, CMake build, and .NET library.
- Add build roles and smaller feature profiles for embedded targets.
- Correct command framing, secure messaging, and record transactions against physical card behavior.
- Expand native, managed, and conformance test coverage.

## 1.0.2 (2026-09-15)

- Fix DER parsing in optimized builds and expand crypto backend CI coverage.

## 1.0.1 (2026-09-15)

- Add selectable mbedTLS and tiny_crypto_c backends.
- Preserve emulator storage and improve ISO-DEP framing and recovery.

## 1.0.0 (2026-09-15)

- Initial release of the credential model, text and binary codecs, secure messaging, and virtual card.
