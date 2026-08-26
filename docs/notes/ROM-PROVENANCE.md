# Firmware ROM provenance

In 1999 Amstrad granted emulator authors permission to use and distribute the
CPC system ROMs without charge while retaining the original copyrights and
notices. WinAPE records permission from both Amstrad and Locomotive for ROMs
distributed with CPC emulators; the Magnetic Scrolls Memorial independently
records Amstrad's 1999 free-use and distribution statement.

The repository does not modify or strip those embedded notices. The fetcher
uses the stock English images from CPCEC and rejects any byte change:

| Source image | SHA-256 |
|---|---|
| `cpc464.rom` | `00960d9bf75b2b90856c970f1aa078e1e2aa028b2c104f1dded0262f5d37b15e` |
| `cpc664.rom` | `1fcb20cf169f170774bf94954db9372c95edd038a5cb8e5199774552b93f8747` |
| `cpc6128.rom` | `31c3668c67bea027dab698ece233c9434d9324f9ba7dac84db58f400b6689562` |
| `cpcados.rom` | `ea65e0fb44ee93ede4b6c507509b7e5ddf497fb7155023bea91ef229469fa04d` |

Each model image is split, without transformation, into 16 KiB OS and BASIC
socket files. `cpcados.rom` is installed as the common 16 KiB AMSDOS image.
Generated ROMs are ignored by Git; `make roms` is the reproducible installer.

References:

- [WinAPE general information](https://www.winape.net/help/general.html)
- [Magnetic Scrolls Memorial CPC emulation page](https://msmemorial.if-legends.org/emu/amstradcpc.php)
- [Archived Amstrad permission transcript](https://github.com/maziac/DeZog/blob/main/documentation/amstrad-rom-permissions.txt)
- [CPCEC source and images](https://github.com/cpcitor/cpcec)
