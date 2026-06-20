#!/bin/bash
# Firmware solo con los 5 fondos de Miguel. MRUWZUM por la FORMA OFICIAL (PNG -> fbt lo compila).
# Se ejecuta desde el workspace del build (antes de buildRelease.sh / fbt).
set +e
CA=/tmp/customassets/custom_dolphin

# 1) MRUWZUM como animacion EXTERNAL (PNG) -> fbt la compila al formato exacto del firmware
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

# 2) los otros 3 fondos como .bm precompilados (ya funcionan)
for f in L1_POKEMON_WALK_128x64 L1_SPACEINVADERS_INV_128x64 Kirby; do
  cp -rf "$CA/$f" ".blank_files/$f"
done
cp -f "$CA/manifest_5fondos.txt" .blank_files/manifest_Minimal.txt

# 3) buildRelease: tras fbt, dejar SOLO los 5 (wrenchathome + MRUWZUM ya compilados por fbt + los 3 .bm)
if ! grep -q "SOLO5MIGUEL" buildRelease.sh; then
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
print('[inject] buildRelease.sh modificado (forma oficial)')
PYEOF
fi
echo "[inject] HECHO: MRUWZUM compilado por fbt + otros 4 + manifest de 5"
