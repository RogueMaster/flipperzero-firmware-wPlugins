$p = New-Object System.IO.Ports.SerialPort "COM8",115200,None,8,one
$p.ReadTimeout = 1500
$p.WriteTimeout = 1500
try {
    $p.Open()
    Start-Sleep -Milliseconds 300
    [void]$p.ReadExisting()
    foreach($cmd in @("ps", "storage list /ext/apps_data/api_caller", "storage read /ext/apps_data/api_caller/debug.log")) {
        $p.Write($cmd + "`r")
        $sb = New-Object System.Text.StringBuilder
        $deadline = (Get-Date).AddSeconds(4)
        while((Get-Date) -lt $deadline) {
            $t = $p.ReadExisting()
            if($t) { [void]$sb.Append($t) } else { Start-Sleep -Milliseconds 150 }
        }
        Write-Output ("### " + $cmd)
        Write-Output $sb.ToString()
    }
} catch {
    Write-Output ("ERR: " + $_.Exception.Message)
} finally {
    try { $p.Close() } catch {}
}
