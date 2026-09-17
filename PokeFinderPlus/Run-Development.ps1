# Runs the source-built app directly with the matching development Qt runtime.
param(
    [string]$BuildDirectory = (Join-Path $PSScriptRoot '..\..\build-plus'),
    [string]$QtDirectory = 'C:\Qt\6.11.2\msvc2022_64',
    [switch]$DebugPlugins
)
$ErrorActionPreference = 'Stop'
$BuildDirectory = (Resolve-Path -LiteralPath $BuildDirectory).Path
$QtDirectory = (Resolve-Path -LiteralPath $QtDirectory).Path
$appPath = Join-Path $BuildDirectory 'PokeFinderPlusApp.exe'
foreach ($required in @($appPath, (Join-Path $QtDirectory 'bin\Qt6Core.dll'), (Join-Path $QtDirectory 'plugins\platforms\qwindows.dll'))) {
    if (!(Test-Path -LiteralPath $required)) { throw "Missing development runtime file: $required" }
}
$version = (Get-Item -LiteralPath (Join-Path $QtDirectory 'bin\Qt6Core.dll')).VersionInfo.FileVersion
if ($version -ne '6.11.2.0') { throw "This build requires Qt 6.11.2; found $version" }
$startInfo = New-Object System.Diagnostics.ProcessStartInfo
$startInfo.FileName = $appPath
$startInfo.WorkingDirectory = $BuildDirectory
$startInfo.UseShellExecute = $false
$startInfo.EnvironmentVariables['PATH'] = "$QtDirectory\bin;$env:SystemRoot\System32;$env:SystemRoot"
$startInfo.EnvironmentVariables['QT_PLUGIN_PATH'] = "$QtDirectory\plugins"
$startInfo.EnvironmentVariables['QT_QPA_PLATFORM_PLUGIN_PATH'] = "$QtDirectory\plugins\platforms"
$startInfo.EnvironmentVariables.Remove('QT_LOGGING_TO_CONSOLE')
$startInfo.EnvironmentVariables['QT_DEBUG_PLUGINS'] = $(if ($DebugPlugins) { '1' } else { '0' })
$startInfo.EnvironmentVariables['QT_FORCE_STDERR_LOGGING'] = '1'
$app = [System.Diagnostics.Process]::Start($startInfo)
Write-Host "Started $appPath (PID $($app.Id)) with Qt $version"
$app.WaitForExit()
$code = $app.ExitCode
Write-Host ('PokeFinderPlusApp exit code: {0} (0x{0:X8})' -f $code)
$app.Dispose()
exit $code
