# Vendored Z80 CPU core

This directory contains a third-party Z80 emulator. It is **not** covered
by the project coding standard and must be kept byte-identical to
upstream so that it can be re-synchronised cleanly.

## Source

| Field      | Value                                                |
|------------|------------------------------------------------------|
| Project    | `chips` by Andre Weissflog (floooh)                  |
| Repository | https://github.com/floooh/chips                      |
| File       | `chips/z80.h`                                        |
| Commit     | `ca7d7ddd3ba77b48685d24120cf413ea53786767`            |
| Licence    | zlib/libpng (see `LICENSE.upstream`)                 |
| Vendored   | 2026-08-17                                           |

The zlib licence is GPL-3.0 compatible, so it may be combined with the
GPL-3.0 code in the rest of this project.

## Why this core

The server must expose every CPC machine cycle to the Gate Array. That rules
out most Z80 cores, which execute a whole instruction and only then report how
many T-states it took. The CPC synchronizes each memory machine cycle to its
four-T-state bus slots, which cannot be reconstructed faithfully afterward.

`z80.h` is **tick stepped**. One call to `z80_tick()` advances the CPU by
exactly one T-state and exchanges a 64-bit pin mask with the caller,
carrying the address bus, data bus and control signals (`M1`, `MREQ`,
`IORQ`, `RD`, `WR`, `RFSH`, `WAIT`, ...).

That buys two things this project cannot do without:

1. The Gate Array can be clocked in lockstep with the CPU, one T-state at a
   time, so video, interrupts and the processor genuinely interleave.
2. Every machine cycle announces itself on the bus, so the Gate Array can
   apply READY/WAIT at the cycle that actually caused it.

### Note on the `WAIT` pin

The core does implement `WAIT` faithfully:

```c
#define _wait()  {if(pins&Z80_WAIT)goto step_to;}
```

There are 297 such points, covering memory and I/O cycles. The CPC system core
feeds the Gate Array's returned WAIT pin into the next Z80 tick. The Gate Array
uses its four-phase divider to hold CPU bus cycles while continuing to tick the
CRTC, AY and raster. `tests/test_machine.cpp` pins the resulting CPC-rounded
instruction times.

Alternatives considered:

- **redcode/Z80** — extremely accurate and well documented, but executes
  in instruction-sized chunks. Per-T-state contention and beam
  synchronous rendering would have to be bolted on top.
- **z80ex / libz80** — instruction stepped, same objection.

`chips/z80.h` also passes the standard `zexall` / `zexdoc` exercisers and
models undocumented flags (`YF`/`XF`) and most `MEMPTR`/`WZ` behavior. The
project adapter in `lib/cpc/machine.cpp` supplies NMOS Q-latch behavior and
corrects `MEMPTR` after repeating block-I/O instructions; these stay outside
the vendored file so it remains byte-identical to upstream. The identical core
and corrections have also been checked with the sibling Spectrum project's
pinned Fuse vector suite; CPC-specific local tests remain the authority here.

## Local modifications

None. The file is byte-identical to upstream.

If a change ever becomes unavoidable, record it here and mark it clearly
in the source, as required by clause 2 of the zlib licence.

## Re-syncing

```sh
git clone --depth 1 https://github.com/floooh/chips.git
cp chips/chips/z80.h lib/z80/include/z80/z80.h
```

Then re-run `make test`. The machine timing and `xcc` execution suites will
catch integration regressions. Run the external Z80 exercisers recorded in
`docs/research/SOURCES.md` before accepting an upstream CPU change.
