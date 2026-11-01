# cppup Usage

## Commands

### `cppup init <name> [options]`

Create a new project.

```bash
cppup init MyApp
cppup init MyLib -t modules
cppup init Existing --force          # overwrite (dangerous)
```

Options:
- `-t, --template <name>`  Template to use (`basic` | `modules`)
- `-f, --force`            Overwrite existing directory

### `cppup build [-r]`

Configure (if needed) and build the current project.

```bash
cppup build              # Debug
cppup build --release    # Release
```

Uses:
```
cmake -G Ninja -B build -DCMAKE_BUILD_TYPE=... -DCMAKE_CXX_COMPILER=clang++
cmake --build build --parallel
```

### `cppup run [args...]`

Run the built executable. Extra arguments are passed through.

```bash
cppup run
cppup run --help
```

### `cppup go [-r]`

Shortcut for `build` + `run`.

```bash
cppup go
cppup go --release
```

### `cppup clean`

Delete the `build/` directory.

### `cppup doctor`

Check that the required tools are available and print their versions.

### `cppup templates`

List available project templates.

## Finding the project root

Most commands walk up from the current directory looking for a `CMakeLists.txt`.  
You can run them from any subdirectory of the project.

## Environment variables

- `NO_COLOR` — disable coloured output
