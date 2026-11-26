[中文](README.md) | [English](README.en.md)

# 简焕输入法（Jianhuan IME）

简焕是一款本地、离线的中文输入法，面向 Windows 10 / Windows 11，支持 32 位与 64 位应用。核心引擎与词库采用宽松许可组件，应用代码可闭源商用。

## 功能

- 全拼输入。
- 微软风格双拼输入。
- 中英文切换：英文模式下按键直接上屏。
- 中文 / 英文标点切换。
- 用户词库自学习与动态调频，学习结果保存在本机。
- 内置单字与常用词组混合词库，支持用户自定义词库。
- 常用古诗词句库（公有领域文本）。
- Emoji 候选（按拼音匹配）。
- 简拼支持：常见简拼码表（如 xz / xianz → 现在），并支持首字母派生。
- GDI 候选窗：白底淡紫圆角候选条、选中项淡紫底紫字、未选中绿字、暗红预编辑加红色波浪下划线、翻页指示、光标跟随。
- 设置持久化：方案、中英文、全半角、标点状态。
- 单文件安装器：图形界面、自定义安装路径、覆盖安装、完整卸载。
- 纯本地运行：无账号、无云同步、无网络请求。

## 系统要求

- Windows 10 / Windows 11。
- x64 系统上的 32 位与 64 位应用。
- 当前用户安装，无需管理员权限。

## 安装

1. 下载 `JianhuanSetup.exe`。
2. 双击运行，在安装窗口中选择安装目录，默认 `%LOCALAPPDATA%\Jianhuan`。
3. 点击“安装”，完成后在 Windows 输入法设置中切换到“简焕”。

静默安装到指定目录：

```bat
JianhuanSetup.exe /quiet /DIR="D:\Tools\Jianhuan"
```

覆盖安装：直接再次运行安装器即可；新版本 DLL 侧载释放，旧版本被占用也不影响安装。

卸载：
- Windows 设置 → 应用 → 简焕输入法 → 卸载。
- 或运行 `JianhuanSetup.exe /uninstall`。
- 静默卸载：`JianhuanSetup.exe /quiet /uninstall`。

## 使用

| 按键 | 行为 |
|---|---|
| 拼音字母 | 进入中文组合输入 |
| 空格 | 选择当前高亮候选 |
| 数字 1–9 | 选择当前页对应候选 |
| 上 / 下 / 左 / 右 | 移动候选高亮 |
| PageUp / PageDown、- / =、+ / - | 候选翻页 |
| Backspace / Delete | 组合中退格 / 删除；无组合时交还应用 |
| Esc | 清空当前组合 |
| Shift | 切换中文 / 英文 |
| Ctrl + . | 切换中文 / 英文标点 |

英文模式下所有按键直接交给应用，不进行中文转换。

## 设置

入口：
- 任务栏通知区域“简焕”托盘图标：左键打开设置，右键菜单（个性化设置、全拼/双拼、英文标点、全角、关于、退出托盘）。
- 语言栏“简焕”图标点击菜单：全拼/双拼、英文标点、全角、设置…、关于。
- 安装目录或开始菜单的 `jianhuan_settings.exe` 图形设置窗口。

设置保存在用户目录的 `jianhuan.ini`：

```ini
schema_id=jianhuan_pinyin
ascii_mode=false
full_shape=false
ascii_punct=false
page_size=5
```

- `schema_id`：`jianhuan_pinyin` 全拼，`jianhuan_double_pinyin` 双拼。
- `ascii_mode`：英文模式。
- `full_shape`：全角。
- `ascii_punct`：英文标点。
- `page_size`：候选页大小。

## 词库

- 内置单字表与常用词组表。
- 用户词库文件：`data/jianhuan_user.dict.yaml`，格式：

```text
词语<TAB>pin yin<TAB>weight
```

示例：

```text
简焕	jian huan	100
```

- 自学习词库保存在用户目录，不上传、不跨设备。

## 技术架构

```text
src/core/                 共享 C++20 核心：librime 封装、设置、用户词库
src/platform/windows/     Windows TSF 文本服务、Direct2D 候选窗、注册工具
data/                     方案、词库、OpenCC 数据
tools/                    构建、打包、安装器生成
licenses/                 第三方许可与声明
```

- 引擎：librime（BSD-3-Clause）。
- 系统接入：Windows Text Services Framework（TSF）。
- 候选窗渲染：GDI 绘制，窗口区域裁剪实现圆角。
- 构建：CMake + Ninja + MSVC，vcpkg 静态依赖。
- 架构：x86 与 x64 双 DLL，注册表按 32/64 视图分别指向对应 DLL。
- 安装器：单文件 GUI，资源内嵌 payload，侧载覆盖安装。

## 从源码构建

```bat
tools\build_windows_arch.bat x64
tools\build_windows_arch.bat x86
tools\build_setup.bat
```

产物：
- `build/`、`build-x86/`：核心、测试、基准、TSF DLL、注册工具。
- `dist/jianhuan/`：安装 payload。
- `dist/JianhuanSetup.exe`：单文件安装包。

依赖通过项目内 vcpkg 安装到 `.deps/vcpkg`，不污染系统环境。

## 许可

应用代码按闭源商用产品管理。第三方组件遵守各自许可：

| 组件 | 许可 |
|---|---|
| librime | BSD-3-Clause |
| rime-pinyin-simp | Apache-2.0 |
| phrase-pinyin-data | MIT |
| Boost | BSL-1.0 |
| LevelDB | BSD-3-Clause |
| marisa-trie | BSD-2-Clause（本项目选择） |
| OpenCC | Apache-2.0 |
| yaml-cpp | MIT |
| spdlog / fmt | MIT |

不包含 GPL / LGPL 组件。详见 [licenses/THIRD_PARTY_NOTICES.txt](licenses/THIRD_PARTY_NOTICES.txt)。

## 目录结构

```text
CMakeLists.txt
data/
licenses/
src/
  core/
  platform/
    windows/
tests/
tools/
```