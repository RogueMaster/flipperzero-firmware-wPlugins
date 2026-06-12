# Publicar San Morse en el Flipper Application Catalog

Pasos para que la app aparezca en el catalogo oficial (la tienda de apps de
qFlipper y https://lab.flipper.net/apps).

## 1. Repo publico

```bash
gh repo create san-morse-flipper --public --source . --push
# o crea el repo en GitHub y:
# git remote add origin https://github.com/<usuario>/san-morse-flipper.git
# git push -u origin master
```

## 2. Capturas de pantalla

Con el Flipper conectado y la app abierta, usa el boton de captura de
**qFlipper** (icono de camara). Guarda al menos una como
`screenshots/ss0.png` **sin cambiar resolucion ni formato** (el catalogo
rechaza imagenes re-escaladas). Sugerencia: una del arbol de decision y otra
de la reproduccion. Commitea y pushea.

## 3. Completar manifest.yml

- `origin`: URL real del repo (`https://github.com/<usuario>/san-morse-flipper.git`)
- `commit_sha`: SHA completo del ultimo commit pusheado (`git rev-parse HEAD`)

## 4. Enviar al catalogo

1. Haz fork de https://github.com/flipperdevices/flipper-application-catalog
2. Copia `manifest.yml` (sin los comentarios TODO) a
   `applications/Tools/san_morse/manifest.yml`
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
