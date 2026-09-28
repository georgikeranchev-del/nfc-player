$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$checker = Join-Path $PSScriptRoot 'Check-Design.ps1'
$temporary = Join-Path ([IO.Path]::GetTempPath()) ('nfc-design-checks-' + [guid]::NewGuid().ToString('N'))
$cases = @('gpio12', 'duplicate', 'unsupported', 'pinout', 'pinout-owner', 'bom', 'pulldown', 'strap', 'brownout')
try {
    foreach ($case in $cases) {
        $fixture = Join-Path $temporary $case
        $firmware = Join-Path $fixture 'firmware\nfc_player'
        [void][IO.Directory]::CreateDirectory($firmware)
        foreach ($file in @('PINOUT.md', 'BOM.csv', 'hardware.json')) {
            Copy-Item -LiteralPath (Join-Path $root $file) -Destination $fixture
        }
        $pinsPath = Join-Path $firmware 'pins.h'
        Copy-Item -LiteralPath (Join-Path $root 'firmware\nfc_player\pins.h') -Destination $pinsPath
        $expected = ''
        switch ($case) {
            'gpio12' {
                $text = (Get-Content $pinsPath -Raw).Replace('IP5310_KEY = 17;', 'IP5310_KEY = 12;')
                [IO.File]::WriteAllText($pinsPath, $text)
                $expected = 'GPIO12'
            }
            'duplicate' {
                $text = (Get-Content $pinsPath -Raw).Replace('IP5310_KEY = 17;', 'IP5310_KEY = 27;')
                [IO.File]::WriteAllText($pinsPath, $text)
                $expected = 'Duplicate GPIO'
            }
            'unsupported' {
                $text = (Get-Content $pinsPath -Raw).Replace('IP5310_KEY = 17;', 'IP5310_KEY = 20;')
                [IO.File]::WriteAllText($pinsPath, $text)
                $expected = 'Unsupported'
            }
            'pinout' {
                $path = Join-Path $fixture 'PINOUT.md'
                $text = (Get-Content $path -Raw).Replace('| IP5310_KEY | 17 |', '| IP5310_KEY | 12 |')
                [IO.File]::WriteAllText($path, $text)
                $expected = 'PINOUT mismatch'
            }
            'pinout-owner' {
                $path = Join-Path $fixture 'PINOUT.md'
                $text = (Get-Content $path -Raw).Replace('| IP5310_KEY | 17 | Q_KEY |', '| IP5310_KEY | 17 | U_AMP |')
                [IO.File]::WriteAllText($path, $text)
                $expected = 'PINOUT mismatch'
            }
            'bom' {
                $path = Join-Path $fixture 'BOM.csv'
                $remaining = Import-Csv $path | Where-Object { $_.Id -ne 'M1' }
                $remaining | Export-Csv -LiteralPath $path -NoTypeInformation
                $expected = 'BOM missing used component'
            }
            'pulldown' {
                $path = Join-Path $fixture 'hardware.json'
                $hardware = Get-Content $path -Raw | ConvertFrom-Json
                $hardware.motor.connections = @($hardware.motor.connections | Where-Object { $_.component -ne 'R_GATE_PD' })
                [IO.File]::WriteAllText($path, ($hardware | ConvertTo-Json -Depth 20))
                $expected = 'Missing motor gate-to-source'
            }
            'strap' {
                $path = Join-Path $fixture 'hardware.json'
                $hardware = Get-Content $path -Raw | ConvertFrom-Json
                $hardware.strap_reviews = @($hardware.strap_reviews | Where-Object { $_.gpio -ne 2 })
                [IO.File]::WriteAllText($path, ($hardware | ConvertTo-Json -Depth 20))
                $expected = 'boot-strap review'
            }
            'brownout' {
                [IO.File]::WriteAllText((Join-Path $firmware 'unsafe.cpp'), 'WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);')
                $expected = 'Brownout override'
            }
        }
        $caught = $false
        try { & $checker -Root $fixture | Out-Null }
        catch {
            if ($_.Exception.Message -notlike "*$expected*") { throw "Wrong failure for ${case}: $($_.Exception.Message)" }
            $caught = $true
        }
        if (-not $caught) { throw "Design checker accepted invalid fixture: $case" }
        Write-Output "PASS: rejects $case"
    }
} finally {
    if (Test-Path -LiteralPath $temporary) { Remove-Item -LiteralPath $temporary -Recurse -Force }
}