# User guide

## Starting a machine

Build and fetch the verified firmware once:

```sh
make roms
make
```

Start one of the three classic models:

```sh
bin/bin/amstrad-cpc-mcp --model cpc464 --crtc 0 \
  --os-rom data/roms/cpc464-os.rom \
  --basic-rom data/roms/cpc464-basic.rom

bin/bin/amstrad-cpc-mcp --model cpc664 --crtc 1 \
  --os-rom data/roms/cpc664-os.rom \
  --basic-rom data/roms/cpc664-basic.rom \
  --amsdos-rom data/roms/amsdos.rom

bin/bin/amstrad-cpc-mcp --model cpc6128 --crtc 2 \
  --os-rom data/roms/cpc6128-os.rom \
  --basic-rom data/roms/cpc6128-basic.rom \
  --amsdos-rom data/roms/amsdos.rom
```

The default is CPC6128 with CRTC type 0. If any external ROM argument is used,
the complete model-specific set is required. `--load image.dsk` inserts drive
A at startup; `.cdt` and `.tap` insert the cassette. A file with another
extension is loaded at `0x8000` and selected as the execution address.

The server speaks newline-delimited JSON-RPC 2.0 on stdin/stdout. It accepts MCP
protocol `2025-03-26` and `2025-06-18`. Send `initialize`, then the
`notifications/initialized` notification before calling tools. `--verbose`
writes protocol diagnostics to stderr and never contaminates stdout.

## Minimal MCP session

```sh
printf '%s\n' \
  '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2025-03-26","capabilities":{},"clientInfo":{"name":"example","version":"1"}}}' \
  '{"jsonrpc":"2.0","method":"notifications/initialized"}' \
  '{"jsonrpc":"2.0","id":2,"method":"tools/call","params":{"name":"run","arguments":{"frames":100}}}' \
  '{"jsonrpc":"2.0","id":3,"method":"tools/call","params":{"name":"screen_text","arguments":{}}}' \
  | bin/bin/amstrad-cpc-mcp --model cpc464 \
      --os-rom data/roms/cpc464-os.rom \
      --basic-rom data/roms/cpc464-basic.rom
```

Use `--list-tools` to retrieve the machine-readable schemas without starting an
MCP session.

## Media tools

### `load`

Supply exactly one of `path` or inline `data`. Inline data is either a string of
hexadecimal pairs or a JSON byte array.

- `binary` writes at `address` (default `0x8000`); `start` also sets PC.
- `amsdos` loads an AMSDOS-headered binary; the presence of `start` requests
  its header entry point.
- `sna` restores a 64 or 128 KiB MV-SNA snapshot.
- `dsk` inserts a standard or extended CPCEMU disk in drive A.
- `cdt` and `tap` insert a pulse-decoded cassette.
- `rom` accepts OS+BASIC (32 KiB) on a 464 or OS+BASIC+AMSDOS (48 KiB) on a
  664/6128.

`reset: true` clears RAM before loading. `autoplay` defaults to true for tapes.

```json
{"name":"load","arguments":{"path":"build/demo.bin","format":"binary","address":32768,"start":32768}}
```

### `tape` and `disk`

`tape` actions are `status`, `play`, `stop`, `rewind` and `eject`. Status
includes decoded block and segment counts, duration and exact 4 MHz position.
The firmware controls the cassette motor through PPI PC4; transport time moves
only while the emulated motor is on.

`disk` reports drive A insertion/motor state or ejects the image. The CPC464
correctly refuses a built-in disk operation.

### `cpm`

`cpm` provides a complete CPC6128 CP/M 2.2 boot workflow in one MCP request.
It requires stock CPC6128 OS, BASIC and AMSDOS firmware. The tool resets and
clears the machine, inserts the included minimal system disk, waits 100
frames for BASIC, types `|CPM` through the CPC keyboard and runs another 1,000
frames to the `A>` prompt.

```json
{"name":"cpm","arguments":{}}
```

`path` selects another CP/M 2.2-compatible DSK instead of the bundled image.
`frames` changes the post-command run from 1 through 5,000 frames. The default
disk contains the CP/M system tracks but no ancillary command files; its
provenance and separate distribution terms are in `data/disks/README.md`.
CP/M Plus does not yet boot and is not presented as a supported mode.

## Execution and debugging

`run` accepts one or more bounds: `frames`, `tstates`, `instructions`, and
`stop_on_halt`. With no bound it advances one standard 50 Hz frame (79,872
4 MHz T-states). `run_until` stops when an instruction starts at `address` or
its `max_tstates` safety budget expires. `step` retires complete instructions
while all CPC chips continue to tick.

`status` reports the model, CRTC, RAM, PC, clock/frame counters, Gate Array
mode and border, selected AY register, PPI control word, ROM/media state and
motors.

`registers` reads or changes AF, BC, DE, HL, alternate pairs, IX, IY, SP, PC,
WZ, I, R, IM, IFF1 and IFF2. Flags are also shown as `SZYHXPNC`, uppercase when
set. Assigning PC cleanly discards a partial instruction.

`breakpoint` supports execute, memory-read, memory-write, I/O-read and
I/O-write matches. List, add, enable/disable, remove or clear breakpoints. Bus
breakpoints observe the real address and data cycle and stop at the following
instruction boundary.

`read_memory` and `write_memory` are debugger accesses and consume no emulated
time. Reads honor current ROM and RAM paging; writes beneath visible ROM reach
the underlying RAM exactly like CPU writes. Ranges wrap at `0xffff`.

## Keyboard and ports

`press_keys` either types `text` or holds a named chord in `keys`. Newline types
ENTER. Named keys include cursor directions, SPACE, ENTER, ESC, DELETE, CLEAR,
SHIFT, CTRL, TAB, CAPS, COPY and F0 through F9. `hold_frames` and `gap_frames`
make firmware scans deterministic.

`read_port` and `write_port` perform a side-effecting diagnostic I/O operation
without advancing time. They intentionally use all 16 address bits: the CPC
only partially decodes its devices, so a single address can select multiple
chips. Use `ports` to see the decoder conditions before choosing an address.

`chips` returns Gate Array, PPI and all sixteen AY registers. Supplying both
`ay_register` and `ay_value` changes one AY register directly, which is useful
for sound tests but bypasses the PPI bus transaction.

## Screen and audio

The screen is painted while the machine ticks. `screen` returns an MCP image
block containing an indexed PNG. `include_border` defaults to true for the
768×272 visible overscan; false returns the conventional 640×200 central crop.
`scale` is integer nearest-neighbor from 1 through 4. `screenshot` writes the
same result to a host `path`. Both tools accept `monitor: "color"` for the CTM
RGB palette or `monitor: "green"` for the GT64 luminance palette; `color` is
the default. Monitor selection changes PNG presentation, not Gate Array timing
or framebuffer indices.

`screen_text` reduces the central image to an 80×25 luminance preview. It is not
firmware-font OCR, so use PNG for exact pixels.

`audio_start` begins the AY mix at 8,000 through 48,000 samples per second.
`output: "file"` requires a `.wav` path. `output: "play"` returns playable WAV
audio from `audio_stop`; with `live: true`, ALSA's `aplay` becomes the pacing
clock. `speed: 100` is real CPC time. Captured output is mono signed 16-bit PCM.

## xcc development loop

Compile a freestanding program at a known address:

```sh
../xyz/bin/x/bin/xcc -Os -Wall -Wextra --oformat=binary \
  -Ttext=0x8000 source.c -o build/source.bin
```

Load it through startup `--load` or the `load` tool. Raw startup files select
PC `0x8000`; the MCP form can specify another `start`. A standalone program
must disable firmware overlays itself before using RAM at `0x0000` or
`0xc000`, or the debugger can write `0x8c` to a Gate Array port such as
`0x7f00` before execution.

The repository's `tests/fixtures/xcc_probe.c` is the smallest complete example.
`make test` proves the generated binary can execute to HALT and leave the
expected result in RAM.

## External conformance media

`make external-tests` fetches the SHA-256-pinned SHAKER 2.7 DSK. Start a
CPC6128 with firmware and that disk, wait for BASIC, and use
`press_keys(text="run\"shake27a.bin\"\n")`. Modules B through E use the same
name with the final letter changed.

SHAKER is interactive and its authors provide real-machine comparison images.
Launching it is a media-path smoke test, not a claim that every case passes or
that this emulator is officially SHAKER Approved.

## Installation

The normal `make` target creates a copyable Linux-style prefix tree:

```text
bin/
├── bin/amstrad-cpc-mcp
└── share/
    ├── amstrad-cpc-mcp/{roms,disks}/
    └── doc/amstrad-cpc-mcp/
```

`make package` refreshes this as a release-only tree and removes a stale debug
binary. `sudo make install` installs the same layout under `/usr/local`. Set
`PREFIX=/opt/amstrad-cpc-mcp` or use `DESTDIR` for package staging. Firmware
files are included only if `make roms` has installed them locally first.
The included CP/M 2.2 disk is always installed under
`share/amstrad-cpc-mcp/disks/`.
