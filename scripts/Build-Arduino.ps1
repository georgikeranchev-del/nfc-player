param([switch]$Install, [string]$Cli = 'arduino-cli')
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
if (-not (Get-Command $Cli -ErrorAction SilentlyContinue)) {
    $bundledCliPaths = @()
    if ($env:ProgramFiles) {
        $bundledCliPaths += Join-Path $env:ProgramFiles 'Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe'
    }
    if ($env:LOCALAPPDATA) {
        $bundledCliPaths += Join-Path $env:LOCALAPPDATA 'Programs\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe'
    }
    $bundledCli = $bundledCliPaths | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
    if ($bundledCli) { $Cli = $bundledCli } else { throw 'arduino-cli not found.' }
}
$build = Get-Content -LiteralPath (Join-Path $root 'arduino-build.json') -Raw | ConvertFrom-Json
function Invoke-Arduino([string[]]$Arguments) {
    & $Cli @Arguments
    if ($LASTEXITCODE -ne 0) { throw "arduino-cli failed: $($Arguments -join ' ')" }
}
$librariesPath = Join-Path $root 'libraries'
if ($Install) {
    New-Item -ItemType Directory -Path $librariesPath -Force | Out-Null
    $previousUserDirectory = $env:ARDUINO_DIRECTORIES_USER
    try {
        $env:ARDUINO_DIRECTORIES_USER = $root
        Invoke-Arduino @('core', 'update-index', '--additional-urls', $build.board_url)
        Invoke-Arduino @('core', 'install', $build.core, '--additional-urls', $build.board_url)
        foreach ($library in $build.libraries) { Invoke-Arduino @('lib', 'install', $library, '--no-deps') }
    }
    finally {
        if ($null -eq $previousUserDirectory) {
            Remove-Item Env:ARDUINO_DIRECTORIES_USER -ErrorAction SilentlyContinue
        }
        else {
            $env:ARDUINO_DIRECTORIES_USER = $previousUserDirectory
        }
    }
}
Invoke-Arduino @('compile', '--fqbn', $build.fqbn, '--libraries', $librariesPath, (Join-Path $root 'firmware\nfc_player'))