[![en](https://img.shields.io/badge/lang-en-red.svg)](README_en.md)
[![es](https://img.shields.io/badge/lang-es-blue.svg)](README.md)

# Networking NFC + QR para Flipper Zero

Esta aplicación para Flipper Zero permite emular una tarjeta NFC y mostrar un código QR simultáneamente en la pantalla. Es ideal para situaciones de networking, permitiendo compartir tu contacto o información de dos formas distintas al mismo tiempo.
**Esta rama está pensada para ser usada con el firmware de RogueMaster.**

## Características

*   **Emulación NFC**: Carga y emula archivos `.nfc` estándar.
*   **Visualización QR**: Genera y muestra códigos QR a partir de archivos `.qrcode`.
*   **Interfaz Dual**: Muestra el QR en la pantalla mientras el chip NFC está activo siendo emulado.
*   **Optimización**: Ajusta automáticamente el tamaño del QR para maximizar su visibilidad en la pantalla del Flipper.
*   **Créditos**: Pantalla de información sobre el autor.

## Requisitos

*   **Flipper Zero** (obviamente).
*   **Firmware**: Necesitas el código fuente del firmware para compilar la aplicación.
    *   Puedes usar el [Firmware Oficial](https://github.com/flipperdevices/flipperzero-firmware).
    *   También es compatible con **Custom Firmwares (CFW)** como Unleashed, RogueMaster o Xtreme.

## Compilación e Instalación

Se ha incluido un script automatizado para facilitar la compilación.

1.  Clona este repositorio.
2.  Asegúrate de tener descargado el repositorio del firmware (oficial o CFW) en una carpeta cercana (por defecto busca en `../flipperzero-firmware`).
3.  Ejecuta el script de compilación:

```bash
./compile.sh
```

4.  El script te preguntará la ruta de tu firmware. Si está en la ruta por defecto, simplemente pulsa `Enter`.
5.  El script compilará la aplicación y guardará el archivo resultante `.fap` en la carpeta `build/` dentro del proyecto.
6.  Copia el archivo `build/networking_nfc_qr_app.fap` a tu Flipper Zero (usando qFlipper o la tarjeta SD) en la carpeta `/ext/apps/NFC/` (o donde prefieras).

## Uso

1.  Abre la aplicación **Networking NFC + QR** en tu Flipper Zero.
2.  **Select QR Code**: Selecciona el archivo con el contenido del QR (debe ser un archivo de texto plano o `.qrcode` con el mensaje).
3.  **Select NFC File**: Selecciona el archivo `.nfc` que quieres emular.
4.  **Start Emulation**: Comienza la magia. El Flipper empezará a emular la tarjeta NFC y mostrará el código QR en la pantalla.
5.  Pulsa **Back** para detener la emulación y volver al menú.

## Créditos

Este proyecto ha sido posible gracias a:

*   **DonJulve**: Desarrollo principal y adaptación.
*   **[Flipper Zero Firmware](https://github.com/flipperdevices/flipperzero-firmware)**: Por la increíble plataforma y documentación.
*   **[bmatcuk/flipperzero-qrcode](https://github.com/bmatcuk/flipperzero-qrcode)**: Por su excelente librería y código base para la generación de códigos QR en el Flipper Zero.
