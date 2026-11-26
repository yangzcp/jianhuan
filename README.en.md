[中文](README.md) | [English](README.en.md)

# Jianhuan IME (简焕输入法)

Jianhuan is a local, offline Chinese input method for Windows 10 / Windows 11, supporting both 32-bit and 64-bit applications. The engine and dictionaries use permissively licensed components; application code is suitable for closed-source commercial use.

## Features

- Full pinyin input.
- Microsoft-style double pinyin input.
- Chinese / English toggle: in English mode keys pass through directly.
- Chinese / English punctuation toggle.
- User dictionary learning and dynamic frequency adjustment, stored locally.
- Built-in mixed single-character and common-phrase dictionary, plus user-defined dictionaries.
- Common classical poetry lines (public-domain text).
- Emoji candidates matched by pinyin.
- Abbreviated pinyin (jianpin): curated code table (e.g. xz / xianz → 现在) plus initial-letter derivation.
- GDI candidate window: white bar with light-purple rounded border, selected item on lavender with purple text, unselected green text, maroon preedit with red wavy underline, page indicator, caret following.
- Persistent settings: schema, ASCII mode, full-width mode, punctuation mode.
- Single-file installer: GUI, custom install path, overwrite install, complete uninstall.
- Fully local: no account, no cloud sync, no network requests.

## System Requirements

- Windows 10 / Windows 11.
- 32-bit and 64-bit applications on x64 systems.
- Per-user installation, no administrator privileges required.

## Installation

1. Download `JianhuanSetup.exe`.
2. Double-click it and choose an install directory in the installer window; default is `%LOCALAPPDATA%\Jianhuan`.
3. Click Install, then switch to Jianhuan in Windows input settings.

Quiet install to a chosen directory:

```bat
JianhuanSetup.exe /quiet /DIR="D:\Tools\Jianhuan"
```

Overwrite install: run the installer again. New DLLs are side-loaded, so an in-use old version does not block installation.

Uninstall:
- Windows Settings → Apps → Jianhuan IME → Uninstall.
- Or run `JianhuanSetup.exe /uninstall`.
- Quiet uninstall: `JianhuanSetup.exe /quiet /uninstall`.

## Usage

| Key | Behavior |
|---|---|
| Pinyin letters | Start Chinese composition |
| Space | Select the highlighted candidate |
| Digits 1–9 | Select a candidate on the current page |
| Up / Down / Left / Right | Move candidate highlight |
| PageUp / PageDown, - / =, + / - | Page candidates |
| Backspace / Delete | Edit composition; pass through when not composing |
| Esc | Clear composition |
| Shift | Toggle Chinese / English |
| Ctrl + . | Toggle Chinese / English punctuation |

In English mode all keys are delivered to the application without Chinese conversion.

## Settings

Entry points:
- Notification-area "Jianhuan" tray icon: left-click opens settings; right-click menu (Settings, full/double pinyin, English punctuation, full-width, About, exit tray).
- Language-bar "Jianhuan" icon menu: full/double pinyin, English punctuation, full-width, Settings…, About.
- `jianhuan_settings.exe` GUI settings window in the install directory or Start Menu.

Settings are stored in `jianhuan.ini` in the user directory:

```ini
schema_id=jianhuan_pinyin
ascii_mode=false
full_shape=false
ascii_punct=false
page_size=5
```

- `schema_id`: `jianhuan_pinyin` for full pinyin, `jianhuan_double_pinyin` for double pinyin.
- `ascii_mode`: English mode.
- `full_shape`: Full-width characters.
- `ascii_punct`: English punctuation.
- `page_size`: Candidates per page.

## Dictionaries

- Built-in single-character and common-phrase tables.
- User dictionary file: `data/jianhuan_user.dict.yaml`, format:

```text
word<TAB>pin yin<TAB>weight
```

Example:

```text
简焕	jian huan	100
```

- Learned entries are stored in the user directory; they are not uploaded or synced.

## Architecture

```text
src/core/                 Shared C++20 core: librime wrapper, settings, user dictionary
src/platform/windows/     Windows TSF text service, Direct2D candidate window, registration
data/                     Schemas, dictionaries, OpenCC data
tools/                    Build, packaging, installer generation
licenses/                 Third-party licenses and notices
```

- Engine: librime (BSD-3-Clause).
- System integration: Windows Text Services Framework (TSF).
- Candidate rendering: GDI drawing with window-region clipping for rounded corners.
- Build: CMake + Ninja + MSVC, vcpkg static dependencies.
- Architectures: x86 and x64 DLLs; registry views point to the matching DLL.
- Installer: single-file GUI with embedded payload and side-load overwrite install.

## Building from Source

```bat
tools\build_windows_arch.bat x64
tools\build_windows_arch.bat x86
tools\build_setup.bat
```

Outputs:
- `build/`, `build-x86/`: core, tests, benchmark, TSF DLLs, registration tool.
- `dist/jianhuan/`: installer payload.
- `dist/JianhuanSetup.exe`: single-file installer.

Dependencies are installed into project-local `.deps/vcpkg` and do not modify the system environment.

## Licensing

Application code is managed as a closed-source commercial product. Third-party components remain under their own licenses:

| Component | License |
|---|---|
| librime | BSD-3-Clause |
| rime-pinyin-simp | Apache-2.0 |
| phrase-pinyin-data | MIT |
| Boost | BSL-1.0 |
| LevelDB | BSD-3-Clause |
| marisa-trie | BSD-2-Clause (selected by this project) |
| OpenCC | Apache-2.0 |
| yaml-cpp | MIT |
| spdlog / fmt | MIT |

No GPL / LGPL components are included. See [licenses/THIRD_PARTY_NOTICES.txt](licenses/THIRD_PARTY_NOTICES.txt).

## Directory Layout

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