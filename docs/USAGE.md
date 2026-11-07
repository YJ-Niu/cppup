# cppup 使用说明

## 命令

### `cppup init <name> [options]`

创建一个新项目。

```bash
cppup init MyApp
cppup init MyLib -t modules
cppup init MyApp --china            # 项目使用国内 vcpkg 源
cppup init Existing --force          # 覆盖已有目录（危险）
```

参数：
- `-t, --template <name>`  模板名称（`basic` | `modules`）
- `-f, --force`            覆盖已存在的输出目录
- `--china` / `--official` 选择本项目使用的 vcpkg 源（默认读取 `~/.cppup/config.json`）

`init` 会同时创建：

- `vcpkg.json`：vcpkg 清单模式依赖文件
- `vcpkg-configuration.json`：仅在 vcpkg 来自 Gitee 镜像时生成
- `.cppup.json`：项目元数据（项目名、模板、vcpkg 源 / triplet / 已安装包）
- `.clangd`：引用 `build/vcpkg_installed/<triplet>/include`

### `cppup vcpkg setup [--china|--official] [-f]`

安装 vcpkg 本体。默认安装目录随操作系统确定：macOS `~/Library/Application Support/cppup/vcpkg`、
Windows `%LOCALAPPDATA%\cppup\vcpkg`、Linux `$XDG_DATA_HOME/cppup/vcpkg`（无该变量时
`~/.local/share/cppup/vcpkg`）；可用 `VCPKG_ROOT` 或 `~/.cppup/config.json` 覆盖。

```bash
cppup vcpkg setup            # 交互式；已安装时可选择跳过本次安装
cppup vcpkg setup --china    # Gitee 仓库 + 国内下载镜像
cppup vcpkg setup --official # GitHub 官方源
```

执行时先检测是否已安装 vcpkg：

- 未安装：仅显示安装源选项，并打印将要安装到的目录
- 已安装：提示安装位置，并提供 `跳过本次安装`（默认）、切换官网源、切换国内源三个选项
- 安装或切换后会打印实际使用的源、仓库地址和下载镜像；中国大陆网络建议选择国内源

国内源会：

- 从 `https://gitee.com/mirrors/vcpkg.git` 克隆 vcpkg
- 为依赖包下载设置 `X_VCPKG_ASSET_SOURCES=https://mirrors.tuna.tsinghua.edu.cn/vcpkg/assets/`
- 在项目中生成指向 Gitee registry 的 `vcpkg-configuration.json`

选择结果写入 `~/.cppup/config.json`。可通过环境变量 `CPPUP_VCPKG_GIT_MIRROR`、
`CPPUP_VCPKG_ASSET_MIRROR` 覆盖镜像地址。

### `cppup install <port>...`

通过 vcpkg 安装依赖，并把安装信息写入项目配置文件。

```bash
cppup install fmt
cppup install fmt spdlog
cppup install                # 不带包名：仅同步 vcpkg.json
```

执行流程：

1. 检查 vcpkg 是否可用（未安装时提示先运行 `cppup vcpkg setup`）
2. 确保 `vcpkg.json` 存在
3. 对每个包执行 `vcpkg add port <name>`
4. 执行 `vcpkg install --triplet <host> --x-install-root build/vcpkg_installed`
5. 更新 `.cppup.json` 中的 `packages` 列表

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

### `cppup check [--fix]`

配置项目（生成 `build/compile_commands.json`），然后在不编译的情况下执行代码检查：

- **格式检查**：`clang-format --dry-run --Werror`，使用项目根目录的 `.clang-format`；`--fix` 会直接格式化源码。
- **未使用代码**：`clang-diagnostic-unused-*`（未使用变量、函数等）。
- **内存安全与常见缺陷**：`clang-analyzer-*`（空指针解引用、泄漏、use-after-free 等）和 `bugprone-*`；
  检查项由项目根目录的 `.clang-tidy` 配置，`cppup init` 会自动生成。
- **自动修复**：`--fix` 先格式化源码，再应用 `clang-tidy` 提供的建议修复（如 `modernize-*`）；
  无法自动修复的问题仍会导致检查失败。

```bash
cppup check
cppup check --fix
```

缺少 `clang-format` / `clang-tidy` 时会给出警告并跳过对应检查；C++ modules 项目在构建前无法生成
clang-tidy 所需的 module map，因此该阶段只做格式检查。

### `cppup build [-r] [--no-check] [--fix]`

配置并构建当前项目（如果需要会先生成 build 目录）。

```bash
cppup build              # Debug；构建前运行代码检查
cppup build --release    # Release
cppup build --no-check   # 跳过构建前的代码检查
cppup build --fix        # 先自动格式化，再检查并构建
```

代码检查在 CMake configure 之后、`cmake --build` 之前执行；任何一项检查失败都会中止构建。

使用方式：
```bash
cmake -G Ninja -B build -DCMAKE_BUILD_TYPE=... -DCMAKE_CXX_COMPILER=clang++
cmake --build build --parallel
```

如果项目包含 `vcpkg.json`，`cppup build` 会自动定位 vcpkg 并追加：

```bash
-DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake
```

vcpkg 未安装时会自动执行一次 `cppup vcpkg setup`（交互选择源）。

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

检查所需工具是否可用，并输出版本号（包括可选的 vcpkg）。

### `cppup templates`

列出可用的项目模板。

## 定位项目根目录

大多数命令都会从当前目录向上查找 `CMakeLists.txt`。因此你可以在项目任意子目录中运行它们。

## 环境变量

- `NO_COLOR` — 禁用彩色输出
- `VCPKG_ROOT` — 指定已有的 vcpkg 根目录
- `CPPUP_VCPKG_SOURCE` — `official` 或 `china`，跳过安装源选择
- `CPPUP_VCPKG_GIT_MIRROR` — 覆盖国内 vcpkg 仓库地址
- `CPPUP_VCPKG_ASSET_MIRROR` — 覆盖国内依赖下载镜像
- `VCPKG_DEFAULT_TRIPLET` — 覆盖 vcpkg 默认 triplet
