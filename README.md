# Godot Launcher

Manage official Godot engines and projects on Linux, Windows, and macOS.
Install Standard or .NET builds, switch versions, and open your projects.

[Download](https://github.com/stmSi/godot-launcher/releases/tag/nightly) ·
[Releases](https://github.com/stmSi/godot-launcher/releases)

## Build

Install the [platform dependencies](.github/workflows/build.yml), then run:

```sh
python3 tools/bootstrap.py
cmake --preset release
cmake --build --preset release --parallel 3
ctest --test-dir build/release --output-on-failure
cmake --install build/release --prefix out
```

Built with [AF Framework](https://github.com/stmSi/af-framework).
Attribution: [Godot icons](assets/README.md) · [Open Sans](assets/fonts/OFL.txt).
