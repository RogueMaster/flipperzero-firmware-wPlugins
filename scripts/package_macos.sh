#!/bin/zsh

set -euo pipefail

script_dir="${0:A:h}"
project_root="${script_dir:h}"
macos_root="$project_root/macos"
output_root="$project_root/dist/macos"
app_name="Flipper Internet Bridge"
app_path="$output_root/$app_name.app"
dmg_path="$output_root/Flipper-Internet-Bridge.dmg"
scratch_path="$macos_root/.build-package"
module_cache="$scratch_path/module-cache"

mkdir -p "$output_root" "$module_cache"

echo "Building the macOS helper in release mode..."
(
    cd "$macos_root"
    CLANG_MODULE_CACHE_PATH="$module_cache" \
    SWIFTPM_MODULECACHE_OVERRIDE="$module_cache" \
    swift build -c release --scratch-path "$scratch_path"
)

binary_path="$scratch_path/release/FlipperInternetBridge"
if [[ ! -x "$binary_path" ]]; then
    echo "Error: built executable was not found: $binary_path" >&2
    exit 1
fi

if [[ -e "$app_path" ]]; then
    rm -rf "$app_path"
fi
mkdir -p "$app_path/Contents/MacOS" "$app_path/Contents/Resources"
cp "$binary_path" "$app_path/Contents/MacOS/FlipperInternetBridge"
cp "$macos_root/Resources/Info.plist" "$app_path/Contents/Info.plist"
cp "$macos_root/Resources/AppIcon.icns" "$app_path/Contents/Resources/AppIcon.icns"

# Local ad-hoc signature for builds without an Apple Developer certificate.
codesign --force --deep --sign - "$app_path"
codesign --verify --deep --strict --verbose=2 "$app_path"

if [[ -e "$dmg_path" ]]; then
    rm -f "$dmg_path"
fi

staging_dir="$(mktemp -d "${TMPDIR:-/tmp}/fib-dmg.XXXXXX")"
trap 'rm -rf "$staging_dir"' EXIT
cp -R "$app_path" "$staging_dir/"
ln -s /Applications "$staging_dir/Applications"
hdiutil create \
    -volname "$app_name" \
    -srcfolder "$staging_dir" \
    -ov \
    -format UDZO \
    "$dmg_path"

echo
echo "Ready: $app_path"
echo "Ready: $dmg_path"
echo "Architecture: $(file "$binary_path" | sed 's/.*Mach-O 64-bit executable //')"
