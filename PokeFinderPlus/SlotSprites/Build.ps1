param([string]$BuildDirectory = "$PSScriptRoot/build")
$ErrorActionPreference = 'Stop'
$devCommand = '"C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 >nul && set'
& cmd.exe /d /s /c $devCommand | ForEach-Object {
    if ($_ -match '^([^=]+)=(.*)$') { [Environment]::SetEnvironmentVariable($matches[1], $matches[2], 'Process') }
}
& 'C:\Program Files\CMake\bin\cmake.exe' -S $PSScriptRoot -B $BuildDirectory -G Ninja '-DCMAKE_BUILD_TYPE=Release' '-DCMAKE_PREFIX_PATH=C:/PokeFinderDev/qt-sdk'
if ($LASTEXITCODE) { throw 'CMake configure failed' }
& 'C:\Program Files\CMake\bin\cmake.exe' --build $BuildDirectory --parallel 4
if ($LASTEXITCODE) { throw 'Build failed' }
