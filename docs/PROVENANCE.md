# 来源与许可范围 / Provenance

本文件记录当前发布目录可核对的来源信息，不将功能相似视为代码授权。

## 本项目文件

- 当前应用实现位于 `native/`，由 C++ / C Win32 源码构建。
- 图标由仓库中的 `tools/GenerateIcon.ps1` 使用 System.Drawing 绘制，输出为 `Assets/App.ico` 和 `Assets/App.png`。
- 项目自身文件使用根目录 `LICENSE` 中的 MIT 许可；第三方许可不被该许可替代。
- `legacy-build/` 是本机历史备份，不包含在上传源码包或运行包中。

## 功能参考

- MyBigMouse：<https://github.com/scavin/MyBigMouse>
- shake-cursor：<https://github.com/LSSY444/shake-cursor>
- macOS 的晃动定位指针交互。

本次检查未在当前待发布源码中发现上述项目的整份第三方源码或素材副本。这是对当前文件和开发记录的检查，并非对历史来源的逐行鉴定。MyBigMouse 的公开仓库在检查时仅列出 README，未提供可据此授权复用的源码许可证。不要因其公开可见而复制其中内容到本项目的 MIT 许可范围。

后续引入第三方代码、图标或其他素材时，应记录具体文件、来源版本、原作者和许可证，并按要求保留声明。

## 工具链与静态运行时

使用 Zig 0.14.1 自带的 C/C++ 工具链构建。编译器本身不随应用发布。静态运行时组件及相关声明见 `THIRD_PARTY_NOTICES.txt` 和 `licenses/`。Windows 系统 DLL 不随应用发布。

The root MIT license covers project-owned files only. Third-party runtime components retain their original notices and terms. Reference links acknowledge feature inspiration and discussion; they do not grant rights to upstream code or imply endorsement.
