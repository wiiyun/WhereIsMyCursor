# WhereIsMyCursor（我的鼠标在哪里）

**v0.1.0 · 公开测试版** · [English](README.en.md)

晃动鼠标，快速找到指针。停止晃动后，指针短暂停留并平滑缩回。

[演示视频](https://github.com/wiiyun/WhereIsMyCursor/releases/download/v0.1.0/WhereIsMyCursor-v0.1.0-demo-compact.mp4)

## 功能

- 晃动放大鼠标指针，可调整倍数、灵敏度和停留时间。
- 支持全屏停用、应用排除和开机启动。
- 设置即时生效，内置中英文并跟随系统语言。

## 使用

适用于 **Windows 10 2004 及以上版本、Windows 11（x64）**，无需安装额外运行库。

1. 从 [下载](https://github.com/wiiyun/WhereIsMyCursor/releases/tag/v0.1.0) 下载 `WhereIsMyCursor-v0.1.0-win-x64.zip`。
2. 完整解压，运行 `WhereIsMyCursor.exe`。
3. 快速来回晃动鼠标即可放大指针。

双击托盘图标打开设置，右键可暂停或退出。请将 `WhereIsMyCursor.Guard.exe` 与主程序放在一起，它用于意外退出时恢复指针，无需手动运行。

本软件的代码完全由 Codex 编写。

## 开发与许可

使用 **Zig 0.14.1** 构建，详见 [构建说明](native/README.md)。

[详细说明](docs/USAGE.md) · [更新记录](CHANGELOG.md) · [MIT 许可证](LICENSE) · [第三方声明](THIRD_PARTY_NOTICES.txt) · [致谢与来源](docs/PROVENANCE.md)
