# Publicacion de WikiFlip en Flipper Lab

Flipper Lab muestra apps aprobadas en el Flipper Apps Catalog. Para publicar WikiFlip, el flujo correcto es crear un repositorio GitHub publico con el codigo fuente y abrir un pull request al catalogo oficial.

## 1. Preparar el repositorio de la app

El repositorio publico debe contener:

```text
application.fam
wikiflip.c
wikiflip.h
README.md
LICENSE
docs/changelog.md
images/wikiflip_10px.png
screenshots/*.png
```

El icono debe ser PNG 10x10 px, 1-bit. Los screenshots deben tomarse con qFlipper sin cambiar resolucion ni formato.

## 2. Compilar antes de publicar

```powershell
ufbt
```

Verificar que exista:

```text
dist/wikiflip.fap
```

## 3. Crear commit de release

```powershell
git add application.fam wikiflip.c wikiflip.h README.md LICENSE docs images screenshots
git commit -m "Release WikiFlip v1.1"
git tag -a v1.1 -m "WikiFlip v1.1"
git push origin main
git push origin v1.1
```

Guardar el SHA del commit:

```powershell
git rev-parse HEAD
```

## 4. Preparar manifest.yml para el catalogo

Copiar `docs/manifest.yml.example` y reemplazar:

```text
https://github.com/YOUR_USERNAME/WikiFlip.git
REPLACE_WITH_SOURCE_COMMIT_SHA
```

El archivo final debe agregarse en el fork del catalogo oficial bajo:

```text
applications/Tools/wikiflip/manifest.yml
```

## 5. Enviar pull request al catalogo oficial

```powershell
git clone https://github.com/YOUR_USERNAME/flipper-application-catalog.git
cd flipper-application-catalog
git checkout -b YOUR_USERNAME_wikiflip_1.1
mkdir applications\Tools\wikiflip
copy C:\ruta\manifest.yml applications\Tools\wikiflip\manifest.yml
git add applications\Tools\wikiflip\manifest.yml
git commit -m "Add WikiFlip v1.1"
git push origin YOUR_USERNAME_wikiflip_1.1
```

Abrir PR hacia:

```text
https://github.com/flipperdevices/flipper-application-catalog
```

Cuando el PR sea aprobado y mergeado, WikiFlip aparecera en:

```text
https://lab.flipper.net/apps
```

## 6. Instalacion desde Flipper Lab

1. Abrir Google Chrome o Microsoft Edge.
2. Ir a `https://lab.flipper.net/apps`.
3. Conectar el Flipper Zero por USB.
4. Buscar `WikiFlip`.
5. Click en `Install`.
