#!/bin/bash
# set -e: si fbt (u otro paso real) falla, el script sale != 0 y Jenkins marca el build en ROJO.
# Antes terminaba siempre con el echo final (exit 0) y escondía fallos de compilación (4 Jul 2026).
set -e
rm -rf RM*-*-*.tgz RM*-*-*.zip
# .sconsign.dblite dist build
git pull || true          # benigno: puede no aplicar (detached HEAD / sin upstream tracking)
DATE_VAR=`date +%m%d`
TIME_VAR=`date +%H%M`
HASH_VAR=`git rev-parse \`git branch -r --sort=committerdate | tail -1\` | awk '{print substr($0,1,8)}' | tail -1`
# PROTOBUF TAGS: el fork RogueMaster/flipperzero-protobuf sirve CERO tags (verificado 4 Jul 2026),
# asi que fbt no puede versionar protobuf ("Failed to process git tags"). Los tags reales estan en el
# repo ORIGINAL de flipperdevices; los traemos de ahi (0.25 etc. son ancestros del commit fijado).
git -C assets/protobuf fetch --tags --force https://github.com/flipperdevices/flipperzero-protobuf.git || true
git -C assets/protobuf describe --tags --abbrev=0 || echo "AVISO: protobuf sigue sin tag reachable"
# FBT_NO_SYNC=1: NO dejar que fbt re-sincronice los submodulos aqui (pisaria los tags recien traidos).
# Asi coincide con el build local que SI funciona.
FBT_NO_SYNC=1 ./fbt updater_package
# Verificar que fbt realmente generó el paquete; si no, abortar en ROJO (no fingir SUCCESS).
test -d "dist/f7-C/f7-update-rm-420-$HASH_VAR" || { echo "ERROR: fbt no generó dist/f7-C/f7-update-rm-420-$HASH_VAR — la compilación falló"; exit 1; }
cp -r "dist/f7-C/f7-update-rm-420-$HASH_VAR" "RM$DATE_VAR-$TIME_VAR-$HASH_VAR"
zip -rq "RM$DATE_VAR-$TIME_VAR-$HASH_VAR.zip" "RM$DATE_VAR-$TIME_VAR-$HASH_VAR"
tar -czf "RM$DATE_VAR-$TIME_VAR-$HASH_VAR.tgz" "RM$DATE_VAR-$TIME_VAR-$HASH_VAR"
rm -rf "RM$DATE_VAR-$TIME_VAR-$HASH_VAR"
git stash || true
echo "BUILD COMPLETED, ZIP AND TGZ GENERATED FOR RM$DATE_VAR-$TIME_VAR-$HASH_VAR"
