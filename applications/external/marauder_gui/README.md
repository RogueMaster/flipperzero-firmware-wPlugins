<div align="right">

**[🇹🇷 Türkçe](#türkçe)** &nbsp;|&nbsp; **[🇬🇧 English](#english)**

</div>

# Marauder GUI

ESP32 Marauder companion app for Flipper Zero.

## Screenshots

| Main menu | Wi-Fi menu |
| --- | --- |
| ![Main menu](marauder_gui/screenshots/main-menu.png) | ![Wi-Fi menu](marauder_gui/screenshots/wifi-menu.png) |
| Wi-Fi attack menu | Bluetooth menu |
| ![Wi-Fi attack menu](marauder_gui/screenshots/wifi-attack-menu.png) | ![Bluetooth menu](marauder_gui/screenshots/bluetooth-menu.png) |
| Bluetooth detectors | Multi-AP attack selection |
| ![Bluetooth detectors](marauder_gui/screenshots/bluetooth-detectors.png) | ![Multi-AP attack selection](marauder_gui/screenshots/multi-ap-attack-selection.png) |
| Multi-AP deauthentication | |
| ![Multi-AP deauthentication](marauder_gui/screenshots/multi-ap-deauth-attack.png) | |

<a name="türkçe"></a>

## Türkçe

### Bu proje ne yapıyor?

**Marauder GUI**, Flipper Zero'yu GPIO UART üzerinden bağlı bir
[ESP32 Marauder](https://github.com/justcallmekoko/ESP32Marauder) kartı için tam bir kontrol
paneline dönüştüren, `uFBT` ile yazılmış yerli bir Flipper uygulamasıdır. Marauder'ın firmware'ini
hiçbir şekilde değiştirmez — Marauder'ın zaten var olan seri (serial) komut satırı arayüzünü kullanır
ve sunduğu her komutu düzgün bir Flipper menüsüne/ekranına çevirir; hiçbir şeyi elle klavyeden
yazmaya gerek kalmaz.

### Donanım

- Bir ESP32 Marauder kartı (çıplak bir ESP32-S3-N16R8 devboard üzerinde geliştirildi ve test edildi).
- Kablolama: Flipper GPIO **13 (TX) → Marauder RX**, GPIO **14 (RX) → Marauder TX**, artı ortak GND.
  İki taraf da 3.3V lojik seviyesinde — seviye çevirici (level shifter) gerekmiyor.
- 115200 baud hızı, Marauder'ın kendi `Serial.begin(115200)` ayarıyla eşleşiyor.
- **GPS modülü ve SD kart gerekmiyor.** Bu uygulamadaki her özellik yalnızca UART bağlantısı
  üzerinden çalışıyor — orijinal firmware'de GPS ya da SD kart gerektiren hiçbir şey bilinçli olarak
  kapsam dışı bırakıldı (aşağıdaki *Kapsam Dışı* bölümüne bakın).

### Özellikler

- **WiFi saldırıları** — tam AP tarama artı 8 saldırı tipi (Deauth, Probe Flood, AP Klon Spam,
  Hedefli Deauth, SAE Commit Flood, Kanal Değiştirme/CSA, Quiet Time, Bad Msg, Assoc Sleep), artı bir
  **çoklu hedef modu**: *Select APs* ekranında birden fazla AP işaretleyip aynı saldırıyı hepsine
  aynı anda ateşleyebilirsin.
- **WiFi Spam** — rastgele SSID'ler, Rick Roll beacon'ları, komik isimler veya sabit sayıda sahte AP
  listesi.
- **Evil Portal / Karma saldırısı** — seri port üzerinden canlı yüklenen sahte bir giriş sayfası (SD
  kart gerekmez).
- **Bluetooth** — 7 tip BLE spam (Sour Apple, Apple Juice, Swiftpair, Samsung, Google, Flipper,
  hepsi), artı ses çalma ve spoof özellikli bir AirTag/tracker bulucu.
- **10 canlı dedektör** — WiFi Pineapple, MultiSSID anomalisi, Deauth dinleyici, MAC Monitor, genel
  BLE tarama, Flipper bulucu, Pwnagotchi, Meta gözlük, BLE skimmer, Flock kamera.
- **Ağ araçları** — WiFi'a bağlanma (yeni veya kayıtlı bilgilerle), Ping Scan, ARP Scan, Port Scan ve
  canlı bağlantı durumu göstergesi.
- **Cihaz araçları** — reboot, cihaz bilgisi, cihaz üzerindeki ayar anahtarları (Force PMKID, Force
  Probe, Save PCAP, LED, EP Deauth, Channel Hop).
- **Ham Terminal modu** — herhangi bir Marauder CLI komutunu elle gönderip tam cevabı okuma.
- **Tam Türkçe/İngilizce arayüz** — Cihaz menüsünden istediğin an değiştirilebilir, reboot sonrası da
  hatırlanır.
- **Animasyonlu saldırı ekranları** — ham kayan yazı yerine WiFi saldırılarında spektrum çubuğu
  animasyonu, BLE spam'de radar taraması.
- Elle tasarlanmış özel bir uygulama ikonu.

### Normal bir Flipper uygulamasından farkları

- Bu bir **yardımcı/uzaktan kumanda** uygulaması, kendi başına çalışan bir araç değil: tüm gerçek
  radyo işi (WiFi/BLE saldırıları, dinleme) Flipper'ın kendi donanımında değil, harici ESP32 kartında
  gerçekleşiyor. Flipper burada sadece arayüz ve seri-komut katmanı.
- Kalıcı dil değiştirme özellikli tam iki dilli arayüz — çoğu Flipper uygulaması sadece İngilizce
  geliyor, uygulama içinden değiştirme imkânı yok.
- Sıfırdan yazılmış özel bir liste/menü görünümü: Flipper'ın hazır `Submenu` bileşeni uzun metni
  kaydırarak gösteremiyor ve satır başına açıklama gösteremiyor, bu yüzden bu proje için özel olarak
  kendi görünümümüzü yazdık.
- Her saldırı/durum ekranı, Marauder'ın ham seri onay metni yerine elle tasarlanmış animasyonlu bir
  canvas görünümü kullanıyor (zaten bu ham metin firmware'e gömülü sabit İngilizce çıktı olduğu için
  çevrilemez).

### Kapsam dışı

Marauder'ın firmware kaynak kodu doğrudan incelenerek **CLI karşılığı olmadığı** doğrulananlar —
bunlar orijinal donanımda sadece dokunmatik ekrandan erişilebiliyor, seri port üzerinden hiçbir
şekilde tetiklenemiyor (GPS/SD kart olsa da fark etmez):

- Start AP, Host AP Info / Shutdown WiFi
- Channel Analyzer, Channel Summary, Bluetooth Analyzer (grafik araçları)

Bu kartta GPS modülü ve SD kart olmadığı için kapsam dışı bırakılanlar:

- GPS/Wardriving
- SD kart gerektiren özellikler: ham paket yakalama, PCAP/EAPOL kaydetme, firmware güncelleme, dosya
  yönetimi

---

<a name="english"></a>

## English

### What is this?

**Marauder GUI** is a native Flipper Zero app (built with `uFBT`) that turns the Flipper into a full
control surface for an [ESP32 Marauder](https://github.com/justcallmekoko/ESP32Marauder) board wired
to its GPIO UART. It doesn't modify Marauder's firmware in any way — it drives Marauder's existing
serial CLI, and translates every command it exposes into a proper Flipper menu/screen, so nothing has
to be typed by hand on a keyboard.

### Hardware

- An ESP32 Marauder board (developed and tested against a bare ESP32-S3-N16R8 devboard).
- Wiring: Flipper GPIO **13 (TX) → Marauder RX**, GPIO **14 (RX) → Marauder TX**, plus a common GND.
  Both sides run 3.3V logic — no level shifter needed.
- 115200 baud, matching Marauder's own `Serial.begin(115200)`.
- **No GPS module and no SD card required.** Every feature in this app works over the UART link
  alone — anything in the original firmware that needs GPS or an SD card was deliberately left out
  (see *Out of scope* below).

### Features

- **WiFi attacks** — full AP scan plus 8 attack types (Deauth, Probe Flood, AP Clone Spam, Targeted
  Deauth, SAE Commit Flood, Channel Switch/CSA, Quiet Time, Bad Msg, Assoc Sleep), plus a
  **multi-target mode**: check off several APs in *Select APs* and fire one attack at all of them at
  the same time.
- **WiFi Spam** — random SSIDs, Rick Roll beacons, funny names, or a fixed-count fake AP list.
- **Evil Portal / Karma attack** — a captive-portal phishing page, uploaded live over serial (no SD
  card needed).
- **Bluetooth** — 7 BLE spam types (Sour Apple, Apple Juice, Swiftpair, Samsung, Google, Flipper,
  all), plus an AirTag/tracker finder with sound-play and spoofing.
- **10 live detectors** — WiFi Pineapple, MultiSSID anomaly, Deauth sniffer, MAC Monitor, general BLE
  scan, Flipper finder, Pwnagotchi, Meta glasses, BLE skimmer, Flock camera.
- **Network tools** — Join WiFi (new or saved credentials), Ping Scan, ARP Scan, Port Scan, and a
  live connection-status indicator.
- **Device tools** — reboot, device info, on-device settings toggles (Force PMKID, Force Probe, Save
  PCAP, LED, EP Deauth, Channel Hop).
- **Raw Terminal mode** — send any Marauder CLI command by hand and read the full reply.
- **Full Turkish/English UI** — switchable anytime from the Device menu, persisted across reboots.
- **Animated attack-status screens** — a spectrum-bar animation for WiFi attacks, a radar sweep for
  BLE spam, in place of a raw scrolling text log.
- A custom hand-designed app icon.

### What's different from a normal Flipper app

- It's a **companion/remote-control** app, not a self-contained tool: all the actual radio work
  (WiFi/BLE attacks, sniffing) happens on the external ESP32 board, not on the Flipper's own
  hardware. The Flipper is purely the UI and serial-command layer.
- A fully bilingual interface with a persistent language switch — most Flipper apps ship
  English-only, with no in-app way to change that.
- A from-scratch custom list/menu view: Flipper's stock `Submenu` widget can't marquee-scroll long
  text or show a per-item description overlay, so this app replaces it with its own, built
  specifically for this project.
- Every attack/status screen is a hand-built animated canvas view instead of Marauder's raw serial
  confirmation text (which is hardcoded English firmware output and can't be translated anyway).

### Out of scope

Confirmed, by reading Marauder's firmware source directly, to have **no CLI equivalent** —
these are touchscreen-only features on the original hardware and can't be triggered over serial at
all, regardless of GPS/SD-card availability:

- Start AP, Host AP Info / Shutdown WiFi
- Channel Analyzer, Channel Summary, Bluetooth Analyzer (graphing tools)

Left out because this board has no GPS module or SD card:

- GPS/Wardriving
- SD-dependent features: raw packet capture, PCAP/EAPOL saving, firmware updates, file management
