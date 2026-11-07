#!/bin/sh
# First build of cppup. Detects the OS, locates Homebrew (if any),
# then configures CMake + Ninja. After this, rebuild with the tool itself:
#   ./build/cppup build --release
set -eu

ROOT=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
cd "$ROOT"

case "$(uname -s)" in
  Darwin) OS=macos ;;
  Linux)  OS=linux ;;
  MINGW*|MSYS*|CYGWIN*) OS=windows ;;
  *) OS=unknown ;;
esac

echo "cppup bootstrap"
echo "  OS   $OS"

find_brew() {
  if command -v brew >/dev/null 2>&1; then
    brew --prefix
    return 0
  fi
  for b in \
      /opt/homebrew/bin/brew \
      /usr/local/bin/brew \
      "${HOME:-}/.linuxbrew/bin/brew" \
      /home/linuxbrew/.linuxbrew/bin/brew
  do
    if [ -n "$b" ] && [ -x "$b" ]; then
      "$b" --prefix
      return 0
    fi
  done
  if [ -d /opt/homebrew/bin ]; then
    echo /opt/homebrew
    return 0
  fi
  if [ -d /usr/local/Homebrew ] || [ -d /usr/local/Cellar ]; then
    echo /usr/local
    return 0
  fi
  if [ -d /home/linuxbrew/.linuxbrew/bin ]; then
    echo /home/linuxbrew/.linuxbrew
    return 0
  fi
  if [ -n "${HOME:-}" ] && [ -d "$HOME/.linuxbrew/bin" ]; then
    echo "$HOME/.linuxbrew"
    return 0
  fi
  return 1
}

BREW=""
if BREW=$(find_brew); then
  echo "  brew $BREW"
  # Apple Silicon shells often omit this from PATH.
  if [ -d "$BREW/bin" ]; then
    PATH="$BREW/bin:$PATH"
  fi
  if [ -d "$BREW/opt/llvm/bin" ]; then
    PATH="$BREW/opt/llvm/bin:$PATH"
  fi
  export PATH
else
  echo "  brew (not installed)"
  BREW=""
fi

find_tool() {
  name=$1
  if command -v "$name" >/dev/null 2>&1; then
    command -v "$name"
    return 0
  fi
  if [ -n "$BREW" ]; then
    for p in "$BREW/bin/$name" "$BREW/opt/llvm/bin/$name"; do
      if [ -x "$p" ]; then
        echo "$p"
        return 0
      fi
    done
  fi
  for p in \
      /opt/homebrew/bin/$name \
      /opt/homebrew/opt/llvm/bin/$name \
      /usr/local/bin/$name \
      /usr/bin/$name \
      /home/linuxbrew/.linuxbrew/bin/$name
  do
    if [ -x "$p" ]; then
      echo "$p"
      return 0
    fi
  done
  return 1
}

CMAKE=$(find_tool cmake) || {
  echo "error: cmake not found" >&2
  echo "  macOS:  brew install cmake ninja llvm" >&2
  echo "  Linux:  sudo apt install cmake ninja-build clang" >&2
  exit 1
}
NINJA=$(find_tool ninja) || {
  echo "error: ninja not found" >&2
  echo "  macOS:  brew install ninja" >&2
  echo "  Linux:  sudo apt install ninja-build" >&2
  exit 1
}
CXX=""
for cand in clang++ g++ c++; do
  if CXX=$(find_tool "$cand"); then
    break
  fi
done
if [ -z "$CXX" ]; then
  echo "error: no C++ compiler (clang++ or g++)" >&2
  exit 1
fi

echo "  cmake $CMAKE"
echo "  ninja $NINJA"
echo "  c++   $CXX"

# Drop a cache that was generated on another machine or checkout path.
if [ -f build/CMakeCache.txt ]; then
  recorded=$(sed -n 's/^CMAKE_CACHEFILE_DIR:INTERNAL=//p' build/CMakeCache.txt | head -n 1 | tr -d '\r')
  here=$(CDPATH= cd -- "$ROOT/build" && pwd)
  if [ -n "$recorded" ] && [ "$recorded" != "$here" ]; then
    echo "  stale CMake cache ($recorded) — removing build/"
    rm -rf build
  fi
fi

set -- \
  -G Ninja \
  -B "$ROOT/build" \
  -S "$ROOT" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_COMPILER="$CXX" \
  -DCMAKE_MAKE_PROGRAM="$NINJA"

echo "Configuring..."
"$CMAKE" "$@"
echo "Building..."
"$CMAKE" --build "$ROOT/build" --parallel

echo
echo "cppup is ready: $ROOT/build/cppup"
echo "Rebuild this repo with itself:"
echo "  $ROOT/build/cppup build --release"

CPPUP_BIN="$ROOT/build/cppup"
if [ ! -x "$CPPUP_BIN" ] && [ -x "$ROOT/build/cppup.exe" ]; then
  CPPUP_BIN="$ROOT/build/cppup.exe"
fi

echo
echo "Installing vcpkg (C++ package manager)..."
if [ "${CPPUP_VCPKG_SOURCE:-}" = "china" ]; then
  "$CPPUP_BIN" vcpkg setup --china || {
    echo "warning: vcpkg setup failed — rerun '$CPPUP_BIN vcpkg setup'" >&2
  }
else
  "$CPPUP_BIN" vcpkg setup || {
    echo "warning: vcpkg setup failed — rerun '$CPPUP_BIN vcpkg setup'" >&2
  }
fi
