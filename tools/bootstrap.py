#!/usr/bin/env python3
"""Fetch the pinned Arcflow framework and its source dependencies."""

import pathlib
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
DEPS = ROOT / "deps"
SOURCES = [
    (
        "af-framework",
        "https://github.com/stmSi/af-framework.git",
        "7ccb2eaf313970d2e39f635158270af644fe6d80",
    ),
    ("third_party/SDL", "https://github.com/libsdl-org/SDL.git", "release-3.4.12"),
    (
        "third_party/Vulkan-Headers",
        "https://github.com/KhronosGroup/Vulkan-Headers.git",
        "v1.3.296",
    ),
    ("third_party/volk", "https://github.com/zeux/volk.git", "vulkan-sdk-1.3.296.0"),
]

for name, url, revision in SOURCES:
    if sys.platform == "darwin" and name in (
        "third_party/Vulkan-Headers",
        "third_party/volk",
    ):
        continue
    destination = DEPS / name
    if (destination / ".git").exists():
        print(f"Keeping existing checkout: {destination}")
        continue
    destination.mkdir(parents=True, exist_ok=True)
    subprocess.run(["git", "init", str(destination)], check=True)
    subprocess.run(
        ["git", "-C", str(destination), "remote", "add", "origin", url], check=True
    )
    subprocess.run(
        ["git", "-C", str(destination), "fetch", "--depth", "1", "origin", revision],
        check=True,
    )
    subprocess.run(
        ["git", "-C", str(destination), "checkout", "--detach", "FETCH_HEAD"],
        check=True,
    )
print(
    "Sources ready. Install the system libraries listed in README.md, then configure with CMake."
)
