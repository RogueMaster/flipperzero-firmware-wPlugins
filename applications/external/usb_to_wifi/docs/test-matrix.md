# MVP test matrisi

Varsayılan otomatik testler canlı internete istek göndermez ve macOS ağ
ayarlarını değiştirmez. Ağ davranışı enjekte edilen seri/HTTP katmanlarıyla;
gerçek byte-stream parçalanması ise Python PTY simülatörüyle sınanır.

## Bu çalışma ortamındaki gerçek sonuçlar

| Katman | Sonuç | Not |
| --- | --- | --- |
| Portable C codec/parser | **PASS** | `-Wall -Wextra -Werror -pedantic`; ayrıca ASan ve UBSan |
| Python codec + PTY simülatörü | **19/19 PASS** | Gerçek pseudo-terminal üzerinden parçalı okuma/yazma dahil |
| Flipper uFBT build | **PASS** | Target 7, API 87.1; FAP 34.312 bayt |
| Swift warnings-as-errors tip kontrolü + arm64 link | **PASS** | BridgeCore ve menu-bar executable; linked helper 260.568 bayt |
| Repo Swift test yöntemleri | **58/58 PASS** | Derlenmiş geçici XCTest-uyumlu explicit runner |
| Standart `swift test` | **ORTAM NEDENİYLE BLOKE** | CLT `PackageDescription` ABI/modulemap uyumsuzluğu manifest aşamasında; tam Xcode gerekir |
| Fiziksel Flipper ↔ Mac uçtan uca | **ÇALIŞTIRILMADI** | Geliştirme sırasında bağlı Flipper yoktu |

Geçici runner repo içindeki 58 test yöntemini tek tek çağırıp çalıştırdı. Standart
SwiftPM runner'ın manifest aşamasında bloklanması proje testlerinin sonucu
değildir; yine de normal Xcode/`swift test` koşusu başka bir makinede tekrarlanmalıdır.
Donanım testi simülatör başarısıyla eşitlenmez.

## Senaryo kapsamı

| Senaryo | Otomatik katman/test kaynağı | Beklenen değişmez |
| --- | --- | --- |
| Başarılı handshake | Swift coordinator + Python rol motorları | HELLO/ACK nonce ve sürüm eşleşmeden izin yok |
| Uyumsuz protokol | Swift coordinator test kaynağı | `UNSUPPORTED_VERSION`; prompt/network görevi yok |
| Bir kez izin | Swift coordinator | Yalnız mevcut seri bağlantı nesli için READY |
| İzin reddi | Swift coordinator | HTTP client hiç çağrılmaz |
| Kalıcı izin | Swift permission store | Hash'lenmiş UID/sürüm anahtarı grant'i geri yükler |
| İzin kaldırma | Swift coordinator/store | Grant silinir ve aktif görev iptal edilir |
| Başarılı HTTPS GET | Swift mock HTTP/coordinator | Start/header/chunk/end sırası korunur |
| HTTPS POST + header/body chunk | Swift mock HTTP/coordinator | Yalnız negotiated capability ve limitler içinde |
| HTTP 404 | Swift mock HTTP/coordinator | Geçerli response status olarak aktarılır |
| Sunucu zaman aşımı | Swift coordinator + mock HTTP completion | Timeout sonucu terminal FIBP ERROR'a eşlenir |
| DNS çözümlemesinde zaman aşımı | Gerçek HTTPSNetworkClient + blocking resolver test kaynağı | Toplam request deadline DNS süresini kapsar; DNS bitse de geç task başlamaz |
| İstek sırasında USB kaybı | Swift coordinator mock transport | Network iptal; once-grant/request/session sıfırlanır |
| Bozuk frame | C, Swift ve Python codec/parser | CRC öncesi dispatch yok; parser yeniden senkronize olur |
| Bozuk frame içine gömülü geçerli frame | C parser regresyonu | Sabit 544 B tamponda suffix recovery; taşma yok |
| Eksik frame | Swift parser timeout testi + Python partial-frame codec testi | Bounded tampon; Swift/FAP'te 1 s reset, Python'da explicit partial-frame raporu |
| Eksik mantıksal request | Swift coordinator | 5 s sonra aktif slot serbest ve TIMEOUT |
| Yanlış payload uzunluğu | C, Swift ve Python codec | Büyük allocation/bekleme olmadan reddedilir |
| Fazla büyük response | Swift mock HTTP/coordinator | 4 MiB sınırında truncate/cancel terminal sonucu |
| Düz HTTP | Swift URL policy | URLSession'dan önce `SECURITY_BLOCKED` |
| localhost | Swift URL policy | DNS/connect öncesi engellenir |
| Private/link-local/ULA/CGNAT IP | Swift address policy | Public olmayan IPv4/IPv6 reddedilir |
| Public+private karışık DNS cevabı | Swift address policy | Cevaptaki tek private adres bile hedefi engeller |
| Private hedefe redirect | Swift NetworkPolicy + injected resolver | Redirect hedefi aynı DNS/IP politikasında reddedilir |
| İptal | Swift coordinator test kaynağı | HTTP task iptal edilir ve doğru CANCEL frame'i gönderilir |
| Tekrarlanan request ID | Swift coordinator test kaynağı | İkinci network side effect yok |
| Reddedilen benzersiz request-ID seli | Swift coordinator | Reddedilen ID'ler tutulmaz/poison etmez; ilk kabul edilen ID tek scalar watermark'ı kurar |
| Duplicate/gap sequence | Swift coordinator test kaynağı | Yalnız ilgili istek etkilenir; process yaşamaya devam eder |
| Uzak ERROR | Swift coordinator test kaynağı + FAP implementation review | Framing-geçerli expected ERROR control sequence tüketilir; ERROR'a ERROR dönülmez |
| PONG gelmemesi | Swift coordinator | 2 s sonra stale bağlantı ve aktif iş kapatılır |
| Parser/RX overflow | Swift coordinator + C parser | Aktif istek iptal; process çökmez |

## Donanım üzerinde yapılması gerekenler

1. Resmî firmware üzerinde dual-CDC yeniden enumeration.
2. IOKit'in bridge kanalını beklenen interface numarasıyla raporlaması.
3. DTR'nin helper port open/close durumunu doğru izlemesi.
4. Gerçek URLSession isteği sürerken kablo çekmenin görevi hemen iptal etmesi.
5. FAP kapanışında önceki USB configuration'ın dönmesi ve qFlipper/CLI'ın
   çalışmaya devam etmesi.
6. İmzalı/sandboxed dağıtım seçilirse serial ve network entitlement testi.

Bu maddeler bağlı bir Flipper ve normal bir Xcode kurulumu olmadan **PASS** olarak
işaretlenemez.
