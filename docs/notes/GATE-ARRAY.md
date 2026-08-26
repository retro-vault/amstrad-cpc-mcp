# Amstrad 40010 Gate Array notes

## Responsibilities

The 40010 synchronizes Z80 memory access, generates the 1 MHz CCLK for the CRTC
and AY, fetches display bytes, decodes video modes, maps inks to the 27-colour
CPC palette, combines sync, creates interrupts, and controls ROM overlays. On a
CPC6128, a cooperating PAL supplies the extra RAM mapping selected through the
same I/O region; the implementation models them as one component.

## I/O register groups

The Gate Array is selected whenever A15=0 and A14=1, conventionally `0x7fxx`.
It does not inspect RD, so overlapping accesses can affect it. Data bits 7 and
6 choose the operation:

| D7:D6 | Operation |
|---|---|
| `00` | select ink 0..15 or border pen |
| `01` | assign a 5-bit hardware colour to the selected pen |
| `10` | video mode, lower/upper ROM disable, interrupt-counter reset |
| `11` | 6128 PAL RAM configuration 0..7 |

Mode changes become active at the synchronized position following HSYNC. Ink
and border writes affect pixels as the raster reaches them, so timed raster
bars appear inside a captured frame.

## Pixel generation

The CRTC supplies MA and RA. The Gate Array turns them into a 16-bit RAM fetch
address, reads two bytes per 1 MHz character period and expands them to sixteen
host pixels:

| Mode | Logical width | Inks | Host pixels per CPC pixel |
|---|---:|---:|---:|
| 0 | 160 | 16 | 4 |
| 1 | 320 | 4 | 2 |
| 2 | 640 | 2 | 1 |
| 3 | 160 | 2 | 4 |

The public raster is the 768×272 CRT-visible window. The central 640×200 crop is
a convenience only; no second renderer exists.

## Interrupt and sync

Falling HSYNC edges increment a 6-bit counter. Count 52 requests an interrupt
and clears the counter. VSYNC starts a separate HSYNC count; two HSYNCs later
the main counter is reset, requesting an interrupt first if its old value was
at least 32. A Gate Array control write can also clear the interrupt counter.
Z80 interrupt acknowledge clears the pending request.

CRTC HSYNC is delayed and clipped before it becomes monitor sync. The Gate
Array uses long combined sync to drive vertical retrace in the host CRT model.

## Memory arbitration

The sequencer provides the READY/WAIT relationship that rounds CPU machine
cycles into four-T-state slots while video fetches retain their position. The
implementation begins reset at the verified divider phase, eliminating an
otherwise spurious three-state wait before the first opcode.

## Validation and limits

Tests cover the decoder, ROM/PAL commands, mid-run mode changes, palette and
border state, CPC-rounded CPU timings, raster geometry and complete stock-ROM
boots. SHAKER 2.7 reaches its interactive menu through the actual firmware and
disk path.

The underlying CRT monitor model has documented edge limitations when sync is
missing or deliberately malformed, and some exact out-of-spec sync/picture
positions need SHAKER comparison against real hardware. Until those cases are
closed, “cycle stepped” is precise while an unrestricted “cycle perfect” claim
is not.

## References

- [Amstrad Gate Array technical description](https://cpctech.cpcwiki.de/docs/garray.html)
- [Amstrad firmware manual](https://cpctech.cpcwiki.de/docs/manual/s968se01.pdf)
- [Exploring the CPC RAM/video subsystem](https://bread80.com/2021/06/03/understanding-the-amstrad-cpc-video-ram-and-gate-array-subsystem/)
- [CRTC Compendium 1.10](https://shaker.logonsystem.eu/ACCC1.10-EN.pdf)
- [SHAKER real-machine test portal](https://shaker.logonsystem.eu/)
