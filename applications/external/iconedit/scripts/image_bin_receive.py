#!/usr/bin/env python3

"""
Takes keyboard input that describes:
- a filename
- Hex encoded representation of that file
and saves it as that filename in the current directory.
"""

import os, sys

END_OF_INPUT = "EOF"  # This can't be interpreted as hex since it contains an'O'!

user_supplied_filename = False
filename = ""
if len(sys.argv) == 2:
    filename = sys.argv[1]
    user_supplied_filename = True
    print(f"Using provided filename: {filename}")

if not user_supplied_filename:
    # Ready to receive the filename
    print("Ready to receive the filename! Press OK on the Flipper")

    # read filename
    while True:
        line = sys.stdin.readline().strip()
        if line == END_OF_INPUT:
            break
        filename = line

# check if file exists, if so, prompt user if they want to overwrite
if os.path.exists(filename):
    if input("File already exists, overwrite? [Y/n]: ").lower() == "n":
        print("Preserving file. Exiting...")
        exit(1)

# Ready to receive file data
output = open(filename, "wb")
if user_supplied_filename:
    print(
        "When asked to 'Send filename?' on Flipper, press RIGHT to skip and immediately send data"
    )
else:
    print("Ready to receive file data. Press OK on the Flipper")

write_complete = False
while not write_complete:
    line = sys.stdin.readline().strip().upper()
    if line == END_OF_INPUT:
        write_complete = True
        continue
    if len(line) == 0:
        continue
    if len(line) % 2 != 0 or any(h not in "0123456789ABCDEF" for h in line):
        print("Malformed data! Exiting...")
        break
    output.write(bytes.fromhex(line))
output.close()

if write_complete:
    print(f"File {filename} saved!")
else:
    os.remove(filename)
    exit(1)
