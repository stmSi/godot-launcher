#!/usr/bin/env python3
"""Publish only the packages produced by a successful GitHub Actions build."""

import json
import os
import pathlib
import re
import subprocess

ROOT = pathlib.Path(__file__).resolve().parents[1]


def gh(*args, check=True):
    result = subprocess.run(["gh", *args], check=False, text=True, capture_output=True)
    if check and result.returncode:
        raise RuntimeError(f"GitHub CLI failed: {result.stderr.strip()}")
    return result


def publish():
    ref = os.environ["GITHUB_REF"]
    sha = os.environ["GITHUB_SHA"]
    repo = os.environ["GH_REPO"]
    nightly = ref == "refs/heads/main"
    if not nightly and not re.fullmatch(
        r"refs/tags/v\d+\.\d+\.\d+(?:-[A-Za-z0-9.-]+)?", ref
    ):
        raise ValueError(f"Not a release ref: {ref}")
    tag = "nightly" if nightly else ref.removeprefix("refs/tags/")
    if nightly:
        existing = gh("api", f"repos/{repo}/git/ref/tags/nightly", check=False)
        if existing.returncode == 0:
            gh(
                "api",
                "--method",
                "PATCH",
                f"repos/{repo}/git/refs/tags/nightly",
                "-f",
                f"sha={sha}",
                "-F",
                "force=true",
            )
        else:
            gh(
                "api",
                "--method",
                "POST",
                f"repos/{repo}/git/refs",
                "-f",
                "ref=refs/tags/nightly",
                "-f",
                f"sha={sha}",
            )
    title = "Development build" if nightly else f"Godot Launcher {tag}"
    notes = ROOT / "dist/RELEASE-NOTES.md"
    notes.write_text(
        f"{'Latest successful main build.' if nightly else 'Versioned release.'}\n\n"
        f"Built and tested by [GitHub Actions](https://github.com/{repo}/actions/runs/{os.environ['GITHUB_RUN_ID']}).\n\n"
        f"Source: [`{sha[:12]}`](https://github.com/{repo}/commit/{sha}).\n\n"
        "Download the archive for Linux x86_64, Windows x86_64, macOS Apple Silicon (arm64), or macOS Intel (x86_64). "
        "Extract the archive before launching. Windows runtime DLLs and macOS dylibs are included. "
        "Linux requires Vulkan 1.3 and the runtime libraries listed in README.md.\n\n"
        "Verify downloads using `SHA256SUMS.txt`. macOS bundles are ad-hoc signed, not notarized.\n"
    )
    exists = gh("release", "view", tag, "--json", "isDraft", check=False)
    prerelease = nightly or "-" in tag
    if exists.returncode:
        gh(
            "release",
            "create",
            tag,
            "--verify-tag",
            "--draft",
            "--title",
            title,
            "--notes-file",
            str(notes),
        )
    assets = sorted(
        path
        for path in (ROOT / "dist").iterdir()
        if path.suffix in {".zip", ".gz"} or path.name == "SHA256SUMS.txt"
    )
    if len(assets) != 5:
        raise RuntimeError("Expected four platform archives and SHA256SUMS.txt")
    gh("release", "upload", tag, *map(str, assets), "--clobber")
    gh(
        "release",
        "edit",
        tag,
        "--draft=false",
        f"--prerelease={str(prerelease).lower()}",
        f"--latest={str(not prerelease).lower()}",
        "--title",
        title,
        "--notes-file",
        str(notes),
    )
    url = json.loads(gh("release", "view", tag, "--json", "url").stdout)["url"]
    print(url)
    if "GITHUB_STEP_SUMMARY" in os.environ:
        with open(os.environ["GITHUB_STEP_SUMMARY"], "a") as summary:
            summary.write(
                f"Published [Godot Launcher {tag}]({url}) with four platform packages and checksums.\n"
            )


if __name__ == "__main__":
    publish()
