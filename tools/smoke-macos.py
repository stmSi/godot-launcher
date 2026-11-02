#!/usr/bin/env python3
"""Check the installed macOS app's icon, signature, and first rendered frames."""

import argparse
import pathlib
import plistlib
import subprocess
import tempfile


def smoke(app):
    app = app.resolve()
    contents = app / "Contents"
    with (contents / "Info.plist").open("rb") as stream:
        info = plistlib.load(stream)
    icon_name = info.get("CFBundleIconFile")
    if not icon_name:
        raise ValueError("The app has no CFBundleIconFile")
    icon = contents / "Resources" / icon_name
    if icon.suffix != ".icns":
        icon = icon.with_suffix(".icns")
    if icon.read_bytes()[:4] != b"icns":
        raise ValueError(f"Not an ICNS app icon: {icon}")
    subprocess.run(["codesign", "--verify", "--deep", "--strict", str(app)], check=True,
                   timeout=30)
    executable = contents / "MacOS" / info["CFBundleExecutable"]
    with tempfile.TemporaryDirectory(prefix="godot-launcher-smoke-") as library:
        subprocess.run([str(executable), "--offline", "--data-dir", library, "--frames", "4"],
                       check=True, timeout=30)
    print("macOS app icon, signature, and rendered-frame smoke test passed")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("app", type=pathlib.Path)
    smoke(parser.parse_args().app)
