# cppup

**One-command C++ 项目工具** —— 只需几条命令，就能创建、构建和运行现代 C++ 项目。

它受 `cargo`、`npm` 和 `go` 启发，但保持轻量：只负责生成干净的 CMake + Ninja 项目，并提供少量高频命令。

## 功能特性 (v0.1)

- `cppup init MyApp` — 创建一个可直接构建的 C++23 项目（自动初始化 vcpkg 清单）
- `cppup install fmt` — 通过 vcpkg 添加并安装依赖，写入项目配置
- `cppup vcpkg setup` — 安装 vcpkg，可选择官网源或国内镜像源
- `cppup add path/to/module.cppm` — 将本地模块加入 `modules` 项目
- `cppup update` — 解析模块 import，并刷新 `modules/modules.cmake`
- `cppup build` — 使用 **CMake + Ninja + Clang** 进行配置与构建（自动接入 vcpkg）
- `cppup bundle` — 打包应用及其非系统运行时依赖
- `cppup run` — 运行可执行文件
- `cppup go` — 一步构建并运行
- `cppup clean` — 删除构建产物和生成的 module PCM 文件
- `cppup doctor` — 检查 Clang、CMake、Ninja 和 vcpkg 是否已安装
- 模板代码内嵌在二进制中，不依赖外部模板文件

## 快速开始

首先构建（会检测 macOS / Linux / Windows，并优先选择 Homebrew 或系统工具链）：

```bash
# macOS 和 Linux
./bootstrap.sh

# Windows (PowerShell)
.\bootstrap.ps1
```

这个脚本会：

1. 判断操作系统（`uname` / Windows）。
2. 在 macOS 上查找 Homebrew：Apple Silicon 通常在 `/opt/homebrew`，Intel 在 `/usr/local`；Linux 也会检查 Linuxbrew。Windows 会搜索 PATH、`Program Files`、scoop、winget 和 chocolatey。
3. 用找到的 CMake 和 Ninja 配置当前仓库。
4. 构建完成后自动执行 `cppup vcpkg setup` 安装 vcpkg，并询问使用官网源还是国内源。

之后，cppup 会自行构建自身：

```bash
./build/cppup doctor
./build/cppup build --release     # Windows: .\build\cppup.exe build --release
```

在其他项目中使用：

```bash
cppup init Hello
cd Hello
cppup go
```

## `init` 创建的项目结构

```text
MyApp/
├── CMakeLists.txt
├── .clangd
├── .cppup.json          # 项目元数据（vcpkg 源、triplet、已安装包）
├── vcpkg.json           # vcpkg 清单模式依赖
├── vcpkg-configuration.json  # 使用国内源时生成
├── .vscode/
│   └── launch.json
├── modules/
│   └── .gitkeep
├── src/
│   └── main.cpp
├── .gitignore
└── README.md
```

依赖安装后会放在 `build/vcpkg_installed/<triplet>/`，`.clangd` 会引用其中的 `include` 目录。

## 架构说明

- `src/app` 包含可执行文件入口；`src/cli` 负责参数解析与命令分发。
- `src/core`、`src/platform`、`src/templates` 和 `src/util` 分别负责项目操作、工具链探测、模板生成和共享工具。
- `cppup_core` 构建可复用的非 CLI 代码；`cppup` 是链接到它的 CLI 前端。
- `src/version.h` 是 CLI 和 CMake 项目元数据的唯一版本来源。
- 未来 GUI 应该是独立的可执行文件，并复用同一个 core 库；GUI 依赖不应塞进 `cppup_core`。

cppup 源码默认使用标准 C++23。若要开启 CMake 的实验性 `import std` 支持，在开发 cppup 时可配置：
`-DCPPUP_ENABLE_CXX_MODULES=ON`；这要求 CMake 3.30+ 且具备兼容的 Clang/libc++ 工具链。

对于完整跨平台桌面应用，**Qt 6 Widgets** 是推荐起点：控件成熟、CMake 集成强、平台支持广。它适合作为可选 GUI 目标，因为 Qt 依赖更大，且有 LGPL / 商业许可考虑。**Dear ImGui** 更适合内部诊断面板或开发工具，而非普通终端用户桌面应用。

## 模板

| 名称 | 描述 |
|------|------|
| `basic` | 经典 C++23 可执行程序（默认） |
| `modules` | C++23 模块（`import std;`、`app.math`、`app.service`） |

```bash
cppup init Foo -t modules
```

> 说明：`modules` 模板需要 CMake ≥ 3.30、Ninja ≥ 1.11，以及提供 `import std;` 的编译器和标准库；`basic` 模板仅要求 CMake ≥ 3.20。

## vcpkg 依赖管理

项目使用 vcpkg **清单模式**（`vcpkg.json`）。常用的几个操作：

```bash
cppup vcpkg setup            # 安装 vcpkg；交互选择官网源 / 国内源
cppup vcpkg setup --china    # 直接使用国内源（Gitee 仓库 + 清华下载镜像）
cppup install fmt            # 添加依赖并 vcpkg install
cppup build                  # 配置时自动使用 vcpkg toolchain
```

国内源的可选环境变量（未设置时使用默认值）：

| 变量 | 默认值 |
|------|--------|
| `CPPUP_VCPKG_GIT_MIRROR` | `https://gitee.com/mirrors/vcpkg.git` |
| `CPPUP_VCPKG_ASSET_MIRROR` | `https://mirrors.tuna.tsinghua.edu.cn/vcpkg/assets/` |

安装信息记录在项目的 `.cppup.json` 中（源类型、triplet、vcpkg 根目录、已安装包），
全局的源与根目录记录在 `~/.cppup/config.json`。

## 环境要求

- **clang++** (C++23)
- **CMake** ≥ 3.20
- **Ninja**
- **vcpkg**（`bootstrap.sh` / `bootstrap.ps1` 会自动安装；也可运行 `cppup vcpkg setup`）

运行 `cppup doctor` 进行检查。

## 路线图

- [x] 基础 C++23 项目创建与构建
- [x] C++23 模块模板（`import std`、`app.math`、`app.service`）
- [ ] GUI 模板
- [ ] `cppup test`
- [x] `cppup add` 和 `cppup update`：本地 C++ 模块依赖管理
- [ ] Windows 一等公民支持

## 许可证

Apache License 2.0
