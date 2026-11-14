# USB PurpleCheck 0.3 - RogerF5

Herramienta autonoma para Flipper Zero. No requiere scripts ni instalar programas en la PC auditada.

1. Instala el FAP compatible con tu firmware en `/ext/apps/USB/` usando qFlipper.
2. Cierra qFlipper y abre la app en el Flipper. OK pasa del banner al menu.
3. Derecha abre Settings. En Language cambia EN a ES o UK con OK. Back guarda.
4. Elige disco lectura/escritura, disco solo lectura o teclado. OK prepara una cuenta regresiva de ocho segundos para conectar la PC y cerrar qFlipper. Back cancela o detiene. Limite configurable: 30, 60 o 120 segundos.
5. Para teclado, enfoca un editor de texto en la PC y pulsa OK adicional en el Flipper: escribe solo un identificador, sin Enter ni comandos.
6. Para archivos, copia SEND.TXT a una carpeta temporal de la PC y devuelvelo al disco como RETURN.TXT, reemplazando el archivo vacio. Expulsa el volumen antes de detener. El Flipper verifica automaticamente los 4 KB sinteticos devueltos.
7. Arriba/Abajo en resultados alterna el resultado de integridad. Izquierda/Derecha elige una conclusion manual; OK guarda en la SD.

El volumen aislado es de 1 MiB. No expone tus archivos de la SD ni extrae documentos de la PC. El host debe iniciar las copias. Lecturas de bloques pueden ser metadatos o antivirus: no equivalen a un bypass. Sin respuesta significa inconcluso. El rechazo en modo solo lectura lo realiza el Flipper.

En ajustes figura Creada por RogerF5. Los registros quedan en `/ext/apps_data/usb_purplecheck/sessions/`. El nuevo historial no importa last.bin de 0.2; los JSONL anteriores se conservan.

La publicacion en Flipper Lab requiere compilacion oficial, pruebas fisicas y revision de sus mantenedores. Consulta docs/VALIDATION.md para distinguir lo probado de lo pendiente.
