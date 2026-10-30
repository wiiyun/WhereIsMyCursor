# Details / 详细说明

[返回首页](../README.md) · [Back to README](../README.en.md)

## 设置

| 项目 | 默认值 / 行为 |
| --- | --- |
| 最大放大倍数 | 8 倍，范围 2–20；是动画的放大上限 |
| 晃动灵敏度 | 0.8，范围 0.25–2；数值越高越容易触发 |
| 停留时间 | 0.35 秒，范围 0–2 秒，步长 0.05 秒；停止甩动后保持当前大小，随后缩回 |
| 全屏应用中自动停用 | 开启 |
| 登录 Windows 时自动启动 | 默认关闭，按当前用户设置 |
| 排除应用 | 留空；填进程名，以逗号分隔，如 `game.exe, player.exe` |
| 界面语言 | 跟随当前用户的 Windows 显示语言，重启程序后应用语言变更 |

修改后立即生效，停止编辑约 300ms 后自动保存，关闭窗口前也会保存。“默认”按钮恢复对应项目的默认值。数值暂时为空或无效时，继续使用上一个有效值。

配置保存在 `%LOCALAPPDATA%\ShakeCursor\settings.json`，保留旧目录名以兼容早期版本。已有有效配置会继续沿用，不会仅因默认值变化而覆盖。

## 已知限制

- 全屏停用依赖窗口检测，可能无法识别所有游戏、无边框窗口或覆盖层；可手动暂停或加入进程排除名单。
- 放大窗口排除屏幕捕获以避免反色采样自身，因此放大指针可能不会出现在截图、录屏或屏幕共享中。
- 动画指针读取使用未正式文档化的 `GetCursorFrameInfo`，不保证所有指针主题兼容或与系统动画相位完全同步。
- 大倍数反色使用后台采样；高对比背景边缘的颜色可能存在一次采样周期的延迟。
- HDR、受保护画面及各类远程桌面的表现可能不同，仍需具体环境验证。
- 晃动判定和动画为本项目实现，并非 macOS 内部参数的精确复刻。


## Settings

| Setting | Default / behavior |
| --- | --- |
| Maximum scale | 8×, range 2–20; the animation's upper limit |
| Shake sensitivity | 0.8, range 0.25–2; higher values trigger more easily |
| Hold duration | 0.35 seconds, range 0–2, step 0.05; hold after shaking stops before shrinking |
| Disable in fullscreen apps | Enabled |
| Start at sign-in | Disabled by default; per-user setting |
| Excluded apps | Empty; comma-separated process names, such as `game.exe, player.exe` |

Settings apply immediately and save about 300ms after editing stops, or before closing the window. Invalid or incomplete numeric input keeps the last valid value active. The Default buttons restore the corresponding defaults.

Configuration is stored at `%LOCALAPPDATA%\ShakeCursor\settings.json`; the old directory name is retained for compatibility. Existing valid preferences are preserved.

## Limitations

- Fullscreen detection cannot cover every game, borderless window or overlay. Use process exclusions or pause manually when needed.
- The enlarged overlay is excluded from screen capture to avoid sampling itself, so it may be absent from screenshots, recordings and screen sharing.
- Animated cursors use the undocumented `GetCursorFrameInfo` API; compatibility and exact animation synchronization are not guaranteed.
- Background inversion is sampled asynchronously. Colors may lag one sampling cycle at high-contrast edges.
- HDR, protected content and remote desktop applications need environment-specific verification.
- Gesture thresholds and animation are this project's implementation, not an exact reproduction of macOS internals.
