param([switch]$Install, [string]$Cli = 'arduino-cli')
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
if (-not (Get-Command $Cli -ErrorAction SilentlyContinue)) {
    $bundled = Join-Path $env:ProgramFiles 'Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe'
    if (Test-Path $bundled) { $Cli = $bundled } else { throw 'arduino-cli not found.' }
}
$build = Get-Content -LiteralPath (Join-Path $root 'arduino-build.json') -Raw | ConvertFrom-Json
function Invoke-Arduino([string[]]$Arguments) {
    & $Cli @Arguments
    if ($LASTEXITCODE -ne 0) { throw "arduino-cli failed: $($Arguments -join ' ')" }
}
if ($Install) {
    Invoke-Arduino @('core', 'update-index', '--additional-urls', $build.board_url)
    Invoke-Arduino @('core', 'install', $build.core, '--additional-urls', $build.board_url)
    foreach ($library in $build.libraries) { Invoke-Arduino @('lib', 'install', $library, '--no-deps') }
}
Invoke-Arduino @('compile', '--fqbn', $build.fqbn, (Join-Path $root 'firmware\nfc_player'))