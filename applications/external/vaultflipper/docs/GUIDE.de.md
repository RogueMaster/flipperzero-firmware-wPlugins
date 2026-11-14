# VaultFlipper – Anleitung

*English version: [GUIDE.md](GUIDE.md)*

Diese Anleitung führt dich von null bis zum ersten Passwort, das der Flipper für dich tippt, und erklärt die tägliche Nutzung, mehrere Computer, Updates und Fehlerbehebung.

**Inhalt**

1. [Was du brauchst](#1-was-du-brauchst)
2. [Flipper-App installieren](#2-flipper-app-installieren)
3. [Begleit-Tool installieren](#3-begleit-tool-installieren)
4. [Ersteinrichtung](#4-ersteinrichtung)
5. [VaultFlipper benutzen](#5-vaultflipper-benutzen)
6. [Auf verschiedenen Computern tippen](#6-auf-verschiedenen-computern-tippen)
7. [Den Flipper aktuell halten](#7-den-flipper-aktuell-halten)
8. [Konten, Organisationen und Filter](#8-konten-organisationen-und-filter)
9. [Sicherheit kurz erklärt](#9-sicherheit-kurz-erklärt)
10. [Fehlerbehebung](#10-fehlerbehebung)
11. [VaultFlipper entfernen](#11-vaultflipper-entfernen)

---

## 1. Was du brauchst

| | |
| --- | --- |
| Flipper Zero | offizielle Firmware 1.5 oder neuer ([Update](https://flipperzero.one/update)) |
| Computer | macOS, Linux oder Windows, USB-Kabel zum Flipper |
| Python | 3.11 oder neuer, dazu [uv](https://docs.astral.sh/uv/getting-started/installation/) |
| Bitwarden CLI | `bw` – `brew install bitwarden-cli` oder `npm install -g @bitwarden/cli` |
| Ein Tresor | bitwarden.com, bitwarden.eu oder ein eigener Bitwarden-/Vaultwarden-Server |

## 2. Flipper-App installieren

**Aus einem Release (empfohlen).** `vaultflipper.fap` von der [Release-Seite](https://github.com/its-sven/vaultflipper/releases) laden. In qFlipper im Dateimanager nach `SD Card/apps/Tools/` kopieren.

**Aus dem Quellcode.**

```bash
git clone https://github.com/its-sven/vaultflipper.git
cd vaultflipper
make launch        # baut mit ufbt, installiert und startet die App
```

Einmal **Apps ▸ Tools ▸ VaultFlipper** öffnen. Ohne Tresor zeigt die App eine **Geräte-ID** wie `C74D-75C4` – die vergleichst du in Schritt 4.

## 3. Begleit-Tool installieren

```bash
uv tool install "git+https://github.com/its-sven/vaultflipper#subdirectory=companion"
vaultflipper --version
```

`pipx install "git+https://github.com/its-sven/vaultflipper#subdirectory=companion"` geht auch. Bei deutscher Systemsprache spricht das Tool Deutsch.

## 4. Ersteinrichtung

qFlipper und die VaultFlipper-App schließen – nur ein Programm kann den USB-Anschluss des Flippers nutzen – dann:

```bash
vaultflipper setup
```

Der Assistent hat fünf Schritte:

| Schritt | Was passiert |
| --- | --- |
| 1 · Bitwarden CLI | prüft, ob `bw` installiert ist |
| 2 · Konten | fragt nach einem Namen (erscheint am Flipper), dem Server und der Anmeldeart und meldet dich mit `bw` an |
| 3 · Koppeln | liest den Geräteschlüssel per USB und zeigt die Geräte-ID – sie muss mit der am Flipper übereinstimmen |
| 4 · Tastaturlayout | erkennt das Layout dieses Computers, z. B. `de-DE-mac` |
| 5 · Synchronisieren | entsperrt alle Konten, fragt nach einer **PIN**, verschlüsselt und überträgt den Tresor |

**Anmeldung.** *E-Mail und Master-Passwort*, *Single Sign-on (SSO)* – der Browser öffnet die Firmen-Anmeldung – oder *API-Schlüssel*. Geheimnisse tippst du immer direkt in `bw` oder den Browser; VaultFlipper sieht dein Master-Passwort nie.

**PIN wählen.** Am Computer die Pfeiltasten drücken (oder `U D L R` tippen), mindestens 8-mal, dann Enter. Dieselbe Folge entsperrt den Flipper. Nimm **12 oder mehr** Tasten – siehe [Sicherheit](#9-sicherheit-kurz-erklärt).

## 5. VaultFlipper benutzen

### Entsperren

**Apps ▸ Tools ▸ VaultFlipper** öffnen, Pfeilfolge drücken, dann **OK**. **Zurück** löscht den letzten Druck. Entsperren dauert etwa eine Sekunde.

### Login finden

| Bildschirm | Inhalt |
| --- | --- |
| Start | deine Konten mit Anzahl der Logins und Ordner, dann *2FA-Codes*, *Suchen*, *Einstellungen*, *Sperren* |
| Konto | Ordner und Collections; große Ordner sind aufgeteilt, z. B. *Mail* · `Am – Ko` |
| Ordner | die Logins, jeweils mit Benutzername (oder der Website, wenn keiner hinterlegt ist) |
| Suchen | Teil von Name, Benutzer oder URL eingeben; bis zu 50 Treffer über alle Konten, mit Benutzernamen |

![Ordner mit Benutzernamen](../flipper/vaultflipper/docs/screenshots/4-entries.png)

In Menüs klappt die ausgewählte Zeile auf und zeigt Details; gibt es mehr, schaut unten die nächste Zeile an.

### Die Eintragskarte

![Eintragskarte](../flipper/vaultflipper/docs/screenshots/5-card.png)

| Taste | Aktion |
| --- | --- |
| **OK** | Benutzer, Tab, Passwort, Enter tippen |
| **◀ Links** | Benutzername tippen |
| **▶ Rechts** | Passwort tippen |
| **▲ Hoch** | Passwort zeigen oder verbergen |
| **▼ Runter** | mehr: Login ohne Enter, einzelne Felder, alle Details |
| **Zurück** | zurück zur Liste |

Vorher am Computer in das Login-Feld klicken; der Flipper tippt dort, wo der Cursor steht. Die LED blinkt grün, wenn das Tippen fertig ist.

### 2FA-Codes

Haben Logins in deinem Tresor einen 2FA-Schlüssel (TOTP), zeigt der Startbildschirm **2FA-Codes**. Die Liste enthält alle diese Logins aus allen Konten, nach Namen sortiert, mit aktuellem Code und Benutzername. Die Linie unter dem Titel wird kürzer, und die Sekunden rechts zählen herunter, bis die Codes wechseln.

![2FA-Codes](../flipper/vaultflipper/docs/screenshots/8-codes.png)

Ein Eintrag öffnet den Code groß mit Countdown – gut ablesbar, wenn du ihn selbst am Handy eintippst.

![2FA-Code](../flipper/vaultflipper/docs/screenshots/9-code.png)

| Taste | Aktion |
| --- | --- |
| **OK** | Code tippen (ohne Enter) |
| **◀ ▶** | voriger oder nächster Login |
| **Zurück** | zurück zur Liste |

Von der Eintragskarte aus gibt es dieselbe Ansicht unter **▼ ▸ 2FA-Code zeigen**. SHA-1, SHA-256, SHA-512 und Steam Guard funktionieren; zählerbasierte Schlüssel (HOTP) lässt der Sync weg.

**Uhr.** Die Codes hängen von der Uhrzeit ab. Der Flipper führt Ortszeit, gestellt von qFlipper oder der Flipper-App am Handy; `vaultflipper sync` misst den Abstand zu UTC und legt ihn auf dem Flipper ab. Nach der Zeitumstellung erneut synchronisieren oder unter *Einstellungen ▸ Zeitzone* die richtige Zone wählen.

### Sperren

Überall **Zurück** gedrückt halten, auf dem Startbildschirm **Sperren** wählen oder zwei Minuten warten. Auch das Verlassen der App sperrt. Gesperrt ist der Flipper wieder ein normales USB-Gerät.

## 6. Auf verschiedenen Computern tippen

Entsperrt ist der Flipper eine USB-Tastatur namens **VaultFlipper Keyboard** – wie ein Security-Key. Eine Tastatur sendet Tasten*positionen*, deshalb muss der Flipper das Layout des Computers verwenden, in den er tippt.

**Layout wählen:** *Einstellungen ▸ Tastaturlayout*. Alle BadUSB-Layouts sind dabei, dazu `de-DE-mac` für deutsche Macs. Die Wahl bleibt gespeichert.

**Prüfen:** `vaultflipper typetest` starten, Terminal im Vordergrund lassen und am Flipper *Einstellungen ▸ Tipptest* wählen. Jedes falsch angekommene Zeichen wird angezeigt.

**Mac – nur beim ersten Mal:** macOS öffnet beim Verbinden den *Tastatur-Assistenten*. *Fortfahren* klicken, am Flipper *Einstellungen ▸ Mac-Tastatur einrichten* wählen und zweimal **Taste senden** drücken. Den vorgeschlagenen Typ bestätigen und *Fertig* klicken. macOS merkt sich das.

**Handys:** per USB-C-/OTG-Adapter verbinden. Unter Android in den Systemeinstellungen das Layout der physischen Tastatur einstellen; iPhones mit USB-C nehmen den Flipper als Tastatur an.

**Nicht tippbare Zeichen:** Umlaute, `€` und andere Nicht-ASCII-Zeichen haben keine feste Tastenposition. Das Begleit-Tool warnt bei Logins, die solche Zeichen enthalten; der Flipper lässt sie aus und zeigt danach *Nicht alles getippt* mit der Zahl der fehlenden Zeichen.

## 7. Den Flipper aktuell halten

Nach Änderungen in Bitwarden die Flipper-App sperren und:

```bash
vaultflipper sync
```

`vaultflipper status` zeigt, ob `bw` funktioniert, welche Konten angemeldet sind und ob der angesteckte Flipper der gekoppelte ist. Zeigt VaultFlipper einmal eine andere Geräte-ID (neuer Flipper, Enclave zurückgesetzt), `vaultflipper pair` erneut ausführen.

## 8. Konten, Organisationen und Filter

```bash
vaultflipper account list
vaultflipper account add Arbeit --server https://vault.example.com --login sso
vaultflipper account add Familie --server https://vault.bitwarden.eu --collection "Familie/Streaming"
vaultflipper login Arbeit --sso
vaultflipper account remove Alt
```

| Option | Wirkung |
| --- | --- |
| `--folder NAME` | nur diese persönlichen Ordner (mehrfach möglich) |
| `--collection Org/Collection` | nur diese Collections einer Organisation (mehrfach möglich) |
| `--favorites-only` | nur Favoriten |
| `--no-orgs` | geteilte Organisations-Einträge weglassen |
| `--no-totp` | 2FA-Schlüssel weglassen – keine Codes am Flipper |

Nur mit `--folder` kommen keine Organisations-Einträge mit, nur mit `--collection` keine persönlichen. Beides zusammen kombiniert sie. Einträge, die mit mehreren deiner Konten geteilt sind, erscheinen nur einmal. Ein Tresor fasst rund 20.000 Logins in wenigen Ordnern oder etwa 350 Ordner; die Grenze ist das Ordnerverzeichnis (24 KB).

**Single Sign-on:** `vaultflipper login NAME --sso` öffnet die Firmen-Anmeldung; danach fragt `bw` nach dem Master-Passwort zum Entschlüsseln. Bei Vaultwarden wird die SSO-Kennung automatisch gesetzt. Konten ganz ohne Master-Passwort (Bitwarden nur mit *vertrauenswürdigen Geräten*) kann die Bitwarden CLI nicht entsperren.

## 9. Sicherheit kurz erklärt

- Die Tresor-Datei öffnet sich nur auf **deinem** Flipper und nur mit **deiner** PIN: Der Schlüssel verbindet ein Geheimnis aus dem Krypto-Chip des Flippers mit deiner PIN (Argon2id).
- Entschlüsselt wird nur der geöffnete Ordner, und nur im Arbeitsspeicher. Sperren löscht ihn.
- Ab der 3. falschen PIN bremst die App, nach 10 löscht sie den Tresor.
- Der Flipper hat keinen Secure Boot: Wer dein Gerät hat, kann eine andere Firmware aufspielen und am Gerät etwa eine PIN pro Sekunde raten. Mit 12 Tasten dauert das Monate. Geht der Flipper verloren, ändere die wichtigsten Passwörter.
- Tippe nur an Computern, denen du vertraust – eine Tastatur kann der Computer mitlesen, an dem sie steckt.
- Mit 2FA-Codes liegt der zweite Faktor auf demselben Gerät wie das Passwort – wie bei Bitwarden selbst, wenn es TOTP speichert. Willst du beides trennen, das Konto mit `--no-totp` anlegen.

Details: [SECURITY.md](../SECURITY.md) und [FORMAT.md](FORMAT.md).

## 10. Fehlerbehebung

| Problem | Lösung |
| --- | --- |
| *Kein Flipper gefunden* | qFlipper schließen und VaultFlipper sperren oder beenden, dann erneut versuchen |
| *Konto … ist nicht angemeldet* | `vaultflipper login NAME` (mit `--sso` für Firmen-Anmeldung) |
| Text landet im falschen Fenster | vorher ins Zielfeld klicken; VMs (VMware, Parallels) schnappen sich neue USB-Tastaturen |
| Falsche Zeichen | anderes Layout wählen und `vaultflipper typetest` ausführen |
| *Kein USB-Host* am Flipper | USB-Kabel an Computer oder Handy stecken, nicht an ein Ladegerät |
| *Tresor ungültig – neu syncen* | Datei aus älterer Version oder beschädigt: `vaultflipper sync` |
| *Zu wenig Speicher* | andere Apps schließen, Flipper neu starten |
| 2FA-Codes werden abgelehnt | Zone unter *Einstellungen ▸ Zeitzone* prüfen oder `vaultflipper sync` ausführen, das misst die Uhr neu |
| *Uhr nicht gestellt* | Flipper einmal mit qFlipper verbinden (stellt die Uhr), dann `vaultflipper sync` |
| PIN vergessen | vom Computer neu synchronisieren und eine neue PIN wählen |

## 11. VaultFlipper entfernen

- Flipper: `apps/Tools/vaultflipper.fap` und den Ordner `apps_data/vaultflipper/` löschen.
- Computer: `uv tool uninstall vaultflipper` und `~/.config/vaultflipper/` löschen (meldet alle dort gespeicherten `bw`-Sitzungen ab).
