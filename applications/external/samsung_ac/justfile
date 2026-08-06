set shell := ["bash", "-eu", "-o", "pipefail", "-c"]

# --- config (override on the CLI, e.g. `just target=7 origin= build`) ---
appid    := "samsung_ac_remote"
category := "Infrared"
fw       := justfile_directory() + "/../flipperzero-firmware"
port     := "/dev/cu.usbmodemflip_Wlipurk1"
# f6 fork (az0v) by default; for stock f7 use `target=7 origin=`
target   := "6"
origin   := "az0v"
builddir := fw + "/build/f" + target + "-firmware-D"

# show recipes
default:
    @just --list

# symlink this app into the firmware tree's applications_user/
link:
    @ln -sfn "{{justfile_directory()}}" "{{fw}}/applications_user/{{appid}}"
    @echo "linked {{appid}} -> {{fw}}/applications_user/{{appid}}"

# build the fap against the SDK (uses the firmware tree's pinned fbt toolchain)
build: link
    cd "{{fw}}" && FBT_NO_SYNC=1 ./fbt TARGET_HW={{target}} {{ if origin != "" { "FIRMWARE_ORIGIN=" + origin } else { "" } }} fap_{{appid}}
    @echo "built: {{builddir}}/.extapps/{{appid}}.fap"

# deploy the built fap to the device over USB, into /ext/apps/<Category>/
deploy:
    #!/usr/bin/env bash
    set -euo pipefail
    fap="{{builddir}}/.extapps/{{appid}}.fap"
    [ -f "$fap" ] || { echo "not built: $fap (run 'just build')"; exit 1; }
    python3 "{{fw}}/scripts/storage.py" -p "{{port}}" mkdir "/ext/apps/{{category}}" || true
    python3 "{{fw}}/scripts/storage.py" -p "{{port}}" send "$fap" "/ext/apps/{{category}}/{{appid}}.fap"
    echo "deployed {{appid}} -> /ext/apps/{{category}}/{{appid}}.fap"

# build + deploy in one go
install: build deploy
    @echo "installed {{appid}}"

# remove the symlink from the firmware tree
clean:
    rm -f "{{fw}}/applications_user/{{appid}}"
    @echo "unlinked {{appid}}"
