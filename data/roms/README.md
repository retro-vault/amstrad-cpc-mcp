# Amstrad CPC firmware ROMs

Run `make roms` to fetch verified English firmware for the CPC464, CPC664 and
CPC6128. The downloader obtains the unmodified images distributed with CPCEC,
checks their SHA-256 hashes, and splits the 32 KiB OS+BASIC images into the
16 KiB files expected by the server. It also installs the common AMSDOS ROM.

The generated `.rom` files are ignored by Git. Start a machine with, for
example:

```sh
bin/bin/amstrad-cpc-mcp --model cpc6128 \
  --os-rom data/roms/cpc6128-os.rom \
  --basic-rom data/roms/cpc6128-basic.rom \
  --amsdos-rom data/roms/amsdos.rom
```

## Distribution permission and provenance

In 1999 Amstrad granted emulator authors permission to redistribute the CPC
system ROMs at no charge, provided the copyright messages remain intact. This
project does not alter those images or their embedded notices. Contemporary
records of the permission are retained by [WinAPE][winape], [Marcel de
Kogel's emulator archive][msm], and the [DeZog permission transcript][dezog].

The download source is [CPCEC][cpcec], whose repository includes the stock ROM
images alongside its GPLv3 emulator. Hashes are pinned in `fetch-roms.sh`; a
changed download is rejected rather than silently installed.

[winape]: https://www.winape.net/help/general.html
[msm]: https://msmemorial.if-legends.org/emu/amstradcpc.php
[dezog]: https://github.com/maziac/DeZog/blob/main/documentation/amstrad-rom-permissions.txt
[cpcec]: https://github.com/cpcitor/cpcec
