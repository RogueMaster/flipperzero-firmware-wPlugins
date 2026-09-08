# Geliştirme aşamaları ve doğrulama kaydı

Bu belge her aşamanın sonunda oluşan dosyaları, doğrulanmış davranışı ve bu
makinede doğrulanamayan noktaları ayırır. “PASS” yalnız gerçekten çalıştırılmış
komutlar için kullanılır.

## Aşama 1 — Tasarım

Değişen/eklenen dosyalar:

- `protocol/protocol-spec.md`, `protocol/message-types.md`, `protocol/test-vectors/v1.json`
- `docs/architecture.md`, `docs/threat-model.md`, `docs/future-research.md`
- `config.h`

Bulunan mevcut durum:

- FAP, uFBT ile derlenen bağımsız C kaynaklarından oluşuyor.
- FAP manifesti yalnız USB internet bridge kaynaklarını derliyor.
- Yerel resmî SDK release 1.4.3/API 87.1 içinde `usb_cdc_dual`,
  `furi_hal_cdc_*`, USB config ve donanım UID/name/model API'leri doğrulandı.
- Resmî USB-UART Bridge örneği uygulama trafiği için dual-CDC kanal 1'i, CLI için
  kanal 0'ı kullanıyor; FAP aynı modeli izliyor.
- macOS seri erişimi için dependency'siz IOKit discovery + POSIX termios seçildi.

Kesinleşen mimari:

```text
Flipper UI -> session -> FIBP codec -> USB CDC IF 1
                                      |
macOS SwiftUI <- coordinator <- POSIX/IOKit
                         |
                 permission + SSRF policy -> ephemeral URLSession -> HTTPS
```

Doğrulanamayan risk: donanım bağlı olmadığı için gerçek interface numarası,
DTR ve re-enumeration gözlenemedi. UID cihaz ilişkilendirme anahtarıdır,
kriptografik attestation değildir.

Sonraki adım: normatif wire formatı portable codec ve HELLO/PING/PONG ile iki
tarafta uygulamak.

## Aşama 2 — Haberleşme prototipi

Değişen/eklenen dosyalar:

- `bridge_protocol.[ch]`: 28 B header, iki CRC32, encode/decode ve bounded parser
- `usb_transport.[ch]`: dual-CDC kanal 1, worker, RX stream, serialized TX
- `bridge_session.[ch]`: HELLO/ACK, permission state ve PING/PONG state machine
- `scripts/fibp_codec.py`, `scripts/fibp_simulator.py`: PTY peer ve fault injection
- `tests/test_bridge_protocol.c`, `tests/test_fibp_codec.py`,
  `tests/test_fibp_simulator.py`

Çalışan davranış:

- CDC read sınırlarından bağımsız binary framing ve magic resync.
- Header CRC doğrulanmadan payload uzunluğuna güvenmeme; frame CRC doğrulanmadan
  semantic dispatch yapmama.
- Bozuk dış frame içine gömülü geçerli frame'i sabit 544 B buffer içinde kurtarma.
- HELLO/HELLO_ACK nonce/version/limit negotiation ve permission-gated PING/PONG.
- 1 s eksik-frame, 5 s HELLO, 30 s idle ve 2 s PONG zaman aşımı.

Çalıştırılan doğrulama:

```text
portable C codec/parser: PASS
portable C ASan + UBSan: PASS
Python codec/simülatör: 19/19 PASS
clang-format --dry-run --Werror (yeni C dosyaları): PASS
```

Sonraki adım: kullanıcı izni olmadan hiçbir request'in network katmanına
ulaşamayacağı macOS permission akışını eklemek.

## Aşama 3 — İzin sistemi

Değişen/eklenen dosyalar:

- `macos/Sources/BridgeCore/Permissions/PermissionStore.swift`
- `macos/Sources/BridgeCore/Session/BridgeCoordinator.swift`
- `macos/Sources/FlipperInternetBridge/MacPermissionPrompter.swift`
- `macos/Sources/FlipperInternetBridge/BridgeMenuModel.swift`
- ilgili `BridgeCoordinatorTests` ve `PayloadAndPermissionTests`

Çalışan davranış:

- Geçerli HELLO ve cihaz kimliği gelmeden izin penceresi yok.
- Reddet, bir kez izin ver ve her zaman izin ver kararları.
- Kalıcı grant `(permission schema, id type, UID, protocol)` hash'iyle UserDefaults
  içinde tutuluyor; raw UID ve herhangi bir secret saklanmıyor.
- İzin request başlangıcında ve URLSession oluşturulmadan hemen önce tekrar
  kontrol ediliyor. Revoke/kablo kaybı aktif işi iptal ediyor.
- Menüden mevcut cihaz için grant verme/kaldırma ve aktif isteği iptal etme.

Doğrulanamayan nokta: NSAlert sheet davranışı gerçek menu-bar uygulaması ve tam
Xcode altında görsel olarak sınanmadı.

Sonraki adım: bounded GET/POST request assembly ve chunk'lı response akışı.

## Aşama 4 — İnternet isteği

Değişen/eklenen dosyalar:

- `macos/Sources/BridgeCore/Networking/HTTPSNetworkClient.swift`
- `macos/Sources/BridgeCore/Protocol/FIBPPayloads.swift`
- `usb_internet_bridge.c`, `bridge_session.[ch]`
- Swift coordinator/network test hedefleri

Çalışan davranış:

- Dağıtılan FAP HTTPS GET gönderir; capability negotiated eden istemciler için
  helper POST, güvenli request header ve body chunk destekler.
- HTTP status, allow-list response header, 192 B body chunk ve terminal result.
- Mac en çok 16 KiB aktarır; Flipper yalnız ilk 768 B preview'ı tutar.
- Flipper menüsünde bağlantı testi, örnek metin, UTC zaman, özel URL ve bağlantı
  bilgileri; aktif istek ekranındaki merkez **İptal** düğmesiyle CANCEL.
- Aynı anda tek aktif request; monoton non-zero ID ve exact directional sequence.
- 404 geçerli response olarak, timeout/cancel/oversize ayrı terminal durum olarak
  modellenir.

Doğrulanamayan nokta: gerçek endpoint ↔ URLSession ↔ USB ↔ ekran zinciri bağlı
Flipper olmadan uçtan uca çalıştırılmadı.

Sonraki adım: SSRF, redirect, credential isolation, parser/transport failure ve
unplug dayanıklılığını kapatmak.

## Aşama 5 — Güvenlik ve dayanıklılık

Değişen/eklenen dosyalar:

- `macos/Sources/BridgeCore/Networking/NetworkPolicy.swift`
- `macos/Sources/BridgeCore/Serial/IOKitSerialDeviceMonitor.swift`
- `macos/Sources/BridgeCore/Serial/POSIXSerialTransport.swift`
- session/parser/transport sağlamlaştırmaları ve regresyon testleri

Çalışan davranış:

- Yalnız HTTPS; embedded credential, localhost/`.local`, private, loopback,
  link-local, ULA, CGNAT, multicast, reserved ve mixed public/private DNS engeli.
- Her redirect hedefinde DNS/IP politikası yeniden uygulanır; en çok üç redirect.
- Cookie/cache/credential store olmayan ephemeral URLSession, sabit User-Agent,
  default TLS trust ve auth challenge kısıtlaması.
- Bozuk/duplicate/gap paket process'i çökertmez; ERROR-to-ERROR döngüsü yok.
- Request replay geçmişi sınırsız Set yerine tek monoton `UInt32` ile bounded.
- Bir frame'in CDC TX işlemi tek absolute deadline kullanır; USB loss/parser
  overflow/assembly timeout aktif işi ve session state'ini temizler.

Kalan güvenlik sınırı: URLSession doğrulanan `getaddrinfo` sonucuna socket'i bind
etmediği için DNS validation ile TLS connect arasında TOCTOU tamamen kapatılamaz.
Ayrıca `getaddrinfo` iptal edilebilir bir API değildir; bir resolver çağrısı işletim
sistemi düzeyinde takılırsa network policy kuyruğu helper yeniden başlatılana kadar
gecikebilir. Request deadline yine çağırana zamanında timeout döndürür ve geç
sonuç network görevi başlatamaz.

Sonraki adım: bütün build/test yollarını yeniden çalıştırmak ve kurulum/hardware
matrisini belgelemek.

## Aşama 6 — Dokümantasyon ve son doğrulama

Değişen/eklenen dosyalar:

- `README.md`
- `docs/development-status.md`, `docs/test-matrix.md`
- son protokol/test-vector ve regresyon düzeltmeleri
- `dist/usb_internet_bridge.fap`

Bu makinedeki son sonuçlar:

```text
uFBT FAP build: PASS — Target 7 / API 87.1
dist/usb_internet_bridge.fap: 34.312 bayt
portable C protocol tests: PASS
portable C ASan + UBSan: PASS
Python codec/simulator tests: 19/19 PASS
Swift BridgeCore + SwiftUI warnings-as-errors tip kontrolü: PASS
Swift arm64 helper link: PASS — 260.568 bayt Mach-O
Repo Swift test yöntemleri: 58/58 PASS — geçici XCTest-uyumlu runner
Standart swift test: BLOKE — CLT PackageDescription ABI/modulemap, manifest aşaması
physical Flipper end-to-end: ÇALIŞTIRILMADI — cihaz bağlı değildi
```

FAP build'inden sonra C/H dosyalarına yalnız `clang-format -i` ile mekanik
boşluk/satır düzeni uygulandı; semantik kaynak değişikliği yapılmadı. Aynı güncel
kaynakla ek bir uFBT koşusu platform onay kotası tarafından başlatılamadı. Teslim
artifact'i biçimlendirme öncesindeki semantik olarak aynı revizyondan üretilmiştir;
kullanıcı tarafındaki son doğrulama komutu yine `../../venv/bin/ufbt`'dir.

Tekrarlanabilir temel komutlar:

```sh
# Flipper
../../venv/bin/ufbt

# Portable C
mkdir -p .build-tests
clang -std=c11 -Wall -Wextra -Werror -pedantic -I. \
  bridge_protocol.c tests/test_bridge_protocol.c \
  -o .build-tests/test_bridge_protocol
./.build-tests/test_bridge_protocol

# Aynı portable suite için bellek/UB doğrulaması
clang -std=c11 -Wall -Wextra -Werror -pedantic \
  -fsanitize=address,undefined -fno-omit-frame-pointer -I. \
  bridge_protocol.c tests/test_bridge_protocol.c \
  -o .build-tests/test_bridge_protocol_san
./.build-tests/test_bridge_protocol_san

# Python
python3 -m unittest -v tests.test_fibp_codec tests.test_fibp_simulator

# Tam Xcode kurulmuş Mac
cd macos
swift test
swift build -c release
```

Normal bir Xcode kurulumu ve gerçek Flipper ile kalan manuel maddeler
`docs/test-matrix.md` altında listelenmiştir. MVP kaynak ve FAP teslimi tamamdır;
fiziksel başarı kabulü için o donanım matrisi ayrıca yürütülmelidir.
