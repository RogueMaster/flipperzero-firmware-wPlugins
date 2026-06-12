# Publicar San Morse en el Flipper Application Catalog

Pasos para que la app aparezca en el catalogo oficial (la tienda de apps de
qFlipper y https://lab.flipper.net/apps).

## 1. Repo publico — HECHO

`https://github.com/sanjorgek/san-morse-flipper`

## 2. Capturas de pantalla — HECHO

`screenshots/ss0.png` … `ss4.png`, capturas de qFlipper sin modificar
(512x256). Si las regeneras, **no cambies resolucion ni formato** (el
catalogo rechaza imagenes re-escaladas).

## 3. Completar manifest.yml — HECHO

- `origin`: `https://github.com/sanjorgek/san-morse-flipper.git`
- `commit_sha`: SHA completo del commit pusheado a publicar
  (`git rev-parse HEAD` tras el push)

## 4. Enviar al catalogo

1. Haz fork de https://github.com/flipperdevices/flipper-application-catalog
2. Copia `manifest.yml` a `applications/Tools/san_morse/manifest.yml`
3. Valida localmente (opcional, desde el fork):
   ```bash
   pip install -r tools/requirements.txt
   python3 tools/bundle.py --nolint applications/Tools/san_morse/manifest.yml bundle.zip
   ```
4. Abre un Pull Request. El bot valida el manifiesto, compila la app contra
   los SDK soportados y, si pasa, el equipo la publica.

## Actualizaciones futuras

Sube `fap_version` en `application.fam` (p. ej. `1.1`) — el catalogo rechaza
versiones que no aumenten — pushea, y manda otro PR al catalogo actualizando
`commit_sha` y el changelog.
