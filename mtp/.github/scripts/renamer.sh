#!/bin/bash

# Check if $DIST_DIR and $FILE_SUFFIX are set
if [ -z "$FILE_SUFFIX" ] || [ -z "$DIST_FILE" ]; then
    echo "Error: \$DIST_DIR and \$FILE_SUFFIX must be set"
    exit 1
fi

# $DIST_FILE
DIST_FILE=$(echo "$DIST_FILE" | head -n 1)
DIST_DIR=$(dirname "$DIST_FILE")

# Loop through files in $DIST_DIR
for file in "$DIST_DIR"/*; do
    # Check if the item is a regular file
    if [ -f "$file" ]; then
        # Extract the filename and extension
        filename=$(basename "$file")
        extension="${filename##*.}"
        name="${filename%.*}"

        # Construct the new filename
        new_filename="${name}-${FILE_SUFFIX}.${extension}"

        # Rename the file
        mv "$file" "$DIST_DIR/${new_filename}"
    fi
done
