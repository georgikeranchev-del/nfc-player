$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path $vswhere)) { throw 'Install Visual Studio C++ build tools and Windows SDK.' }
$compiler = & $vswhere -latest -products * -find 'VC\Tools\MSVC\**\bin\Hostx64\x64\cl.exe' | Select-Object -Last 1
if (-not $compiler) { throw 'MSVC compiler not found.' }
$msvc = $compiler -replace '\\bin\\Hostx64\\x64\\cl.exe$', ''
$sdkRoot = Join-Path ${env:ProgramFiles(x86)} 'Windows Kits\10'
$sdk = Get-ChildItem (Join-Path $sdkRoot 'Include') -Directory |
    Where-Object { Test-Path (Join-Path $_.FullName 'ucrt') } |
    Sort-Object Name -Descending | Select-Object -First 1
if (-not $sdk) { throw 'Windows SDK not found.' }
$sdkLib = Join-Path (Join-Path $sdkRoot 'Lib') $sdk.Name
$tempDir = Join-Path ([IO.Path]::GetTempPath()) ('nfc-hal-tests-' + [guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($tempDir)
$oldInclude = $env:INCLUDE
$oldLib = $env:LIB
try {
    $env:INCLUDE = "$msvc\include;$($sdk.FullName)\ucrt;$($sdk.FullName)\shared;$($sdk.FullName)\um"
    $env:LIB = "$msvc\lib\x64;$sdkLib\ucrt\x64;$sdkLib\um\x64"
    $firmware = Join-Path $root 'firmware\nfc_player'
    & $compiler /nologo /EHsc /std:c++14 /W4 /WX /D_CRT_SECURE_NO_WARNINGS "/I$firmware" `
        (Join-Path $root 'tests\control_tests.cpp') (Join-Path $firmware 'player.cpp') `
        "/Fo:$tempDir\" "/Fe:$tempDir\control-tests.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Host compilation failed.' }
    & "$tempDir\control-tests.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Control tests failed.' }
} finally {
    $env:INCLUDE = $oldInclude
    $env:LIB = $oldLib
    Remove-Item -LiteralPath $tempDir -Recurse -Force
}