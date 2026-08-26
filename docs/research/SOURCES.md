# Research sources

Hash-pinned copies of the primary PDF inputs can be fetched into the build
tree with `make references`; see [REFERENCES.md](REFERENCES.md). Web pages and
source repositories remain linked directly below.

## Primary component documentation

- Zilog, [Z80 CPU User Manual UM0080](https://www.zilog.com/docs/z80/um0080.pdf).
- General Instrument, [AY-3-8910/8912/8913 Programmable Sound Generator data manual](https://map.grauw.nl/resources/sound/generalinstrument_ay-3-8910.pdf).
- Motorola, [MC6845 CRT Controller data sheet](https://archive.decromancer.ca/bitsavers.org/components/motorola/_dataSheets/6845.pdf).
- Intel, [8255A Programmable Peripheral Interface data sheet](https://deramp.com/downloads/intel/8255.pdf).
- NEC, [uPD765A Floppy Disk Controller data sheet](https://hxc2001.com/download/datasheet/floppy/thirdparty/FDC/NEC/UPD765_Datasheet_OCRed.pdf).
- Amstrad, [CPC464/664/6128 Firmware manual](https://cpctech.cpcwiki.de/docs/manual/s968se01.pdf).

## CPC hardware and formats

- Amstrad Gate Array description: <https://cpctech.cpcwiki.de/docs/garray.html>
- CPC I/O partial decoding: <https://www.grimware.org/doku.php/documentations/devices/io.devices?do=export_xhtml>
- CRTC Compendium 1.10: <https://shaker.logonsystem.eu/ACCC1.10-EN.pdf>
- CPC Z80 timing cheat sheet: <https://www.cpcwiki.eu/imgs/b/b4/Z80_CPC_Timings_cheat_sheet.20230709.pdf>
- Video-RAM/Gate Array timing analysis: <https://bread80.com/2021/06/03/understanding-the-amstrad-cpc-video-ram-and-gate-array-subsystem/>
- CPC464, CTM640 and GT64 service manual: <https://www.retronik.fr/DOCUMENTS/Info/Amstrad_CPC/Amstrad%20CPC464%20CTM640%20GT64%20Service%20Manual.pdf>
- Preserved GT64 green-level table: <https://github.com/libretro/libretro-cap32/issues/49>
- CDT format: <https://www.cpcwiki.eu/index.php?mobileaction=toggle_view_desktop&title=Format%3ACDT_tape_image_file_format>
- DSK format: <https://www.cpcwiki.eu/index.php/Format:DSK_disk_image_file_format>

## Emulator and test references

- Pinned component implementation, `floooh/chips` commit
  `ca7d7ddd3ba77b48685d24120cf413ea53786767`:
  <https://github.com/floooh/chips/tree/ca7d7ddd3ba77b48685d24120cf413ea53786767>
- Corresponding upstream chip tests: <https://github.com/floooh/chips-test>
- SHAKER test suite and real-machine results: <https://shaker.logonsystem.eu/>
- CPCEC source, ROMs and broad software test list:
  <https://github.com/cpcitor/cpcec> and
  <https://github.com/AmatCoder/CPCEG/blob/master/cpcec-e.txt>
- Arnold emulator source: <https://github.com/rofl0r/arnold>
- Fuse Z80 vectors, run by the sibling Spectrum project's identical core:
  <https://sourceforge.net/projects/fuse-emulator/files/fuse-test-suite/>
- Patrik Rak Z80 tests: <https://github.com/raxoft/z80test>

## ROM permission and images

- WinAPE permission record: <https://www.winape.net/help/general.html>
- 1999 permission summary: <https://msmemorial.if-legends.org/emu/amstradcpc.php>
- Permission transcript: <https://github.com/maziac/DeZog/blob/main/documentation/amstrad-rom-permissions.txt>
- Pinned download source: <https://github.com/cpcitor/cpcec>

## Showcase media

- Juan J. Martínez's free-to-play game catalog:
  <https://www.usebox.net/jjm/games/>
- The five author-hosted release pages used by the MCP boot showcase:
  [The Heart of Salamanderland](https://www.usebox.net/jjm/heart-of-salamanderland/),
  [Brick Rick](https://www.usebox.net/jjm/brick-rick/),
  [Kitsune's Curse](https://www.usebox.net/jjm/kitsunes-curse/),
  [The Dawn of Kernel](https://www.usebox.net/jjm/dawn-of-kernel/) and
  [Magica](https://www.usebox.net/jjm/magica/).
- Exact archive and DSK hashes, launch commands, capture points and media-rights
  notes are recorded in [the screenshot manifest](../screenshots/README.md).

## CP/M media and distribution

- CPCWiki's CPC6128 [system-disk inventory](https://www.cpcwiki.eu/index.php/System_Disk).
- The preserved English/French/German
  [CPC6128 disk archive](https://ftp.nvg.ntnu.no/pub/cpc/utils/cpc/cpmplus.zip).
- The Unofficial CP/M Web Site's July 2022
  [redistribution grant](https://www.cpm.z80.de/license.html).
- Exact source, transformation and result hashes are recorded beside the
  included disk in [data/disks/README.md](../../data/disks/README.md).

Sources were used to distinguish documented component behavior from CPC wiring
and from empirically observed out-of-spec CRTC behavior. Community test suites
are cited as tests, not silently treated as specifications.
