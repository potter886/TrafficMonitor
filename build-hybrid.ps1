param([switch]$Test)
$ErrorActionPreference = 'Stop'
$repo = $PSScriptRoot
$cache = Join-Path $repo '.deps'
$sensorDir = Join-Path $cache 'LibreHardwareMonitor'
$out = Join-Path $repo 'Bin\x64\Release'
$null = New-Item -ItemType Directory -Path $cache,$out,(Join-Path $out 'plugins') -Force

function Get-VerifiedFile([string]$Url, [string]$Path, [string]$Hash) {
    if (-not (Test-Path -LiteralPath $Path) -or (Get-FileHash -LiteralPath $Path).Hash -ne $Hash) {
        Invoke-WebRequest -Uri $Url -OutFile $Path
    }
    if ((Get-FileHash -LiteralPath $Path).Hash -ne $Hash) { throw "Hash mismatch: $Path" }
}
$archive = Join-Path $cache 'LibreHardwareMonitor-0.9.6.zip'
Get-VerifiedFile 'https://github.com/LibreHardwareMonitor/LibreHardwareMonitor/releases/download/v0.9.6/LibreHardwareMonitor.zip' $archive '086D9F1B5A99E643EDC2CFAAAC16051685B551E4C5AC0B32A57C58C0E529C001'
Expand-Archive -LiteralPath $archive -DestinationPath $sensorDir -Force

$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path -LiteralPath $vswhere)) { throw 'Visual Studio 2022 or newer is required.' }
$installations = @(& $vswhere -all -products '*' -format json | ConvertFrom-Json)
$chosen = $null
foreach ($installation in ($installations | Sort-Object { [version]$_.installationVersion } -Descending)) {
    $toolsRoot = Join-Path $installation.installationPath 'VC\Tools\MSVC'
    foreach ($tools in @(Get-ChildItem -LiteralPath $toolsRoot -Directory -ErrorAction SilentlyContinue | Sort-Object { [version]$_.Name } -Descending)) {
        if (([version]$tools.Name).Minor -ge 30 -and (Test-Path -LiteralPath (Join-Path $tools.FullName 'atlmfc\include\afxwin.h'))) {
            $chosen = @{Installation=$installation.installationPath; Tools=$tools}
            break
        }
    }
    if ($chosen) { break }
}
if (-not $chosen) { throw 'Install the MSVC MFC libraries and C++/CLI support for the same toolset.' }
$toolset = if (([version]$chosen.Tools.Name).Minor -ge 50) { 'v145' } else { 'v143' }
$msbuild = Join-Path $chosen.Installation 'MSBuild\Current\Bin\MSBuild.exe'
& $msbuild (Join-Path $repo 'TrafficMonitor.sln') /t:TrafficMonitor /m:4 '/p:Configuration=Release' /p:Platform=x64 "/p:PlatformToolset=$toolset" "/p:VCToolsVersion=$($chosen.Tools.Name)" /nologo /v:minimal
if ($LASTEXITCODE) { throw 'TrafficMonitor build failed.' }

$runtimeFiles = @('LibreHardwareMonitorLib.dll','BlackSharp.Core.dll','DiskInfoToolkit.dll','HidSharp.dll',
    'RAMSPDToolkit-NDD.dll','System.Buffers.dll','System.Memory.dll','System.Numerics.Vectors.dll','System.Runtime.CompilerServices.Unsafe.dll')
foreach ($name in $runtimeFiles) { Copy-Item -LiteralPath (Join-Path $sensorDir $name) -Destination (Join-Path $out $name) -Force }
Copy-Item -LiteralPath (Join-Path $sensorDir 'LibreHardwareMonitor.exe.config') -Destination (Join-Path $out 'TrafficMonitor.exe.config') -Force

$vcvars = Join-Path $chosen.Installation 'VC\Auxiliary\Build\vcvars64.bat'
$pluginBuild = Join-Path $cache 'build-plugin.cmd'
$commands = @('@echo off', 'chcp 65001 >nul', ('call "{0}" -vcvars_ver={1}' -f $vcvars, $chosen.Tools.Name), 'if errorlevel 1 exit /b %errorlevel%',
    ('cd /d "{0}"' -f $repo),
    ('cl /nologo /std:c++20 /EHsc /utf-8 /W4 /MT /O2 /LD HybridFrequencyPlugin.cpp /Fo:"{0}\HybridFrequencyPlugin.obj" /Fe:"{0}\plugins\HybridCpuFrequency.dll" /link pdh.lib powrprof.lib' -f $out),
    'if errorlevel 1 exit /b %errorlevel%')
if ($Test) {
    $commands += @(
        ('cl /nologo /std:c++20 /EHsc /utf-8 /W4 /MT /O2 /DHYBRID_FREQUENCY_TEST HybridFrequencyPlugin.cpp /Fo:"{0}\HybridFrequencyTest.obj" /Fe:"{0}\HybridFrequencyTest.exe" /link pdh.lib powrprof.lib' -f $out),
        'if errorlevel 1 exit /b %errorlevel%',
        ('cl /nologo /std:c++20 /EHsc /utf-8 /W4 /MT /O2 tests\NetworkSpeedFormatTest.cpp /Fo:"{0}\NetworkSpeedFormatTest.obj" /Fe:"{0}\NetworkSpeedFormatTest.exe"' -f $out),
        'if errorlevel 1 exit /b %errorlevel%')
}
$commands += 'exit /b 0'
[IO.File]::WriteAllLines($pluginBuild, $commands, [Text.UTF8Encoding]::new($false))
& cmd.exe /d /c $pluginBuild
if ($LASTEXITCODE) { throw 'Plugin or test build failed.' }

$pawnSetup = Join-Path $out 'PawnIO_setup.exe'
Get-VerifiedFile 'https://raw.githubusercontent.com/LibreHardwareMonitor/LibreHardwareMonitor/v0.9.6/LibreHardwareMonitor/Resources/PawnIO_setup.exe' $pawnSetup 'A3A46226C5E2824F4CDD42BE0EECBABFC672C86F7889710F5AB1E6AD385B47A0'
if ((Get-AuthenticodeSignature -LiteralPath $pawnSetup).Status -ne 'Valid') { throw 'PawnIO setup signature is invalid.' }
if ($Test) {
    & (Join-Path $out 'NetworkSpeedFormatTest.exe')
    if ($LASTEXITCODE) { throw 'Network format regression test failed.' }
    & (Join-Path $out 'HybridFrequencyTest.exe')
    if ($LASTEXITCODE) { throw 'Frequency regression/live test failed. Multiple processor groups are unsupported.' }
}
Write-Output "Build complete: $out"
Write-Output 'Install PawnIO_setup.exe before using CPU temperature monitoring. The build does not install it.'
