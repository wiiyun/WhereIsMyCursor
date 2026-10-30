# WhereIsMyCursor（我的鼠标在哪里）

**v0.1.0 · Public preview** · [中文](README.md)

Shake your mouse to find your cursor. It enlarges while shaking, holds briefly, then shrinks smoothly.

![Cursor enlargement demo](Assets/demo.gif)

[Demo video](https://github.com/wiiyun/WhereIsMyCursor/releases/download/v0.1.0/WhereIsMyCursor-v0.1.0-demo-compact.mp4)

## Features

- Adjustable cursor scale, shake sensitivity and hold duration.
- Fullscreen suppression, app exclusions and optional startup at sign-in.
- Settings apply immediately. Chinese and English follow the Windows display language.

## Usage

Requires **Windows 10 2004 or later, or Windows 11 (x64)**. No extra runtime installation needed.

1. Download `WhereIsMyCursor-v0.1.0-win-x64.zip` from [Download](https://github.com/wiiyun/WhereIsMyCursor/releases/tag/v0.1.0).
2. Extract the entire archive and run `WhereIsMyCursor.exe`.
3. Shake your mouse quickly back and forth.

Double-click the tray icon for settings; right-click to pause or exit. Keep `WhereIsMyCursor.Guard.exe` alongside the main executable. It restores the cursor after an unexpected exit and starts automatically.

All code for this software was written by Codex.

## Development and license

Build with **Zig 0.14.1**. See the [build instructions](native/README.md).

[Details](docs/USAGE.md) · [Changelog](CHANGELOG.md) · [MIT License](LICENSE) · [Third-party notices](THIRD_PARTY_NOTICES.txt) · [Acknowledgments](docs/PROVENANCE.md)
