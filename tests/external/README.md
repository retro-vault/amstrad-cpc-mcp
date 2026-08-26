# External CPC tests

Run `make external-tests` to fetch the pinned SHAKER 2.7 disk into
`build/external-tests/`. The file is downloaded from the authors' site and
verified before installation.

After `make roms`, `make firmware-test` boots all three model firmware sets and
compares their BASIC screens byte-for-byte with the pinned captures.
`make conformance-smoke` boots the CPC6128, types the SHAKER module-A command,
loads it through AMSDOS/uPD765A, and compares its menu capture. Both tests use
the release server through real MCP messages.

`make cpm-test` calls the public `cpm` MCP tool with its default committed disk
and compares the resulting CP/M 2.2 `A>` prompt with a pinned capture.

SHAKER is an interactive Gate Array and CRTC test suite. It is not part of the
automatic unit-test result, and merely launching it is not a compliance claim.
Use CPC6128 firmware, insert the disk, and type:

```text
RUN"SHAKE27A.BIN"
```

Replace `A` with `B`, `C`, `D`, or `E` for the remaining modules. Compare each
result with the real-machine photographs at [SHAKERLAND][shaker]. The project
does not use the words "SHAKER Approved" unless the suite's maintainers have
performed their stated verification procedure.

[shaker]: https://shaker.logonsystem.eu/
