# C++23 Modules + CMake + Ninja（macOS Apple Silicon / Homebrew LLVM）

这是一个最小可扩展的模块化项目骨架，演示：

- `src/math.cppm` 导出 `app.math`
- `src/service.cppm` 导出 `app.service` 并导入 `app.math`
- `src/main.cpp` 导入 `std` 和业务模块
- CMake 扫描模块依赖，Ninja 按依赖顺序构建 BMI 与目标文件，最后链接 `build/app`

> **重要限制：** libc++ 的 `std` / `std.compat` 模块仍属实验性功能；具体可用性依赖 Clang、libc++、CMake、Ninja 版本与打包选项。此项目是起点，不保证未经检查即可在所有 Homebrew LLVM 版本上构建。请先看下方环境检查与故障排查。

## 目录

```text
cpp23-cmake-ninja-starter/
├── CMakeLists.txt
└── src/
    ├── math.cppm
    ├── service.cppm
    └── main.cpp
```

## 1. 安装工具

```bash
brew install llvm cmake ninja
```

确认版本：

```bash
brew --prefix llvm
"$(brew --prefix llvm)/bin/clang++" --version
cmake --version
ninja --version
```

libc++ 文档指出 Ninja 需要 1.11 或更新版本。使用模块时，建议采用较新的 CMake 和与之配套的 Clang。

## 2. 确认标准库模块支持

先检查安装是否提供 `std.cppm`：

```bash
find "$(brew --prefix llvm)" -name 'std.cppm' -o -name 'std.compat.cppm'
```

如果找不到，不要手工伪造 `std.cppm`。检查你安装的 LLVM/libc++ 包说明。某些发行包不提供安装版模块接口，或需要按 libc++ 文档从构建树生成模块。

## 3. 配置编译器路径

libc++ 模块文档提醒：用于模块扫描的编译器路径不应是符号链接。可先解析 Homebrew 路径：

```bash
export LLVM_PREFIX="$(brew --prefix llvm)"
export CLANGXX="$(python3 -c 'import os,sys; print(os.path.realpath(sys.argv[1]))' "$LLVM_PREFIX/bin/clang++")"
echo "$CLANGXX"
```

然后在项目目录执行配置命令：

```bash
cmake -S . -B build -G Ninja \
  -DCMAKE_CXX_COMPILER="$CLANGXX" \
  -DCMAKE_CXX_FLAGS="-stdlib=libc++"
```

如果当前 CMake 版本要求显式开启 `import std` 实验门控，请参照下方说明，不要盲目复制其他 CMake 版本的实验 UUID。

## 4. 构建与运行

```bash
cmake --build build --verbose
./build/app
```

预期输出：

```text
2 + 3 = 5
C++23 modules are working
```

## 5. 依赖关系

```text
libc++ 的 std 模块（由 CMake/工具链提供）
                 │
                 ▼
src/math.cppm ──► app.math 的 BMI
      │                  │
      ▼                  ▼
 app.math.o        src/service.cppm
                         │ import app.math
                         ▼
                  app.service 的 BMI
                         │
                         ▼
                   app.service.o

src/main.cpp ── imports std, app.math, app.service
       │
       ▼
    main.o
       │
       └────────── 所有目标文件 + libc++ ──► build/app
```

实际 BMI 文件名、存放路径和中间目标文件布局由 CMake、Clang 与 Ninja 管理；不要在源码中硬编码 `.pcm` 路径。

## 6. CMake 对 `import std;` 的支持说明

- 自定义 C++ 模块依赖扫描由 CMake 管理，建议使用 `Ninja`。
- CMake 3.30 引入了 `CXX_MODULE_STD`，但早期版本需要通过 `CMAKE_EXPERIMENTAL_CXX_IMPORT_STD` 实验门控启用。
- 本项目会在 `project()` 前将 CMake 3.30 至 4.4 对应的实验门控写入缓存，以便编译器探测也能读取。CMake 4.5 及以上需要按官方文档更新 UUID。
- `import std` 仍是 CMake 的实验功能；如果遇到 `CXX_MODULE_STD` / 实验门控错误，请查阅当前版本的官方文档。实验 UUID 不是跨版本稳定接口。

## 7. 常见故障

### 找不到 `std.cppm` 或 `import std` 模块

说明当前安装可能未提供安装版标准库模块，或 CMake 无法发现它。检查 libc++ 官方 Modules 文档及当前 LLVM 包内容。必要时，按 libc++ 官方文档构建其模块，再接入项目。

### `module file ... cannot be loaded due to a configuration mismatch`

确保标准库 BMI 与项目使用同一套 Clang/libc++、目标平台和关键编译选项。`-stdlib=libc++`、线程选项、宏、SDK 和语言标准等差异都可能影响兼容性。清理构建目录后重新配置：

```bash
rm -rf build
cmake -S . -B build -G Ninja \
  -DCMAKE_CXX_COMPILER="$CLANGXX" \
  -DCMAKE_CXX_FLAGS="-stdlib=libc++"
cmake --build build --verbose
```

### CMake 没有扫描 `.cppm`

检查 CMake 版本、生成器是否为 Ninja，以及源文件是否放入 `FILE_SET CXX_MODULES`。查看构建日志中的模块扫描步骤。

### `std::print` 不可用

示例刻意只使用 `std::println` 以演示 C++23；若当前 libc++ 没有实现该接口，可把 `main.cpp` 改为 `std::cout`，并按需在合适位置处理标准库模块与头文件混用限制。`-std=c++23` 不代表所有 C++23 标准库特性都已实现。

## 官方资料

- Clang C++ Modules: https://clang.llvm.org/docs/StandardCPlusPlusModules.html
- libc++ Modules: https://libcxx.llvm.org/Modules.html
- CMake C++ Modules: https://cmake.org/cmake/help/latest/manual/cmake-cxxmodules.7.html
- CMake `CXX_MODULE_STD`: https://cmake.org/cmake/help/latest/prop_tgt/CXX_MODULE_STD.html
