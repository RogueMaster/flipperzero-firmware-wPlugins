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
./fbt updater_package
# Verificar que fbt realmente generó el paquete; si no, abortar en ROJO (no fingir SUCCESS).
test -d "dist/f7-C/f7-update-rm-420-$HASH_VAR" || { echo "ERROR: fbt no generó dist/f7-C/f7-update-rm-420-$HASH_VAR — la compilación falló"; exit 1; }
cp -r "dist/f7-C/f7-update-rm-420-$HASH_VAR" "RM$DATE_VAR-$TIME_VAR-$HASH_VAR"
zip -rq "RM$DATE_VAR-$TIME_VAR-$HASH_VAR.zip" "RM$DATE_VAR-$TIME_VAR-$HASH_VAR"
tar -czf "RM$DATE_VAR-$TIME_VAR-$HASH_VAR.tgz" "RM$DATE_VAR-$TIME_VAR-$HASH_VAR"
rm -rf "RM$DATE_VAR-$TIME_VAR-$HASH_VAR"
git stash || true
echo "BUILD COMPLETED, ZIP AND TGZ GENERATED FOR RM$DATE_VAR-$TIME_VAR-$HASH_VAR"
