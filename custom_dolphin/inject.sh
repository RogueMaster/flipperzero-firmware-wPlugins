#!/bin/bash
# Inyecta la personalizacion de Miguel en el arbol antes de compilar (buildRelease.sh / fbt).
# Se ejecuta desde la raiz del workspace: `bash custom_dolphin/inject.sh`.
set +e
# CA = este mismo directorio custom_dolphin del repo (antes clonaba la rama custom-assets a /tmp,
# que ya no existe). Ahora los assets viven en 420 y se toman de aqui directamente. (4 Jul 2026)
CA="$(cd "$(dirname "$0")" && pwd)"

# 1) MRUWZUM como animacion EXTERNAL (PNG) -> fbt la compila al formato exacto del firmware.
#    Es el fondo principal de Miguel y va por la via oficial (robusta).
echo "[inject] MRUWZUM como external PNG (la compila fbt)"
rm -rf assets/dolphin/external/MRUWZUM
cp -rf "$CA/MRUWZUM_official" assets/dolphin/external/MRUWZUM
if ! grep -q "Name: MRUWZUM" assets/dolphin/external/manifest.txt; then
cat >> assets/dolphin/external/manifest.txt <<'MEOF'

Name: MRUWZUM
Min butthurt: 0
Max butthurt: 18
Min level: 1
Max level: 30
Weight: 9
MEOF
fi

# 2) Los otros 3 fondos (.bm) usaban el directorio .blank_files, que RogueMaster ELIMINO en su
#    reestructuracion (jul 2026). Se aplican SOLO si ese directorio existe; si no, se saltan
#    limpiamente (sin ensuciar el log ni romper el build). Si upstream lo reintroduce, se reaplican.
if [ -d .blank_files ]; then
  echo "[inject] .blank_files presente -> aplicando los 3 fondos .bm + manifest de 5"
  for f in L1_POKEMON_WALK_128x64 L1_SPACEINVADERS_INV_128x64 Kirby; do
    cp -rf "$CA/$f" ".blank_files/$f"
  done
  cp -f "$CA/manifest_5fondos.txt" .blank_files/manifest_Minimal.txt
  # 3) SOLO5MIGUEL: post-proceso en buildRelease.sh, solo si el ancla existe (si no, no-op limpio).
  ANCHOR='cp -rf .blank_files/MjK_blank_128x64 build/f7-firmware-C/resources/dolphin/'
  if grep -qF "$ANCHOR" buildRelease.sh && ! grep -q "SOLO5MIGUEL" buildRelease.sh; then
    python3 - <<'PYEOF'
s=open('buildRelease.sh').read()
inject='''# SOLO5MIGUEL: dejar solo los 5 fondos de Miguel
mkdir -p /tmp/keep5
cp -rf build/f7-firmware-C/resources/dolphin/wrenchathome_F0Pattern_128x64 /tmp/keep5/ 2>/dev/null
cp -rf build/f7-firmware-C/resources/dolphin/MRUWZUM /tmp/keep5/ 2>/dev/null
rm -rf build/f7-firmware-C/resources/dolphin/*
mkdir -p build/f7-firmware-C/resources/dolphin
cp -rf /tmp/keep5/* build/f7-firmware-C/resources/dolphin/ 2>/dev/null
cp -rf .blank_files/L1_POKEMON_WALK_128x64 .blank_files/L1_SPACEINVADERS_INV_128x64 .blank_files/Kirby build/f7-firmware-C/resources/dolphin/
'''
anchor='cp -rf .blank_files/MjK_blank_128x64 build/f7-firmware-C/resources/dolphin/'
s=s.replace(anchor, inject+anchor, 1)
open('buildRelease.sh','w').write(s)
print('[inject] buildRelease.sh modificado (SOLO5MIGUEL)')
PYEOF
  fi
else
  echo "[inject] .blank_files no existe (RogueMaster lo elimino) -> por ahora solo MRUWZUM"
fi
echo "[inject] HECHO"
