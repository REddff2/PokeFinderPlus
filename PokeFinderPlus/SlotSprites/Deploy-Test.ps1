param([switch]$VerifyHost, [ValidateSet('online','reuse','missing','cancel')][string]$NetworkTest, [switch]$FreshAssets)
$ErrorActionPreference = 'Stop'
$testRoot = 'C:\PokeFinderDev\PokeFinderPlus-package\test\PokeFinder+'
$runtimePath = Join-Path $testRoot 'PokeFinderPlusApp.exe'
$running = Get-Process -Name PokeFinderPlusApp -ErrorAction SilentlyContinue | Where-Object { $_.Path -eq $runtimePath }
if ($running) { throw 'The TEST application is running. Close it before deploying.' }
foreach ($name in @('SlotSprites.dll')) {
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot "build/$name") -Destination (Join-Path $testRoot "plugins/$name") -Force
}
$dataRoot = Join-Path $testRoot 'data/SlotSprites'
if ($FreshAssets) {
    if (!$NetworkTest) { throw 'FreshAssets is only for an explicit network verification run.' }
    $resolvedData = [IO.Path]::GetFullPath($dataRoot)
    if ($resolvedData -ne 'C:\PokeFinderDev\PokeFinderPlus-package\test\PokeFinder+\data\SlotSprites') { throw 'Unexpected TEST asset path.' }
    if (Test-Path -LiteralPath $resolvedData) {
        $linked = Get-ChildItem -LiteralPath $resolvedData -Recurse -Force | Where-Object { $_.Attributes -band [IO.FileAttributes]::ReparsePoint }
        if ($linked -or ((Get-Item -LiteralPath $resolvedData).Attributes -band [IO.FileAttributes]::ReparsePoint)) { throw 'Refusing linked asset paths.' }
        Remove-Item -LiteralPath $resolvedData -Recurse -Force
    }
}
if (!$VerifyHost) { Write-Output "Deployed Slot Sprites to TEST: $testRoot"; exit 0 }
$iniPath = Join-Path $testRoot 'plugins.ini'
$iniExisted = Test-Path -LiteralPath $iniPath
$iniBytes = if ($iniExisted) { [IO.File]::ReadAllBytes($iniPath) } else { $null }
$probePath = Join-Path $testRoot 'plugins/SlotSpritesHostProbe.dll'
$networkProbe = Join-Path $testRoot 'plugins/AA_SlotSpritesNetworkProbe.dll'
$process = $null
$profileKey = 'HKCU:\Software\PokeFinder Team\PokeFinder\settings'
$originalProfilePath = (Get-ItemProperty -LiteralPath $profileKey).profiles
try {
    # Only the TEST process reads this fixture; restore the setting even on failure.
    if (Get-Process -Name PokeFinder,PokeFinderPlusApp -ErrorAction SilentlyContinue) { throw 'Close other PokeFinder processes before isolated profile verification.' }
    $fixture = @{gen3=@();gen4=@();gen5=@();gen8=@()}
    foreach($generation in @(@('gen3',0,1,2,3,4,5,6),@('gen4',7,8,9,10,11),@('gen5',12,13,14,15),@('gen8',24,25,26,27))) {
        foreach($bit in $generation[1..($generation.Count-1)]) { $fixture[$generation[0]] += @{name="TEST version $bit";version=(1 -shl $bit);tid=12345;sid=54321} }
    }
    New-Item -ItemType Directory -Force -Path "$dataRoot/verification" | Out-Null
    $fixturePath = "$dataRoot/verification/profiles.json"
    [IO.File]::WriteAllText($fixturePath,($fixture | ConvertTo-Json -Depth 5))
    Set-ItemProperty -LiteralPath $profileKey -Name profiles -Value $fixturePath
    if ($NetworkTest -notin @('missing','cancel')) { Copy-Item -LiteralPath "$PSScriptRoot/build/SlotSpritesHostProbe.dll" -Destination $probePath }
    if ($NetworkTest) {
        Copy-Item -LiteralPath "$PSScriptRoot/build/SlotSpritesNetworkProbe.dll" -Destination $networkProbe
        $env:SLOT_SPRITES_NETWORK_TEST = $NetworkTest
    }
    # Temporary test settings, restored byte-for-byte after the app exits.
    [IO.File]::WriteAllText($iniPath, "[plugins]`nSlotSprites.dll\enabled=true`nAnnotations.dll\enabled=false`nIVTotal.dll\enabled=false`n")
    $env:SLOT_SPRITES_VERIFY_HOST = '1'
    $env:QT_QPA_PLATFORM_PLUGIN_PATH = 'C:\PokeFinderDev\qt-sdk\plugins\platforms'
    $env:QT_QPA_PLATFORM = 'offscreen'
    # Application windows/settings used for verification stay isolated from the user's normal session.
    $process = Start-Process -FilePath (Join-Path $testRoot 'PokeFinder+.exe') -WorkingDirectory $testRoot -WindowStyle Hidden -PassThru
    if (!$process.WaitForExit(240000)) { throw 'TEST probe did not finish within 240 seconds.' }
    Write-Output "TEST_APPLICATION_EXIT_CODE=$($process.ExitCode)"
    if ($NetworkTest -notin @('missing','cancel')) { Get-Content -LiteralPath "$dataRoot/verification/host-probe.log" }
    if ($NetworkTest) {
        $result = Get-Content -LiteralPath "$dataRoot/verification/network-$NetworkTest.log"
        Write-Output $result
        if ($result -notmatch '^PASS ') { throw 'Network verification failed.' }
        $saved = Join-Path $PSScriptRoot "verification/self-install/$NetworkTest"
        New-Item -ItemType Directory -Force -Path $saved | Out-Null
        Copy-Item -Path "$dataRoot/verification/*" -Destination $saved -Recurse -Force
    }
    if ($process.ExitCode -ne 0) { throw 'TEST application verification failed.' }
} finally {
    Set-ItemProperty -LiteralPath $profileKey -Name profiles -Value $originalProfilePath
    if ($iniExisted) { [IO.File]::WriteAllBytes($iniPath, [byte[]]$iniBytes) }
    elseif (Test-Path -LiteralPath $iniPath) { Remove-Item -LiteralPath $iniPath }
    if (!$process -or $process.HasExited) {
        # Exact temporary DLL path only; never recursively delete or remove another plugin.
        if (Test-Path -LiteralPath $probePath) { Remove-Item -LiteralPath $probePath }
        if (Test-Path -LiteralPath $networkProbe) { Remove-Item -LiteralPath $networkProbe }
    }
    Remove-Item Env:SLOT_SPRITES_NETWORK_TEST -ErrorAction SilentlyContinue
}
