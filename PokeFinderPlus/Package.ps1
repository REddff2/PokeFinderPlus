# Build two isolated add-ons. Distribute only release/PokeFinder+.
[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$ParentReleaseDirectory,
    [string]$QtDirectory = (Join-Path $PSScriptRoot '..\..\qt-sdk'),
    [string]$BuildDirectory = (Join-Path $PSScriptRoot '..\..\build-plus-parent'),
    [string]$OutputDirectory = (Join-Path $PSScriptRoot '..\..\PokeFinderPlus-package'),
    [switch]$SkipBuild
)
$ErrorActionPreference = 'Stop'
$sourceDirectory = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$ParentReleaseDirectory = (Resolve-Path -LiteralPath $ParentReleaseDirectory).Path
$QtDirectory = (Resolve-Path -LiteralPath $QtDirectory).Path
$BuildDirectory = [IO.Path]::GetFullPath($BuildDirectory)
$OutputDirectory = [IO.Path]::GetFullPath($OutputDirectory).TrimEnd('\')
function Assert-OutputChild([string]$path) {
    $full = [IO.Path]::GetFullPath($path)
    if (!$full.StartsWith($OutputDirectory + '\', [StringComparison]::OrdinalIgnoreCase)) {
        throw "Path is outside the output directory: $full"
    }
    $current = $full
    while ($current.Length -ge $OutputDirectory.Length) {
        if ((Test-Path -LiteralPath $current) -and ((Get-Item -LiteralPath $current -Force).Attributes -band [IO.FileAttributes]::ReparsePoint)) {
            throw "Refusing redirected output path: $current"
        }
        $current = [IO.Path]::GetDirectoryName($current)
    }
}
$requiredRuntime = @('PokeFinder.exe','Qt6Core.dll','Qt6Gui.dll','Qt6Widgets.dll','Qt6Network.dll','platforms\qwindows.dll')
foreach ($relative in $requiredRuntime) {
    if (!(Test-Path -LiteralPath (Join-Path $ParentReleaseDirectory $relative) -PathType Leaf)) { throw "Missing parent runtime: $relative" }
}
$sdkVersion = (Get-Item -LiteralPath (Join-Path $QtDirectory 'bin\Qt6Core.dll')).VersionInfo.FileVersion
$parentVersion = (Get-Item -LiteralPath (Join-Path $ParentReleaseDirectory 'Qt6Core.dll')).VersionInfo.FileVersion
if ($sdkVersion -ne $parentVersion) { throw "Build Qt $sdkVersion must match parent Qt $parentVersion" }
foreach ($name in @('Qt6Gui.dll','Qt6Widgets.dll','Qt6Network.dll','platforms\qwindows.dll')) {
    if ((Get-Item -LiteralPath (Join-Path $ParentReleaseDirectory $name)).VersionInfo.FileVersion -ne $parentVersion) { throw "Mixed parent Qt runtime: $name" }
}
if (!$SkipBuild) {
    if (!(Get-Command cl.exe -ErrorAction SilentlyContinue)) {
        $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
        $vs = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
        if (!$vs) { throw 'MSVC x64 build tools are required' }
        & (Join-Path $vs 'Common7\Tools\Launch-VsDevShell.ps1') -Arch amd64 -HostArch amd64 -SkipAutomaticLocation
    }
    $ninja = (Get-Command ninja.exe -ErrorAction SilentlyContinue).Source
    if (!$ninja -and (Test-Path -LiteralPath 'C:\Qt\Tools\Ninja\ninja.exe')) { $ninja = 'C:\Qt\Tools\Ninja\ninja.exe' }
    if (!$ninja) { throw 'Ninja is required for the separate packaging build' }
    & cmake -S $sourceDirectory -B $BuildDirectory -G Ninja "-DCMAKE_MAKE_PROGRAM=$ninja" "-DCMAKE_PREFIX_PATH=$QtDirectory" '-DCMAKE_BUILD_TYPE=Release'
    if ($LASTEXITCODE -ne 0) { throw 'CMake configure failed' }
    & cmake --build $BuildDirectory --target PokeFinder PokeFinderPlusLauncher Annotations --parallel 6
    if ($LASTEXITCODE -ne 0) { throw 'Build failed; existing outputs were not changed' }
}
$cache = Get-Content -LiteralPath (Join-Path $BuildDirectory 'CMakeCache.txt') -Raw
$qtConfig = [regex]::Match($cache, '(?m)^Qt6_DIR:PATH=(.+)$').Groups[1].Value.Trim()
if ([IO.Path]::GetFullPath($qtConfig) -ne [IO.Path]::GetFullPath((Join-Path $QtDirectory 'lib\cmake\Qt6'))) {
    throw 'The build cache does not use the requested parent-compatible Qt SDK'
}
foreach ($relative in @('PokeFinder+.exe','PokeFinderPlusApp.exe','plugins\Annotations.dll')) {
    if (!(Test-Path -LiteralPath (Join-Path $BuildDirectory $relative) -PathType Leaf)) { throw "Missing build output: $relative" }
}
New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
$generation = [DateTime]::Now.ToString('yyyyMMdd-HHmmss') + '-' + [guid]::NewGuid().ToString('N').Substring(0,8)
$stage = Join-Path $OutputDirectory ('.stage-' + $generation)
Assert-OutputChild $stage
# Never copy a plugins directory or an existing package into the clean output.
foreach ($flavor in @('release','test')) {
    $addon = Join-Path $stage "$flavor\PokeFinder+"
    New-Item -ItemType Directory -Path (Join-Path $addon 'plugins'),(Join-Path $addon 'data') -Force | Out-Null
    foreach ($file in @('PokeFinder+.exe','PokeFinderPlusApp.exe')) {
        Copy-Item -LiteralPath (Join-Path $BuildDirectory $file) -Destination (Join-Path $addon $file)
    }
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'qt.conf') -Destination (Join-Path $addon 'qt.conf')
    [IO.File]::WriteAllText((Join-Path $addon 'plugins.ini'), '')
    if ($flavor -eq 'test') {
        Copy-Item -LiteralPath (Join-Path $BuildDirectory 'plugins\Annotations.dll') -Destination (Join-Path $addon 'plugins\Annotations.dll')
        [IO.File]::WriteAllText((Join-Path $addon 'plugins.ini'), "[diagnostics]`nlogging=true`n`n[plugins]`nAnnotations.dll\enabled=true`n")
    }
}
$clean = Join-Path $stage 'release\PokeFinder+'
$actual = @(Get-ChildItem -LiteralPath $clean -Force | ForEach-Object Name | Sort-Object)
$expected = @('PokeFinder+.exe','PokeFinderPlusApp.exe','qt.conf','plugins','plugins.ini','data') | Sort-Object
if (Compare-Object $actual $expected) { throw 'Clean package contains unexpected entries' }
if (@(Get-ChildItem -LiteralPath (Join-Path $clean 'plugins') -Force).Count -ne 0) { throw 'Clean plugins directory is not empty' }
if (@(Get-ChildItem -LiteralPath (Join-Path $clean 'data') -Force).Count -ne 0) { throw 'Clean data directory is not empty' }
if (@(Get-ChildItem -LiteralPath $clean -Recurse -File -Filter '*.dll').Count -ne 0) { throw 'Clean add-on must contain no DLLs' }
# Provision normal-release host files OUTSIDE each add-on for local launch checks.
$hostFiles = @('PokeFinder.exe','Qt6Core.dll','Qt6Gui.dll','Qt6Widgets.dll','Qt6Network.dll','Qt6Svg.dll','icuuc.dll','dxcompiler.dll','dxil.dll','libcrypto-3-x64.dll','libssl-3-x64.dll')
foreach ($flavor in @('release','test')) {
    $hostDirectory = Join-Path $OutputDirectory $flavor
    Assert-OutputChild $hostDirectory
    New-Item -ItemType Directory -Path $hostDirectory -Force | Out-Null
    foreach ($file in $hostFiles) {
        $from = Join-Path $ParentReleaseDirectory $file
        if (Test-Path -LiteralPath $from -PathType Leaf) {
            $to = Join-Path $hostDirectory $file
            Assert-OutputChild $to
            if (!(Test-Path -LiteralPath $to) -or (Get-FileHash -LiteralPath $from).Hash -ne (Get-FileHash -LiteralPath $to).Hash) {
                Copy-Item -LiteralPath $from -Destination $to
            }
        }
    }
    foreach ($directory in @('platforms','styles','tls','imageformats','iconengines')) {
        $from = Join-Path $ParentReleaseDirectory $directory
        if (Test-Path -LiteralPath $from -PathType Container) {
            $to = Join-Path $hostDirectory $directory
            Assert-OutputChild $to
            New-Item -ItemType Directory -Path $to -Force | Out-Null
            foreach ($file in Get-ChildItem -LiteralPath $from -File) {
                $destination = Join-Path $to $file.Name
                Assert-OutputChild $destination
                if (!(Test-Path -LiteralPath $destination) -or (Get-FileHash -LiteralPath $file.FullName).Hash -ne (Get-FileHash -LiteralPath $destination).Hash) {
                    Copy-Item -LiteralPath $file.FullName -Destination $destination
                }
            }
        }
    }
}
# Preserve previous generated add-ons, including any test plugins/data, before replacement.
foreach ($flavor in @('release','test')) {
    $destination = Join-Path $OutputDirectory "$flavor\PokeFinder+"
    $prepared = Join-Path $stage "$flavor\PokeFinder+"
    Assert-OutputChild $destination
    Assert-OutputChild $prepared
    if (Test-Path -LiteralPath $destination) {
        $archive = Join-Path $OutputDirectory "archive\$generation\$flavor"
        Assert-OutputChild $archive
        New-Item -ItemType Directory -Path $archive -Force | Out-Null
        Move-Item -LiteralPath $destination -Destination (Join-Path $archive 'PokeFinder+')
    }
    Move-Item -LiteralPath $prepared -Destination $destination
    Remove-Item -LiteralPath (Join-Path $stage $flavor)
    Write-Host "$flavor : $destination"
}
Remove-Item -LiteralPath $stage
Write-Host "Parent runtime: Qt $parentVersion. Distribute only release\PokeFinder+; parent host files are outside the add-on."
