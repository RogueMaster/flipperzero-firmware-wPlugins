$ErrorActionPreference = "Stop"
$p = New-Object System.IO.Ports.SerialPort "COM8",115200,None,8,one
$p.ReadTimeout = 2000
$p.WriteTimeout = 2000
$p.Open()
Start-Sleep -Milliseconds 400

# Wake the CLI and clear any banner
$p.Write("`r")
Start-Sleep -Milliseconds 300
$p.Write("`r")
Start-Sleep -Milliseconds 300

$sb = New-Object System.Text.StringBuilder
foreach($cmd in @("storage list /ext/apps_data/api_caller", "storage read /ext/apps_data/api_caller/debug.log")) {
    $p.Write($cmd + "`r")
    $deadline = (Get-Date).AddSeconds(3)
    while((Get-Date) -lt $deadline) {
        $t = $p.ReadExisting()
        if($t) { [void]$sb.Append($t) } else { Start-Sleep -Milliseconds 150 }
    }
}
$p.Close()

$out = $sb.ToString()
if($out.Length -gt 12000) { $out = $out.Substring($out.Length - 12000) }
Write-Output "===CLISTART==="
Write-Output $out
Write-Output "===CLIEND==="
