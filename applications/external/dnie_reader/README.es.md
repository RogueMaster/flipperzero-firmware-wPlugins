# IDentity (antes DNIe Reader) para Flipper Zero

*[English version](README.md)*

App para [Flipper Zero](https://flipperzero.one) que lee por NFC el **DNI electrónico español**, **pasaportes electrónicos** y DNI de otros países que siguen el estándar ICAO. Muestra un "carnet" con la foto en pixel art y **verifica que el documento es auténtico**.

> Proyecto educativo de seguridad RFID/NFC. Úsalo solo con tu propio documento o con el permiso de su titular.

## Qué hace

- **PACE con el CAN** (los 6 dígitos del anverso): canal seguro ECDH sobre brainpoolP256r1 con AES-128, el mismo protocolo que usan los lectores oficiales. Hasta ahora no existía para Flipper.
- **Secure Messaging** AES y 3DES.
- **Lectura de DG1** (MRZ): nombre, apellidos, número de DNI, soporte, fechas, con comprobación de los dígitos de control.
- **Foto (DG2) en pixel art**: decodificador JPEG2000 propio y mínimo, que solo reconstruye la luminancia a resolución reducida. Tramado Atkinson, Floyd-Steinberg o Bayer.
- **Verificación de autenticidad**:
  - hashes de DG1, DG2 y DG14 frente al EF.SOD;
  - firma RSA del SOD con el certificado del Document Signer;
  - **Chip Authentication** (detecta chips clonados).
- **Pasaportes y DNI de otros países**: PACE o BAC con la MRZ, chips 4A y 4B, foto JPEG o JPEG2000.
- **Cadena CSCA de 20 países** (RSA y RSA-PSS) y **himno del país** cuando el documento es auténtico.
- **Interfaz en inglés.** Al abrir una lectura guardada suena el himno de su país.
- **Lecturas guardadas**, cifradas con la clave única del enclave del Flipper. Dos modos: DNI completo, o solo nombre, foto y cumpleaños.

## Instalar

1. Descarga el `.fap` de la sección *Releases*, o compílalo tú (ver abajo).
2. Cópialo a la SD del Flipper en `apps/NFC/`.
3. En el Flipper: **Apps → NFC → IDentity**.

## Compilar

Necesitas Python 3 y [ufbt](https://github.com/flipperdevices/flipperzero-ufbt):

```sh
pip install ufbt
cd dnie_reader
ufbt          # genera dist/dnie_reader.fap
ufbt launch   # compila, instala y abre la app en un Flipper conectado por USB
```

## Pruebas

La carpeta `tests/` contiene pruebas en PC, con vectores oficiales siempre que existen:

- **PACE**: ICAO Doc 9303-11, apéndice G.1.
- **Secure Messaging 3DES**: ICAO 9303-11, apéndice D.
- **CMAC**: RFC 4493.
- **Decodificador JPEG2000**: comparado con OpenJPEG.
- **SOD, firma RSA, Chip Authentication, MRZ y registros guardados.**
- **Cadena CSCA SPAIN**: con certificados reales y públicos de la Policía.

Consulta `tests/README.md`.

## Documentos

- **DNI español**: PACE con el CAN (6 cifras del anverso).
- **Pasaportes** de cualquier país (estándar ICAO): PACE o BAC con los datos de la MRZ.
- **DNI de otros países** que sigan el estándar ICAO: con su CAN o con su MRZ, según la tarjeta.
  Solo el acceso estándar con los datos impresos en el propio documento. Las tarjetas que
  exigen un terminal autorizado (por ejemplo, el DNI alemán) no son compatibles.

## Privacidad

- Todos los datos de prueba del repositorio son **sintéticos**.
- Las lecturas guardadas se cifran con la clave única del Flipper, así que no se pueden abrir en otro dispositivo.
- La foto sin cifrar se borra de la SD en cuanto se pasa a pixel art.

## Estructura

| Fichero | Contenido |
|---|---|
| `dnie_pace.c`, `dnie_pace_math.c`, `dnie_ec.c` | PACE ECDH-GM, curva brainpoolP256r1 |
| `dnie_sm.c`, `dnie_crypto.c` | Secure Messaging AES/3DES, KDF, CMAC, MAC retail |
| `dnie_lds.c`, `dnie_mrz.c` | Lectura de ficheros del chip, parser de la MRZ |
| `dnie_j2k.c`, `dnie_pixelart.c` | Decodificador JPEG2000 reducido, pixel art |
| `dnie_sod.c`, `dnie_pa.c`, `dnie_dg14.c`, `dnie_ca.c` | Autenticidad: SOD, firma, Chip Authentication |
| `dnie_csca.c`, `dnie_csca_keys.inc`, `assets/csca.bin` | Cadena de confianza: CSCA SPAIN dentro de la app y las de los 20 países principales en la SD (Master List del BSI). RSA PKCS#1 y RSA-PSS |
| `dnie_bac.c`, `dnie_jpeg.c` | Pasaporte: BAC y foto JPEG |
| `dnie_store.c`, `dnie_record.c` | Lecturas guardadas cifradas |
| `scenes/` | Pantallas de la app |

## Pendiente

- Carnet de conducir (investigar: probablemente sin chip).

## Licencia

MIT (ver `LICENSE`). `dnie_reader/lib/mbedtls_ct/` es código de [Mbed TLS](https://github.com/Mbed-TLS/mbedtls) bajo su propia licencia (Apache-2.0 o GPL-2.0-or-later).
