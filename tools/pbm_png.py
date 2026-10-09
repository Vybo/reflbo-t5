#!/usr/bin/env python3
"""Convert binary PBM (P4) and 8-bit PGM (P5) images to PNG using only the standard library.
  python3 tools/pbm_png.py IN.pbm|IN.pgm OUT.png
"""
import gzip
import struct
import sys
import zlib


def _header(data, magic, count):
    """The first `count` whitespace-separated header fields after skipping comments, and the raster's start."""
    fields = []
    pos = 0
    while len(fields) < count:
        while pos < len(data) and data[pos:pos + 1].isspace():
            pos += 1
        if data[pos:pos + 1] == b"#":
            while pos < len(data) and data[pos:pos + 1] not in (b"\n", b"\r"):
                pos += 1
            continue
        start = pos
        while pos < len(data) and not data[pos:pos + 1].isspace():
            pos += 1
        if start == pos:
            raise ValueError("truncated header")
        fields.append(data[start:pos])
    if fields[0] != magic:
        raise ValueError(f"not a {magic.decode()} image")
    return fields, pos + 1  # the single whitespace after the last field


def parse_pbm(data):
    """Returns (width, height, raster) for a P4 image; the raster is row-major, MSB first, 1 = black."""
    fields, pos = _header(data, b"P4", 3)
    width, height = int(fields[1]), int(fields[2])
    size = (width + 7) // 8 * height
    raster = data[pos:pos + size]
    if len(raster) != size:
        raise ValueError("truncated PBM raster")
    return width, height, raster


def parse_pgm(data):
    """Returns (width, height, raster) for an 8-bit P5 image; a byte a pixel, 0 = black."""
    fields, pos = _header(data, b"P5", 4)
    width, height, maxval = int(fields[1]), int(fields[2]), int(fields[3])
    if maxval > 255:
        raise ValueError("only 8-bit PGMs")
    raster = data[pos:pos + width * height]
    if len(raster) != width * height:
        raise ValueError("truncated PGM raster")
    return width, height, raster


def _png(width, height, depth, raw):
    def chunk(tag, body):
        return struct.pack(">I", len(body)) + tag + body + struct.pack(">I", zlib.crc32(tag + body) & 0xFFFFFFFF)

    header = struct.pack(">IIBBBBB", width, height, depth, 0, 0, 0, 0)
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", header) + chunk(b"IDAT", zlib.compress(raw, 9))
            + chunk(b"IEND", b""))


def png_from_pbm(data):
    width, height, raster = parse_pbm(data)
    row_bytes = (width + 7) // 8
    raw = bytearray()
    for y in range(height):
        raw.append(0)  # filter: none
        raw += bytes(b ^ 0xFF for b in raster[y * row_bytes:(y + 1) * row_bytes])  # PNG grey: 0 = black
    return _png(width, height, 1, bytes(raw))


def png_from_pgm(data):
    width, height, raster = parse_pgm(data)
    raw = bytearray()
    for y in range(height):
        raw.append(0)  # filter: none
        raw += raster[y * width:(y + 1) * width]
    return _png(width, height, 8, bytes(raw))


def png_from_image(data):
    """PNG from a P4 PBM or a P5 PGM, either one gzip-compressed or not (the T5's goldens)."""
    if data[:2] == b"\x1f\x8b":
        data = gzip.decompress(data)
    return png_from_pgm(data) if data[:2] == b"P5" else png_from_pbm(data)


def main(argv=None):
    argv = sys.argv[1:] if argv is None else argv
    if len(argv) != 2:
        print(__doc__.strip(), file=sys.stderr)
        return 2
    with open(argv[0], "rb") as src, open(argv[1], "wb") as dst:
        dst.write(png_from_image(src.read()))
    return 0


if __name__ == "__main__":
    sys.exit(main())
