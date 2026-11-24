# POCSAG TX — Flipper Zero

Encodeur et émetteur POCSAG (CCIR Rec. 584) pour Flipper Zero, en 2-FSK via le CC1101 interne.

- Catégorie : Sub-GHz — `appid` : `pocsag`
- Modes : simulation (sans émission) et émission réelle
- Débits : 512 / 1200 / 2400 bauds, polarité normale ou inversée
- Messages numériques ou alphanumériques (80 caractères max), RIC sur 21 bits
- Bandes SRD européennes uniquement, liste blanche codée en dur : 433 / 434 / 869 MHz (certaines limitées à 10 % de duty cycle)
- Historique des envois et réglages sauvegardés

## Installation

Copier `dist/pocsag.fap` dans `apps/Sub-GHz/` sur la carte SD du Flipper.

## Compilation

Avec le firmware Flipper (ufbt) :

```
~/.local/share/ufbt-venv/bin/ufbt          # produit dist/pocsag.fap
~/.local/share/ufbt-venv/bin/ufbt launch   # compile et lance sur le Flipper connecté
```

## Tests (PC)

L'encodeur (`pocsag.c`) n'a aucune dépendance au firmware :

```
gcc -Wall -Wextra -I. test/test_pocsag.c pocsag.c -o test_pocsag && ./test_pocsag
```

Validation par un décodeur tiers (multimon-ng) :

```
./test_pocsag --raw out.raw 1200 123456 a "Hello"
multimon-ng -t raw -a POCSAG512 -a POCSAG1200 -a POCSAG2400 out.raw
```

## Structure

| Fichier | Rôle |
|---|---|
| `pocsag.c/.h` | Encodeur (BCH, lots, mots de code) |
| `pocsag_tx.c/.h` | Émission radio CC1101 (TIM2/DMA) |
| `pocsag_app.c` | Interface (menu, RIC, message, simulation, émission) |

## Avertissement

N'émettre que sur des fréquences et avec une puissance autorisées dans votre pays, et uniquement vers du matériel dont vous êtes responsable.

## Licence

MIT — voir [LICENSE](LICENSE).
