# CP/M system disk

`cpm-2.2-en.dsk` is a bootable English CP/M 2.2 system disk for the CPC6128.
It is included so the MCP `cpm` tool works without a separate media download.
The image contains the original system tracks, including their copyright
message, and empty CP/M file tracks. Ancillary programs, DR Logo, help files
and demonstration software from the original CPC6128 disk are not included.

The MCP workflow resets a CPC6128 with its stock firmware, inserts this disk,
waits for BASIC, types `|CPM`, and runs to the `A>` prompt. An alternative
CP/M 2.2-compatible DSK can be supplied through the tool's `path` argument.

## Provenance

The source is side 4 of the English CPC6128 system-disk set preserved in the
[NVG CPC archive][nvg] and described by [CPCWiki][cpcwiki]. In the source
archive it is named `cpmplue4.dsk`. `prepare-cpm-disk.sh` verifies the archive
and source image, retains tracks 0 and 1, fills the data areas of tracks 2
through 39 with CP/M's unused-entry byte `0xe5`, and verifies the result.

| Item | SHA-256 |
|---|---|
| `cpmplus.zip` | `92342890d77545e265f4de314526bc7b784466026067965c444c84e46df836e0` |
| source `cpmplue4.dsk` | `6a45b70d51478d68ace1aaa85ba694fd26068c0c9300d12f426761b8740219f0` |
| `cpm-2.2-en.dsk` | `1cd34b92fd8045abbc39ba9854406a5545ff08c79a2cb2076fb0c40db85b9aac` |

## Distribution

CP/M remains copyrighted. In July 2022, Bryan Sparks of DRDOS, Inc., successor
to Digital Research's assets, clarified a nonexclusive right to use,
distribute, modify and otherwise make CP/M and its derivatives available. The
full grant and authority statement is preserved by the [Unofficial CP/M Web
Site][license]. Amstrad's separate emulator permission allows its copyrighted
CPC code to be included while Amstrad retains copyright. This project retains
the boot image's on-screen copyright notice and charges nothing for it.

[nvg]: https://ftp.nvg.ntnu.no/pub/cpc/utils/cpc/cpmplus.zip
[cpcwiki]: https://www.cpcwiki.eu/index.php/System_Disk
[license]: https://www.cpm.z80.de/license.html
