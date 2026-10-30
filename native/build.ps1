param([string]$Zig)
$ErrorActionPreference='Stop'
$nativeRoot=$PSScriptRoot
$projectRoot=Split-Path $nativeRoot -Parent
if(!$Zig){
    $command=Get-Command zig -ErrorAction SilentlyContinue
    if($command){$Zig=$command.Source}
    else {
        $toolRoot=Join-Path (Split-Path $projectRoot -Parent) '.toolchains'
        $candidate=Get-ChildItem $toolRoot -Filter zig.exe -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1
        if($candidate){$Zig=$candidate.FullName}
    }
}
if(!$Zig -or !(Test-Path -LiteralPath $Zig)){throw 'Pass -Zig with the path to zig.exe (Zig 0.14.1 tested).'}
$build=Join-Path $nativeRoot 'build'
New-Item -ItemType Directory -Path $build -Force | Out-Null
$version=(Get-Content -LiteralPath (Join-Path $projectRoot 'VERSION') -Raw).Trim()
if($version -notmatch '^(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)$'){throw 'VERSION must contain major.minor.patch'}
$parts=$version.Split('.')
foreach($part in $parts){if([double]$part -gt 65535){throw 'Version components must be at most 65535'}}
$header=@"
#pragma once
#define APP_VERSION_NUM $($parts -join ','),0
#define APP_VERSION_STR "$version"
#define APP_VERSION_W L"$version"
"@
[IO.File]::WriteAllText((Join-Path $build 'version.h'),$header,[Text.Encoding]::ASCII)
$manifest=Get-Content -LiteralPath (Join-Path $nativeRoot 'app.manifest') -Raw
$manifest=$manifest.Replace('version="1.0.0.0" name="ShakeCursor"',('version="'+$version+'.0" name="WhereIsMyCursor"'))
[IO.File]::WriteAllText((Join-Path $build 'app.manifest'),$manifest,[Text.Encoding]::UTF8)
Push-Location $nativeRoot
try {
    & $Zig rc /fo (Join-Path $build 'app.res') app.rc
    if($LASTEXITCODE -ne 0){throw 'Resource compilation failed'}
    & $Zig rc /fo (Join-Path $build 'guard.res') guard.rc
    if($LASTEXITCODE -ne 0){throw 'Guard resource compilation failed'}
    & $Zig cc -target x86_64-windows-gnu -O2 -municode '-Wl,--subsystem,windows' -static -s guard.c (Join-Path $build 'guard.res') -o (Join-Path $build 'WhereIsMyCursor.Guard.exe')
    if($LASTEXITCODE -ne 0){throw 'Guard compilation failed'}
    & $Zig c++ -target x86_64-windows-gnu -std=c++17 -O2 -DNDEBUG -DUNICODE -D_UNICODE -DWINVER=0x0A00 -municode '-Wl,--subsystem,windows' -static -s app.cpp core.cpp render.cpp async_inverse.cpp tests.cpp (Join-Path $build 'app.res') -luser32 -lgdi32 -lcomctl32 -lshell32 -lole32 -ladvapi32 -ldwmapi -lwinmm -o (Join-Path $build 'WhereIsMyCursor.exe')
    if($LASTEXITCODE -ne 0){throw 'Native compilation failed'}
    Get-Item (Join-Path $build 'WhereIsMyCursor.exe') | Select-Object FullName,Length
}
finally {Pop-Location}
