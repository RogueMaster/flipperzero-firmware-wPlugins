#!/bin/bash

# Define source and destination paths
SRC=".vscode/compile_commands.json"
DEST=".vscode/clang_compile_commands/compile_commands.json"

# Copy the file
cp "$SRC" "$DEST"

# Remove the phrase "-mword-relocations" from the copied file
sed -i '' 's/-mword-relocations//g' "$DEST"

echo "File copied and modified successfully."