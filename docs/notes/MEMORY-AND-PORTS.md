# Memory and I/O ports

## Memory map

The Z80 always sees four 16 KiB windows:

| Address | Reset read mapping | Write destination |
|---|---|---|
| `0000-3fff` | OS lower ROM | RAM in current bank |
| `4000-7fff` | RAM | RAM |
| `8000-bfff` | RAM | RAM |
| `c000-ffff` | selected upper ROM | RAM in current bank |

Gate Array control bits disable either ROM overlay. The upper-ROM latch uses
slot 0 for BASIC and slot 7 for AMSDOS on the CPC664/6128. The CPC464 always
uses BASIC because it has no built-in AMSDOS ROM.

CPC464 and CPC664 expose physical RAM banks 0,1,2,3. CPC6128 configuration
values use these physical bank indices:

| Config | `0000` | `4000` | `8000` | `c000` |
|---:|---:|---:|---:|---:|
| 0 | 0 | 1 | 2 | 3 |
| 1 | 0 | 1 | 2 | 7 |
| 2 | 4 | 5 | 6 | 7 |
| 3 | 0 | 3 | 2 | 7 |
| 4 | 0 | 4 | 2 | 3 |
| 5 | 0 | 5 | 2 | 3 |
| 6 | 0 | 6 | 2 | 3 |
| 7 | 0 | 7 | 2 | 3 |

## Partial I/O decoding

The CPC does not have one exclusive number per chip. Every I/O transaction is
offered to every decoder whose address-line condition is true. Conventional
addresses set otherwise-unused bits high to avoid accidental overlaps, but MCP
deliberately permits every alias.

| Device | Address-line condition | Conventional use | Direction |
|---|---|---|---|
| Gate Array/PAL | A15=0, A14=1 | `7fxx` | write/effect on access |
| CRTC | A14=0; A9=R/W, A8=RS | `bcxx`-`bfxx` | read/write by subport |
| upper ROM latch | A13=0 | `dfxx` | write |
| printer latch | A12=0 | `efxx` | write |
| 8255 PPI | A11=0; A9,A8 select | `f4xx`-`f7xx` | read/write |
| expansion select | A10=0 | device-specific | no generic device attached |
| disk motor | A10=0,A8=0,A7=0 | `fa7e` | write, 664/6128 |
| uPD765A | A10=0,A8=1,A7=0 | `fb7e`/`fb7f` | read/write, 664/6128 |

The printer data latch is present, but a host printer sink is not currently
exposed. No arbitrary expansion-card callback is attached; A10 appears in the
map because it participates in disk/expansion selection.

## MCP behavior

`ports` returns these conditions for the selected model. `read_port` and
`write_port` use the same overlapping composition as a Z80 I/O cycle but do
not advance time. They are debugger operations: use a small Z80 program when
the exact relative timing of a port access and the raster is under test.

The real CPU path evaluates the PPI before the Gate Array, which allows the
PPI's read value to become Gate Array input during intentionally overlapping
transactions. This order is required by Arnold's `OnlyInc` hardware test.

## References

- [CPC I/O devices and partial decoding](https://www.grimware.org/doku.php/documentations/devices/io.devices?do=export_xhtml)
- [Gate Array documentation](https://cpctech.cpcwiki.de/docs/garray.html)
- [Amstrad firmware manual](https://cpctech.cpcwiki.de/docs/manual/s968se01.pdf)
