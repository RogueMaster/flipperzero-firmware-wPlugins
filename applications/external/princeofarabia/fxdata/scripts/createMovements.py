import sys

if len(sys.argv) != 3:
    print("Usage: python program.py input_file output_file")
    sys.exit(1)

input_file_path = sys.argv[1]
output_file_path = sys.argv[2]

with open(input_file_path, "r") as input_file, open(
    output_file_path, "w"
) as output_file:

    for line in input_file:
        words = line.strip().split()
        if words and words[0] == "#define":
            word = words[1]
            if word.endswith("SEQ"):
                rest_of_line = " ".join(words[2:]).rstrip().replace(" ", "")
                output_line = f"{rest_of_line}\n"
                output_file.write(output_line)
