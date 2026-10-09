"""The UI names its fonts and icons by role and by name, never by a board's asset (T3a): only the profiles
(components/ui/ui_profile_rlcd42.c, ui_profile_t547.c) point at gfx_font_* and gfx_icon_* directly."""
import pathlib
import re
import unittest

UI_DIR = pathlib.Path(__file__).resolve().parents[2] / "components" / "ui"
PROFILES = {"ui_profile_rlcd42.c", "ui_profile_t547.c"}


def offenders(pattern):
    found = []
    for path in sorted(UI_DIR.glob("*.c")):
        if path.name in PROFILES:
            continue
        for number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
            if re.search(pattern, line):
                found.append(f"{path.name}:{number}: {line.strip()}")
    return found


class UiSourcesTest(unittest.TestCase):
    def test_no_font_is_named_outside_the_profiles(self):
        found = offenders(r"\bgfx_font_(sans|num|t5)_")
        self.assertEqual(found, [], f"{len(found)} font references:\n" + "\n".join(found))

    def test_no_icon_is_named_outside_the_profiles(self):
        found = offenders(r"\bgfx_icon_[a-z]")
        self.assertEqual(found, [], f"{len(found)} icon references:\n" + "\n".join(found))


if __name__ == "__main__":
    unittest.main()
