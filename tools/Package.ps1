param([string]$BuildDirectory,[string]$OutputDirectory)
$ErrorActionPreference='Stop'
$projectRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$version=(Get-Content -LiteralPath (Join-Path $projectRoot 'VERSION') -Raw).Trim()
if($version -notmatch '^\d+\.\d+\.\d+$'){throw 'Invalid VERSION'}
if(!$BuildDirectory){$BuildDirectory=Join-Path $projectRoot 'native\build'}
if(!$OutputDirectory){$OutputDirectory=Join-Path $projectRoot 'artifacts'}
$BuildDirectory=[IO.Path]::GetFullPath($BuildDirectory)
$OutputDirectory=[IO.Path]::GetFullPath($OutputDirectory)
New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem

function New-Package([string]$Destination,[System.Collections.IDictionary]$Entries){
    foreach($source in $Entries.Values){if(!(Test-Path -LiteralPath $source -PathType Leaf)){throw "Missing package input: $source"}}
    $temporary=$Destination+'.'+[Guid]::NewGuid().ToString('N')+'.tmp'
    try {
        $stream=[IO.File]::Open($temporary,[IO.FileMode]::CreateNew)
        try {
            $zip=New-Object IO.Compression.ZipArchive($stream,[IO.Compression.ZipArchiveMode]::Create,$true)
            try {
                foreach($entry in $Entries.GetEnumerator()){
                    [IO.Compression.ZipFileExtensions]::CreateEntryFromFile($zip,$entry.Value,$entry.Key.Replace('\','/'),[IO.Compression.CompressionLevel]::Optimal) | Out-Null
                }
            } finally {$zip.Dispose()}
        } finally {$stream.Dispose()}
        Move-Item -LiteralPath $temporary -Destination $Destination -Force
    } finally {if(Test-Path -LiteralPath $temporary){Remove-Item -LiteralPath $temporary -Force}}
}

$common=@('README.md','README.en.md','LICENSE','THIRD_PARTY_NOTICES.txt','VERSION','CHANGELOG.md')
$portable=[ordered]@{}
foreach($name in @('WhereIsMyCursor.exe','WhereIsMyCursor.Guard.exe')){
    $path=Join-Path $BuildDirectory $name
    if(!(Test-Path -LiteralPath $path)){throw "Missing binary: $path"}
    if([Diagnostics.FileVersionInfo]::GetVersionInfo($path).ProductVersion -ne $version){throw "Rebuild $name for version $version before packaging"}
    $portable[$name]=$path
}
foreach($name in $common){$portable[$name]=Join-Path $projectRoot $name}
foreach($file in Get-ChildItem -LiteralPath (Join-Path $projectRoot 'licenses') -File){$portable['licenses/'+$file.Name]=$file.FullName}

# Explicit source allowlist excludes build output, diagnostics and backups.
$source=[ordered]@{}
foreach($name in ($common+@('.gitignore','.gitattributes'))){$source[$name]=Join-Path $projectRoot $name}
foreach($folder in @('native','Assets','tools','licenses','docs')){
    foreach($file in Get-ChildItem -LiteralPath (Join-Path $projectRoot $folder) -File){
        $include=switch($folder){
            'native' {$file.Extension -in @('.cpp','.c','.h','.ps1','.rc','.manifest','.md')}
            'Assets' {$file.Extension -in @('.png','.ico','.gif')}
            'tools' {$file.Extension -in @('.ps1','.py')}
            'licenses' {$file.Extension -eq '.txt'}
            'docs' {$file.Extension -eq '.md'}
        }
        if($include){$source[$folder+'/'+$file.Name]=$file.FullName}
    }
}
$portablePath=Join-Path $OutputDirectory "WhereIsMyCursor-v$version-win-x64.zip"
$sourcePath=Join-Path $OutputDirectory "WhereIsMyCursor-v$version-source.zip"
New-Package $portablePath $portable
New-Package $sourcePath $source
Copy-Item -LiteralPath $portablePath -Destination (Join-Path $OutputDirectory 'WhereIsMyCursor-win-x64.zip') -Force
Copy-Item -LiteralPath $sourcePath -Destination (Join-Path $OutputDirectory 'WhereIsMyCursor-source.zip') -Force
$checksums=foreach($path in @($portablePath,$sourcePath)){
    $hash=Get-FileHash -LiteralPath $path -Algorithm SHA256
    $hash.Hash.ToLowerInvariant()+'  '+[IO.Path]::GetFileName($path)
}
[IO.File]::WriteAllLines((Join-Path $OutputDirectory 'SHA256SUMS.txt'),[string[]]$checksums,[Text.Encoding]::ASCII)
Get-Item -LiteralPath $portablePath,$sourcePath | Select-Object FullName,Length
