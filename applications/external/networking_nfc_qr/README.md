[![en](https://img.shields.io/badge/lang-en-red.svg)](README_en.md)
[![es](https://img.shields.io/badge/lang-es-blue.svg)](README.md)

# Networking NFC + QR para Flipper Zero

Esta aplicación para Flipper Zero permite emular una tarjeta NFC y mostrar un código QR simultáneamente en la pantalla. Es ideal para situaciones de networking, permitiendo compartir tu contacto o información de dos formas distintas al mismo tiempo.

## Características

*   **Creación Rápida (QR + NFC)**: Genera a la vez un archivo `.qrcode` y un archivo `.nfc` (NTAG215 con NDEF URI) introduciendo el nombre y la URL directamente en el Flipper.
*   **Emulación NFC**: Carga y emula archivos `.nfc` estándar.
*   **Visualización QR**: Genera y muestra códigos QR a partir de archivos `.qrcode`.
*   **Interfaz Dual**: Muestra el QR en la pantalla mientras el chip NFC está activo siendo emulado.
*   **Optimización**: Ajusta automáticamente el tamaño del QR para maximizar su visibilidad en la pantalla del Flipper.
*   **Créditos**: Pantalla de información sobre el autor.

## Requisitos

*   **Flipper Zero** (obviamente).
*   **Firmware**: Puedes compilar con **uFBT** (recomendado y más rápido) o con el código fuente del firmware oficial o Custom Firmwares (CFW) como Unleashed, RogueMaster o Xtreme.

## Compilación e Instalación

### Opción 1: Con uFBT (Recomendado)
Si tienes `ufbt` instalado:
```bash
ufbt
```
El archivo `.fap` compilado se generará en la carpeta `dist/`.

### Opción 2: Con firmware local mediante script
1.  Asegúrate de tener descargado el repositorio del firmware en `../flipperzero-firmware`.
2.  Ejecuta el script de compilación:

```bash
./compile.sh
```

El script compilará la aplicación y guardará el archivo resultante en `build/networking_nfc_qr_app.fap`.

Copia el archivo `.fap` a tu Flipper Zero (usando qFlipper o la tarjeta SD) en la carpeta `/ext/apps/NFC/`.

## Uso

1.  Abre la aplicación **Networking NFC + QR** en tu Flipper Zero.
2.  **Create QR + NFC**: Introduce el nombre para tus archivos y la URL deseada. La app creará automáticamente el `.qrcode` en `/ext/apps_data/qrcodes/` y el `.nfc` en `/ext/nfc/`, y los dejará seleccionados listos para emular.
3.  **Select QR Code**: Selecciona manualmente un archivo `.qrcode` existente si prefieres otro.
4.  **Select NFC File**: Selecciona manualmente un archivo `.nfc` existente si prefieres otro.
5.  **Start Emulation**: Comienza la magia. El Flipper empezará a emular la tarjeta NFC y mostrará el código QR en la pantalla.
6.  Pulsa **Back** para detener la emulación y volver al menú.

## Créditos

Este proyecto ha sido posible gracias a:

*   **DonJulve**: Desarrollo principal y adaptación.
*   **[Flipper Zero Firmware](https://github.com/flipperdevices/flipperzero-firmware)**: Por la increíble plataforma y documentación.
*   **[bmatcuk/flipperzero-qrcode](https://github.com/bmatcuk/flipperzero-qrcode)**: Por su excelente librería y código base para la generación de códigos QR en el Flipper Zero.
