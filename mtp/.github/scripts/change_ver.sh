#!/bin/bash

# Check if $TAG_NAME is set
if [ -z "$TAG_NAME" ]; then
    echo "Error: \$TAG_NAME must be set"
    exit 1
fi

# Extract the version from $TAG_NAME
version="${TAG_NAME##v}"

# Loop through all files in the current directory
for file in *; do
    # Check if the file is a regular file
    if [ -f "$file" ]; then
        # Make a temporary copy of the file
        cp "$file" "$file.tmp"

        # Perform the replacement
        sed "s/fap_version=\"[0-9]*\.[0-9]*\"/fap_version=\"$version\"/" "$file.tmp" > "$file"

        # Remove the temporary file
        rm "$file.tmp"
    fi
done
