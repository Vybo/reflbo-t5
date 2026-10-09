#!/usr/bin/env python3
"""Render host-side images to PNG (spec §15) with the renderers built in build-host.
  cmake -S test/host -B build-host -G Ninja && cmake --build build-host
  python3 tools/render.py            # captures/render/<name>.{pbm,pgm,png}
  python3 tools/render.py --board t5 # every fixture on the T5: captures/render/t5/<name>.{pgm.gz,png}
"""
import argparse
import pathlib
import subprocess
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import pbm_png  # noqa: E402

# Renderers that take a fixture name; `--list` prints their fixtures (test/host/*_fixtures.h).
LISTED = {"dash": "render_dashboard", "screen": "render_screen"}


def renderers(build_dir, board=None):
    """name -> the command to run, without the output path that comes last. With board "t5", every listed
    fixture as the T5 draws it, and no test pattern."""
    commands = {} if board else {"test_pattern": [str(build_dir / "render_test_pattern")],
                                 "t5_test_pattern": [str(build_dir / "render_test_pattern"), "--board", "t5"]}
    board_args = ["--board", board] if board else []
    for prefix, exe in LISTED.items():
        path = str(build_dir / exe)
        names = subprocess.run([path, "--list"], check=True, capture_output=True, text=True).stdout.split()
        commands.update({f"{prefix}_{name}": [path, *board_args, name] for name in names})
    return commands


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--build-dir", default="build-host")
    parser.add_argument("--out-dir", default="captures/render")
    parser.add_argument("--board", choices=["t5"], help="render the fixtures as this board draws them")
    args = parser.parse_args(argv)
    out_dir = pathlib.Path(args.out_dir) / (args.board or "")
    out_dir.mkdir(parents=True, exist_ok=True)
    for name, command in renderers(pathlib.Path(args.build_dir), args.board).items():
        if args.board:
            image = out_dir / f"{name}.pgm.gz"  # as the T5's goldens are kept (T3a)
        else:
            image = out_dir / f"{name}.{'pgm' if name.startswith('t5_') else 'pbm'}"  # the T5's are 4 bpp (T5 spec §6.5)
        png = out_dir / f"{name}.png"
        subprocess.run([*command, str(image)], check=True)
        png.write_bytes(pbm_png.png_from_image(image.read_bytes()))
        print(f"{name}: {image} and {png}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
