# 上传 GitHub 与发布版本

## 源码仓库

仓库中保留 `native/` 源码、`Assets/`、`tools/`、`licenses/`、`docs/`、两份 README、`LICENSE`、第三方声明以及 Git 配置文件。

`tools/Package.ps1` 生成的 `artifacts/WhereIsMyCursor-source.zip` 只包含这些发布文件。通过网页上传时，可解压该源码包，将其**内容**上传到仓库根目录。不要将 ZIP 本身作为唯一的仓库文件，也不要使用便携运行包作为源码仓库。

如果使用 Git，可在当前项目目录执行：

```powershell
git init
git add .
git diff --cached --stat
git status --short
git commit -m "Initial public source release"
git branch -M main
# 在 GitHub 新建空仓库后，替换下面的地址
git remote add origin https://github.com/YOUR_ACCOUNT/WhereIsMyCursor.git
git push -u origin main
```

提交前查看暂存文件，确认不含本机备份、配置、编译产物或其他个人文件。`.gitignore` 不会移除已被 Git 跟踪的文件。

当前 MIT 版权署名为 `WhereIsMyCursor contributors`。如需署个人名或 GitHub 用户名，可在首次发布前修改 `LICENSE` 的版权行，保留许可正文。

## 构建与打包

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\native\build.ps1 -Zig C:\Tools\zig\zig.exe
Start-Process .\native\build\WhereIsMyCursor.exe -ArgumentList '--self-test' -Wait
Get-Content .\native\build\native-test-result.txt
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\Package.ps1
```

生成：

- `artifacts/WhereIsMyCursor-win-x64.zip`：用户运行包，含主程序、守护程序、README、许可证及第三方声明。
- `artifacts/WhereIsMyCursor-source.zip`：干净源码包，不含本机历史记录、诊断输出或可执行文件。
- `artifacts/SHA256SUMS.txt`：两个包的 SHA-256 校验值。

## GitHub Releases

推送源码后，在仓库的 Releases 中创建版本，选择相应提交并填写版本号和更新说明。上传便携运行包及校验值文件；源码已在仓库中，GitHub 还会自动提供对应版本的源码下载。

发布说明应写明 Windows x64、最低系统版本、主程序与守护程序需保留在同一文件夹。版本号应与 `VERSION` 中的文件和产品版本同步；当前二进制版本为 `0.1.0`。

运行包没有代码签名；不要在发布说明中声称已经签名。

## Versioning

Edit the root `VERSION` file (major.minor.patch), update README version labels and
`CHANGELOG.md`, then rebuild and package. Both EXEs use the same product version;
Windows numeric versions add a trailing zero. Settings title and tray tooltip show
the version. Current release assets are `WhereIsMyCursor-v0.1.0-win-x64.zip` and
`WhereIsMyCursor-v0.1.0-source.zip`. Stable unversioned aliases are generated for
local compatibility. Publish the versioned files and tag the commit `v0.1.0`.
Packaging rejects binaries whose product version differs from VERSION.
