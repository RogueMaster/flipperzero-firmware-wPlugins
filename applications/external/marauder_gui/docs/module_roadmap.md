# Marauder GUI — Modül Yol Haritası

Orijinal ESP32 Marauder'ın kendi ekran menüsündeki (`MenuFunctions.cpp`) tüm özelliklerin, bizim Flipper GUI'sindeki karşılıklarıyla envanteri. Kaynak: `reference/ESP32Marauder` taraması (2026-09-03).

Donanım kısıtları: **GPS yok**, **SD kart yok** (ESP32-S3-N16R8 çıplak devboard).

---

## ✅ Tamamlananlar

### WiFi
- [x] AP Tara + Deauth (`scanall` = `WIFI_SCAN_AP_STA`, `attack -t deauth`)
- [x] WiFi Spam — Rastgele SSID, Rick Roll, Komik İsimler, Sayı Belirle (N sahte AG)
- [x] Karma Attack — Probe Sniff + Evil Portal (`sethtmlstr` ile SD'siz çözüldü)

### Bluetooth
- [x] BLE Spam — 7 tip: Sour Apple, Apple Juice, Swiftpair, Samsung, Google, Flipper, Hepsi (**Bluetooth Attack menüsü %100 tamam**)
- [x] AirTag/Tracker Bul + Ses Çal (FindMy) + Spoof Et

### Genel
- [x] Terminal (ham komut gönder, tam cevabı gör)

---

## Yapılacaklar (öncelik sırasıyla)

### 1. Dedektörler (kolay — mevcut canlı liste altyapısını aynen kullanır)
- [x] WiFi Pineapple Tespiti (`sniffpinescan` + `list -x`) — WiFi > Dedektörler
- [x] MultiSSID Anomali Tespiti (`sniffmultissid` + `list -m`) — WiFi > Dedektörler
- [x] Genel BLE Tarama (`sniffbt` + `list -b`, format `[N][RSSI] isim`) — Bluetooth > Dedektörler
- [x] Flipper Bulucu (`sniffbt -t flipper` + `list -f`, format `[N]MAC: ... isim`) — Bluetooth > Dedektörler
- [x] Pwnagotchi Tespiti (`sniffpwn`, "Name:"/"Pwnd #:" iki satırını birleştirir) — WiFi > Dedektörler
- [x] Deauth Sniff (`sniffdeauth`, satır: `rssi Ch: n src -> dst`) — WiFi > Monitor&Sniff/
- [x] MAC Monitor (`mactrack`, 1Hz periyodik top-10 tablo, `---------------` ile sıfırlanır) — WiFi > Monitor&Sniff/
- [x] Meta Gözlük Dedektörü (`sniffbt -t meta`, satır: `Meta Device: rssi isim`) — Bluetooth > Dedektörler
- [x] Bluetooth Skimmer Dedektörü (`sniffskim`, filtresiz akış — HC-03/05/06 istemci tarafında filtrelendi) — Bluetooth > Dedektörler
- [x] Flock Kamera Dedektörü (`sniffbt -t flock`, kontrol baytları temizlenerek ham günlük) — Bluetooth > Dedektörler

**Grup 1 tamamlandı.**

### 2. WiFi Saldırı Çeşitleri (kolay — mevcut saldırı ekranı deseni)
- [x] SAE Commit Flood (WPA3, AP seçimi **gerekiyor** — bkz. düzeltme notu) — WiFi > WiFi Saldırıları
- [x] Kanal Değiştirme Saldırısı (CSA, AP seçimi gerekiyor) — WiFi > WiFi Saldırıları
- [x] Quiet Time (AP seçimi gerekiyor) — WiFi > WiFi Saldırıları
- [x] Bad Msg (AP seçimi + o AP'nin bilinen istemcileri gerekiyor) — WiFi > WiFi Saldırıları
- [x] Assoc Sleep (AP seçimi + o AP'nin bilinen istemcileri gerekiyor) — WiFi > WiFi Saldırıları
- [x] Probe Flood (`attack -t probe`, AP seçimi gerekir) — WiFi > WiFi Saldırıları
- [x] AP Klon Spam (`attack -t beacon -a`, AP seçimi gerekir) — WiFi > WiFi Saldırıları
- [x] Hedefli Deauth (`select -c` ile istemci seçimi, `attack -t deauth -c`) — WiFi > WiFi Saldırıları

**Düzeltme (2026-09-03):** SAE/CSA/Quiet/Bad Msg/Assoc Sleep ilk yazıldığında "hedef gerektirmiyor" sanılmıştı (CLI seviyesinde `filterActive()` kontrolü yok diye). Kaynak koddaki gerçek döngüler (`saeAttackLoop`, `sendBadMsgAttack`, `sendAssocSleepAttack`, CSA/Quiet'in `broadcastCustomBeacon` çağrısı) incelenince hepsinin `access_points`'teki `.selected` bayrağını kontrol ettiği görüldü — hiçbir AP seçili değilse sessizce hiçbir şey yapmıyorlardı. Kullanıcı bunu fark edip sorunca düzeltildi: artık 5'i de AP Tara akışını (AP seç → `select -a` → saldırı) kullanıyor, "hedefsiz" ayrı ekran (`wifi_attack_simple.c`) kaldırıldı.

**Grup 2 tamamlandı** (Bad Msg/Assoc Sleep'in `-c` hedefli-tek-istemci varyantları henüz yok — aynı istasyon seçici altyapısı üzerinden ileride kolayca eklenebilir).

### 3. Ağa Bağlanma + Ağ Keşif Araçları (orta-büyük efor — önce "Join WiFi" gerekiyor)
- [x] Join WiFi (`join -a N -p "sifre"`, AP seç → şifre gir → ~10sn bağlantı durumu) — WiFi > Ag Araclari/
- [x] Join Saved WiFi (`join -s`, gerçek SSID `settings` çıktısından okunur) — WiFi > Ag Araclari/
- [x] Ping Scan — WiFi > Ag Araclari/
- [x] ARP Scan — WiFi > Ag Araclari/
- [x] Port Scan (IP seç → tüm portları tara) — WiFi > Ag Araclari/
- [x] Bağlı ağ göstergesi ("Bagli: X" / "Bagli degil") — Ag Araclari menü başlığı
- [ ] ~~Start AP~~ **kapsam dışı** — orijinal firmware'de bunun için hiçbir CLI komutu yok (sadece dokunmatik ekran menüsünden erişilebiliyor), serial üzerinden tetiklenemez.

**Grup 3 tamamlandı** (Start AP hariç — CLI'de karşılığı yok).

**Not (2026-09-03):** `joinWiFi()` ESP32'nin ana döngüsünü ~10sn boyunca bloke ediyor, başarıda özel bir onay mesajı yok (sadece başarısızlıkta "Could not connect..." / "no saved WiFi credentials" mesajı var) — bu yüzden durum ekranı ~12sn bekleyip bu hata mesajlerinden birini görmediyse "bağlandı" olarak varsayıyor (kesin onay değil, çıkarım).

**Kritik hata + düzeltme (2026-09-03):** İlk sürümde "Kayıtlı Ağa Bağlan"ın da dahil olduğu her durum ekranının `on_exit`'i `stopscan` gönderiyordu. `stopscan` → `shutdownWiFi()`, Marauder'ın `wifi_connected` bayrağı o an `false` ise (bağlantı henüz oturmamışsa/düşmüşse) WiFi sürücüsünü tamamen `esp_wifi_deinit()`/`esp_netif_deinit()` ile kapatıyor — cihaz yeniden başlatılana kadar telafisi yok. Join WiFi akışında bulunup düzeltildi (`wifi_join_status.c`'nin `on_exit`'i artık hiç `stopscan` göndermiyor); aynı riskin Ping/ARP/Port Scan ekranlarında da geçerli olduğu anlaşılınca (kullanıcı "ağdan atılıyor gibi" diye bildirdi) oradan da kaldırıldı — bkz. `marauder_gui_log_scan.c`'deki `_keep_running` yardımcıları.

### 4. Cihaz/Ayarlar (kolay)
- [x] Reboot (`reboot`) — Cihaz/
- [x] Cihaz Bilgisi (`info` komutu) — Cihaz/
- [x] Ayarlar aç/kapa listesi (Force PMKID, Force Probe, Save Pcap, Enable LED, EP Deauth, Channel Hop — `settings` + `settings -s <isim> enable/disable`) — Cihaz/Ayarlar/
- [x] ~~Client SSID/Password ayarı~~ **ayrı ekrana gerek yok** — CLI'de String ayar için setter yok (`settings -s` sadece bool toggle destekliyor); Join WiFi zaten başarılı bağlantıda bunları otomatik kaydediyor.

**Grup 4 tamamlandı.** Not: Kullanıcı Channel Hop'u açıp kapatınca AP Tara sonuçlarında fark gözlemledi, ama kaynak kod incelemesi bizim kullandığımız `scanall`/`WIFI_SCAN_AP_STA` modunun `channelHop()`'taki ChanHop kontrolünden etkilenmediğini gösteriyor (sadece `WIFI_SCAN_AP` gibi farklı bir mod etkileniyor) — muhtemelen rastlantı, kullanıcı şimdilik geçmeyi tercih etti, ileride tekrar test edilebilir.

### 5. WiFi Ek Araçlar (orta)
- [x] Select APs — çoklu AP seçimi (`select -a`, ac/kapa liste + sag ust secili-AP sayaci) — WiFi > Ek Araclar/
- [x] MAC Ayarları — Rastgele AP/Istemci MAC (`randapmac`/`randstamac`) + AP/Istemci MAC Klonla (`cloneapmac`/`clonestamac`) — WiFi > Ek Araclar/MAC Ayarlari/
- [x] AP Bilgisi Görüntüleme (`info -a <index>`, tek ekranda ozet: SSID/BSSID/Kanal/RSSI/Guvenlik/Istemci/EAPOL) — WiFi > Ek Araclar/
- [ ] ~~Host AP Info / Shutdown WiFi~~ **kapsam dışı** — Start AP gibi bunların da CLI komutu yok (sadece dokunmatik ekranda, dogrudan WiFi.disconnect()/softAP kodu cagriliyor).

**Grup 5 tamamlandı** (Host AP Info/Shutdown WiFi hariç — CLI'de karşılığı yok). Not: Select APs şu an bağımsız bir seçim ekranı; mevcut saldırı akışları (SAE/CSA/Quiet/vb.) hâlâ tek-AP seçip otomatik geri alan eski mantıkla çalışıyor — gerçek "aynı anda çoklu AP'ye saldır" akışı kullanıcı tarafından bilinçli olarak kapsam dışı bırakıldı.

### 6. Sinyal Grafikleri (düşük öncelik — küçük Flipper ekranında anlamlı göstermek zor)
- [ ] Packet Count
- [ ] Channel Analyzer / Channel Summary
- [ ] Bluetooth Analyzer
- [ ] Fox Hunt (WiFi + BT yön bulma)

### 7. Kapsam Dışı (donanım eksik, atlanacak)
- GPS/Wardrive özellikleri (GPS modülü yok)
- SD gerektiren, `-serial` ile de çözülemeyen özellikler: Save/Load Files, Update Firmware, Delete SD Files, WiGLE/WDGWars log yükleme (GPS'e bağımlı zaten), SPIFFS backup/restore (`migrateSPIFFS`, saf SD↔SPIFFS kopyalama, serial fallback'i yok)
- SAE Commit Sniff (niş, düşük öncelik — SAE Commit Flood'la karıştırılmasın)

### 8. Paket Yakalama (2026-09-05, SD kart olmadan çözüldü)
- [x] Ham Paket Yakala (`sniffraw -serial`) — WiFi > Analiz&Araclar/Paket Yakalama/
- [x] EAPOL/PMKID Yakala (`sniffpmkid -serial`) — WiFi > Analiz&Araclar/Paket Yakalama/

**Bulgu:** Marauder firmware'i her tarama/saldırı komutuna eklenebilen global bir `-serial` bayrağına sahip (`CommandLine.cpp`, `wifi_scan_obj.save_serial`) — bu bayrak açıkken `Buffer` sınıfı (`Buffer.cpp`) PCAP/log/GPX verisini SD yerine (veya SD ile birlikte) doğrudan ana Serial hattına, `[BUF/BEGIN]...[BUF/CLOSE]` işaretleriyle sarılmış ham baytlar olarak akıtıyor (`Buffer::saveSerial()`) — firmware'in kendi yorum satırı bile "kullanıcı arayüzü bu işaretleri yok saymalı" diyor. Bu mekanizma Raw Capture, EAPOL/PMKID, Pineapple/MultiSSID/Pwnagotchi pcap'leri, Ping/ARP/Port/SSH/Telnet/SMTP/DNS/HTTP/HTTPS/RDP logları, Airtags/APs/SSIDs logları ve Evil Portal logunu kapsıyor — yani SD'ye bağımlı özelliklerin neredeyse tamamını, **hiçbir firmware (C++) değişikliği gerekmeden**. Flipper tarafında yapılan iş: `marauder_gui_serial_capture.c` adında paylaşılan bir modül, gelen her ham baytı (satır bazlı ayrıştırmadan ÖNCE, çünkü veri `\r`/`\n`/`\0` içerebilir) `[BUF/BEGIN]`/`[BUF/CLOSE]` işaretlerine karşı arka planda eşleştiriyor ve aradaki baytları doğrudan Flipper'ın kendi SD kartındaki bir `.pcap` dosyasına (`/ext/apps_data/marauder_gui/captures/`) yazıyor. Şu an sadece hedefsiz (no-target) iki tip eklendi; ileride diğer log/pcap türleri de aynı `marauder_gui_serial_capture_start(app, prefix, ext)` çağrısıyla kolayca eklenebilir.

---

## Notlar
- Her yeni "dedektör" tipi, `marauder_gui_scene_wifi_scanning.c`'deki (WiFi) veya `bt_tracker_scan.c`'deki (BT) canlı liste desenini birebir taklit ediyor — sadece başlatma komutu ve `list -X` sorgu bayrağı değişiyor.
- Her yeni "saldırı çeşidi", `wifi_spam.c`/`bt_spam.c`'deki tablo+durum ekranı desenini kullanıyor.
- Sırayla yukarıdan aşağı ilerlenecek; her modül tamamlanınca bu dosyadaki kutucuk işaretlenecek.
