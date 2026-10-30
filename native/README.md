# WhereIsMyCursor — native Win32 sources

Build on Windows x64 with Zig 0.14.1 (portable compiler; its bundled MinGW headers/libraries are used):

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\native\build.ps1 -Zig D:\path\to\zig.exe
```

The resulting `native/build/WhereIsMyCursor.exe` is a native C++17 Win32 executable. Keep `WhereIsMyCursor.Guard.exe` next to it; the guard is a separate small C/Win32 executable. Its C++ runtime is linked statically; it needs only Windows system DLLs. No .NET, Python, Visual C++ Redistributable or compiler is needed to run it. The compiler is only needed for rebuilding.

- `app.cpp`: message loop, tray icon, native trackbars/edit boxes/check boxes, DPI-aware settings.
- `core.cpp`: existing JSON settings migration, startup registry, fullscreen exclusion, shake detection.
- `render.cpp`: native DIB cursor masks, nearest-neighbor scaling, per-pixel background inversion, cursor visibility and recovery guard.
- `guard.c`: kernel32-only recovery process; magnification is loaded only after the parent exits.
- `async_inverse.cpp` / `.h`: latest-job background capture, bounded frame reuse and cancellation; mouse movement stays on the UI thread.
- `tests.cpp`: deterministic gesture/animation/config tests plus native display and controls integration tests.

Run `WhereIsMyCursor.exe --self-test` or `--integration-test`; reports are written next to the EXE. Integration testing briefly displays the program's own test windows and validates their pixels. It does not read arbitrary documents or save desktop captures.

`--quit` requests a clean exit of the native instance. The guard is started automatically; do not launch it manually.



## Validation and packaging

`language.h` selects Chinese for Chinese Windows UI language IDs and English otherwise.
`--integration-test` also checks Chinese and English controls, immediate settings
and automatic persistence. It requires an interactive desktop and briefly opens test windows.

```powershell
$test = Start-Process .\native\build\WhereIsMyCursor.exe -ArgumentList '--self-test' -Wait -PassThru
Get-Content .\native\build\native-test-result.txt
if ($test.ExitCode -ne 0) { throw 'Self-test failed' }
python .\tools\InspectNative.py .\native\build\WhereIsMyCursor.exe
python .\tools\InspectNative.py .\native\build\WhereIsMyCursor.Guard.exe
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\Package.ps1
```

Python is optional and used only for PE inspection. Run these commands from the
repository root. The recovery test deliberately exits with code 77 after hiding
the cursor, allowing the guard to restore it.

`--render-benchmark` measures foreground render call duration, not display FPS or
end-to-end latency. Old local memory/performance measurements are not guarantees.

Package outputs go to `artifacts/`. Optional `-BuildDirectory` and
`-OutputDirectory` parameters accept alternate paths. The source archive uses an
allowlist; local diagnostics, binaries and historical backups are excluded.
Both packages include the project license and third-party notices.

`tools/GenerateIcon.ps1` regenerates the transparent multi-size icon using
System.Drawing. Commit both generated files in `Assets/` after changing it.

The root `VERSION` file is the source for generated `native/build/version.h`,
EXE resources and the embedded manifest. Use build.ps1 to regenerate these files
after changing the version. Both executables carry product/file version metadata.
The package script verifies their product versions before creating versioned archives.
