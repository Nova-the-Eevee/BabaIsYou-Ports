
## v1g: straight trapezoid 3D projection

The tabletop renderer now uses one shared perspective parameter for both horizontal convergence and vertical position. This keeps the left/right edges straight instead of bowing outward. Distant rows are packed closer together and the top of the playfield is pulled farther downward, producing a much cleaner flat-plane/trapezoid look.

Tuning constants in `source/render.c`:

```c
#define PLANE_TOP_SCALE       0.52f
#define PLANE_FAR_DROP_PX    96.0f
#define PLANE_ROW_CURVE       1.12f
#define PLANE_MAX_EYE_SHIFT   8.0f
```

# v1e - Tabletop stereoscopic 3D

This build re-enables true left/right-eye rendering and replaces the old per-object depth layers with a receding flat-plane projection. As the physical 3D slider rises, rows farther up the playfield shrink, move downward, converge toward the horizontal center, and receive increasing behind-the-screen stereo disparity. At slider 0 the top screen remains the original flat 2D view.

The large `Game` state remains static/BSS, preserving the v1d stack-overflow fix. Automatic NDSP startup is still disabled; audio can still be initialized manually from Debug.

# v1d stack fix

This build fixes an early-boot crash/hang caused by allocating the approximately 197 KiB `Game` structure as a local variable in `main()`. The 3DS main-thread stack is far smaller than that. The game state now lives in static/BSS memory and the ordinary call stack is explicitly set to 128 KiB.

The safe-boot diagnostics remain enabled: stereo and automatic NDSP initialization are still disabled until the base game is confirmed to boot.

# Baba Is You - Native Nintendo 3DS Port

A native Nintendo 3DS port of Nova's older PSP project, continued from the working Nintendo DS port.

This is **not** the DS version running through compatibility code. The renderer, input, UI and audio are native 3DS code using **libctru + Citro2D/Citro3D**.

## What is ported

- Same C gameplay core as the DS version
- Same three ASCII `.txt` levels in RomFS
- BABA / WALL / ROCK / FLAG rule system
- YOU / STOP / PUSH / WIN
- Horizontal and vertical rules
- Push chains
- Undo
- Smooth movement
- Directional Baba animation
- Native 400x240 top-screen rendering
- 320x240 bottom-screen whole-level minimap
- Pause menu with Continue / Debug / Rules
- Touchscreen pause-menu buttons
- D-pad **and Circle Pad** movement
- Existing PSP/DS music and sound effects via NDSP
- **Stereoscopic 3D** controlled by the real 3D slider

## 3D effect

The top screen is rendered once per eye when the 3D slider is above zero.

The game remains completely 2D mechanically, but visual layers have different stereo depth:

- Floor: screen plane
- Walls / rocks / flag: slightly forward
- Rule words: farther forward
- Baba: closest

This gives the scene a small layered/cardboard-diorama effect without changing the original graphics.

## Requirements

Install the devkitPro Nintendo 3DS development environment (`3ds-dev`). You need:

- devkitARM
- libctru
- Citro3D
- Citro2D
- tex3ds

These are normally installed by the devkitPro 3DS development group rather than individually.

## Build

From a devkitPro shell/terminal in this directory:

```sh
make
```

The main outputs should be:

```text
baba-is-you-3ds.3dsx
baba-is-you-3ds.smdh
```

The Makefile converts `gfx/sprites.t3s` into a texture atlas and embeds the `romfs` directory in the `.3dsx`.

## Running

A typical SD-card layout is:

```text
sd:/3ds/baba-is-you-3ds/
    baba-is-you-3ds.3dsx
    baba-is-you-3ds.smdh
```

Launch it through the Homebrew Launcher.

### Audio / DSP firmware

NDSP homebrew audio normally needs:

```text
sdmc:/3ds/dspfirm.cdc
```

With Luma3DS, open the Rosalina menu and use:

```text
Miscellaneous options... -> Dump DSP firmware
```

If NDSP cannot initialize, this port deliberately keeps running silently instead of failing to boot. The Debug page will show `NO DSP`.

## Controls

| Control | Action |
|---|---|
| D-Pad | Move |
| Circle Pad | Move |
| L | Undo |
| R | Reset level |
| X | Toggle music |
| START | Pause / resume |
| SELECT | Exit |
| Touch screen | Select pause-menu buttons |
| 3D slider | Stereoscopic depth |

Reset confirmation uses **A = yes** and **B = no**.

In the pause menu, use D-Pad + A or tap the buttons. B returns from the Rules/Debug page.

## Project layout

```text
source/
    game.c/h       Gameplay and rule system
    render.c/h     Citro2D top-screen renderer + stereo camera
    assets3ds.c/h  Sprite-sheet loader
    ui.c/h         Bottom-screen map and menus
    audio.c/h      NDSP sound/music
    main.c         3DS setup, input and main loop

gfx/
    sprites.t3s    tex3ds atlas definition
    *.png          Original graphics

romfs/
    levels/         Original ASCII level files
    audio/          Raw PCM audio

tools/
    level_editor.html
    test_game.c
```

## Editing graphics

Edit the PNGs in `gfx/` and run `make` again. `tex3ds` rebuilds the atlas automatically.

Unlike the DS hardware-sprite port, the 3DS renderer uses the PNGs at their **actual dimensions**, so 24x24 images do not need to be padded into 32x32 sprite slots.

## Editing levels

Levels are still plain ASCII files:

```text
romfs/levels/level1.txt
romfs/levels/level2.txt
romfs/levels/level3.txt
```

The old browser level editor is included at `tools/level_editor.html`.

## Notes

The gameplay core was regression-tested on the host after changing the level paths from NitroFS to RomFS. The actual `.3dsx` cannot be cross-compiled in the environment that generated this source because devkitARM isn't installed there, so the first real build should be done in your devkitPro setup.

Level 3 is preserved from the original project as-is, including its original rule layout.

## v1b startup-crash fix

This revision removes Citro2D's shared 3DS system-font dependency from the UI.
All bottom-screen text now uses a small built-in 5x7 bitmap font rendered with Citro2D rectangles.
This avoids `fontEnsureMapped()`/`svcBreak()` startup failures in emulators that do not have dumped system font data.
The unnecessary New 3DS speedup request was also removed from startup to keep initialization minimal.

## v1c emulator-safe boot

This build intentionally boots in conservative 2D mode and does **not** initialize
NDSP audio during startup. It presents visible boot stages before loading each
subsystem:

- GPU OK
- ROMFS OK
- SPRITES OK
- LEVEL OK

This is useful for diagnosing emulator hangs. Once the game is running, open
Pause -> Debug and press **Y** to attempt NDSP audio manually. If an emulator
freezes only after Y, the audio/DSP service is the culprit.

Stereoscopic rendering is temporarily disabled in this diagnostic build. Once
base boot is confirmed, it can be re-enabled separately without mixing two
possible startup failure sources.


## v1f stronger tabletop 3D

The tabletop projection is deliberately more dramatic now:

- far/top rows shrink to 50% scale at full slider (previously 68%)
- maximum downward horizon pull is 82 px (previously 38 px)
- maximum stereo eye shift is 7.5 px (previously 4.5 px)
- depth now uses a 1.35 power curve, so upper rows are affected much more strongly while the foreground stays relatively stable

The tuning constants live near the top of `source/render.c`.
