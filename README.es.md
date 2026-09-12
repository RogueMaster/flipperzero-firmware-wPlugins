# Time Clock - Flipper Zero

[![Build & Release](https://github.com/vladpereverzyev/flipper-timeclock/actions/workflows/build.yml/badge.svg)](https://github.com/vladpereverzyev/flipper-timeclock/actions/workflows/build.yml)
[![Latest release](https://img.shields.io/github/v/release/vladpereverzyev/flipper-timeclock?cacheSeconds=300)](https://github.com/vladpereverzyev/flipper-timeclock/releases)
[![Downloads](https://img.shields.io/github/downloads/vladpereverzyev/flipper-timeclock/total?cacheSeconds=300)](https://github.com/vladpereverzyev/flipper-timeclock/releases)
[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](LICENSE)
[![Languages](https://img.shields.io/badge/lang-EN%20%7C%20IT%20%7C%20ES%20%7C%20FR%20%7C%20DE-blue)](#)

[English](README.md) | [Italiano](README.it.md) | **Espanol** | [Francais](README.fr.md) | [Deutsch](README.de.md)

App de **control de fichajes** para el [Flipper Zero](https://flipperzero.one/).
Sirve para registrar a tus colaboradores y sus entradas/salidas: asigna a cada
persona una tarjeta **en blanco** dedicada NFC o RFID, la acercas, la app elige
sola **IN** o **OUT**, y cada fichaje se guarda con fecha y hora en la microSD
como hoja CSV que puedes abrir en Excel. Funciona de forma autonoma, sin telefono
ni PC.

> **Usa tarjetas en blanco/dedicadas - solo uso autorizado.** La app esta pensada
> para **tarjetas en blanco** que asignas a tus colaboradores. Solo lee el **UID**
> de la tarjeta para distinguir a una persona de otra: **no** sirve para leer
> tarjetas de control de acceso ajenas, **no** emula tarjetas y **no** elude
> ningun sistema de autenticacion. Usa solo tarjetas y sistemas autorizados. Ver
> [SECURITY.md](SECURITY.md).

## Pantallas

Maquetas estilo Flipper de las pantallas principales (128x64):

<table>
  <tr>
    <td align="center"><img src="docs/img/menu.svg" width="240" alt="Menu principal"><br><b>Menu principal</b><br>Fichar, Work mode, Tarjetas, Historial...</td>
    <td align="center"><img src="docs/img/work-clock.svg" width="240" alt="Work mode"><br><b>Work mode</b><br>Reloj, fichas con el chip</td>
  </tr>
  <tr>
    <td align="center"><img src="docs/img/greeting.svg" width="240" alt="Saludo"><br><b>Pasa la tarjeta</b><br>Bienvenido / Adios por nombre</td>
    <td align="center"><img src="docs/img/today.svg" width="240" alt="Resumen diario"><br><b>Hoy</b><br>Fichajes + horas totales</td>
  </tr>
  <tr>
    <td align="center"><img src="docs/img/pin.svg" width="240" alt="Bloqueo PIN"><br><b>Bloqueo PIN</b><br>Protege la salida de la app</td>
    <td align="center"><img src="docs/img/settings.svg" width="240" alt="Ajustes"><br><b>Ajustes</b><br>Sonido / Vibra / LED, PIN</td>
  </tr>
</table>

## Funciones

- **Fichar** acercando una tarjeta - las tarjetas conocidas se reconocen por UID.
- **Eleccion de lector** en Ajustes: **NFC** (13.56 MHz) o **RFID LF** (125 kHz).
  El lector detecta los protocolos que soporta el firmware.
- **Registra un colaborador** la primera vez que pasas su tarjeta en blanco, con
  un nombre. Cada persona esta ligada a ese chip (su UID): cada fichaje apunta a
  ese chip.
- **Gestiona colaboradores (tarjetas)**: renombrar, **cambiar el chip** si se
  pierde (conserva nombre e historial, solo cambia el chip), ver el historial de
  la persona, borrar (el historial se conserva).
- **IN/OUT automatico**: al acercar una tarjeta alterna solo (primero IN, luego
  OUT, luego IN...) - sin eleccion manual, el fichaje es instantaneo.
- **Feedback al fichar**: **sonido**, **vibracion** y **LED** distintos para IN y
  OUT (tono ascendente + 1 vibracion + verde para IN; tono descendente + 2
  vibraciones + azul para OUT). Cada uno desactivable en Ajustes (activados por
  defecto).
- **Historial** con filtros (todos / hoy / semana, o por colaborador desde
  Tarjetas), resumen **Hoy** (primera entrada, ultima salida, horas y pausas) y
  **Semana** (horas por dia + total).
- **Guardado en microSD** en CSV, mas **exportar JSON**, **CSV con fecha**, una
  **Copia** (copia fechada de tarjetas + historial) y **Restaurar** (recargar
  tarjetas + historial desde una copia).
- **Modo protegido (PIN)**: codigo opcional en **secuencia de flechas** (Arriba /
  Abajo / Izquierda / Derecha - rapido) que bloquea la salida de la app; se
  ofrece al primer inicio o mas tarde en Ajustes.
- **Idiomas**: ingles, italiano, espanol, frances, aleman - seleccionables en
  Ajustes (el firmware oficial no expone un idioma de sistema).

Ver la [Roadmap](#roadmap) para las ideas v1.1 / v2.0.

### Un chip por persona (y perdida del chip)

Cada colaborador se identifica por el **UID** del chip: asigna un chip por
persona y mantenlo como referencia - todos sus fichajes apuntan a ese chip. Si
alguien **pierde su chip**, abre **Tarjetas -> (persona) -> Cambiar chip** y pasa
un chip nuevo en blanco: se conservan nombre y fichajes, solo cambia el chip de
referencia.

## Archivos de datos

Todo se guarda en la microSD en `/ext/apps_data/timeclock/`:

| Archivo       | Contenido                                                       |
|---------------|-----------------------------------------------------------------|
| `badges.csv`  | Tarjetas: `uid,name,tech,created,last_used,last_event`          |
| `punches.csv` | Historial: `date,time,name,uid,type` (`IN`/`OUT`)               |
| `config.txt`  | Ajustes + **hash** del PIN y salt (nunca el PIN en claro)       |
| `export.json` | Exportacion JSON del historial (*Export -> Export JSON*)        |
| `punches-YYYY-MM-DD.csv` | Copia CSV con fecha (*Export -> Export CSV*)         |
| `backup/`     | Copias fechadas de tarjetas + historial (*Export -> Backup*)    |

`punches.csv` es la hoja de fichajes interna: cada entrada/salida de cada
colaborador, por dia y hora. Se abre en Excel, LibreOffice, Google Sheets, etc.

Ejemplo de `punches.csv`:

```csv
date,time,name,uid,type
2026-09-12,08:02,Mario,04A1B2C3D4,IN
2026-09-12,12:31,Mario,04A1B2C3D4,OUT
```

## Compilacion e instalacion

Es una app externa (FAP) para el **firmware oficial**. Se compila con
[`ufbt`](https://github.com/flipperdevices/flipperzero-ufbt):

```bash
python3 -m pip install --upgrade ufbt
```

Desde la carpeta del proyecto (la que tiene `application.fam`):

```bash
ufbt
```

Instala y ejecuta en un Flipper conectado:

```bash
ufbt launch
```

El `.fap` compilado queda en `dist/`. Tambien puedes copiarlo a
`SD Card/apps/Tools/` con qFlipper y abrirlo en **Apps -> Tools -> Time Clock**.

> **Nota de firmware.** La capa de radio esta en `timeclock_reader.c` (NFC con el
> poller ISO14443-3A - MIFARE Classic/Ultralight, NTAG, DESFire, las tarjetas en
> blanco que usas - mas el worker LF RFID de 125 kHz). Es la parte mas sensible a
> los cambios de API; si un simbolo cambia, la correccion esta en ese unico
> archivo.

## Compatibilidad

Time Clock funciona en el firmware **oficial** del Flipper Zero y en los fork mas
populares. Un FAP se compila para la API de un firmware concreto, asi que cada
Release trae **un `.fap` por firmware** - descarga el que corresponda:

| Firmware    | Archivo de la release       |
|-------------|-----------------------------|
| Oficial     | `timeclock-official.fap`    |
| Momentum    | `timeclock-momentum.fap`    |
| Unleashed   | `timeclock-unleashed.fap`   |
| RogueMaster | `timeclock-roguemaster.fap` |

RogueMaster se basa en el SDK de Unleashed (compatible a nivel binario). Para un
firmware no listado, compila desde el codigo con `ufbt` (ver arriba): el codigo
usa APIs estandar y es portable.

## Modo protegido y PIN - lo que puede y no puede

El PIN es una **secuencia rapida de 4 flechas** (p. ej. Arriba, Arriba, Izquierda,
Derecha). Se ofrece al primer inicio, o en cualquier momento desde *Ajustes ->
Definir PIN*. Cuando esta activo, la app arranca bloqueada y **Back ya no sale de
la app**; la unica via por software es *Ajustes -> Salir* (o Work mode -> Back),
que pide la secuencia. Se guarda solo como **hash con salt**, nunca en claro.

**Limites honestos (a proposito):**

- Ninguna app puede impedir un apagado **por hardware** o un cierre forzado a
  nivel firmware (p. ej. `Izquierda` + `Back` para reiniciar, o quitar la
  alimentacion). El modo protegido cubre solo lo que se controla por software.
- El hash del PIN (FNV-1a) evita guardarlo en claro y protege la interfaz, pero
  **no** es una defensa fuerte contra quien tiene acceso fisico a la microSD y
  prueba offline las secuencias cortas.
- No hay **ningun bypass oculto**. Al borrar `config.txt` de la SD se reinician
  los ajustes (y el PIN).

## Estructura del proyecto

```
timeclock/
|-- application.fam            # manifiesto de la app
|-- timeclock.h / .c           # ciclo de vida, entry point, helpers
|-- timeclock_storage.h / .c   # persistencia microSD + modelo de datos
|-- timeclock_pin.h / .c       # hash del PIN con salt
|-- views/
|   `-- pin_view.h / .c        # vista personalizada del PIN
`-- scenes/
    |-- timeclock_scene*.{h,c} # scene manager (X-macro)
    `-- timeclock_scene_*.c    # un archivo por pantalla
```

## Roadmap

- **v1.1 / v1.2** - resumen semanal, calculo de pausas y filtros (hechos).
- **v2.0** - copia y restauracion (hechas); luego: sync Bluetooth, app companion, import.

## Contribuir

Las contribuciones son bienvenidas - ver [CONTRIBUTING.md](CONTRIBUTING.md) y el
[Codigo de Conducta](CODE_OF_CONDUCT.md).

## Apoyar

Si Time Clock te resulta util, puedes apoyar el desarrollo:

[![Sponsor en GitHub](https://img.shields.io/badge/Sponsor-GitHub-ea4aaa?logo=githubsponsors&logoColor=white)](https://github.com/sponsors/vladpereverzyev)
[![Invitar un cafe en Ko-fi](https://img.shields.io/badge/Ko--fi-Invitar%20un%20cafe-ff5e5b?logo=ko-fi&logoColor=white)](https://ko-fi.com/vladpereverzyev)

## Licencia

Copyright © 2026 Vladyslav Pereverzyev.

Distribuido bajo **GNU General Public License v3.0 o posterior** - ver
[LICENSE](LICENSE). Los archivos fuente llevan la cabecera
`SPDX-License-Identifier: GPL-3.0-or-later`.
