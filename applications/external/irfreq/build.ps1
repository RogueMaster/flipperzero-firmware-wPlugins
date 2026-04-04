param(
    [string]$SourceDir = (Split-Path -Parent $MyInvocation.MyCommand.Path),
    [string]$FirmwareDir = "C:\Users\Joe\Projects\flipperzero-firmware",
    [string]$TargetDir,
    [string]$AppSrc = "applications_user/flipirfreq",
    [switch]$PreviewSync,
    [switch]$SkipBuild
)

$ErrorActionPreference = "Stop"

$SourceDir = [System.IO.Path]::GetFullPath($SourceDir)
$FirmwareDir = [System.IO.Path]::GetFullPath($FirmwareDir)

if([string]::IsNullOrWhiteSpace($TargetDir)) {
    $targetDir = Join-Path $FirmwareDir "applications_user\flipirfreq"
} else {
    $targetDir = [System.IO.Path]::GetFullPath($TargetDir)
}

if(!(Test-Path $SourceDir -PathType Container)) {
    throw "SourceDir does not exist or is not a directory: $SourceDir"
}

if(!(Test-Path $FirmwareDir -PathType Container)) {
    throw "FirmwareDir does not exist or is not a directory: $FirmwareDir"
}

$resolvedAppSrc = $AppSrc.Trim()
if([string]::IsNullOrWhiteSpace($resolvedAppSrc)) {
    throw "AppSrc cannot be empty."
}

Write-Host "SourceDir : $SourceDir"
Write-Host "FirmwareDir: $FirmwareDir"
Write-Host "TargetDir : $targetDir"
Write-Host "AppSrc    : $resolvedAppSrc"

if($targetDir.TrimEnd('\') -eq $SourceDir.TrimEnd('\')) {
    throw "Refusing to mirror because SourceDir and TargetDir are the same path."
}

$firmwareRoot = $FirmwareDir.TrimEnd('\') + '\'
$normalizedTargetDir = $targetDir.TrimEnd('\') + '\'

if(-not $normalizedTargetDir.StartsWith($firmwareRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Refusing to mirror outside FirmwareDir. TargetDir must be inside $FirmwareDir"
}

Write-Host "Syncing app files to $targetDir..."

if(!(Test-Path $targetDir)) {
    New-Item -ItemType Directory -Path $targetDir | Out-Null
}

$stalePaths = @(
    (Join-Path $targetDir "build.ps1")
)

if(-not $PreviewSync) {
    foreach($path in $stalePaths) {
        if(Test-Path $path) {
            Remove-Item -Force $path
        }
    }
}

$robocopyArgs = @(
    $SourceDir
    $targetDir
    "/MIR"
    "/XD"
    ".git"
    "/XF"
    "README.md"
    "build.ps1"
)

if($PreviewSync) {
    $robocopyArgs += "/L"
    Write-Host "Preview mode enabled. No files will be copied, deleted, or built."
}

& robocopy @robocopyArgs | Out-Host

if($LASTEXITCODE -gt 7) {
    throw "robocopy failed with exit code $LASTEXITCODE"
}

if($PreviewSync -or $SkipBuild) {
    Write-Host "Sync step complete."
    if($SkipBuild -and -not $PreviewSync) {
        Write-Host "Build skipped."
    }
    return
}

Push-Location $FirmwareDir
try {
    Write-Host "Building FlipIRFreq..."
    & .\fbt.cmd build APPSRC=$resolvedAppSrc
    if($LASTEXITCODE -ne 0) {
        throw "fbt build failed with exit code $LASTEXITCODE"
    }
} finally {
    Pop-Location
}

Write-Host "Build complete."
