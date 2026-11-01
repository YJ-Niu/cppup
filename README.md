# cppup

**One-command C++ project tool** — create, build and run modern C++ projects with almost zero configuration.

Inspired by `cargo`, `npm` and `go`, but stays out of the way: it just scaffolds a clean CMake + Ninja project and gives you a few convenient commands.

## Features (v0.1)

- `cppup init MyApp` — create a ready-to-build C++23 project
- `cppup build` — configure + build with **CMake + Ninja + Clang**
- `cppup bundle` — package the app with non-system runtime dependencies
- `cppup run` — run the executable
- `cppup go` — build **and** run in one step
- `cppup clean` — remove build output and generated module PCM files
- `cppup doctor` — check that Clang, CMake and Ninja are installed
- Templates are **embedded** in the binary (no external template files needed)

## Quick start

First build (detects macOS / Linux / Windows, then Homebrew or the system toolchain):

```bash
# macOS and Linux
./bootstrap.sh

# Windows (PowerShell)
.\bootstrap.ps1
```

That script:

1. Reads the OS (`uname` / Windows).
2. On macOS looks up Homebrew at `/opt/homebrew` (Apple Silicon) or `/usr/local` (Intel). On Linux it also accepts Linuxbrew. On Windows it searches PATH, `Program Files`, scoop, winget, and chocolatey.
3. Configures this repo with the cmake and ninja it found.

After that, cppup builds itself:

```bash
./build/cppup doctor
./build/cppup build --release     # Windows: .\build\cppup.exe build --release
```

Use it on other projects:

```bash
cppup init Hello
cd Hello
cppup go
```

## Project layout created by `init`

```
MyApp/
├── CMakeLists.txt
├── .clangd
├── .vscode/
│   └── launch.json
├── modules/
│   └── .gitkeep
├── src/
│   └── main.cpp
├── .gitignore
└── README.md
```

## Architecture

- `cppup_core` contains project operations, toolchain detection, templates, and utilities.
- `cppup` is the CLI frontend and links against `cppup_core`.
- A future GUI should be a separate executable linking the same core library; GUI dependencies should stay out of `cppup_core`.

For a full cross-platform desktop app, **Qt 6 Widgets** is the recommended starting point: mature controls, strong CMake integration, and broad platform support. Keep it as an optional GUI target because Qt has a larger dependency footprint and LGPL/commercial licensing considerations. **Dear ImGui** is a better fit for an internal diagnostics panel or developer tool than a conventional end-user desktop app.

## Templates

| Name     | Description                          |
|----------|--------------------------------------|
| `basic`  | Classic C++23 executable (default)   |
| `modules`| C++23 modules (`import std;`, `app.math`, `app.service`) |

```bash
cppup init Foo -t modules
```

> Note: the `modules` template requires CMake ≥ 3.30, Ninja ≥ 1.11, and a compiler/standard library that provides `import std;`. The `basic` template only requires CMake ≥ 3.20.

## Requirements

- **clang++** (C++23)
- **CMake** ≥ 3.20
- **Ninja**

Run `cppup doctor` to verify.

## Roadmap

- [x] Basic C++23 project creation & build
- [x] C++23 modules template (`import std`, `app.math`, `app.service`)
- [ ] GUI template
- [ ] `cppup test`
- [ ] `cppup add` (dependency helpers)
- [ ] Windows first-class support

## License

Apache License 2.0
