[CmdletBinding()]
param(
    [switch]$Flash,
    [switch]$Clean,
    [switch]$NoSync
)

$ErrorActionPreference = 'Stop'
$root   = $PSScriptRoot
$shared = Join-Path $root '_shared\emv_lib'
$appDir = Join-Path $root 'emv_reader'
$appName = 'emv_reader'

# Files synced from _shared/emv_lib/ into the app dir before each build
$sharedFiles = @('ber_tlv.c','ber_tlv.h','emv_apdu.c','emv_apdu.h')

function Sync-Shared {
    if ($NoSync) { return }
    $keep = @{}
    foreach ($f in $sharedFiles) { $keep[$f] = $true }
    Get-ChildItem -Path $shared -File | ForEach-Object {
        if (-not $keep.ContainsKey($_.Name)) {
            $stale = Join-Path $appDir $_.Name
            if (Test-Path $stale) { Remove-Item $stale -Force }
        }
    }
    foreach ($f in $sharedFiles) {
        Copy-Item -Path (Join-Path $shared $f) -Destination $appDir -Force
    }
}

if (-not (Test-Path $appDir)) {
    throw "App directory not found: $appDir"
}

Write-Host ""
Write-Host "==> $appName" -ForegroundColor Cyan
Sync-Shared
Push-Location $appDir
try {
    if ($Clean) { & ufbt -c | Out-Null }
    if ($Flash) {
        $lines = cmd /c "ufbt launch 2>&1"
        $rc = $LASTEXITCODE
        $lines | ForEach-Object { Write-Host $_ }
        $joined = ($lines -join "`n")
        $installOk = $joined -match '100%, chunk'
        $closeBlocked = $joined -match 'has to be closed manually'
        if ($rc -ne 0 -and -not ($installOk -and $closeBlocked)) {
            throw "ufbt launch failed (exit $rc)"
        }
        if ($closeBlocked) {
            Write-Host "    NOTE: install OK; back out of running app on Flipper to use new build" -ForegroundColor Yellow
        }
    } else {
        $lines = cmd /c "ufbt 2>&1"
        $rc = $LASTEXITCODE
        $lines | ForEach-Object { Write-Host $_ }
        if ($rc -ne 0) { throw "ufbt failed (exit $rc)" }
    }
    $fap = Join-Path $appDir "dist\$appName.fap"
    if (Test-Path $fap) {
        $size = (Get-Item $fap).Length
        Write-Host ("    OK  {0} ({1} bytes)" -f $fap, $size) -ForegroundColor Green

        $distRoot = Join-Path $root 'dist'
        $relRoot  = Join-Path $root 'releases'
        if (-not (Test-Path $distRoot)) { New-Item -ItemType Directory -Path $distRoot | Out-Null }
        if (-not (Test-Path $relRoot))  { New-Item -ItemType Directory -Path $relRoot  | Out-Null }
        Copy-Item -Path $fap -Destination (Join-Path $distRoot "$appName.fap") -Force
        Copy-Item -Path $fap -Destination (Join-Path $relRoot  "$appName.fap") -Force
        Write-Host "    Staged in dist/ and releases/" -ForegroundColor Green
    }
} finally {
    Pop-Location
}

Write-Host ""
Write-Host "Done." -ForegroundColor Green
