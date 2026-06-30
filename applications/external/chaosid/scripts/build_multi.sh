#!/bin/bash
# build_multi.sh - Build ChaosID .fap para multiplos firmwares
# Gera dist/multi/chaos_id-<firmware>.fap
#
# Uso: ./scripts/build_multi.sh
#
# Adiciona um novo firmware: insere a linha na lista FIRMWARES abaixo
# com o nome curto + URL do directory.json do canal SDK.

set -e

# cd pra raiz do projeto (script pode ser chamado de qualquer lugar)
cd "$(dirname "$0")/.."

# Mapa: firmware_name -> SDK index URL
declare -A FIRMWARES=(
  ["momentum"]="https://up.momentum-fw.dev/firmware/directory.json"
  ["official"]="https://update.flipperzero.one/firmware/directory.json"
  # ["unleashed"]="https://up.unleashedflip.com/directory.json"
)

# Firmware default - SDK fica setado nele no fim, pra continuar desenvolvendo
DEFAULT_FW="momentum"

OUT_DIR="dist/multi"
mkdir -p "$OUT_DIR"
rm -f "$OUT_DIR"/*.fap

for fw in momentum official; do
  url="${FIRMWARES[$fw]}"
  echo ""
  echo "=========================================="
  echo "  Building for: $fw"
  echo "  SDK: $url"
  echo "=========================================="
  ufbt update --index-url="$url"
  ufbt -c >/dev/null
  ufbt
  cp dist/chaos_id.fap "$OUT_DIR/chaos_id-${fw}.fap"
  echo "[ok] $OUT_DIR/chaos_id-${fw}.fap"
done

# Restaura SDK default pra continuar desenvolvendo
echo ""
echo "=========================================="
echo "  Restoring default SDK ($DEFAULT_FW)"
echo "=========================================="
ufbt update --index-url="${FIRMWARES[$DEFAULT_FW]}" >/dev/null

echo ""
echo "=========================================="
echo "  FAPs gerados:"
ls -la "$OUT_DIR/"*.fap
echo "=========================================="
