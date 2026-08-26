# Vendored chips components

The files under `include/chips` come from André Weissflog's `floooh/chips`
repository, pinned to commit `ca7d7ddd3ba77b48685d24120cf413ea53786767`
(2025-06-03). They retain the upstream zlib licence in each source file; the
licence is also copied as `LICENSE.upstream`.

Source: <https://github.com/floooh/chips/tree/ca7d7ddd3ba77b48685d24120cf413ea53786767>

Most headers are byte-for-byte upstream copies. Three are plainly marked local
derivatives:

- `am40010.h` adds the explicit pointer casts required when the C header is
  compiled in the project's C++20 implementation unit.
- `kbd.h` makes one narrowing conversion explicit for C++20.
- `cpc_system.h` derives from upstream `systems/cpc.h`. It adds a real CPC664
  configuration, selectable CRTC types 0/1/2, cassette input/output and motor
  wiring, the printer data latch, model-specific floppy and ROM behavior, a
  snapshot length fix, a controller-state reset that preserves inserted media,
  and C++20-compatible descriptor construction.

The system wrapper is deliberately kept beside the individual chip headers so
the complete provenance and local delta can be audited with one upstream diff.
