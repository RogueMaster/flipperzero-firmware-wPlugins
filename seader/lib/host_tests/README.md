# Host Test Layers

- `make test-host`: fast deterministic unit/policy tests. Keep this limited to Seader-owned helpers, formatting, parsing, and ownership-policy logic.
- `make test-asn1-integration`: narrow integration coverage for real ASN.1 ownership/free behavior.
- `make test-runtime-integration`: narrow mock-based integration coverage for HF release ordering and final state publication.

Do not add generated ASN.1 or firmware-heavy runtime dependencies to `make test-host` unless they are already a supported part of that surface.
