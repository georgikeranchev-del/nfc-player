$ErrorActionPreference = 'Stop'

$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path $vswhere)) { throw 'Visual Studio C++ build tools are required.' }
$compiler = & $vswhere -latest -products * -find 'VC\Tools\MSVC\**\bin\Hostx64\x64\cl.exe' |
    Select-Object -Last 1
if (-not $compiler) { throw 'MSVC x64 compiler not found.' }
$msvc = $compiler -replace '\\bin\\Hostx64\\x64\\cl.exe$', ''
$sdkRoot = Join-Path ${env:ProgramFiles(x86)} 'Windows Kits\10'
$sdk = Get-ChildItem (Join-Path $sdkRoot 'Include') -Directory |
    Where-Object { Test-Path (Join-Path $_.FullName 'ucrt') } |
    Sort-Object Name -Descending | Select-Object -First 1
if (-not $sdk) { throw 'Windows SDK not found.' }
$sdkLib = Join-Path (Join-Path $sdkRoot 'Lib') $sdk.Name
$tempDir = Join-Path ([IO.Path]::GetTempPath()) ('nfc-player-tests-' + [guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($tempDir)

$sourcePath = Join-Path (Split-Path $PSScriptRoot -Parent) 'nfc_player.cpp'
$source = Get-Content -LiteralPath $sourcePath -Raw -Encoding UTF8
$source = [regex]::Replace($source, '(?m)^#include[^\r\n]*', '')
[IO.File]::WriteAllText((Join-Path $tempDir 'nfc_player_under_test.inc'), $source)
$testSource = Join-Path $PSScriptRoot 'nfc_player_control_tests.cpp'

$oldInclude = $env:INCLUDE
$oldLib = $env:LIB
try {
    $env:INCLUDE = "$msvc\include;$($sdk.FullName)\ucrt;$($sdk.FullName)\shared;$($sdk.FullName)\um"
    $env:LIB = "$msvc\lib\x64;$sdkLib\ucrt\x64;$sdkLib\um\x64"
    foreach ($major in @(2, 3)) {
        $exe = Join-Path $tempDir "control-tests-$major.exe"
        $obj = Join-Path $tempDir "control-tests-$major.obj"
        & $compiler /nologo /EHsc /std:c++17 /W3 /D_CRT_SECURE_NO_WARNINGS `
            "/DESP_ARDUINO_VERSION_MAJOR=$major" "/I$tempDir" $testSource "/Fo:$obj" "/Fe:$exe"
        if ($LASTEXITCODE -ne 0) { throw "Host compile failed for API $major" }
        & $exe
        if ($LASTEXITCODE -ne 0) { throw "Control tests failed for API $major" }
    }
} finally {
    $env:INCLUDE = $oldInclude
    $env:LIB = $oldLib
    Remove-Item -LiteralPath $tempDir -Recurse -Force
}