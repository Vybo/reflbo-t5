#!/usr/bin/env python3
"""Render icons from an icon font into 1-bpp or 4-bit anti-aliased C bitmaps for components/gfx (spec §4.5, T5 spec §6.3).

Run it through tools/gen_icons.sh, which supplies the pinned Pillow via uv:
  uv run --python 3.13 --with-requirements tools/requirements.txt tools/imggen.py \\
      --ttf assets/icons/MaterialIcons-Regular.ttf --codepoints assets/icons/MaterialIcons-Regular.codepoints \\
      --manifest assets/icons/icons.txt --licence assets/icons/LICENSE-MaterialIcons.txt
Writes components/gfx/icons/gfx_icons.c and components/gfx/include/gfx_icons.h. Each icon is a
square bitmap of its size (the font's em box), so icons of one size line up. Glyphs are rendered
with FreeType's monochrome hinting, like tools/fontgen.py.

More fonts come with --font PREFIX=TTF,CODEPOINTS,LICENCE; a manifest source "PREFIX:name" is drawn
from that font, its ink fitted to the square inside Material's 2/24 padding, as such fonts don't fill
their em box the way Material Icons does.
"""
import argparse
import pathlib
import sys


def parse_manifest(text):
    """'c_name source_name size...' lines -> [(c_name, source_name, [sizes])]; '#' starts a comment."""
    icons = []
    for number, line in enumerate(text.splitlines(), 1):
        line = line.split("#", 1)[0].strip()
        if not line:
            continue
        parts = line.split()
        if len(parts) < 3 or not parts[0].isidentifier():
            raise ValueError(f"manifest line {number}: expected 'c_name source_name size...': {line!r}")
        icons.append((parts[0], parts[1], [int(s) for s in parts[2:]]))
    return icons


def parse_codepoints(text):
    """Material Icons' 'name hex' lines -> {name: codepoint}."""
    return {name: int(hexcp, 16) for name, hexcp in (line.split() for line in text.splitlines() if line.strip())}


def pack_rows(rows, bpp=1):
    """Rows of pixels -> bytes, each row padded to whole bytes. 1 bpp: 0/1, MSB first. 4 bpp: coverage
    0-15, two pixels a byte, the first in the high nibble."""
    out = bytearray()
    for row in rows:
        if bpp == 4:
            for start in range(0, len(row), 2):
                pair = list(row[start:start + 2]) + [0]
                out.append((pair[0] & 0x0F) << 4 | (pair[1] & 0x0F))
            continue
        for start in range(0, len(row), 8):
            byte = 0
            for i, bit in enumerate(row[start:start + 8]):
                if bit:
                    byte |= 0x80 >> i
            out.append(byte)
    return bytes(out)


def split_source(source):
    """'wi:day-sunny' -> ('wi', 'day-sunny'); 'bolt' -> (None, 'bolt')."""
    prefix, sep, name = source.partition(":")
    return (prefix, name) if sep else (None, source)


def parse_font_spec(spec):
    """'wi=a.ttf,a.codepoints,LICENCE.txt' -> ('wi', 'a.ttf', 'a.codepoints', 'LICENCE.txt')."""
    prefix, sep, rest = spec.partition("=")
    parts = rest.split(",")
    if not sep or not prefix.isidentifier() or len(parts) != 3 or not all(parts):
        raise ValueError(f"--font wants PREFIX=TTF,CODEPOINTS,LICENCE: {spec!r}")
    return (prefix, *parts)


def fit_pad(size):
    """Material Icons leave 2/24 of the em box free on each side; fitted glyphs keep the same margin."""
    return max(1, round(size * 2 / 24))


def fit_font_size(ink_w, ink_h, measured_size, size):
    """The font size that makes ink of ink_w x ink_h (measured at measured_size) fit size minus padding."""
    room = size - 2 * fit_pad(size)
    return max(1, int(measured_size * room / max(ink_w, ink_h)))


def render_fitted(ttf, codepoint, size, bpp=1):
    """size x size rows: the glyph's ink scaled to the padded square and centred; 0/1 at 1 bpp, coverage
    0-15 at 4."""
    from PIL import Image, ImageDraw, ImageFont

    def ink(font_size, mode):
        font = ImageFont.truetype(ttf, font_size, layout_engine=ImageFont.Layout.BASIC)
        canvas = Image.new(mode, (font_size * 3, font_size * 3), 0)
        draw = ImageDraw.Draw(canvas)
        draw.fontmode = mode
        draw.text((font_size, font_size), chr(codepoint), font=font, fill=1 if mode == "1" else 255)
        box = canvas.getbbox()
        if box is None:
            raise SystemExit(f"U+{codepoint:04X}: no ink in {ttf}")
        return canvas.crop(box)

    probe = ink(size * 4, "1")
    glyph = ink(fit_font_size(probe.width, probe.height, size * 4, size), "1" if bpp == 1 else "L")
    while max(glyph.width, glyph.height) > size - 2 * fit_pad(size):  # hinting may round up a pixel
        glyph = glyph.resize((max(1, glyph.width - 1), max(1, glyph.height - 1)))
    img = Image.new(glyph.mode, (size, size), 0)
    img.paste(glyph, ((size - glyph.width) // 2, (size - glyph.height) // 2))
    return _rows(img, size, bpp)


def _rows(img, size, bpp):
    """The image's pixels: 0/1 at 1 bpp, coverage 0-15 at 4."""
    px = img.load()
    if bpp == 1:
        return [[1 if px[x, y] else 0 for x in range(size)] for y in range(size)]
    return [[(px[x, y] * 15 + 127) // 255 for x in range(size)] for y in range(size)]


def render_icon(font, codepoint, size, bpp=1):
    """size x size rows, the glyph drawn at the em box's top-left corner; 0/1 at 1 bpp, coverage 0-15 at 4."""
    from PIL import Image, ImageDraw

    mode = "1" if bpp == 1 else "L"
    img = Image.new(mode, (size, size), 0)
    draw = ImageDraw.Draw(img)
    draw.fontmode = mode
    draw.text((0, 0), chr(codepoint), font=font, fill=1 if bpp == 1 else 255)
    return _rows(img, size, bpp)


def emit_c(icons, source, licence=None, bpp=1, header="gfx_icons.h"):
    """icons: [(symbol, size, rows)] -> the .c file text."""
    lines = [
        f"/* Generated by tools/imggen.py from {source}. Do not edit; run tools/gen_icons.sh. */",
        *([f"/* Icon licence: {licence} (it also covers these bitmaps). */"] if licence else []),
        f'#include "{header}"',
        "",
    ]
    for symbol, size, rows in icons:
        data = pack_rows(rows, bpp)
        lines.append(f"static const uint8_t s_{symbol}[] = {{")
        for i in range(0, len(data), 16):
            lines.append("    " + ", ".join(f"0x{b:02X}" for b in data[i:i + 16]) + ",")
        lines += ["};", f"const gfx_bitmap_t gfx_icon_{symbol} = {{ s_{symbol}, {size}, {size}, {bpp} }};", ""]
    return "\n".join(lines)


def emit_h(icons, source):
    lines = [
        "#pragma once",
        "",
        '#include "gfx.h"',
        "",
        f"/* Icons generated by tools/imggen.py from {source} (see THIRD_PARTY.md). */",
    ]
    lines += [f"extern const gfx_bitmap_t gfx_icon_{symbol};" for symbol, _size, _rows in icons]
    return "\n".join(lines) + "\n"


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--ttf", required=True, help="icon font")
    parser.add_argument("--codepoints", required=True, help="the font's 'name hex' codepoint list")
    parser.add_argument("--manifest", required=True, help="icons to render: 'c_name source_name size...'")
    parser.add_argument("--licence", help="licence file of the icons, named in the generated source")
    parser.add_argument("--font", action="append", default=[], metavar="PREFIX=TTF,CODEPOINTS,LICENCE",
                        help="another icon font for manifest sources 'PREFIX:name'")
    parser.add_argument("--bpp", type=int, choices=(1, 4), default=1, help="1: monochrome; 4: anti-aliased coverage")
    parser.add_argument("--out-c", default="components/gfx/icons/gfx_icons.c")
    parser.add_argument("--out-h", default="components/gfx/include/gfx_icons.h")
    args = parser.parse_args(argv)

    from PIL import ImageFont

    codepoints = parse_codepoints(pathlib.Path(args.codepoints).read_text())
    extra = {}
    for spec in args.font:
        prefix, ttf, cps, licence = parse_font_spec(spec)
        extra[prefix] = (ttf, parse_codepoints(pathlib.Path(cps).read_text()), licence)
    rendered = []
    for c_name, source_name, sizes in parse_manifest(pathlib.Path(args.manifest).read_text()):
        prefix, name = split_source(source_name)
        if prefix is not None and prefix not in extra:
            raise SystemExit(f"{source_name}: no --font {prefix}=...")
        table = extra[prefix][1] if prefix is not None else codepoints
        if name not in table:
            raise SystemExit(f"{source_name}: not in its codepoint list")
        for size in sizes:
            if prefix is not None:
                rows = render_fitted(extra[prefix][0], table[name], size, args.bpp)
            else:
                font = ImageFont.truetype(args.ttf, size, layout_engine=ImageFont.Layout.BASIC)
                rows = render_icon(font, table[name], size, args.bpp)
            rendered.append((f"{c_name}_{size}", size, rows))
    source = ", ".join([pathlib.Path(args.ttf).name] + [pathlib.Path(e[0]).name for e in extra.values()])
    licence = "; ".join(x for x in [args.licence] + [e[2] for e in extra.values()] if x)
    pathlib.Path(args.out_c).write_text(emit_c(rendered, source, licence or None, bpp=args.bpp,
                                               header=pathlib.Path(args.out_h).name), encoding="utf-8")
    pathlib.Path(args.out_h).write_text(emit_h(rendered, source), encoding="utf-8")
    print(f"{args.out_c}: {len(rendered)} icons")
    return 0


if __name__ == "__main__":
    sys.exit(main())
