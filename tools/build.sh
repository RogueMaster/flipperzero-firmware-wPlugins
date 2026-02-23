#!/bin/sh

application_name="co2_app"
repo_root=$(dirname $0)/..

mkdir -p ${repo_root}/dist

# Fetch all firmwares submodules
git submodule update --init --recursive

# Set default build mode
build_mode="unleashed"
is_run=false

while getopts "f:i" opt; do
  case $opt in
    f)
      build_mode=$OPTARG
      ;;
    i)
      is_run=true
      ;;
    \?)
      echo "Invalid option: -$OPTARG" >&2
      exit 1
      ;;
    :)
      echo "Option -$OPTARG requires an argument." >&2
      exit 1
      ;;
  esac
done

cd "${repo_root}/.${build_mode}-firmware"

# Define the possible file paths for API version
file_path1="firmware/targets/f7/api_symbols.csv"
file_path2="targets/f7/api_symbols.csv"

extract_api_version() {
    local file_path=$1
    local api_version=$(awk -F',' 'NR == 2 {print $3}' "$file_path")
    echo "$api_version"
}

api_version=$(extract_api_version "$file_path1")

if [ -z "$api_version" ]; then
    api_version=$(extract_api_version "$file_path2")
fi

if [ -z "$api_version" ]; then
    echo "Warning: API version not found, using 'unknown'"
    api_version="unknown"
else
    echo "API version: $api_version"
fi

app_suffix="${build_mode}_${api_version}"

export FBT_NO_SYNC=1

rm -rf applications_user/$application_name
rm -rf build/f7-firmware-D/.extapps

# Copy app source into firmware tree (source is in repo root, not a subdirectory)
mkdir -p applications_user/$application_name
cp -r ../application.fam applications_user/$application_name/
cp -r ../co2_app.c ../co2_app.h ../co2_app_i.h applications_user/$application_name/
cp -r ../sensors applications_user/$application_name/
cp -r ../views applications_user/$application_name/
cp -r ../scenes applications_user/$application_name/
cp -r ../helpers applications_user/$application_name/
cp -r ../flap applications_user/$application_name/
test -d ../images && cp -r ../images applications_user/$application_name/
test -f ../co2_app_10.png && cp ../co2_app_10.png applications_user/$application_name/

if $is_run; then
  ./fbt launch_app APPSRC=$application_name
else
  ./fbt "fap_${application_name}"
fi

cp "build/f7-firmware-D/.extapps/${application_name}.fap" "../dist/${application_name}_${app_suffix}.fap" 2>/dev/null && \
  echo "Built: dist/${application_name}_${app_suffix}.fap" || \
  echo "Build failed or .fap not found"
