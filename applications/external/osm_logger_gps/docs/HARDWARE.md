# Matériel & câblage

## Modules supportés

- **GPS NEO-6M V2** (u-blox NEO-6) — sortie NMEA 0183, 9600 bauds, 1 Hz
- Antenne céramique (fournie avec le module) ou antenne active externe via U.FL

> D'autres modules NMEA devraient marcher (PA1010D, BN-180, NEO-M8N, etc.). Le **baud rate est configurable** via Settings → `GPS baud` (4800 / 9600 / 19200 / 38400 / 57600 / 115200). En cas de test réussi avec un autre module, ouvrir une PR pour mettre à jour cette liste.

## Câblage sur Flipper Zero

Le Flipper Zero expose son **USART1** sur les pins 13 (TX) et 14 (RX) du GPIO principal. L'app utilise `FuriHalSerialIdUsart` = USART1.

### Schéma (3 fils)

```
  Flipper Zero GPIO                          NEO-6M GPS
 ┌────┬──────────┐                          ┌──────────┐
 │  9 │ 3V3      ├──────── 🔴 red ────────▶ │ VCC      │
 │ 10 │ SWC      │                          │          │
 │ 11 │ GND      ├──────── ⚫ black ──────▶ │ GND      │
 │ 12 │ SIO      │                          │          │
 │ 13 │ TX       │                          │ RX ◁ ── non utilisé (voir note)
 │ 14 │ RX       │◀─────── 🟢 green ─────── │ TX       │
 │ 15 │ C1       │                          └──────────┘
 │ 16 │ C0       │
 │ 17 │ 1W       │
 │ 18 │ GND      │
 └────┴──────────┘
```

### Tableau récapitulatif

| NEO-6M | Fil | Flipper GPIO     | Rôle                                   |
|--------|-----|------------------|----------------------------------------|
| VCC    | 🔴  | pin 9 (3V3)      | Alim 3,3 V                             |
| GND    | ⚫  | pin 11 (ou 18)   | Masse                                  |
| **TX** | 🟢  | **pin 14** (RX)  | NMEA GPS → Flipper                     |
| RX     | —   | —                | **Non utilisé** : on ne lit que le GPS |

### Pourquoi seulement 3 fils ?

L'app ne fait que **lire** les trames NMEA émises par le GPS (1 Hz). Elle ne renvoie jamais de commandes de configuration (ex. `$PMTK...` pour ublox) — le module fonctionne avec ses réglages par défaut. Donc la pin RX du GPS peut rester en l'air.

Pour configurer le module (passer en 10 Hz, désactiver des trames, etc.), il faudra relier le 4e fil : **pin 13 Flipper (TX) → RX GPS**.

**⚠️ 3,3 V uniquement.** Le NEO-6M accepte 3,3 à 5 V côté alim, mais son TX peut sortir en 5 V si alimenté en 5 V → destruction possible de l'UART Flipper. Alimente-le impérativement en 3,3 V.

> Pinout complet du Flipper : https://docs.flipper.net/gpio-and-modules

## Premier fix

Un GPS froid peut mettre **30 s à 5 min** pour obtenir son premier fix. Pour l'accélérer :
- Extérieur, vue dégagée du ciel (pas sous arbres, pas derrière vitres doublées, pas à l'ombre d'un bâtiment)
- Antenne face au ciel
- Laisser le module alimenté en continu — les éphémérides sont perdues à l'extinction

Après le premier fix en extérieur, le module garde un warm start de quelques minutes.

## Diagnostic : pas de données NMEA

1. **Fils inversés** — le symptôme n°1. Vérifier : TX du GPS → pin 14 Flipper.
2. **Alim absente** — LED rouge du NEO-6M doit clignoter 1 Hz quand fix OK, et reste allumée sans fix.
3. **Conflit UART** — sur Momentum/autres firmwares, le service *Expansion* intercepte l'UART. L'app le désactive automatiquement, mais en cas de spam `ExpansionSrvWorker` dans les logs, ouvrir une issue.
4. **qFlipper tourne** — il monopolise le port série lors du `ufbt launch`. Le fermer avec Cmd+Q.
5. **Baud rate mismatch** — si le module n'est pas à 9600, ajuster Settings → `GPS baud`.

### Observer l'arrivée des trames

Le moyen le plus rapide : aller dans **Menu → GPS status** et regarder la ligne `NMEA: X B / Y lines`. Ce compteur monte en temps réel tant que l'UART reçoit. Trois cas :

- **`0 B / 0 lines`** qui ne bougent pas → aucun octet reçu (fil, alim, module déconnecté)
- **`X B / 0 lines`** qui monte mais Y=0 → octets reçus mais trames pas formées → baud rate faux
- **X et Y montent** → l'UART marche, c'est juste la qualité du fix qui est mauvaise (va dehors, attends)

### Logs détaillés via CLI

```bash
ufbt cli
> log debug
```

L'app logge sous le tag `OSM` :
- `[I][OSM] GPS fix acquired` à chaque transition no-fix → fix (cooldown 10 s)
- `[D][OSM] save: ...`, `[D][OSM] write: ...` pour tracer chaque étape d'une sauvegarde
- `[E][OSM] ...` pour les erreurs critiques (SD, allocations, ...)
