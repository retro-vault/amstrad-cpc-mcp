# Reproducible reference bundle

Run `make references` to download hash-pinned copies of the primary manuals
used for the implementation into `build/references/`. The PDFs are research
inputs owned by their respective publishers and are deliberately not committed
or installed with the program.

The bundle contains the Zilog Z80, General Instrument AY-3-8910/8912, Motorola
6845, Intel 8255A and NEC uPD765 manuals; the Amstrad firmware manual; CRTC
Compendium 1.10; and the archived 2013 CPC Z80 timing sheet. The current 2023
timing sheet remains linked from `SOURCES.md`, but its host rejects unattended
downloads, so the reproducible bundle uses the independently hosted earlier
edition.

SHA-256 verification makes a changed upstream document fail loudly. A failure
does not mean the document is malicious: compare the new artifact manually,
record why it changed, then deliberately update the pin.
