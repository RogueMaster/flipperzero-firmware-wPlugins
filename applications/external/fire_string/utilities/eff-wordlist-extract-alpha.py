with open("eff_short_wordlist_1.txt", "r", encoding="utf-8") as f:
    words = f.read().splitlines()

output_file = f"../files/eff_short_wordlist_1_alpha.txt"

alpha = []

for x in words:
    alpha.append(x.split()[1])

with open(output_file, "w", encoding="utf-8") as f:
    f.write("\n".join(alpha))
