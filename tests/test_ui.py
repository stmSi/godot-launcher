import argparse
import json
import pathlib
import shutil
import subprocess
import tempfile
import time

parser = argparse.ArgumentParser(
    description="Linux X11 UI smoke test; requires xdotool, ImageMagick, and a fetched catalog."
)
parser.add_argument("--catalog", type=pathlib.Path, required=True)
args = parser.parse_args()
repo = pathlib.Path(__file__).resolve().parents[1]
root = pathlib.Path(tempfile.mkdtemp(prefix="arcflow-launcher-ui-"))
marker = root / "launch-args.txt"
project = root / "project with spaces" / "project.godot"
project.parent.mkdir()
project.write_text('[application]\nconfig/name="UI Test Project"\n')
installs = []
for tag in ["4.7.1-stable", "4.6.3-stable"]:
    id = tag + "-standard"
    engine = root / "engines" / id / "Godot"
    engine.parent.mkdir(parents=True)
    engine.write_text(
        "#!/bin/sh\nprintf '%s\\n' \"$@\" > " + "'" + str(marker) + "'" + "\n"
    )
    engine.chmod(0o700)
    installs.append({"id": id, "tag": tag, "executable": "Godot", "mono": False})
state = {
    "preferred": installs[0]["id"],
    "installs": installs,
    "projects": [
        {"path": str(project), "name": "UI Test Project", "engine": installs[0]["id"]}
    ],
}
(root / "library.json").write_text(json.dumps(state))
shutil.copy(args.catalog, root / "releases.json")
p = subprocess.Popen(
    [str(repo / "build/debug/godot-launcher"), "--offline", "--data-dir", str(root)],
    stdout=subprocess.PIPE,
    stderr=subprocess.PIPE,
)


def x(*args):
    return subprocess.check_output(["xdotool", *map(str, args)], text=True).strip()


def click(px, py):
    x("mousemove", "--window", window, px, py)
    x("click", 1)
    time.sleep(0.25)


def wait_for(condition):
    end = time.time() + 10
    while time.time() < end:
        if condition():
            return
        if p.poll() is not None:
            raise RuntimeError(p.communicate())
        time.sleep(0.1)
    raise AssertionError("timed out")


try:
    window = None
    for _ in range(100):
        try:
            window = x("search", "--pid", p.pid).splitlines()[-1]
            break
        except subprocess.CalledProcessError:
            time.sleep(0.1)
    assert window
    x("windowmap", window)
    x("windowsize", window, 1240, 840)
    time.sleep(0.8)
    click(1106, 432)
    wait_for(lambda: marker.exists())
    assert marker.read_text().splitlines() == ["--project-manager"]
    marker.unlink()
    click(98, 218)
    click(1123, 287)
    wait_for(lambda: marker.exists())
    assert marker.read_text().splitlines() == [
        "--editor",
        "--path",
        str(project.parent),
    ]
    marker.unlink()
    click(100, 167)
    click(1108, 480)
    time.sleep(0.2)
    click(492, 469)
    assert len(json.loads((root / "library.json").read_text())["installs"]) == 2
    click(1108, 480)
    click(741, 469)
    wait_for(
        lambda: len(json.loads((root / "library.json").read_text())["installs"]) == 1
    )
    assert project.exists()
    # Exercise search, preview, editions, scroll, resize and settings.
    click(96, 269)
    click(1034, 203)
    click(411, 255)
    click(420, 204)
    x("type", "--clearmodifiers", "4.8")
    time.sleep(0.3)
    x("key", "Escape")
    click(861, 203)  # Clear the query before leaving the release page.
    click(93, 323)
    time.sleep(0.25)
    x("windowsize", window, 1000, 760)
    time.sleep(0.3)
    x("windowsize", window, 1240, 840)
    click(95, 269)
    click(940, 203)
    click(313, 255)
    click(861, 203)
    click(1129, 374)
    wait_for(lambda: (root / "downloads" / "engine.zip.part").exists())
    time.sleep(0.3)
    subprocess.run(
        ["import", "-window", window, str(repo / "build/download-progress.png")],
        check=True,
    )
    click(1143, 763)
    wait_for(lambda: not (root / "downloads" / "engine.zip.part").exists())
    assert len(json.loads((root / "library.json").read_text())["installs"]) == 1
    # A second process must refuse the same library.
    other = subprocess.run(
        [
            str(repo / "build/debug/godot-launcher"),
            "--offline",
            "--data-dir",
            str(root),
            "--list",
        ],
        capture_output=True,
        text=True,
        check=False,
    )
    assert other.returncode == 1 and "another launcher" in other.stderr
    print(
        "UI smoke passed: launch, project arguments, modal cancel/remove, filters, search, resize, download cancellation, library locking"
    )
finally:
    if p.poll() is None:
        p.terminate()
    try:
        p.wait(timeout=35)
    except subprocess.TimeoutExpired:
        p.terminate()
        p.wait()
    out, err = p.communicate()
    print("Exit:", p.returncode, err.decode()[:1500])
    shutil.rmtree(root)
