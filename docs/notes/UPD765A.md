# NEC uPD765A and CPC drive notes

## Model presence and bus

CPC664 and CPC6128 contain the floppy interface; CPC464 does not. The internal
controller is an NEC uPD765A-family device connected for programmed I/O. When
A10=0, A8=0 and A7=0, a write controls the drive motor from D0. With A8=1, the
same region selects the controller: A0=0 reads the main status register and
A0=1 transfers command, execution or result bytes. Common aliases are `0xfa7e`
for motor and `0xfb7e`/`0xfb7f` for status/data.

The implementation maintains command, execution and result phases, main/status
bytes, a 16-byte command/result FIFO, drive track/head state and motor state.
The DSK backend connects sector lookup and data transfer callbacks to drive A.

## DSK images

Both standard `MV - CPCEMU Disk-File` and extended `EXTENDED CPC DSK File`
containers are accepted. Track headers, sides, sector IDs, sizes, status bytes
and per-track lengths are validated before insertion. An unsuccessful insert
does not become a mounted disk.

The image remains in memory and is read through controller commands issued by
AMSDOS. There is no firmware-side shortcut. In the external smoke test, AMSDOS
successfully catalogued the pinned SHAKER DSK and loaded its 23 KiB module A.
The bundled minimal CPC6128 system disk also boots CP/M 2.2 to its `A>` prompt
through the MCP `cpm` workflow.

## Implemented command path

The core handles the CPC's normal specify, seek/recalibrate, sense interrupt,
sense drive, read ID, read data and write data flows. Sector status and missing,
write-protected or end-of-sector conditions are returned through ST0-ST2.

## Conformance boundary

The current uPD765A core is functional rather than rotationally cycle-exact.
RQM is treated as ready throughout command/result phases instead of going low
for the data-sheet's microsecond handshake interval; seeks do not consume real
time; multi-sector EOT and the N=0/DTL special length need further work. Read
deleted, write deleted, read track, format and scan commands are not complete.
Weak sectors, flux timing and copy-protection behavior cannot be represented by
ordinary DSK sector data.

These limits do not affect the demonstrated AMSDOS CAT/load or CP/M 2.2 boot
paths, but they do mean CP/M Plus, protected disks and direct FDC diagnostics
are not yet supported completely or “cycle perfect.”

## References

- [NEC uPD765A data sheet](https://hxc2001.com/download/datasheet/floppy/thirdparty/FDC/NEC/UPD765_Datasheet_OCRed.pdf)
- [Amstrad firmware manual](https://cpctech.cpcwiki.de/docs/manual/s968se01.pdf)
- [CPCEMU DSK format](https://www.cpcwiki.eu/index.php/Format:DSK_disk_image_file_format)
- [Floooh uPD765 core at the pinned revision](https://github.com/floooh/chips/blob/ca7d7ddd3ba77b48685d24120cf413ea53786767/chips/upd765.h)
