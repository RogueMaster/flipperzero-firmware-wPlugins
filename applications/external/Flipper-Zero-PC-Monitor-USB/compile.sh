#!/bin/bash
set -e

APP_DIR=$(pwd)
# Default relative path from the project structure observed
DEFAULT_FIRMWARE_DIR="../flipperzero-firmware-wPlugins"

# Ask for firmware directory
read -p "Enter firmware directory path [default: $DEFAULT_FIRMWARE_DIR]: " FIRMWARE_DIR
FIRMWARE_DIR=${FIRMWARE_DIR:-$DEFAULT_FIRMWARE_DIR}

# Resolve absolute path for firmware dir
if [[ "$FIRMWARE_DIR" != /* ]]; then
    # Helper to resolve relative path to absolute
    FIRMWARE_DIR=$(realpath -m "$APP_DIR/$FIRMWARE_DIR")
fi

APP_ID="pc_monitor_usb"
LINK_TARGET="$FIRMWARE_DIR/applications_user/$APP_ID"

if [ ! -d "$FIRMWARE_DIR" ]; then
    echo "Error: Firmware directory not found at $FIRMWARE_DIR"
    exit 1
fi

# Create local build dir
mkdir -p build

if [ ! -L "$LINK_TARGET" ] && [ ! -d "$LINK_TARGET" ]; then
    echo "Linking app to firmware..."
    ln -s "$APP_DIR" "$LINK_TARGET"
else
    echo "Link already exists at $LINK_TARGET"
fi

echo "Compiling..."
cd "$FIRMWARE_DIR"
./fbt fap_$APP_ID

# Copy artifact to local build dir
echo "Copying artifact to local build directory..."
# Detect build directory
BUILD_TARGET=$(find build -maxdepth 1 -type d -name "f7-firmware-*" | head -n 1)

if [ -z "$BUILD_TARGET" ]; then
    echo "Error: Could not detect build target directory (e.g., f7-firmware-D or f7-firmware-C)"
    exit 1
fi

cp "$BUILD_TARGET/.extapps/${APP_ID}.fap" "$APP_DIR/build/"
echo "Done! App saved to $APP_DIR/build/${APP_ID}.fap"
