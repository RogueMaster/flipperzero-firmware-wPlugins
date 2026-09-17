# Flipper Zero'da C ile GUI'li Uygulama Geliştirme Rehberi

Bu rehber, [Flipper Zero resmi geliştirici dokümantasyonu](https://developer.flipper.net/flipperzero/doxygen/app_dev_your_first_app_in_c.html) taranarak hazırlanmıştır. Amaç, bundan sonraki aşamalarda birlikte yazacağımız GUI'li Flipper Zero uygulaması için ortak bir referans noktası oluşturmaktır.

Kaynak sayfalar:
- `app_dev_your_first_app_in_c.html` — İlk C uygulaması eğitimi
- `app_dev_gui.html` — Uygulamalarda GUI kullanımı
- `app_manifests.html` — `application.fam` manifest dosyası referansı

---

## 1. Geliştirme Ortamı Kurulumu

Gerekli araçlar:

- **uFBT** ("micro Flipper Build Tool") — https://github.com/flipperdevices/flipperzero-ufbt
- **Visual Studio Code** — uFBT ile entegrasyonu iyi olduğu için önerilir
- **Flipper Zero cihazı** + USB kablosu

uFBT, tam FBT (Flipper Build Tool) firmware derleme sisteminin küçük/bağımsız bir sürümüdür; tek başına uygulama (`.fap`) geliştirmek için kullanılır, tüm firmware'i derlemeye gerek kalmaz.

---

## 2. Uygulama Şablonu Oluşturma

Adımlar:

1. Yeni uygulama için bir klasör oluştur (örn. `test_app`)
2. O klasörde terminal aç ve çalıştır:
   ```
   ufbt create APPID=test_app
   ```
3. VS Code entegrasyonu için:
   ```
   ufbt vscode_dist
   ```

**Kısıtlama:** `APPID` yalnızca küçük harf `a-z`, rakam `0-9` ve alt çizgi `_` içerebilir.

**Oluşan dosya yapısı:**

```
test_app/
├── application.fam        # Uygulama manifest dosyası (build config)
├── test_app.c              # Ana uygulama dosyası (entry point)
├── test_app.png            # Uygulama ikonu
├── .github/
│   └── workflows/
│       └── build.yml       # GitHub Actions CI yapılandırması
└── images/
    └── .gitkeep             # images/ klasörünü Git'e tanıtmak için placeholder
```

---

## 3. İlk Kod: Şablon Uygulama (Konsol Log)

```c
#include <furi.h>

#include <test_app_icons.h>

int32_t test_app_app(void* p) {
    UNUSED(p);

    FURI_LOG_I("TEST", "Hello world");
    FURI_LOG_I("TEST", "I'm test_app!");

    return 0;
}
```

**Açıklamalar:**

| Öğe | Açıklama |
|---|---|
| `#include <furi.h>` | Flipper firmware'inin çekirdek FURI header dosyası |
| `#include <test_app_icons.h>` | Uygulama görsel/ikon kullanıyorsa gerekir; uFBT PNG'leri otomatik Flipper formatına çevirir |
| `int32_t` | App giriş fonksiyonunun tipik dönüş tipi |
| `UNUSED(p);` | Kullanılmayan parametre için derleyici uyarısını bastıran makro |
| `FURI_LOG_I(tag, msg)` | Bilgi seviyesinde log basar; `ufbt cli` ardından `log info` ile izlenir |
| `return 0;` | Programın başarıyla tamamlandığını belirtir |

---

## 4. Basit GUI: Dialog ile "Hello World"

En basit GUI yaklaşımı **Dialogs API**'dir — senkron çalışan basit mesaj kutuları sağlar.

```c
#include <furi.h>
#include <dialogs/dialogs.h>
#include <test_app_icons.h>

int32_t test_app_app(void* p) {
    UNUSED(p);

    DialogsApp* dialogs = furi_record_open(RECORD_DIALOGS);
    DialogMessage* message = dialog_message_alloc();

    dialog_message_set_header(message, "Hello world", 64, 20, AlignCenter, AlignTop);
    dialog_message_set_text(message, "I'm test_app!", 64, 32, AlignCenter, AlignTop);

    dialog_message_show(dialogs, message);

    dialog_message_free(message);
    furi_record_close(RECORD_DIALOGS);

    return 0;
}
```

**Bileşenler:**

- `DialogsApp*` — sistemin dialog GUI servisine erişim sağlayan record
- `DialogMessage*` — ekranda gösterilecek mesaj yapısı
- `dialog_message_set_header(msg, text, x, y, horiz_align, vert_align)` — başlık metni ve konumu
- `dialog_message_set_text(msg, text, x, y, horiz_align, vert_align)` — gövde metni ve konumu
- `dialog_message_show(dialogs, msg)` — mesajı ekranda gösterir (bloklayıcı/senkron)
- `dialog_message_alloc()` / `dialog_message_free()` — bellek yönetimi (alloc/free çifti unutulmamalı)
- `furi_record_open(RECORD_X)` / `furi_record_close(RECORD_X)` — Flipper'ın servis kaydı (record) sistemine erişim açma/kapama deseni; hemen hemen tüm sistem servislerine bu şekilde erişilir

> **Flipper'daki "record" deseni önemli:** Her sistem servisi (GUI, Dialogs, Storage, Notification, vs.) bir "record" olarak kayıtlıdır. Kullanmadan önce `furi_record_open()`, işin bitince mutlaka `furi_record_close()` çağrılır. Bu, bundan sonra yazacağımız her GUI uygulamasında tekrar tekrar karşımıza çıkacak.

---

## 5. GUI Geliştirme Yaklaşımları (Önemli — Mimari Karar)

Flipper Zero'da GUI yazmanın üç seviyesi var. Yazacağımız uygulamanın karmaşıklığına göre birini seçeceğiz.

### a) Dialogs API (En basit)
- **Artı:** Kolay, hızlı kurulum
- **Eksi:** Karmaşık arayüz kurulamaz; dialoglar senkron çalışır (main loop'u bloklar)
- **Kullanım alanı:** Tek seferlik mesaj/onay ekranları

### b) ViewPort (Orta seviye)
- Tek, tam ekran bir `ViewPort` üzerinden çizim (render) ve girdi (input) yönetimi yapılır.
- **Artı:** Kolay, hızlı, oyun geliştirmeye uygun, ekranın tam kontrolü elde
- **Eksi:** Yerleşik navigasyon yardımcıları yok, view'lar tekrar kullanılabilir değil
- **Örnek kullanımlar:** Debug araçları, BLE eşleştirme ekranı, çoğu oyun

Temel kavramlar:
- **Canvas** — soyutlama içermeyen ham çizim alanı (özel tasarımlar/oyunlar için)
- **ViewPort** — belirli konum/boyutta bir ekran alanının davranış ve render mantığını tanımlar. Birden fazla ViewPort üst üste bir "Drawing Stack" oluşturabilir (alttan üste render edilir, üst üste binebilirler). Oluşturulduktan sonra görünürlüğü aç/kapat yapılabilir.
- **EventLoop** — polling yapmadan, olay tabanlı asenkron mekanizma; olay geldiğinde atanmış handler fonksiyon tetiklenir, main loop bu sırada başka iş yapabilir.

### c) Views & Scenes (İleri seviye)
- Karmaşık, hiyerarşik arayüzler için asıl/önerilen yaklaşım.
- **Artı:** Karmaşık navigasyon, view'ların yeniden kullanımı, hazır modüller
- **Eksi:** Öğrenme eğrisi dik, implementasyonu karmaşık
- **Örnek kullanımlar:** Desktop, iButton, Infrared, GPIO, NFC, RFID, Sub-GHz uygulamaları (yani firmware'in kendi büyük uygulamaları)

Temel kavramlar:
- **Model** — uygulama verisinin bellekte tutulduğu yapı
- **View** — arayüz render'ı ve olay işleme; Model'den veri okuyup ekrana basar, olay işlerken Model'i günceller
- **View Dispatcher** — birden fazla View'ı yönetir, aralarında geçiş yapar
- **Scene** — tek bir ekranın davranışını yöneten birim; her scene şunları içerir:
  - `on_enter` — scene'e girişte view'ları başlatır
  - `on_exit` — çıkışta kaynakları serbest bırakır
  - `on_event` — dış olayları işler
- **Scene Manager** — scene'ler arası navigasyonu yönetir (karmaşık uygulamalarda gerekli)

Hazır **View Modülleri** (firmware'e gömülü, doğrudan kullanılabilir): `button_menu`, `button_panel`, `byte_input`, `dialog_ex`, `empty_screen`, `file_browser`, `loading`, `menu`, `number_input`, `popup`, `submenu`, `text_box`, `text_input`, `variable_item_list`, `widget`.

> **Notum:** Marauder-GUI-Flipper projesi muhtemelen tek ekranlı basit bir araç olmayacaksa (menü + birden fazla ekran gerekiyorsa), **ViewPort** ile başlayıp gerekirse **Scenes**'e geçmek mantıklı bir yol olur. Basit tek-ekran durumlarda ViewPort yeterli ve çok daha az boilerplate gerektirir.

---

## 6. `application.fam` Manifest Dosyası Referansı

`application.fam`, Python söz dizimiyle yazılan ve build sisteminin uygulamayı nasıl derleyeceğini tanımlayan manifest dosyasıdır.

### Zorunlu parametreler

| Parametre | Açıklama |
|---|---|
| `appid` | Build sistemi içinde benzersiz kimlik (bağımlılık çözümü ve build referansları için) |
| `apptype` | `FlipperAppType` enum değeri (aşağıda) |

### `FlipperAppType` değerleri

| Tip | Amaç |
|---|---|
| `SERVICE` | Açılışta erken başlatılan sistem servisi |
| `SYSTEM` | Menüde görünmeyen, başka app/CLI tarafından çağrılabilen uygulama |
| `APP` | Ana menüde görünen standart uygulama |
| `PLUGIN` | Firmware'e gömülü, Plugins menüsünde |
| `DEBUG` | Yalnızca debug modu aktifken Debug menüsünde görünür |
| `ARCHIVE` | Archive uygulamasının tek örneği |
| `SETTINGS` | Sistem ayarları menüsünde görünür |
| `STARTUP` | Sistem açılışında çalışan callback |
| `EXTERNAL` | `.fap` plugin dosyası olarak derlenir (bizim asıl kullanacağımız tip) |
| `METAPACKAGE` | Çalıştırılabilir kod içermez, sadece bağımlılık bildirir |

### Opsiyonel parametreler (tüm uygulamalar için)

| Parametre | Açıklama |
|---|---|
| `name` | Menüde görünecek isim |
| `entry_point` | C giriş fonksiyonu adı (C++ ise `extern "C"` ile sarılmalı) |
| `cdefines` | Uygulama dahil edildiğinde global tanımlanacak C preprocessor define'ları |
| `requires` | Build'e dahil edilecek bağımlı app ID listesi |
| `conflicts` | Bu uygulamayla çakışan app ID'leri; varsa build başarısız olur |
| `provides` | `requires` ile işlevsel olarak aynı |
| `stack_size` | Başlangıçta uygulama için ayrılan stack (byte) |
| `icon` | Animasyonlu ikon için gömülü asset adı |
| `order` | Grup içi sıralama (küçük değer önce görünür) |
| `sdk_headers` | Harici uygulamalara açılan header dosyaları |
| `targets` | Uyumlu hedef isimleri; varsayılan `["all"]` |
| `resources` | SD karta paketlenecek kaynak klasörü adı |

### Yalnızca harici uygulamalar (FAP) için parametreler

| Parametre | Açıklama |
|---|---|
| `sources` | Kaynak dosya maskeleri; varsayılan `["*.c*"]` |
| `fap_version` | Sürüm string/tuple (x,y); varsayılan `"0.1"` |
| `fap_icon` | 10x10px, 1-bit PNG, FAP içine gömülür |
| `fap_libs` | Ek bağlanacak kütüphaneler |
| `fap_category` | Alt kategori / dosya sistemi yolu |
| `fap_description` | Kısa açıklama metni |
| `fap_author` | Yazar adı |
| `fap_weburl` | Proje ana sayfası URL'i |
| `fap_icon_assets` | Görsel assetlerin bulunduğu klasör (build sırasında ön işlenir) |
| `fap_extbuild` | `ExtFile` tanımlarıyla harici build tool desteği |
| `fal_embedded` | Yalnızca `PLUGIN` tipi için: plugin'i host app FAP'ına gömme (bool) |
| `fap_private_libs` | `Lib()` tanımlarıyla uygulamayla birlikte derlenen dahili kütüphaneler |

### Örnek manifest

```python
App(
    appid="bt_settings",
    name="Bluetooth",
    apptype=FlipperAppType.SETTINGS,
    entry_point="bt_settings_app",
    stack_size=1 * 1024,
    requires=["bt", "gui"],
    order=10,
)
```

Bizim gibi bağımsız/harici bir GUI uygulaması için tipik iskelet muhtemelen şöyle olacak:

```python
App(
    appid="marauder_gui",
    name="Marauder GUI",
    apptype=FlipperAppType.EXTERNAL,
    entry_point="marauder_gui_app",
    stack_size=2 * 1024,
    requires=["gui"],
    fap_category="GPIO",          # örnek; uygulamanın amacına göre değişir
    fap_icon="marauder_gui.png",
    fap_icon_assets="images",
    fap_version="0.1",
    fap_author="mel4mi",
    fap_description="ESP32 Marauder companion GUI for Flipper Zero",
)
```

### Özel kütüphaneler (`lib` klasörü)

`lib` klasörüne konan kütüphaneler `Lib()` tanımıyla yapılandırılır. Parametreler: `name`, `fap_include_paths` (varsayılan `["."]`), `sources`, `cflags`, `cdefines`, `cincludes`.

---

## 7. Derleme ve Çalıştırma

### Yöntem 1 — Otomatik (önerilen)
1. Flipper Zero'yu USB ile bağla
2. Çalıştır:
   ```
   ufbt launch
   ```
   Bu komut derler, cihaza yükler ve uygulamayı başlatır.

### Yöntem 2 — Manuel derleme
1. VS Code'da `Shift+Ctrl+B` (Win/Linux) ya da `Shift+Cmd+B` (macOS) ile derle
2. Çıktı olarak bağımsız bir `.fap` dosyası üretilir
3. qFlipper üzerinden: **File Manager → SD Card → apps → Examples**
4. Derlenen `.fap` dosyasını bu klasöre sürükle-bırak
5. Flipper'da: **Apps → Examples →** uygulamayı seç

### SDK sürüm uyumu
Cihazın firmware SDK sürümü, uFBT'nin SDK sürümüyle eşleşmeli. Uyuşmazlık varsa:
```
ufbt flash_usb
```

### Konsol log takibi
`FURI_LOG_I` çıktısını görmek için:
```
ufbt cli
```
ardından konsolda:
```
log
```
veya
```
log info
```

---

## 8. İlgili Dokümantasyon Sayfaları (ileride bakılabilir)

- FBT dokümantasyonu — `fbt.html`
- App Manifests (detaylı) — `app_manifests.html`
- GUI in Apps — `app_dev_gui.html`
- Debugging FAPs — `app_dev_debugging_faps.html`
- Application Development Hub — `applications.html`

Ek topluluk kaynakları (resmi dokümantasyonda önerilen):
- Christopher Hranj — Flipper Zero GUI modüllerine görsel rehber
- Derek Jamison — Scenes Demo Application eğitimi
- YouTube üzerinde ViewPort ve Scenes implementasyon anlatımları

Referans kitap: *The C Programming Language* (Kernighan & Ritchie)

---

## 9. Sonraki Adım İçin Notlar

Bir sonraki adımda birlikte gerçek bir GUI uygulaması yazacağız. Karar vermemiz gereken noktalar:

1. **Mimari seçimi:** ViewPort mu (basit/tek ekran) yoksa Scene Manager mı (çoklu ekran/menü)?
2. **Uygulamanın amacı:** Marauder-GUI-Flipper adından yola çıkarsak, muhtemelen bir ESP32 Marauder ile UART/Bluetooth üzerinden haberleşen bir companion app — bu durumda `gui` servisinin yanında `expansion` (GPIO/UART) veya seri port erişimi gibi ek servislere de ihtiyaç olabilir.
3. **`application.fam`** dosyasında `requires` listesine hangi sistem servislerinin ekleneceği bu karara göre netleşecek.

Bu rehber, yukarıdaki kararları verirken ve kodu yazarken referans olarak kullanılacak.
