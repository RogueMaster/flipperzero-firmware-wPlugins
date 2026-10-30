<p align="center">
  <img src="assets/catdriver_logo_final.jpg" alt="CatDriver Logo" width="340"/>
</p>

<h1 align="center">CatDriver (Wardrive Wi-Fi + BLE + GPS) 🐾📡</h1>

<p align="center">
  <b>Advanced Wardriving Suite for Flipper Zero + ESP32-S3 + GPS</b><br>
  Captura y exportación estandarizada WiGLE CSV 1.4 de redes Wi-Fi 2.4 GHz y dispositivos BLE con geolocalización satelital en tiempo real.
</p>

<p align="center">
  <a href="https://github.com/EVL87/catwardrive/releases/tag/v1.0"><img src="https://img.shields.io/badge/Release-v1.0-orange.svg" alt="Release v1.0"></a>
  <a href="https://github.com/EVL87/catwardrive/blob/FlipperZeroApps/LICENSE"><img src="https://img.shields.io/badge/License-GPL--3.0-blue.svg" alt="License"></a>
  <a href="https://github.com/flipperdevices/flipper-application-catalog/pull/1362"><img src="https://img.shields.io/badge/Flipper%20Lab-PR%20%231362-yellow.svg" alt="Flipper Lab PR"></a>
</p>

---

**CatDriver** es una aplicación avanzada y optimizada de wardriving para **Flipper Zero** junto a la placa **Electronic Cats Marauder Add-on V1.1 (ESP32-S3)** y módulo **GPS NEO-6M V2**.

Permite capturar redes **Wi-Fi 2.4 GHz**, balizas **Bluetooth Low Energy (BLE)** y coordenadas de satélites **GPS** de forma simultánea o independiente, exportando registros limpios en formato **WiGLE CSV 1.4** listos para subir a [WiGLE.net](https://wigle.net).

---

## 🚀 Características Principales

* **Formatos WiGLE 1.4 Oficiales:** Encabezado y estructura de datos estandarizada compatible con Wi-Fi y dispositivos BLE (`[LE]`, canales, RSSI, tipos de seguridad y coordenadas).
* **Cero Crasheos por Memoria (No OOM):** Sistema de buffer en RAM en lotes de 30 APs que se agregan mediante *append* al archivo `.csv` en la MicroSD, eliminando fugas de memoria y desbordamientos.
* **Detección Real de Seguridad Wi-Fi:** Captura cifrados reales del beacon frame (`[WPA3-SAE-CCMP]`, `[WPA2-PSK-CCMP]`, `[WPA-PSK-CCMP+TKIP]`, `[WEP]`, `[ESS]`, `[OWE]`).
* **Soporte Bluetooth LE (BLE):** Escaneo pasivo de balizas y periféricos BLE en el ESP32-S3 sin saturar el procesador.
* **Interfaz Gráfica con Menú de Tarjetas:** Menú horizontal tipo carrusel para elegir modo de trabajo:
  * **`ALL`:** Wi-Fi + BLE + GPS simultáneo.
  * **`WIFI`:** Escaneo exclusivo de redes Wi-Fi 2.4 GHz.
  * **`BLE`:** Escaneo exclusivo de dispositivos Bluetooth LE.
  * **`Settings`:** Ajustes de idioma al instante.
  * **`About` / `Acerca de`:** Información de hardware, especificaciones y créditos.
* **Archivos Independientes por Sesión:** Cada sesión de escaneo genera su propio archivo `.csv` con marca de tiempo y sufijo de modo (`catdriver_YYYY_MM_DD_HH_MM_SS_all.csv`, `..._wifi.csv`, `..._ble.csv`), manteniendo limpios los búferes entre escaneos.
* **Gestión de Energía ESP32 (Modo Reposo):** La tarjeta ESP32-S3 permanece en reposo silencioso sin emitir tráfico serial ni gastar batería hasta que se selecciona un modo de escaneo en el Flipper Zero.
* **Pantalla GPS en Vivo:** Visualización en tiempo real de coordenadas, satélites y hora satelital presionando la flecha **IZQUIERDA** en el escáner.
* **Bilingüe:** Soporte completo en **English** (por defecto) y **Español**.

---

## 🛠️ Requisitos de Hardware

1. **Flipper Zero**
2. **Add-on ESP32-S3:** Electronic Cats WiFi Marauder Add-on V1.1 (o placa ESP32-S3 equivalente conectada a UART).
3. **Módulo GPS:** GPS NEO-6M V2 (o compatible NMEA).

---

## ⚡ Compatibilidad de Firmware
La aplicación está construida sobre el SDK oficial de Flipper Zero (**Official Firmware SDK 1.5+**) y es compatible de forma nativa con:
* **Firmware Oficial de Flipper Zero (OFW)**
* **Momentum Firmware**
* **Unleashed / Xtreme Firmware**

### Canales UART (Hardware Dual Serial):
La aplicación adquiere directamente los puertos seriales por hardware en el bus GPIO del Flipper Zero:
* **Canal ESP32-S3 (USART):** Pines **13 (TX) y 14 (RX)** a 115200 baudios.
* **Canal GPS NMEA (LPUART):** Pines **15 (TX) y 16 (RX)** a 9600 baudios.
*(En firmwares como Momentum, puedes verificar los pines en Settings -> Protocols -> GPS).*

---

## 📲 Instalación de la App en Flipper Zero

1. Copia el archivo binario compilado [**`catdriver.fap`**](catdriver.fap) a tu tarjeta MicroSD en:
   `/ext/apps/GPIO/` o `/ext/apps/Tools/`
2. Puedes abrir la aplicación directamente desde el menú de aplicaciones de tu Flipper Zero.

> [!NOTE]
> 🏬 **Pronto en Flipper Lab:**  
> Próximamente se estará enviando `catdriver.fap` a **Flipper Lab** para que sea incluida directamente en el catálogo oficial de aplicaciones de Flipper Zero.

---

## 🔌 Flasheo del Firmware ESP32-S3

Tienes dos opciones para flashear el microcontrolador ESP32-S3:

### Opción 1: Flasheo directo con Flipper Zero (ESP Flasher App)
Si utilizas la aplicación **ESP Flasher** de Flipper Zero ([0xchocolate/flipperzero-esp-flasher](https://github.com/0xchocolate/flipperzero-esp-flasher)):

1. Copia los archivos preparados de la carpeta [`firmware/esp_flasher_ready/`](firmware/esp_flasher_ready/) hacia la MicroSD en:
   `/ext/apps_data/esp_flasher/`
   * `bootloader.bin`
   * `partitions.bin`
   * `firmware.bin`
2. Conecta la placa ESP32-S3 en modo Bootloader (mantén presionado **BOOT**, presiona y suelta **RESET**, luego suelta **BOOT**).
3. Abre **ESP Flasher** en el Flipper (`Apps` $\rightarrow$ `GPIO` $\rightarrow$ `[ESP] Flash`).
4. Selecciona **ESP32-S3** $\rightarrow$ **Manual Flash** y asigna los archivos:
   * `bootloader.bin` en `0x0000`
   * `partitions.bin` en `0x8000`
   * `firmware.bin` en `0x10000`
5. Ejecuta el flasheo y al finalizar presiona **RESET** en la tarjeta.

---

### Opción 2: Flasheo desde el Navegador (Marauder Web Flasher)
Puedes flashear la tarjeta directamente desde tu PC usando el instalador web oficial:

1. Ingresa a: [**Marauder Web Installer**](https://justcallmekoko.github.io/MarauderInstaller/)
2. Conecta la placa ESP32-S3 a tu PC mediante USB-C en modo Bootloader.
3. Elige la opción **Custom** para cargar un binario personalizado.
4. Selecciona el archivo binario compilado [`firmware/build/sketch.ino.bin`](firmware/build/sketch.ino.bin) o la imagen fusionada [`firmware/build/sketch.ino.merged.bin`](firmware/build/sketch.ino.merged.bin).
5. Haz clic en **Program** y espera a que termine.

---

## 📁 Estructura del Repositorio

```
catwardrive/
├── README.md
├── LICENSE
├── catdriver.fap              <-- Binario listo para Flipper Zero
├── fap/                       <-- Código fuente de la app FAP
│   ├── application.fam
│   ├── catdriver.c
│   ├── catdriver.h
│   ├── catdriver_uart.c
│   ├── catdriver_uart.h
│   ├── helpers/
│   └── icons/
└── firmware/                  <-- Código y binarios del ESP32-S3
    ├── sketch.ino             <-- Código fuente Arduino
    ├── build/                 <-- Binarios generados de compilación
    └── esp_flasher_ready/     <-- Binarios listos para Flipper ESP Flasher
```

---

## 👏 Créditos y Agradecimientos

* Proyecto basado originalmente en el trabajo de **[Sil333033 (ll-wardrive)](https://github.com/Sil333033/flipperzero-wardriver)**.
* Adaptado, optimizado y extendido con soporte Wi-Fi + BLE, memoria en lotes, WiGLE 1.4 y UI por **EVL87**.
* Compatible con [WiGLE.net](https://wigle.net).

---

## 📜 Licencia

Distribuido bajo la licencia GPL v3. Consulta el archivo [LICENSE](LICENSE) para más detalles.
