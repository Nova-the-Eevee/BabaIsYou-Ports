# Baba Is You - Nintendo DS port

A BlocksDS/libnds port of Nova's older LuaPlayer Plus PSP project.

## Current DS version

- Real DS **OBJ/OAM hardware sprites** on the top screen.
- The original ASCII `.txt` level format in NitroFS.
- `YOU`, `STOP`, `PUSH`, and `WIN` rules, horizontal and vertical rule scanning,
  push chains, overlapping objects, undo, reset, and win progression.
- Baba's 3-frame directional animation.
- Original graphics, move/win sounds, and low-fi PCM music.
- Camera follow for levels larger than 256x192.
- A **whole-level minimap on the bottom screen**.
- The minimap includes a cyan rectangle showing the part currently visible on
  the top LCD.
- A touch-friendly **START pause menu** with `CONTINUE`, `DEBUG`, and `RULES`.
- The `RULES` page shows the currently active rules, so they no longer occupy
  the bottom screen during gameplay.

## Controls

### Gameplay

- **D-Pad**: Move
- **L**: Undo
- **R**: Reset prompt
- **X**: Toggle music
- **START**: Pause menu
- **SELECT**: Exit

### Pause menu

- **D-Pad Up/Down + A**: Choose an item
- **Touch screen**: Tap Continue / Debug / Rules directly
- **B**: Resume from the main pause menu, or go back from a sub-page
- **START**: Resume immediately
- **X** on the Debug page: Toggle music

## Bottom-screen minimap

The minimap is drawn as a native 8-bit bitmap background on the sub screen.
Instead of abstract colored squares, the asset converter now generates a tiny
10x10 preview of every real source PNG. The map therefore looks like a miniature
version of the actual puzzle: Baba looks like Baba, walls look like walls, and
rule words retain their real colors/shapes.

A cyan outline shows the section currently visible on the top LCD. It redraws
every frame, so the outline follows the smooth camera while puzzle objects remain
snapped to their cells.

## Bottom-screen text layering

Pause-menu labels, rules, debug information, reset prompts, and error messages
now use a real libnds **BG0 text console**. The minimap/buttons remain on the
8-bit **BG3 bitmap** underneath. This avoids the old hand-drawn bitmap font and
ensures text is always rendered above the menu boxes.

## Sprite anchoring fix

Every source PNG is stored in a 32x32 hardware OBJ slot. Smaller graphics such
as the 24x24 rocks, flags, and rule blocks are **top-left anchored** in that
slot. Transparent padding is only added to the right and bottom.

This matches the PSP project's coordinate convention: source pixel `(0, 0)` is
also the gameplay cell's top-left pixel. The earlier DS version centered 24x24
art inside the 32x32 slot, which introduced an unwanted 4-pixel offset.

## Building

You only need a normal current BlocksDS installation; no additional third-party
libraries are required.

From this folder:

```sh
make
```

The output should be:

```text
baba-is-you-ds.nds
```

## Editing levels

Levels remain plain text files:

```text
nitrofs/levels/level1.txt
nitrofs/levels/level2.txt
nitrofs/levels/level3.txt
```

The original browser level editor is included at:

```text
tools/level_editor.html
```

## Editing graphics

Original PNGs are in `assets_src/`.

The generated OBJ data lives in `source/assets.c`. After editing a PNG, run:

```sh
python3 tools/convert_assets.py
make
```

The converter needs Pillow, but Pillow is not required just to build the
already-generated project.

## Audio note

The original long stereo music file is converted to 8 kHz, mono, signed 8-bit
PCM so it fits comfortably with the rest of the game. Move and win effects use
11025 Hz PCM.

## Small logic fix from the PSP version

The old Lua `canMoveTo()` routine could move a push chain while merely checking
whether movement was possible, then move it again during the real move. This DS
port keeps collision testing and committing a push separate, avoiding that
possible double-push.

### v3b bottom-screen VRAM fix
The 8bpp minimap framebuffer is now written exclusively with 16-bit halfword writes. Nintendo DS VRAM does not support normal byte writes reliably; the old pixel-by-pixel byte writes caused miniature map graphics and one-pixel vertical lines to disappear. The text BG is also hidden during normal minimap display and only enabled for pause/rules/debug screens.

## v3c: bottom-screen flicker fix

The bottom screen now uses real 8bpp bitmap **double buffering**. VRAM C is split
into two 64 KiB pages (bitmap bases 0 and 4). The UI draws a complete frame into
the hidden page, and `uiPresent()` swaps pages during VBlank.

The old BG0 console layer has been removed so both bitmap pages can use all of
VRAM C. Pause/rules/debug labels are now drawn with a tiny built-in 6x8 bitmap
font into the same off-screen page. This also means menus and minimaps are one
atomic image: no layer conflicts and no visible partial redraws.
