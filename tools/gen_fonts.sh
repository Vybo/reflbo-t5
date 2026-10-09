#!/usr/bin/env bash
# Regenerate components/gfx/fonts from the TTFs in assets/fonts (spec §4.4). Needs uv; the
# pinned Pillow and fontTools come from tools/requirements.txt.
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/.."

fontgen() {
    uv run --quiet --python 3.13 --with-requirements tools/requirements.txt tools/fontgen.py "$@"
}

fontgen --ttf assets/fonts/DejaVuSans.ttf --size 12 --charset text --name sans_12 --licence assets/fonts/LICENSE-DejaVu.txt
fontgen --ttf assets/fonts/DejaVuSans.ttf --size 16 --charset text --name sans_16 --licence assets/fonts/LICENSE-DejaVu.txt
fontgen --ttf assets/fonts/DejaVuSans.ttf --size 20 --charset text --name sans_20 --licence assets/fonts/LICENSE-DejaVu.txt
fontgen --ttf assets/fonts/DejaVuSans-Bold.ttf --size 20 --charset text --name sans_bold_20 --licence assets/fonts/LICENSE-DejaVu.txt
fontgen --ttf assets/fonts/DejaVuSans-Bold.ttf --size 28 --charset text --name sans_bold_28 --licence assets/fonts/LICENSE-DejaVu.txt
fontgen --ttf assets/fonts/DejaVuSans-Bold.ttf --size 16 --charset text --name sans_bold_16 --licence assets/fonts/LICENSE-DejaVu.txt
fontgen --ttf assets/fonts/DejaVuSansCondensed-Bold.ttf --size 48 --charset digits --name num_cb_48 --licence assets/fonts/LICENSE-DejaVu.txt
fontgen --ttf assets/fonts/DejaVuSansCondensed-Bold.ttf --size 72 --charset digits --name num_cb_72 --licence assets/fonts/LICENSE-DejaVu.txt
fontgen --ttf assets/fonts/DejaVuSansCondensed-Bold.ttf --size 110 --charset digits --name num_cb_110 --licence assets/fonts/LICENSE-DejaVu.txt
fontgen --ttf assets/fonts/DejaVuSansCondensed-Bold.ttf --size 130 --charset digits --name num_cb_130 --licence assets/fonts/LICENSE-DejaVu.txt

# The T5's 4-bit fonts (T5 spec §6.3, §7.2); T2 brings the test pattern's, T3 the rest.
fontgen --ttf assets/fonts/DejaVuSans.ttf --size 26 --charset text --name t5_sans_26 --bpp 4 --licence assets/fonts/LICENSE-DejaVu.txt
fontgen --ttf assets/fonts/DejaVuSans.ttf --size 20 --charset text --name t5_sans_20 --bpp 4 --licence assets/fonts/LICENSE-DejaVu.txt
fontgen --ttf assets/fonts/DejaVuSans.ttf --size 34 --charset text --name t5_sans_34 --bpp 4 --licence assets/fonts/LICENSE-DejaVu.txt
fontgen --ttf assets/fonts/DejaVuSans-Bold.ttf --size 26 --charset text --name t5_bold_26 --bpp 4 --licence assets/fonts/LICENSE-DejaVu.txt
fontgen --ttf assets/fonts/DejaVuSans-Bold.ttf --size 34 --charset text --name t5_bold_34 --bpp 4 --licence assets/fonts/LICENSE-DejaVu.txt
fontgen --ttf assets/fonts/DejaVuSans-Bold.ttf --size 46 --charset text --name t5_bold_46 --bpp 4 --licence assets/fonts/LICENSE-DejaVu.txt
fontgen --ttf assets/fonts/DejaVuSansCondensed-Bold.ttf --size 80 --charset digits --name t5_num_80 --bpp 4 --licence assets/fonts/LICENSE-DejaVu.txt
fontgen --ttf assets/fonts/DejaVuSansCondensed-Bold.ttf --size 120 --charset digits --name t5_num_120 --bpp 4 --licence assets/fonts/LICENSE-DejaVu.txt
fontgen --ttf assets/fonts/DejaVuSansCondensed-Bold.ttf --size 180 --charset digits --name t5_num_180 --bpp 4 --licence assets/fonts/LICENSE-DejaVu.txt
fontgen --ttf assets/fonts/DejaVuSansCondensed-Bold.ttf --size 220 --charset digits --name t5_num_220 --bpp 4 --licence assets/fonts/LICENSE-DejaVu.txt
