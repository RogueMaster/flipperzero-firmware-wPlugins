# Time Clock - Flipper Zero

[![Build & Release](https://github.com/vladpereverzyev/flipper-timeclock/actions/workflows/build.yml/badge.svg)](https://github.com/vladpereverzyev/flipper-timeclock/actions/workflows/build.yml)
[![Latest release](https://img.shields.io/github/v/release/vladpereverzyev/flipper-timeclock?cacheSeconds=300)](https://github.com/vladpereverzyev/flipper-timeclock/releases)
[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](LICENSE)

[English](README.md) | [Italiano](README.it.md) | **Espanol** | [Francais](README.fr.md) | [Deutsch](README.de.md)

App de **control de fichajes** para el [Flipper Zero](https://flipperzero.one/).
Asigna a cada colaborador una **tarjeta en blanco** NFC o RFID, la acercas y el
fichaje (entrada/salida) se guarda con fecha y hora en la microSD como CSV.
Funciona de forma autonoma, sin telefono ni PC.

> **Solo uso autorizado.** La app solo lee el **UID** de la tarjeta para
> reconocerla. No emula ni escribe tarjetas, y no elude ningun sistema de acceso.
> Usa solo tarjetas y sistemas para los que tengas autorizacion.

## Funciones

- **Fichar**: acercas la tarjeta y la app registra sola, alternando **IN** y
  **OUT** (primero IN, luego OUT, luego IN...). Sin eleccion manual.
- **Lector** elegible en Ajustes: **NFC** (13.56 MHz) o **RFID LF** (125 kHz).
- **Registro** de un colaborador en una tarjeta en blanco (solo el nombre). Cada
  persona esta ligada a su tarjeta (UID); tarjeta perdida: *Tarjetas > Cambiar chip*.
- **Work mode**: reloj a pantalla completa, saludo Bienvenido/Adios; para salir
  hace falta el **PIN de flechas**.
- **Historial** con filtros, resumen **Hoy** (entrada/salida/horas/pausas) y
  **Semana**.
- **Exportar** CSV/JSON, **CSV con fecha**, **Copia** y **Restaurar** en microSD.
- **Feedback** sonido/vibracion/LED distintos para IN y OUT (desactivables).
- **PIN** opcional (secuencia de flechas) propuesto al primer inicio.
- **Idiomas**: ingles, italiano, espanol, frances, aleman.

## Compilacion e instalacion

App externa (FAP) para el **firmware oficial**, compilada con
[`ufbt`](https://github.com/flipperdevices/flipperzero-ufbt):

```bash
python3 -m pip install --upgrade ufbt
ufbt            # compila (salida en dist/)
ufbt launch     # instala y ejecuta en el Flipper conectado
```

O descarga desde la [Release](https://github.com/vladpereverzyev/flipper-timeclock/releases)
el `.fap` de tu firmware y copialo en `apps/Tools/`.

| Firmware    | Archivo                     |
|-------------|-----------------------------|
| Oficial     | `timeclock-official.fap`    |
| Momentum    | `timeclock-momentum.fap`    |
| Unleashed   | `timeclock-unleashed.fap`   |
| RogueMaster | `timeclock-roguemaster.fap` |

## Licencia

Copyright (C) 2026 Vladyslav Pereverzyev. Licencia **GNU GPL-3.0-or-later** - ver
[LICENSE](LICENSE). Documentacion completa en el [README en ingles](README.md).
