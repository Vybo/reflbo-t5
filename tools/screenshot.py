#!/usr/bin/env python3
"""Grab the board's framebuffer over USB as a PNG (spec §15): a PBM from 1 bpp, a PGM from the T5's 4 bpp
(T5 spec §6.5).
  tools/idf.sh exec python tools/screenshot.py -p /dev/cu.usbmodemXXXX -o captures/screen.png \
      [--compare test/host/golden/test_pattern.pbm]
Writes OUT.png and the raw OUT.pbm or OUT.pgm. A T5 screenshot is about 690 KB of base64 at 115200 baud: give
it -t 120. Exit codes: as devlog.py, plus 5 when the image differs from
--compare and 6 when no valid image arrived.
"""
import argparse
import base64
import binascii
import io
import pathlib
import sys
from types import SimpleNamespace

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import devlog  # noqa: E402
import pbm_png  # noqa: E402

BEGIN = "-----BEGIN RLCD PBM-----"
END = "-----END RLCD PBM-----"
BEGIN_PGM = "-----BEGIN RLCD PGM-----"
END_PGM = "-----END RLCD PGM-----"


def _between(lines, begin, end):
    start = next(i for i, line in enumerate(lines) if line.endswith(begin))
    stop = next(i for i in range(start + 1, len(lines)) if lines[i] == end)
    return lines[start + 1:stop]


def extract_image(text):
    """Returns ("pbm" or "pgm", data) for the image carried between markers; raises ValueError."""
    lines = [line.strip() for line in text.splitlines()]
    for kind, begin, end in (("pbm", BEGIN, END), ("pgm", BEGIN_PGM, END_PGM)):
        try:
            body = _between(lines, begin, end)
        except StopIteration:
            continue
        try:
            data = base64.b64decode("".join(body), validate=True)
        except binascii.Error as err:
            raise ValueError(f"corrupt screenshot data: {err}") from None
        if kind == "pbm":
            width, height, raster = pbm_png.parse_pbm(data)
            header = len(f"P4\n{width} {height}\n")
        else:
            width, height, raster = pbm_png.parse_pgm(data)
            header = len(f"P5\n{width} {height}\n255\n")
        if len(data) != header + len(raster):
            raise ValueError("screenshot data has the wrong length")  # e.g. base64-looking noise got in
        return kind, data
    raise ValueError("no complete screenshot in the console output")


def extract_pbm(text):
    """The PBM carried between the markers (1 bpp boards); raises ValueError."""
    kind, data = extract_image(text)
    if kind != "pbm":
        raise ValueError("the screenshot is a PGM")
    return data


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("-p", "--port", help=f"serial port (default: the only {devlog.PORT_GLOB})")
    parser.add_argument("-o", "--out", default="captures/screen.png", help="PNG to write (the .pbm or .pgm lands next to it)")
    parser.add_argument("-t", "--seconds", type=float, default=15.0, help="give up after this many seconds")
    parser.add_argument("--compare", help="PBM or PGM the screenshot must equal byte for byte")
    args = parser.parse_args(argv)

    transcript = io.StringIO()
    session = SimpleNamespace(port=args.port, seconds=args.seconds, until=None, cmd=["screenshot"], reset=False,
                              out=None)
    try:
        code = devlog.run(session, out=transcript)
    except devlog.PortError as err:
        print(f"screenshot: {err}", file=sys.stderr)
        return 2
    if code != 0:
        return code
    try:
        kind, image = extract_image(transcript.getvalue())
    except ValueError as err:
        print(f"screenshot: {err}", file=sys.stderr)
        return 6

    out = pathlib.Path(args.out)
    out.parent.mkdir(parents=True, exist_ok=True)
    out.with_suffix("." + kind).write_bytes(image)
    out.write_bytes(pbm_png.png_from_image(image))
    print(f"screenshot: {out}")
    if args.compare:
        if pathlib.Path(args.compare).read_bytes() != image:
            print(f"screenshot: differs from {args.compare}", file=sys.stderr)
            return 5
        print(f"screenshot: identical to {args.compare}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
