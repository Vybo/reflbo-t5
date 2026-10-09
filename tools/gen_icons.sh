#!/usr/bin/env bash
# Regenerate components/gfx/icons from the icon font in assets/icons (spec §4.5). Needs uv; the
# pinned Pillow comes from tools/requirements.txt.
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/.."
mkdir -p components/gfx/icons

uv run --quiet --python 3.13 --with-requirements tools/requirements.txt tools/imggen.py \
    --ttf assets/icons/MaterialIcons-Regular.ttf --codepoints assets/icons/MaterialIcons-Regular.codepoints \
    --manifest assets/icons/icons.txt --licence assets/icons/LICENSE-MaterialIcons.txt \
    --font wi=assets/icons/WeatherIcons-Regular.ttf,assets/icons/WeatherIcons-Regular.codepoints,assets/icons/LICENSE-WeatherIcons.txt

# The T5's 4-bit icons (T5 spec §6.3).
uv run --quiet --python 3.13 --with-requirements tools/requirements.txt tools/imggen.py --bpp 4 \
    --ttf assets/icons/MaterialIcons-Regular.ttf --codepoints assets/icons/MaterialIcons-Regular.codepoints \
    --manifest assets/icons/icons_t5.txt --licence assets/icons/LICENSE-MaterialIcons.txt \
    --font wi=assets/icons/WeatherIcons-Regular.ttf,assets/icons/WeatherIcons-Regular.codepoints,assets/icons/LICENSE-WeatherIcons.txt \
    --out-c components/gfx/icons/gfx_icons_t5.c --out-h components/gfx/include/gfx_icons_t5.h
