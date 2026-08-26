# Conformance status

## What has been demonstrated

- CPC464, CPC664 and CPC6128 identities, 64/64/128 KiB RAM and model-specific
  FDC presence.
- Verified original firmware boots to BASIC v1, v2 and v3 respectively after
  100 standard frames, with visually inspected 768×272 raster captures.
- Four-T-state CPC memory-slot rounding for representative one-, three- and
  four-M-cycle instructions.
- Lower/upper ROM overlays and all eight CPC6128 RAM configurations used by the
  Gate Array/PAL.
- Overlapping Gate Array, CRTC, PPI, ROM, printer and disk address decoding.
- Gate Array pen/border and synchronized mode updates; CRTC type selection and
  register read masks; AY register masks and PPI reset state.
- TAP plus common and extended CDT blocks with exact 8/7 time conversion.
- Standard/extended DSK validation. AMSDOS catalogued the pinned SHAKER 2.7 DSK
  and loaded its module-A binary through the emulated controller.
- The included minimal CP/M 2.2 disk boots through the public `cpm` MCP tool
  and reaches a pinned `A>` prompt on the CPC6128.
- Indexed-PNG structure, CRC, DEFLATE and four/eight-bit pixel round trips.
- MCP handshake, discovery and representative code, port and screen tools.
- Real `xcc` C11 binary compilation, load at `0x8000`, execution to HALT and
  result verification in CPC RAM.
- Debug build and automatic suites under AddressSanitizer and
  UndefinedBehaviorSanitizer with `-Wall -Wextra -pedantic` clean.

## Accuracy language

“Cycle stepped” means every component is advanced from the shared 4 MHz time
base and bus accesses are observed at their actual machine cycle. “Cycle
perfect” would additionally mean every observable edge behavior for every
supported chip variant has been matched to real hardware. The first statement
is implemented and tested. The second remains a target, not a blanket claim.

## Open boundaries

| Area | Current boundary | Consequence |
|---|---|---|
| CRTC 0/1/2 | interlace, skew, cursor/light pen, vertical adjust and extreme live-register behavior incomplete | some SHAKER/demoscene cases may differ |
| CRT/sync | malformed or absent sync and exact running-picture recovery need hardware comparison | unusual overscan may shift or blank differently |
| frame API | MCP counts fixed 79,872-T standard frames | custom CRTC frame length is not reflected in the host frame counter |
| uPD765A | RQM/seek timing, EOT multi-sector and several commands incomplete | CP/M 2.2 boots; CP/M Plus, diagnostics and protected disks may fail |
| DSK | sector-level image, not flux | weak/long/overlapping protected tracks cannot be exact |
| 8255 | generic modes 1/2 and interrupt handshakes absent | irrelevant to stock CPC wiring, visible to synthetic PPI tests |
| analogue sound | ideal mono AY digital mix | speaker/filter/tolerance characteristics are not reproduced |
| cassette output | input playback implemented; recording absent | programs can save electrically but no host image is produced |
| expansions | no generic expansion-card attachment API | built-in ports work; external hardware is not modeled |

## External suite policy

SHAKER 2.7 and the CRTC Compendium are the primary video regression sources.
The maintainers explicitly require emulator authors to submit builds before
claiming their “SHAKER Approved” seal, categorized by CRTC type. This project
does not claim that seal. `make external-tests` fetches the unmodified disk for
manual comparison against their real-machine photographs.

The identical vendored Z80 core is exercised against 1,356 Fuse vectors in the
sibling Spectrum repository. This repository currently tests CPC integration
and timing directly and records the inherited result separately rather than
inflating its local test count.

## Next conformance work

1. Implement CRTC vertical-adjust, skew/interlace and type-dependent update
   rules directly from Compendium 1.10.
2. Automate SHAKER screenshots with its CSL/SSM protocol and compare by CRTC
   type to the portal's real-machine results.
3. Add uPD765A timed RQM/seek events, multi-sector transfer and remaining CPC
   commands; add protected-media formats only with a defensible timing model.
4. Derive frame completion from monitor/CRTC sync while retaining a separately
   named standard-time run bound for deterministic debugger requests.
5. Port the Fuse vector harness into this repository so the wrapper corrections
   are independently pinned here.
