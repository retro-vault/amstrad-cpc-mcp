# Cassette and CDT notes

## Physical path

Cassette input is connected to 8255 port B bit 7. PPI port C bit 4 controls the
motor and bit 5 is the cassette output. The transport advances only when the
emulated program has switched the motor on; stopping MCP execution freezes tape
position as it freezes every other chip.

## Time base

CDT stores pulse lengths in a 3.5 MHz reference time base. The CPC CPU samples
at 4 MHz, so every stored span is converted by 8/7, with integer rounding at
the central conversion boundary. A seven-unit CDT pulse therefore lasts
exactly eight CPC T-states. Millisecond pauses are
generated directly as 4,000 T-states per millisecond.

## Supported images

TAP is decoded as standard ROM-style blocks. CDT supports standard, turbo,
pure tone, pulse sequence, pure data, direct recording, CSW (raw and Z-RLE),
generalized data, pause/stop, signal level, group/text/archive metadata, jump,
loop, call/return and deterministic first-choice selection. Deprecated C64
blocks and embedded snapshots are rejected with a byte-offset error instead of
being silently skipped.

All blocks expand into a bounded series of level spans and control events.
Control flow is capped at one million executed blocks, and arithmetic overflow,
truncation, impossible symbol tables and trailing garbage are diagnosed.

## Transport semantics

Insert can autoplay or leave the deck stopped. Play, stop, rewind and eject are
explicit. Status reports original format/title/size, parsed blocks, data blocks,
expanded segments, total 4 MHz duration, current position, signal level, end
state and a media stop command.

Cassette output is exposed in chip state but recording a new CDT from the
output waveform is not implemented.

## References

- [CDT file format](https://www.cpcwiki.eu/index.php?mobileaction=toggle_view_desktop&title=Format%3ACDT_tape_image_file_format)
- [Amstrad firmware manual](https://cpctech.cpcwiki.de/docs/manual/s968se01.pdf)
