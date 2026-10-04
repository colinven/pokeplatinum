# CLAUDE.md

Pokémon Platinum decompilation (pret) with a **PC port** on top. The same source builds the original DS ROM and native PC builds (Linux, Windows, Switch).

## Layout
- `src/`, `include/`, `asm/`: game code (from pret). Many names are still addresses (`ov5_021D1A94.c`, `sub_0203xxxx`, `unk_*`): undocumented, not a style to copy.
- `src/port/`, `include/port/`: PC-port-only code. Debug ImGui windows (`sim_gui_prj.cpp`, cheats, create mon/item, map jump) and project settings (`sim_config_prj.c` → `sim_config_prj.ini`).
- `subprojects/libntr`: the platform layer (replaces the NitroSDK). Draws the DS 2D/3D engines with OpenGL (`libraries/sim/src/`), plus audio, input, files and threads. It is a separate git repo fetched by meson (`subprojects/libntr.wrap`); same for `libntrdwc`, `libntrwifi`, `libntrsystem`, `tracy` and `metang`.
- `res/`: game data. About 700 `meson.build` files turn sources into `.narc` archives with the tools in `tools/` (nitrogfx, nitroarc, msgenc, datagen…).
- `generated/*.txt`: plain name lists; metang turns them into enums/`#define`s (`generated/meson.build`).
- `platinum.us/`: ROM layout (`main.lsf`), filesystem list, SHA1 checksums.
- `docs/`: data formats, maps, 2D/3D rendering, logging, editor setup.

## Building
- DS ROM: `make` (needs the metroskrew toolchain: `make skrew`). PC: `make linux`, `make win64` (MSYS2, see README), `make nx`. Output goes to `build/`, `build_linux/pokeplatinum/` and `build_win64/pokeplatinum/`.
- Docker: `./drun.sh <command>` runs a command in the build container (`tools/docker/Dockerfile`), e.g. `./drun.sh make linux`. On ARM64 hosts the DS compiler (32-bit x86) also needs qemu binfmt set up on the host.
- Meson options (`meson.options`): `build_target`, `revision`, `logging_enabled`, `gdb_debugging`, `tracy_enable`, `skip_anim_scripts`. Change them with `meson configure <builddir> -D…` (`./meson.sh` runs meson in docker). Changing a global flag like Tracy rebuilds everything.
- Adding a `.c` file: add it to `pokeplatinum_c` in `src/meson.build` (port-only files go in the `build_target != 'arm'` block), **and**, for the ROM, add its object to the right section of `platinum.us/main.lsf`.

## Code rules
- Style: see `CONTRIBUTING.md` (PascalCase functions with a `Module_` prefix, camelCase variables, typedef'd structs, enums not typedef'd and with `= 0` on the first member). Format with `make format` (clang-format 19, `.clang-format`). `src/port/` predates these rules; don't copy its style.
- PC-only changes go inside `#ifdef SDK_PORT … #else <original> #endif`, so the DS build stays unchanged.
- Line endings: LF everywhere, except `res/**/*.txt` and `*.pal`, which **must** stay CRLF (they have to match the ROM). Don't let editors or `core.autocrlf` change them.

## Testing
- `make check` builds the ROM and compares SHA1s with `platinum.us/*.sha1`. **On this fork it fails by design**, because the port changes shared code. Only "Filesystem Checksums" is expected to pass. There are no unit tests, and CI only builds (`.github/workflows/`).
- So verify PC changes by building `linux`/`win64` and running the game, and check that the DS build still compiles with `make`.

## How the PC port runs (non-obvious)
- **One thread does everything**: game logic, 3D command processing, OpenGL calls, CPU 2D rasterizing and the swap. VBlank waits (`OS_WaitIrq`) call `SIM_Render` directly.
- **Game speed = frame rate.** The game ticks once per drawn frame, so low FPS means a slow game. `60Fps` mode (on by default) runs logic at 60 and halves per-frame movement when `enable60fpsSpeedFix` is set (see `MovementAction_InitWalk` in `src/unk_020655F4.c`). Any code that moves things a fixed amount per frame needs the same fix.
- The game reads and writes all its files (`save.bin`, `firmware.bin`, `sim_config*.ini`, extracted ROM data) in **its own folder**, and changes to that folder at startup. On first launch it asks for a US Platinum `.nds` and extracts it.
- Settings: `sim_config.ini` (libntr: resolution scale, keys, vsync) and `sim_config_prj.ini` (game: 60fps, cheats, walk speed). Tab opens the debug GUI.
- Adding a game setting: add a field in `include/port/sim_config_prj.h` → parse, default and save it in `src/port/sim_config_prj.c` → add a widget in `gui_configPrj.cpp` or `gui_cheats.cpp` → read it with `SIM_Config_prj_GetConfig()` under `#ifdef SDK_PORT`.

## Performance work
- Measure before changing anything: build with `-Dtracy_enable=true` and capture with the Tracy 0.13.1 tools (`tracy-capture`, then `tracy-csvexport -u -p`). libntr already has zones (`G3SIM_FlushArray`, `G3 texture upload`, `G2 layer upload`, `DrawEngine`, `SwapWindow`) and per-frame `G3 …` counters in `libraries/sim/src/`.
- Known trap: writing into a GL texture or buffer the GPU may still be using makes the driver stall, and this is very costly on OpenGLOn12 (Windows on ARM). That is why libntr has a 3D texture cache (`g3_draw.cpp`) and a ring of textures per 2D layer (`sim_main.cpp`). Keep that pattern.
- "Render time" in the debug GUI is really CPU frame time (everything except the swap).

## Git
- `subprojects/*` are separate repos: commit and push there, then point the `.wrap` `revision` at a branch that exists on the remote.
- Never commit ROMs (`*.nds`), extracted ROM data, saves or `build*/` output.
