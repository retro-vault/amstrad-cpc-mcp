# CTM colour and GT64 green monitors

## Hardware path

Classic CPCs were sold with either an Amstrad CTM colour monitor or a GT64/65
green-phosphor monitor. The computer exposes RGB and a separate luminance
signal on its six-pin monitor connector. The green monitor consumes luminance,
not merely the Gate Array's green RGB channel, so differently coloured inks
remain distinguishable as green intensity levels.

Monitor selection does not alter CRTC, Gate Array, CPU or framebuffer timing.
The framebuffer retains its 6-bit hardware colour indices; the selected monitor
is applied only when `screen` or `screenshot` encodes those indices as PNG.

## MCP rendering

Both image tools accept `monitor: "color"` or `monitor: "green"`; `color` is
the default. Colour rendering retains the CPC RGB palette. Green rendering uses
the preserved 32-entry GT64 luminance table, including its nonzero phosphor
floor. Blanking remains black, and internal sync diagnostics are converted to
the same green intensity range.

This models the monitor's steady palette response. It does not simulate CRT
mask geometry, bloom, scanline spread, persistence, brightness/contrast knobs
or analogue bandwidth.

## References

- [Amstrad CPC464/CTM640/GT64 service manual](https://www.retronik.fr/DOCUMENTS/Info/Amstrad_CPC/Amstrad%20CPC464%20CTM640%20GT64%20Service%20Manual.pdf)
- [Amstrad CPC464 user manual](https://commonplace.doubleloop.net/files/cpc464.en.pdf)
- [Preserved GT64 green-level table](https://github.com/libretro/libretro-cap32/issues/49)
