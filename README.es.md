# Staff Time Clock - Flipper Zero

[![Build & Release](https://github.com/vladpereverzyev/flipper-staff-time-clock/actions/workflows/build.yml/badge.svg)](https://github.com/vladpereverzyev/flipper-staff-time-clock/actions/workflows/build.yml)
[![Latest release](https://img.shields.io/github/v/release/vladpereverzyev/flipper-staff-time-clock?cacheSeconds=300)](https://github.com/vladpereverzyev/flipper-staff-time-clock/releases)
[![Downloads](https://img.shields.io/github/downloads/vladpereverzyev/flipper-staff-time-clock/total?cacheSeconds=300)](https://github.com/vladpereverzyev/flipper-staff-time-clock/releases)
[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](LICENSE)
[![Languages](https://img.shields.io/badge/lang-EN%20%7C%20IT%20%7C%20ES%20%7C%20FR%20%7C%20DE-blue)](#)

[English](README.md) | [Italiano](README.it.md) | **Espanol** | [Francais](README.fr.md) | [Deutsch](README.de.md)

App de **control de fichajes** para el [Flipper Zero](https://flipperzero.one/).
Sirve para registrar a tus colaboradores y sus entradas/salidas: asigna a cada
persona una tarjeta - **NFC**, **RFID** o **iButton** - la acercas, y cada fichaje
se guarda con fecha y hora en la microSD como hoja CSV que puedes abrir en Excel.
Funciona de forma autonoma, sin telefono ni PC.

El lector lee **NFC**, **RFID** e **iButton**: elige cual usar con
Izquierda/Derecha en la pantalla de lectura (la app recuerda la ultima
eleccion). En la misma empresa una persona puede llevar una tarjeta NFC, otra
un llavero RFID y otra un iButton - solo hay que elegir su tecnologia antes
de fichar.

**Tambien sirve una tarjeta que la persona ya tenga.** Como la app **solo lee el
UID** y no escribe nada en la tarjeta, un carnet ya usado con otra empresa (tarjeta
de acceso de oficina, llavero del gimnasio, tarjeta de transporte...) puede
registrarse y usarse aqui sin modificarlo ni sobrescribirlo de ninguna forma - el
sistema solo recuerda su UID junto a los demas.

> **Solo identificacion - solo uso autorizado.** La app lee el UID de la tarjeta
> para distinguir a una persona de otra. **No** escribe ni emula tarjetas y **no**
> elude ningun sistema de autenticacion; registrar aqui una tarjeta no afecta a
> donde mas se use. Usa solo con personas y tarjetas que estas autorizado a
> gestionar. Ver [SECURITY.md](SECURITY.md).

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
- **Lector multi-tecnologia**: **NFC** (13.56 MHz), **RFID LF** (125 kHz) e
  **iButton** (llaves Dallas 1-Wire). Izquierda/Derecha en la pantalla de
  lectura elige cual usar (recordado entre sesiones), asi NFC, RFID e
  iButton conviven en la misma instalacion.
- **Correccion manual**: **Anadir IN** / **Anadir OUT** desde la tarjeta de una
  persona anade un fichaje que falta a la hora actual.
- **Turnos pasada la medianoche** bien contados; **objetivo diario** opcional con
  **horas extra** en Hoy; **exportar el mes** en CSV.
- **Sirve con tarjetas existentes**: al leer solo el UID (nunca escribir), una
  tarjeta ya usada en otro sitio - incluso de otra empresa - puede registrarse y
  usarse sin alterarla.
- **Registra un colaborador** la primera vez que pasas su tarjeta, con un nombre.
  Cada persona esta ligada a ese chip (su UID): cada fichaje apunta a ese chip.
- **Gestiona colaboradores (tarjetas)**: renombrar, **cambiar el chip** si se
  pierde (conserva nombre e historial, solo cambia el chip), ver el historial de
  la persona, **deshacer el ultimo fichaje** (corrige un error), borrar (el
  historial se conserva).
- **IN/OUT automatico**: al acercar una tarjeta alterna solo (primero IN, luego
  OUT, luego IN...) - sin eleccion manual, el fichaje es instantaneo.
- **Feedback al fichar**: **sonido**, **vibracion** y **LED** distintos para IN y
  OUT (tono ascendente + 1 vibracion + verde para IN; tono descendente + 2
  vibraciones + azul para OUT). Cada uno desactivable en Ajustes (activados por
  defecto).
- **Resumen**: pantalla rapida por colaborador (horas de hoy / semana / mes y
  la pausa de hoy), Izquierda/Derecha para cambiar de persona.
- **Historial** (un solo boton en el menu) con el registro completo (todos /
  hoy / semana, o por colaborador desde Tarjetas), resumen **Hoy** (primera
  entrada, ultima salida, horas y pausas), **Semana** (horas por dia + total) y
  **Mes** (horas por colaborador).
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
un chip nuevo (en blanco o uno que ya tenga): se conservan nombre y fichajes,
solo cambia el chip de referencia.

## Archivos de datos

Todo se guarda en la microSD en `/ext/apps_data/timeclock/`:

| Archivo       | Contenido                                                       |
|---------------|-----------------------------------------------------------------|
| `badges.csv`  | Tarjetas: `uid,name,tech,created,last_used,last_event` (`tech`: `NFC`/`RFID`/`iBTN`) |
| `punches.csv` | Historial: `date,time,name,uid,type` (`IN`/`OUT`)               |
| `config.txt`  | Ajustes + **hash** del PIN y salt (nunca el PIN en claro)       |
| `export.json` | Exportacion JSON del historial (*Export -> Export JSON*)        |
| `punches-YYYY-MM-DD.csv` | Copia CSV con fecha (*Export -> Export CSV*)         |
| `punches-YYYY-MM.csv` | Exportacion CSV del mes (*Export -> Export mes*)        |
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
`SD Card/apps/Tools/` con qFlipper y abrirlo en **Apps -> Tools -> Staff Time Clock**.

> **Nota de firmware.** La capa de radio esta en `timeclock_reader.c` (NFC con el
> poller ISO14443-3A - MIFARE Classic/Ultralight, NTAG, DESFire - el worker LF
> RFID de 125 kHz y el worker iButton para las llaves Dallas 1-Wire, arrancados
> juntos). Es la parte mas sensible a los cambios de API; si un simbolo cambia, la
> correccion esta en ese unico archivo.

## Compatibilidad

Staff Time Clock funciona en el firmware **oficial** del Flipper Zero y en los fork mas
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

## Flipper App Catalog

Un manifest listo para el catalogo esta en [catalog/manifest.yml](catalog/manifest.yml)
y el historial de versiones en [changelog.md](changelog.md). Para publicar en
[apps.flipper.net](https://apps.flipper.net): haz un fork de
[flipper-application-catalog](https://github.com/flipperdevices/flipper-application-catalog),
copia el manifest a `applications/Tools/timeclock/manifest.yml`, pon su
`commit_sha` en el commit de la release etiquetada, anade capturas reales
tomadas con qFlipper (ver [screenshots/README.md](screenshots/README.md)) y
abre un pull request. La app compila con la ultima Release SDK de `ufbt`
(target f7).

## Contribuir

Las contribuciones son bienvenidas - ver [CONTRIBUTING.md](CONTRIBUTING.md) y el
[Codigo de Conducta](CODE_OF_CONDUCT.md).

## Apoyar

Si Staff Time Clock te resulta util, puedes apoyar el desarrollo:

[![Sponsor en GitHub](https://img.shields.io/badge/Sponsor-GitHub-ea4aaa?logo=githubsponsors&logoColor=white)](https://github.com/sponsors/vladpereverzyev)
[![Invitar un cafe en Ko-fi](https://img.shields.io/badge/Ko--fi-Invitar%20un%20cafe-ff5e5b?logo=ko-fi&logoColor=white)](https://ko-fi.com/vladpereverzyev)

## Licencia

Staff Time Clock es **codigo abierto**, con licencia
[GNU General Public License v3.0 o posterior](LICENSE).

- **Usala, modificala y redistribuyela libremente** - para fichajes
  personales o comerciales, en tantos Flipper como quieras.
- **Si redistribuyes una version modificada**, debe seguir bajo la misma
  licencia y su codigo fuente debe estar disponible.
- Copyright © 2026 Vladyslav Pereverzyev. Los archivos fuente llevan la
  cabecera `SPDX-License-Identifier: GPL-3.0-or-later`.
