# Architecture

## Design objective

The server is a persistent physical-machine model behind small MCP debugger
operations. It does not restart between requests. Loading code, pressing a key,
running 20 T-states and reading a port all act on the same chip state.

The timing invariant is simple: one `machine::impl::tick()` advances the Z80
and Gate Array by one 4 MHz T-state. The Gate Array divides that clock and calls
the CRTC and AY at 1 MHz. The cassette deck advances from the resulting PPI
motor state. No device receives an instruction-sized or frame-sized update.

```text
MCP/JSON-RPC tools
        |
        v
 cpc::machine facade ---- breakpoints, media, screen/audio observers
        |
        v  one 4 MHz tick
 Z80 <-> partial I/O bus <-> 8255 <-> AY-3-8912 <-> keyboard
        |                    |          |
        |                    |          +-- 1 MHz audio generators
        |                    +-- cassette motor/output/input
        v
 Gate Array/PAL <-> 6845 CRTC ---- cycle-rendered indexed raster
        |
        +-- ROM overlays and 6128 RAM banking
        +-- interrupt and READY/WAIT timing
        +-- uPD765A + CPC drive on 664/6128
```

## Library boundaries

`lib/cpc` is the project-owned facade. It supplies model selection, deterministic
run limits, safe debugger reads/writes, media ownership, key naming,
breakpoints, instruction retirement, snapshots and host observers.

`lib/chips` contains pin-level chip implementations from the pinned
`floooh/chips` revision. `cpc_system.h` is a documented local derivative that
adds CPC664, CRTC selection, cassette/printer wiring and model-correct ROM/FDC
configuration. Provenance is in `lib/chips/UPSTREAM.md`.

`lib/z80` is the byte-identical tick-stepped Z80 core from the same revision.
Project-owned retirement logic in `machine.cpp` adds the NMOS Q-latch effects
for SCF/CCF and the WZ corrections for repeating block I/O without changing the
vendored file.

The JSON, MCP, PNG and miniz libraries follow the sibling Spectrum emulator's
layout. Tools are archived separately from `main()` so protocol tests exercise
the same implementations the executable registers.

## CPU and Gate Array phase

CPC memory cycles are synchronized into four-T-state Gate Array slots. A plain
Z80 instruction such as `LD BC,nn`, nominally ten T-states, takes twelve on the
CPC because each of its three machine cycles occupies one complete slot.

Reset places the Gate Array divider one count before the first ready slot. The
debugger identifies the opening opcode fetch, but does not retire the preceding
instruction until the corresponding global four-T-state boundary. Tests pin
NOP at 4, `LD BC,nn` at 12, and `LD A,(nn)` at 16 CPC T-states.

## Memory ownership

The system owns eight physical 16 KiB RAM banks. CPC464 and CPC664 expose the
first four. CPC6128 Gate Array/PAL configuration values 0 through 7 choose a
documented four-bank mapping. Lower and upper ROMs are read overlays: writes
always reach RAM beneath them. Upper ROM number 7 selects AMSDOS on disk models;
other unavailable slots fall back to BASIC in the current built-in model.

## I/O composition

CPC devices use address-line conditions rather than exclusive port numbers.
The bus therefore calls every matching decoder for one transaction, in hardware
order. The 8255 is evaluated before the Gate Array, which preserves overlapping
PPI-to-Gate-Array transfers used by the Arnold `OnlyInc` test. CRTC, ROM select,
printer and disk decoding then see the same address/data pins.

Diagnostic MCP port accesses use the identical composition path but consume no
emulated clock. They are intentionally not ordinary memory-mapped variables.

## Video and frames

The CRTC emits memory address, raster address, display enable and sync signals
at 1 MHz. The Gate Array fetches two bytes per character period, decodes mode
0/1/2/3 pixels, applies ink/border changes at tick time and advances its CRT
beam. The public 768×272 raster is copied directly from that framebuffer and is
encoded as an eight-bit indexed PNG because the renderer palette has more than
sixteen entries. CTM colour and GT64 green monitor conversion happens only
while building the PNG palette, so selecting a monitor cannot perturb the
emulated beam.

The MCP `frames` bound uses the standard PAL interval of 79,872 4 MHz T-states.
This makes host capture deterministic. It is not a claim that deliberately
reprogrammed, nonstandard CRTC frames must have that length; see the conformance
record.

## Audio

The AY is ticked by the same 1 MHz callback as the CRTC. Its tone, noise,
envelope and mixer outputs are combined with the chip's logarithmic amplitude
table, resampled to the chosen host rate, and delivered to an observer. WAV and
live playback are consumers of that stream, not alternate timing paths.

## Media

DSK images stay attached to the drive model and are read through uPD765A
commands issued by AMSDOS. TAP and CDT images are decoded to edge spans up
front, then played one 4 MHz T-state at a time into PPI PB7 while firmware
controls motor state on PC4. CDT's 3.5 MHz storage convention is converted by
the exact wall-clock ratio 8/7.

The `cpm` tool composes existing public machine operations rather than adding
an operating-system shortcut: it resets the CPC6128, inserts the included
DSK, advances the stock firmware, presses the real `|CPM` key sequence and
continues clocking the same CPU, memory and disk-controller path.

## Failure containment

All tool arguments are validated before mutation. Runs are bounded; loops in
CDT control flow also has a fixed expansion limit. MCP handler exceptions
become JSON-RPC internal errors and do not terminate the session. Debug builds
use address and undefined-behavior sanitizers, full warnings, and the automatic
test suite.
