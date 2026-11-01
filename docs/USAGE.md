# cppup 使用说明

## 命令

### `cppup init <name> [options]`

创建一个新项目。

```bash
cppup init MyApp
cppup init MyLib -t modules
cppup init Existing --force          # 覆盖已有目录（危险）
```

参数：
- `-t, --template <name>`  模板名称（`basic` | `modules`）
- `-f, --force`            覆盖已存在的输出目录

### `cppup add <path/to/module.cppm>`

向使用 `modules` 模板创建的项目中加入一个本地导出模块接口。cppup 会复制该源文件到 `modules/`，并注册到 `modules/modules.cmake`；随后 `cppup build` 会按当前编译器生成对应的 PCM。

```bash
cppup add ../shared/math.cppm
cppup build
```

源文件必须包含 `export module name;` 声明。cppup 会递归解析其本地命名模块依赖，并把所需的 `.cppm` 源一起复制到 `modules/` 目录中。`std` 和 `std.compat` 由工具链提供，不需要手动放到项目里。

### `cppup update`

重新扫描 `modules/` 中现有的所有 `.cppm` 文件，复制本地导入的模块源码，并重建 `modules/modules.cmake`。

```bash
cppup update
cppup build
```

PCM 文件是编译器特定的构建产物；`cppup update` 不会把它们加入 CMake 源列表。`cppup build` 会根据当前编译器生成 PCM，而 `cppup clean` 会删除这些产物。

### `cppup build [-r]`

配置并构建当前项目（如果需要会先生成 build 目录）。

```bash
cppup build              # Debug
cppup build --release    # Release
```

使用方式：
```bash
cmake -G Ninja -B build -DCMAKE_BUILD_TYPE=... -DCMAKE_CXX_COMPILER=clang++
cmake --build build --parallel
```

### `cppup bundle [--release] [--force]`

构建当前项目，并收集其非系统运行时依赖，输出到 `dist/<project>/`（macOS 下通常为 `dist/<project>.app/Contents/Frameworks`）。CMake 的 `BundleUtilities` 会处理依赖发现和平台特定修正。
常见系统库保留为外部依赖；只有可解析的 C++ 运行时库和第三方库会被打包。

```bash
cppup bundle
cppup bundle --release
cppup bundle --release --force
```

已生成的 bundle 可在下一次运行时替换；使用 `--force` 可替换已有目标目录。这里没有提供单文件 `--file` 模式，因为将任意动态库安全嵌入单个二进制文件，依赖平台和库的具体实现方式，兼容性差；`--standalone` 也不提供，因为 `bundle` 已默认输出自包含应用目录。

### `cppup run [args...]`

运行已构建的可执行文件。额外参数会原样传递给程序。

```bash
cppup run
cppup run --help
```

### `cppup go [-r]`

`build` + `run` 的快捷命令。

```bash
cppup go
cppup go --release
```

### `cppup clean`

删除 `build/` 目录以及生成的 `modules/*.pcm` 文件。模块接口源码 (`*.cppm`) 会保留。

### `cppup doctor`

检查所需工具是否可用，并输出版本号。

### `cppup templates`

列出可用的项目模板。

## 定位项目根目录

大多数命令都会从当前目录向上查找 `CMakeLists.txt`。因此你可以在项目任意子目录中运行它们。

## 环境变量

- `NO_COLOR` — 禁用彩色输出
