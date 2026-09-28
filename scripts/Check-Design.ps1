param([string]$Root = (Split-Path $PSScriptRoot -Parent))
$ErrorActionPreference = 'Stop'
$pinText = Get-Content -LiteralPath (Join-Path $Root 'firmware\nfc_player\pins.h') -Raw
$pins = @{}
foreach ($match in [regex]::Matches($pinText, 'constexpr\s+uint8_t\s+(\w+)\s*=\s*(\d+)\s*;')) {
    $symbol = $match.Groups[1].Value
    if ($pins.ContainsKey($symbol)) { throw "Duplicate pin symbol: $symbol" }
    $pins[$symbol] = [int]$match.Groups[2].Value
}
if ($pins.Count -eq 0) { throw 'No literal constexpr GPIO definitions found in pins.h.' }
$gpioOwners = @{}
foreach ($symbol in $pins.Keys) {
    $gpio = $pins[$symbol]
    if ($gpio -eq 12) { throw 'GPIO12 is prohibited.' }
    if ($gpio -notin @(0, 2, 4, 5, 13, 14, 15, 16, 17, 18, 19, 21, 22, 23, 25, 26, 27, 32, 33)) {
        throw "Unsupported or reserved GPIO $gpio"
    }
    if ($gpioOwners.ContainsKey($gpio)) { throw "Duplicate GPIO $gpio used by $symbol and $($gpioOwners[$gpio])" }
    $gpioOwners[$gpio] = $symbol
}
$hardware = Get-Content -LiteralPath (Join-Path $Root 'hardware.json') -Raw | ConvertFrom-Json
if ($hardware.schema_version -ne 1) { throw 'Unsupported hardware schema.' }
$components = @{}
$signals = @{}
foreach ($component in $hardware.components) {
    if ($components.ContainsKey($component.id)) { throw "Duplicate component $($component.id)" }
    if ($component.quantity -lt 1) { throw "Invalid quantity for $($component.id)" }
    $components[$component.id] = $component
    foreach ($signal in $component.signals) {
        if (-not $pins.ContainsKey($signal)) { throw "Unknown signal $signal in hardware manifest" }
        if ($signals.ContainsKey($signal)) { throw "Two components own $signal" }
        $signals[$signal] = $component.id
    }
}
if ($signals.Count -ne $pins.Count) { throw 'Missing component ownership for a used GPIO.' }
foreach ($strap in @(0, 2, 5, 15)) {
    if ($gpioOwners.ContainsKey($strap)) {
        $review = @($hardware.strap_reviews | Where-Object { $_.gpio -eq $strap -and $_.reason.Length -gt 10 })
        if ($review.Count -ne 1) { throw "Missing explicit boot-strap review for GPIO$strap" }
    }
}
$pinout = Get-Content -LiteralPath (Join-Path $Root 'PINOUT.md') -Raw
$table = [regex]::Match($pinout, '(?s)<!-- PINOUT:BEGIN -->(.*?)<!-- PINOUT:END -->')
if (-not $table.Success) { throw 'PINOUT table markers missing.' }
$documented = @{}
foreach ($row in [regex]::Matches($table.Groups[1].Value, '(?m)^\|\s*(\w+)\s*\|\s*(\d+)\s*\|\s*(\w+)\s*\|[^\r\n|]+\|\s*$')) {
    $symbol = $row.Groups[1].Value
    if ($documented.ContainsKey($symbol) -or -not $pins.ContainsKey($symbol) -or
        $pins[$symbol] -ne [int]$row.Groups[2].Value -or $signals[$symbol] -ne $row.Groups[3].Value) {
        throw "PINOUT mismatch: $symbol"
    }
    $documented[$symbol] = $true
}
if ($documented.Count -ne $pins.Count) { throw 'PINOUT table is incomplete.' }
$bom = @{}
foreach ($entry in (Import-Csv -LiteralPath (Join-Path $Root 'BOM.csv'))) {
    if ($bom.ContainsKey($entry.Id)) { throw "Duplicate BOM id $($entry.Id)" }
    if (-not $components.ContainsKey($entry.Id)) { throw "Undeclared BOM component $($entry.Id)" }
    if ([int]$entry.Quantity -ne $components[$entry.Id].quantity -or [string]::IsNullOrWhiteSpace($entry.Part)) {
        throw "BOM quantity/part mismatch for $($entry.Id)"
    }
    $bom[$entry.Id] = $entry
}
foreach ($id in $components.Keys) { if (-not $bom.ContainsKey($id)) { throw "BOM missing used component $id" } }
$motor = $hardware.motor
foreach ($id in @($motor.fet, $motor.driver, $motor.flyback, $motor.gate_pulldown)) {
    if (-not $components.ContainsKey($id)) { throw "Motor component missing: $id" }
}
if ($motor.pulldown_ohms -ne 10000 -or $bom[$motor.gate_pulldown].Part -notmatch '^10k\b') {
    throw 'Baseline motor gate pulldown must be declared as 10k in manifest and BOM.'
}
$required = @(
    @($motor.fet, 'G', $motor.gate_net), @($motor.fet, 'S', $motor.source_net),
    @($motor.gate_pulldown, '1', $motor.gate_net), @($motor.gate_pulldown, '2', $motor.source_net)
)
if ($motor.gate_net -eq $motor.source_net) { throw 'Motor gate and source nets must differ.' }
foreach ($connection in $required) {
    $found = @($motor.connections | Where-Object {
        $_.component -eq $connection[0] -and $_.terminal -eq $connection[1] -and $_.net -eq $connection[2]
    })
    if ($found.Count -ne 1) { throw 'Missing motor gate-to-source pulldown connection.' }
}
foreach ($file in (Get-ChildItem (Join-Path $Root 'firmware\nfc_player') -File)) {
    if ($file.Extension -notin @('.h', '.cpp', '.ino')) { continue }
    $text = Get-Content -LiteralPath $file.FullName -Raw
    if ($text -match 'RTC_CNTL_BROWN_OUT_REG|DISABLE_BROWNOUT') { throw "Brownout override found in $($file.Name)" }
}
Write-Output "PASS: $($pins.Count) GPIOs; pinout, BOM, straps, gate pulldown and brownout rules"