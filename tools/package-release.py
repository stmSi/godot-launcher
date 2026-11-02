#!/usr/bin/env python3
"""Create a downloadable archive from the CMake install directory."""

import argparse
import json
import os
import pathlib
import platform as host_platform
import re
import shutil
import subprocess
import tarfile
import tempfile
import zipfile

ROOT = pathlib.Path(__file__).resolve().parents[1]


def package(platform, arch, source, destination):
    machine = host_platform.machine().lower()
    machine = {"amd64": "x86_64", "aarch64": "arm64"}.get(machine, machine)
    if machine != arch:
        raise ValueError(
            f"Package architecture {arch} does not match build host {machine}"
        )
    name = f"godot-launcher-{platform}-{arch}"
    source = source.resolve()
    executable = {
        "linux": "bin/godot-launcher",
        "windows": "bin/godot-launcher.exe",
        "macos": "godot-launcher.app/Contents/MacOS/godot-launcher",
    }[platform]
    if not (source / executable).is_file():
        raise FileNotFoundError(source / executable)
    version = re.search(
        r"project\(GodotLauncher VERSION ([\d.]+)",
        (ROOT / "CMakeLists.txt").read_text(),
    )[1]
    shutil.copy2(ROOT / "README.md", source / "README.md")
    commit = os.environ.get("GITHUB_SHA")
    if not commit:
        commit = subprocess.check_output(
            ["git", "rev-parse", "HEAD"], cwd=ROOT, text=True
        ).strip()
    (source / "BUILD-INFO.json").write_text(
        json.dumps(
            {
                "version": version,
                "commit": commit,
                "ref": os.environ.get("GITHUB_REF", "local"),
                "platform": platform,
                "architecture": arch,
            },
            indent=2,
        )
        + "\n"
    )
    destination.mkdir(parents=True, exist_ok=True)
    if platform == "linux":
        result = destination / f"{name}.tar.gz"
        with tarfile.open(result, "w:gz") as archive:
            archive.add(source, arcname=name)
    elif platform == "macos":
        result = destination.resolve() / f"{name}.zip"
        with tempfile.TemporaryDirectory(prefix="launcher-package-") as temporary:
            staging = pathlib.Path(temporary) / name
            subprocess.run(["ditto", str(source), str(staging)], check=True)
            subprocess.run(
                [
                    "ditto",
                    "-c",
                    "-k",
                    "--sequesterRsrc",
                    "--keepParent",
                    str(staging),
                    str(result),
                ],
                check=True,
            )
            unpacked = pathlib.Path(temporary) / "unpacked"
            subprocess.run(
                ["ditto", "-x", "-k", str(result), str(unpacked)], check=True
            )
            subprocess.run(
                [
                    "codesign",
                    "--verify",
                    "--deep",
                    "--strict",
                    str(unpacked / name / "godot-launcher.app"),
                ],
                check=True,
            )
    else:
        result = destination / f"{name}.zip"
        with zipfile.ZipFile(result, "w", compression=zipfile.ZIP_DEFLATED) as archive:
            for path in sorted(source.rglob("*")):
                if path.is_file():
                    archive.write(path, pathlib.Path(name) / path.relative_to(source))
    print(result)
    return result


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--platform", choices=["linux", "windows", "macos"], required=True
    )
    parser.add_argument("--arch", choices=["x86_64", "arm64"], required=True)
    parser.add_argument("--source", type=pathlib.Path, default=ROOT / "out")
    parser.add_argument("--destination", type=pathlib.Path, default=ROOT / "dist")
    args = parser.parse_args()
    package(args.platform, args.arch, args.source, args.destination)
