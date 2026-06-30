#!/bin/bash

if [[ "$OSTYPE" != "darwin"* ]]; then
    echo "Unsupported OS"
    exit 1
fi

PROJECT_PATH="$(pwd)"
VSCODE_PATH="${PROJECT_PATH}/.vscode"
CONFIG="compile_commands.json"

VSCODE_CONFIG_PATH="${VSCODE_PATH}/${CONFIG}"
PROJECT_CONFIG_PATH="${PROJECT_PATH}/${CONFIG}"
TMP_PATH="${PROJECT_PATH}/tmp.json"
TOOLCHAIN_PATH="${HOME}/.ufbt/toolchain/arm64-darwin"
CLANGD_PATH="${TOOLCHAIN_PATH}/bin/clangd"

FLAGS_TO_ADD=" --target=arm-none-eabi -Wno-unused-include-directive --sysroot=${TOOLCHAIN_PATH}/arm-none-eabi"
FLAGS_TO_REMOVE="-mword-relocations"
FLAGS_TO_REMOVE_ARRAY=$(echo "$FLAGS_TO_REMOVE" | jq -R 'split(" ")')

echo "Installing ufbt..."
python3 -m pip install --upgrade ufbt

echo "Updating ufbt..."
ufbt update

echo "Generate compile_commands.json"
ufbt cdb

echo "Move compile_commands.json to project root"
mv "${VSCODE_CONFIG_PATH}" "${PROJECT_CONFIG_PATH}"

echo "Add extra flags to compile_commands.json"
jq --arg flags "${FLAGS_TO_ADD}" 'map(.command += $flags)' "${PROJECT_CONFIG_PATH}" > "${TMP_PATH}" && mv "${TMP_PATH}" "${PROJECT_CONFIG_PATH}"

echo "Remove conflicting flags from compile_commands.json"
jq --argjson flags "${FLAGS_TO_REMOVE_ARRAY}" '
  map(
    .command = (
      (.command | split(" "))
      | map(select(. as $word | $flags | index($word) | not))
      | join(" ")
    )
  )
' "${PROJECT_CONFIG_PATH}" > "${TMP_PATH}" && mv "${TMP_PATH}" "${PROJECT_CONFIG_PATH}"

echo "Remove .vscode dir"
rm -rf "${VSCODE_PATH}"

echo "Remove .clangd file"
rm -rf "${PROJECT_PATH}/.clangd"

printf "Done\n\n"

echo "Update Language Server path in C-Dragon extension settings to:"
echo "${CLANGD_PATH}"
