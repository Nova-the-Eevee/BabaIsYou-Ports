#!/usr/bin/env python3
from pathlib import Path
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "assets_src"
OUT_C = ROOT / "source" / "assets.c"
OUT_H = ROOT / "source" / "assets.h"

FILES = [
    "babaright_0.png", "babaright_1.png", "babaright_2.png",
    "babaleft_0.png", "babaleft_1.png", "babaleft_2.png",
    "babaup_0.png", "babaup_1.png", "babaup_2.png",
    "babadown_0.png", "babadown_1.png", "babadown_2.png",
    "wallobj.png", "rock.png", "flag.png", "tile.png",
    "text_wall.png", "text_is.png", "text_stop.png", "text_rock.png",
    "text_flag.png", "text_push.png", "text_win.png", "text_baba.png", "text_you.png",
]

SPRITE_W = 32
SPRITE_H = 32


def sym(name: str) -> str:
    return name.rsplit('.', 1)[0].replace('-', '_')


def rgb15(r, g, b):
    # DS palette entry (BGR555; bit 15 isn't needed for palettes).
    return ((r >> 3) & 31) | (((g >> 3) & 31) << 5) | (((b >> 3) & 31) << 10)

# The original art only uses a tiny palette. Build one shared OBJ palette so
# every sprite can use 4bpp graphics and palette bank 0.
colors = []
seen = set()
for filename in FILES:
    img = Image.open(SRC / filename).convert("RGBA")
    for r, g, b, a in img.getdata():
        if a < 128:
            continue
        c = (r, g, b)
        if c not in seen:
            seen.add(c)
            colors.append(c)

if len(colors) > 15:
    raise SystemExit(f"Need {len(colors)} opaque colors; 4bpp allows 15 + transparency")

palette = [(0, 0, 0)] + colors
lookup = {c: i + 1 for i, c in enumerate(colors)}
while len(palette) < 16:
    palette.append((0, 0, 0))


def make_canvas(filename):
    src = Image.open(SRC / filename).convert("RGBA")
    if src.width > SPRITE_W or src.height > SPRITE_H:
        raise SystemExit(f"{filename} is larger than {SPRITE_W}x{SPRITE_H}")

    # Keep the source image anchored at the top-left of the hardware sprite.
    # The original PSP project treats each image's (0, 0) as the cell origin,
    # so 24x24 assets should occupy pixels 0..23 rather than being centered.
    canvas = Image.new("RGBA", (SPRITE_W, SPRITE_H), (0, 0, 0, 0))
    canvas.alpha_composite(src, (0, 0))
    return canvas


def encode_4bpp_tiled(img):
    indices = []
    for r, g, b, a in img.getdata():
        indices.append(0 if a < 128 else lookup[(r, g, b)])

    out = bytearray()
    # DS/GBA 4bpp OBJ tiles: 8x8 tiles, row-major tile order, two pixels/byte.
    for tile_y in range(0, SPRITE_H, 8):
        for tile_x in range(0, SPRITE_W, 8):
            for py in range(8):
                y = tile_y + py
                for px in range(0, 8, 2):
                    x0 = tile_x + px
                    p0 = indices[y * SPRITE_W + x0]
                    p1 = indices[y * SPRITE_W + x0 + 1]
                    out.append(p0 | (p1 << 4))
    assert len(out) == 512
    return bytes(out)

header = '''#pragma once\n#include <stdint.h>\n\n#define SPRITE_ASSET_COUNT %d\n\ntypedef struct {\n    const uint8_t *tiles;\n    const uint8_t *mini;\n} ImageAsset;\n\nextern const uint16_t sprite_palette[16];\n\n''' % len(FILES)
source = '#include "assets.h"\n\n'
source += 'const uint16_t sprite_palette[16] = {\n    '
source += ', '.join(f'0x{rgb15(*c):04X}' for c in palette)
source += '\n};\n\n'

names = []
for filename in FILES:
    name = sym(filename)
    names.append(name)
    data = encode_4bpp_tiled(make_canvas(filename))

    # 10x10 raw palette-index preview for the bottom-screen minimap. Unlike
    # the 32x32 hardware OBJ canvas, this scales the original source graphic
    # itself so Baba/words/walls stay recognizable at tiny map size.
    mini_img = Image.open(SRC / filename).convert("RGBA").resize((10, 10), Image.Resampling.NEAREST)
    mini = []
    for r, g, b, a in mini_img.getdata():
        mini.append(0 if a < 128 else lookup[(r, g, b)])

    header += f'extern const uint8_t {name}_tiles[512];\n'
    header += f'extern const uint8_t {name}_mini[100];\n'
    header += f'extern const ImageAsset {name};\n'
    source += f'const uint8_t {name}_tiles[512] = {{\n'
    for i in range(0, len(data), 16):
        source += '    ' + ', '.join(f'0x{v:02X}' for v in data[i:i+16]) + ',\n'
    source += '};\n'
    source += f'const uint8_t {name}_mini[100] = {{\n'
    for i in range(0, len(mini), 16):
        source += '    ' + ', '.join(f'0x{v:02X}' for v in mini[i:i+16]) + ',\n'
    source += '};\n'
    source += f'const ImageAsset {name} = {{ {name}_tiles, {name}_mini }};\n\n'

header += '\nextern const ImageAsset *const sprite_assets[SPRITE_ASSET_COUNT];\n'
source += 'const ImageAsset *const sprite_assets[SPRITE_ASSET_COUNT] = {\n'
for name in names:
    source += f'    &{name},\n'
source += '};\n'

OUT_H.write_text(header)
OUT_C.write_text(source)
print(f"Palette: {len(colors)} opaque colors + transparency")
print(f"Wrote {OUT_H}")
print(f"Wrote {OUT_C}")
