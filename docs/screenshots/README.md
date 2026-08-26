# Game showcase provenance

These are live 640×200 captures from `amstrad-cpc-mcp`, not promotional
images copied from a website. Each disk was inserted at process startup, the
stock CPC6128 firmware booted for 100 frames, and the documented AMSDOS
`RUN` command was typed through the MCP `press_keys` tool. The `screenshot`
tool then encoded the same indexed framebuffer once with `monitor: "color"`
and once with `monitor: "green"`.

The five releases are a varied, modern CPC smoke test selected from Juan J.
Martínez's official [free-to-play catalog][catalog]. The archives themselves
are downloaded to the ignored `build/games/` directory and are not
redistributed by this project. Their copyrights and other rights remain with
their creators.

| Game | Version | Disk SHA-256 | AMSDOS command | Capture point |
|---|---|---|---|---|
| [The Heart of Salamanderland][heart] | 1.0.3 | `66269fd18477bb18fa317ffb30d3a66d3586097689007273dd3f93940f533a1c` | `RUN"HEARTOSA"` | gameplay |
| [Brick Rick][brick] | 1.0.1 | `c5e0cc27f96a3c611d48228b7883d3e83e765ba438291a789423f9049593d8b0` | `RUN"BRICKR"` | interactive title |
| [Kitsune's Curse][kitsune] | 1.0.1 | `4f1733deae1ae76a58b675998507e0fa0675d5a02f817d4f9b15a51ff2d6a8b8` | `RUN"KITCURS"` | interactive title |
| [The Dawn of Kernel][kernel] | 1.0.1 | `88b61193cfc1da50eaea4c0129bf1bfed9cadb291369b4522a170485c2c0fdcb` | `RUN"KERNEL"` | title artwork |
| [Magica][magica] | 1.0.2 | `0081f1ddb100188155ed32bce575fb161f33a98f05809a507aafdd27ca86eb6c` | `RUN"MAGICA"` | interactive title |

Reproduce the download and capture with:

```sh
make roms release showcase-captures
```

Code, graphics, sound and original game design are credited in each bundled
manual and in the main README's acknowledgements. The captures are included
for emulator documentation and inherit any applicable rights in the depicted
games; the project does not relicense that artwork under GPL-3.0.

[catalog]: https://www.usebox.net/jjm/games/
[heart]: https://www.usebox.net/jjm/heart-of-salamanderland/
[brick]: https://www.usebox.net/jjm/brick-rick/
[kitsune]: https://www.usebox.net/jjm/kitsunes-curse/
[kernel]: https://www.usebox.net/jjm/dawn-of-kernel/
[magica]: https://www.usebox.net/jjm/magica/
