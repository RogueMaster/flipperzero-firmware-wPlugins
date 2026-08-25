foreach($portName in @("COM3","COM4","COM5","COM6","COM8")) {
    $p = New-Object System.IO.Ports.SerialPort $portName,115200,None,8,one
    $p.ReadTimeout = 700
    $p.WriteTimeout = 700
    $result = "no-open"
    try {
        $p.Open()
        Start-Sleep -Milliseconds 150
        try {
            $p.Write("`r")
            Start-Sleep -Milliseconds 500
            $got = $p.ReadExisting()
            if($got.Length -gt 0) { $result = "echo:[" + $got.Substring(0, [Math]::Min(80, $got.Length)) + "]" }
            else { $result = "silent" }
        } catch {
            $result = "write-timeout"
        }
    } catch {
        $result = "open-error"
    } finally {
        try { $p.Close() } catch {}
    }
    Write-Output ($portName + " -> " + $result)
}
