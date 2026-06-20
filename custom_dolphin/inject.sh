#!/bin/bash
# Inyecta los 5 fondos de Miguel y deja SOLO esos en el firmware.
# Se ejecuta desde el workspace del build (donde estan buildRelease.sh y .blank_files/).
set +e
CA=/tmp/customassets/custom_dolphin
echo "[inject] copiando 4 fondos de Miguel a .blank_files"
for f in MRUWZUM L1_POKEMON_WALK_128x64 L1_SPACEINVADERS_INV_128x64 Kirby; do
  cp -rf "$CA/$f" ".blank_files/$f"
done
echo "[inject] reemplazando manifest_Minimal por el de 5 fondos"
cp -f "$CA/manifest_5fondos.txt" .blank_files/manifest_Minimal.txt
# Modificar buildRelease.sh: tras compilar, dejar SOLO los 5 fondos en resources/dolphin
if ! grep -q "SOLO5MIGUEL" buildRelease.sh; then
python3 - <<'PYEOF'
s=open('buildRelease.sh').read()
inject='''# SOLO5MIGUEL: dejar solo los 5 fondos de Miguel en el firmware
cp -rf build/f7-firmware-C/resources/dolphin/wrenchathome_F0Pattern_128x64 /tmp/wrench 2>/dev/null
rm -rf build/f7-firmware-C/resources/dolphin/*
mkdir -p build/f7-firmware-C/resources/dolphin
cp -rf /tmp/wrench build/f7-firmware-C/resources/dolphin/wrenchathome_F0Pattern_128x64 2>/dev/null
cp -rf .blank_files/MRUWZUM .blank_files/L1_POKEMON_WALK_128x64 .blank_files/L1_SPACEINVADERS_INV_128x64 .blank_files/Kirby build/f7-firmware-C/resources/dolphin/
'''
anchor='cp -rf .blank_files/MjK_blank_128x64 build/f7-firmware-C/resources/dolphin/'
s=s.replace(anchor, inject+anchor, 1)
open('buildRelease.sh','w').write(s)
print('[inject] buildRelease.sh modificado')
PYEOF
else
  echo "[inject] buildRelease.sh ya modificado"
fi
echo "[inject] HECHO: firmware llevara solo los 5 fondos de Miguel"
