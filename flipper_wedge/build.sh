#!/bin/bash
# Build Flipper Wedge for a specific firmware

APP_NAME="flipper_wedge"
APP_DIR="/home/work/flipper wedge"

# Default values (will be set based on firmware choice)
FIRMWARE="official"
BRANCH=""
TAG=""
USE_DEFAULT="true"

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --branch)
            BRANCH="$2"
            USE_DEFAULT="false"
            shift 2
            ;;
        --tag)
            TAG="$2"
            USE_DEFAULT="false"
            shift 2
            ;;
        official|ofw|unleashed|ul|momentum|mntm|xtreme|xfw|roguemaster|rm)
            FIRMWARE="$1"
            shift
            ;;
        *)
            echo "Unknown argument: $1"
            echo ""
            echo "Usage: $0 [firmware] [--branch BRANCH] [--tag TAG]"
            echo ""
            echo "Firmware options:"
            echo "  official:     ofw      (default: latest stable tag)"
            echo "  unleashed:    ul       (default: release branch)"
            echo "  momentum:     mntm     (default: release branch)"
            echo "  roguemaster:  rm       (default: release branch)"
            echo ""
            echo "Flags:"
            echo "  --branch BRANCH  Git branch to checkout"
            echo "  --tag TAG        Git tag to checkout (overrides --branch)"
            echo ""
            echo "Examples:"
            echo "  $0 official                # Latest stable release (recommended)"
            echo "  $0 official --branch dev   # Development branch"
            echo "  $0 official --tag 1.3.4    # Specific version"
            echo "  $0 momentum                # Momentum release branch"
            exit 1
            ;;
    esac
done

# Git checkout function with version change detection
checkout_firmware_version() {
    local fw_path=$1
    local branch=$2
    local tag=$3
    local fw_name=$4

    echo "📥 Fetching firmware updates..."
    cd "$fw_path"

    # Store current state
    local current_ref=$(git rev-parse --abbrev-ref HEAD 2>/dev/null || git rev-parse --short HEAD)

    # Fetch latest
    git fetch --all --tags --quiet

    # Determine target
    if [ -n "$tag" ]; then
        target="tags/$tag"
        target_name="tag $tag"
    else
        target="origin/$branch"
        target_name="branch $branch"
    fi

    # Validate target exists
    if ! git rev-parse --verify "$target" >/dev/null 2>&1; then
        echo "❌ Error: $target_name not found in ${fw_name} firmware"
        exit 1
    fi

    # Checkout
    git checkout "$target" --quiet

    # Detect change and alert
    local new_ref=$(git rev-parse --short HEAD)
    local old_commit=$(git rev-parse "$current_ref" 2>/dev/null || echo "unknown")
    local new_commit=$(git rev-parse HEAD)

    if [ "$old_commit" != "$new_commit" ]; then
        echo -e "\033[0;31m⚠️  FIRMWARE VERSION CHANGED: $current_ref → $target_name ($(git rev-parse --short HEAD))\033[0m"
    fi

    # Update submodules
    echo "🔄 Updating submodules..."
    git submodule update --init --recursive --quiet
}

# Map firmware name to paths
case $FIRMWARE in
    official|ofw)
        FW_PATH="/home/work/flipperzero-firmware"
        FW_NAME="Official"
        ;;
    unleashed|ul)
        FW_PATH="/home/work/unleashed-firmware"
        FW_NAME="Unleashed"
        ;;
    momentum|mntm|xtreme|xfw)
        FW_PATH="/home/work/Momentum-Firmware"
        FW_NAME="Momentum"
        ;;
    roguemaster|rm)
        FW_PATH="/home/work/roguemaster-firmware"
        FW_NAME="RogueMaster"
        ;;
esac

if [ ! -d "$FW_PATH" ]; then
    echo "❌ Error: ${FW_NAME} firmware not found at ${FW_PATH}"
    echo "Clone it with:"
    case $FIRMWARE in
        official|ofw)
            echo "  git clone --recursive https://github.com/flipperdevices/flipperzero-firmware.git ${FW_PATH}"
            ;;
        unleashed|ul)
            echo "  git clone --recursive https://github.com/DarkFlippers/unleashed-firmware.git ${FW_PATH}"
            ;;
        momentum|mntm|xtreme|xfw)
            echo "  git clone --recursive https://github.com/Next-Flip/Momentum-Firmware.git ${FW_PATH}"
            ;;
        roguemaster|rm)
            echo "  git clone --recursive https://github.com/RogueMaster/flipperzero-firmware-wPlugins.git ${FW_PATH}"
            ;;
    esac
    exit 1
fi

# Set firmware-specific defaults if user didn't specify branch/tag
if [ "$USE_DEFAULT" = "true" ]; then
    cd "$FW_PATH"
    git fetch --all --tags --quiet 2>/dev/null

    case $FIRMWARE in
        official|ofw)
            # Official: Use latest stable tag (e.g., 1.3.4)
            TAG=$(git tag | grep -E '^[0-9]+\.[0-9]+\.[0-9]+$' | sort -V | tail -1)
            if [ -z "$TAG" ]; then
                echo "⚠️  Warning: No stable tags found, using release branch"
                BRANCH="release"
                TAG=""
            fi
            ;;
        unleashed|ul)
            # Unleashed: Use release branch
            BRANCH="release"
            ;;
        momentum|mntm|xtreme|xfw)
            # Momentum: Use release branch
            BRANCH="release"
            ;;
        roguemaster|rm)
            # RogueMaster: Use release branch
            BRANCH="release"
            ;;
    esac
fi

# Ensure symlink exists
if [ ! -L "${FW_PATH}/applications_user/${APP_NAME}" ]; then
    echo "Creating symlink..."
    ln -s "${APP_DIR}" "${FW_PATH}/applications_user/${APP_NAME}"
fi

# Checkout requested firmware version
checkout_firmware_version "$FW_PATH" "$BRANCH" "$TAG" "$FW_NAME"

echo ""
echo "🔨 Building ${APP_NAME} for ${FW_NAME} firmware..."
cd "${FW_PATH}"
./fbt fap_${APP_NAME}

# Get app version from application.fam
APP_VERSION=$(grep 'fap_version=' "${APP_DIR}/application.fam" | sed 's/.*fap_version="\([^"]*\)".*/\1/')
if [ -z "$APP_VERSION" ]; then
    APP_VERSION="unknown"
fi

# Determine firmware version for filename
if [ -n "$TAG" ]; then
    FW_VERSION="$TAG"
else
    # Try to get version from git describe or use branch name
    cd "$FW_PATH"
    FW_VERSION=$(git describe --tags --abbrev=0 2>/dev/null || echo "$BRANCH")
fi

# Create output directory structure
OUTPUT_DIR="${APP_DIR}/dist"
mkdir -p "${OUTPUT_DIR}"

# Dynamically find the FAP file in the build directory
# Build directories can be f7-firmware-C (compact), f7-firmware-D (debug), etc.
SOURCE_FAP=""
for build_dir in "${FW_PATH}/build/f7-firmware-"*; do
    if [ -d "$build_dir" ]; then
        candidate="${build_dir}/.extapps/${APP_NAME}.fap"
        if [ -f "$candidate" ]; then
            SOURCE_FAP="$candidate"
            break
        fi
    fi
done

# Filename format: flipper-wedge_appVersion_firmware_firmwareVersion.fap
FIRMWARE_LOWER=$(echo "$FIRMWARE" | tr '[:upper:]' '[:lower:]')
DEST_FAP="${OUTPUT_DIR}/flipper-wedge_${APP_VERSION}_${FIRMWARE_LOWER}_${FW_VERSION}.fap"

if [ -n "$SOURCE_FAP" ] && [ -f "$SOURCE_FAP" ]; then
    cp "$SOURCE_FAP" "$DEST_FAP"
    echo ""
    echo "✅ Build complete for ${FW_NAME}!"
    echo "📦 FAP saved to: ${DEST_FAP}"
    echo "   (Built from: ${SOURCE_FAP})"
else
    echo ""
    echo "❌ Error: Build succeeded but FAP not found in ${FW_PATH}/build/f7-firmware-*/.extapps/"
    echo "   Searched directories:"
    ls -d "${FW_PATH}/build/f7-firmware-"* 2>/dev/null || echo "   No build directories found"
    exit 1
fi
