#!/bin/bash

# Check for file argument
if [ "$#" -ne 1 ]; then
    echo "Usage: $0 <bitmap file>"
    exit 1
fi

FILE="$1"

# Function to print in color
print_color() {
    echo -e "\e[$2m$1\e[0m"
}


# Read BMP header using xxd
HEADER=$(xxd -l 54 -g 1 -c 54 "$FILE")

# Extract fields
TYPE=$(echo "$HEADER" | awk '{print $2 $3}')
SIZE=$(echo "$HEADER" | awk '{print $6 $5 $4 $3}')
RESERVED=$(echo "$HEADER" | awk '{print $8 $7 $10 $9}')
OFFSET=$(echo "$HEADER" | awk '{print $12 $11 $14 $13}')
DIB_HEADER_SIZE=$(echo "$HEADER" | awk '{print $16 $15 $18 $17}')

# Print fields with colors
print_color "Type: $TYPE (BM)" 32
print_color "Size: $SIZE" 33
print_color "Reserved: $RESERVED" 34
print_color "Offset: $OFFSET" 35
print_color "DIB Header Size: $DIB_HEADER_SIZE" 36
