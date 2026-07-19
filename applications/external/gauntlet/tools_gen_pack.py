#!/usr/bin/env python3
"""Generate the Gauntlet starter challenge pack.

Single source of truth: emits BOTH
  packs/starter.pack        - a human-readable pack shipped in the repo, and
  helpers/ctf_starter.h     - the same bytes embedded for first-run provisioning.

Challenge payloads (DATA) are produced from the intended plaintext with the very
same transforms the on-device Toolkit reverses, so every cipher task is provably
solvable on the Flipper. Answers are stored as ANSWER <hash> (FNV-1a of the
normalised flag) so a solver reading the .pack still can't read the solution -
the hashing rule mirrors helpers/ctf_pack.c exactly."""
import os

HERE = os.path.dirname(__file__)


# --- hashing (mirror of ctf_answer_hash / ctf_hash in helpers/ctf_pack.c) ---
def answer_hash(s, exact=False):
    s = s.lstrip(" \t")
    if not exact:
        s = "".join(chr(ord(c) + 32) if "A" <= c <= "Z" else c for c in s)
    s = s.rstrip(" \t")
    h = 2166136261
    for b in s.encode("latin-1", "replace"):
        h ^= b
        h = (h * 16777619) & 0xFFFFFFFF
    return h


# --- encoders (inverse of the on-device decoders) --------------------------
def caesar(text, shift):
    out = []
    for c in text:
        if "a" <= c <= "z":
            out.append(chr((ord(c) - 97 + shift) % 26 + 97))
        elif "A" <= c <= "Z":
            out.append(chr((ord(c) - 65 + shift) % 26 + 65))
        else:
            out.append(c)
    return "".join(out)


def b64(text):
    import base64

    return base64.b64encode(text.encode()).decode()


def hexspace(text):
    return " ".join(f"{b:02x}" for b in text.encode())


def binspace(text):
    return " ".join(f"{b:08b}" for b in text.encode())


MORSE = {
    "A": ".-", "B": "-...", "C": "-.-.", "D": "-..", "E": ".", "F": "..-.",
    "G": "--.", "H": "....", "I": "..", "J": ".---", "K": "-.-", "L": ".-..",
    "M": "--", "N": "-.", "O": "---", "P": ".--.", "Q": "--.-", "R": ".-.",
    "S": "...", "T": "-", "U": "..-", "V": "...-", "W": ".--", "X": "-..-",
    "Y": "-.--", "Z": "--..", "0": "-----", "1": ".----", "2": "..---",
    "3": "...--", "4": "....-", "5": ".....", "6": "-....", "7": "--...",
    "8": "---..", "9": "----.",
}


def morse(text):
    words = text.upper().split(" ")
    return " / ".join(" ".join(MORSE[ch] for ch in w if ch in MORSE) for w in words)


def atbash(text):
    out = []
    for c in text:
        if "a" <= c <= "z":
            out.append(chr(122 - (ord(c) - 97)))
        elif "A" <= c <= "Z":
            out.append(chr(90 - (ord(c) - 65)))
        else:
            out.append(c)
    return "".join(out)


# --- the challenges --------------------------------------------------------
# Each: title, category, points, difficulty, prompt, data, hint, flag, [exact]
CH = [
    dict(
        title="Welcome to the Gauntlet", category="misc", points=50, difficulty="easy",
        prompt="Every run starts with a warmup. Your flag is the word GAUNTLET, all lowercase. Press OK and type it.",
        data="", hint="It is literally in the prompt.", flag="gauntlet", plain_flag=True,
    ),
    dict(
        title="Caesar's Salad", category="cipher", points=100, difficulty="easy",
        prompt="A classic shift cipher hides a name. Decode it and submit ONLY the name.",
        data=caesar("THE KEY IS BRUTUS", 3),
        hint="Every letter moved by 3. Open the Toolkit and dial ROT.", flag="brutus",
    ),
    dict(
        title="Onion Router", category="cipher", points=100, difficulty="easy",
        prompt="ROT-13 is its own inverse. What is the password? Submit just the word.",
        data=caesar("THE PASSWORD IS ONION", 13),
        hint="ROT-13. The Toolkit opens on it.", flag="onion",
    ),
    dict(
        title="Base of Operations", category="cipher", points=100, difficulty="easy",
        prompt="This string ends in '='. Decode it.",
        data=b64("gh0st_protocol"),
        hint="That padding means Base64.", flag="gh0st_protocol",
    ),
    dict(
        title="Hex Marks the Spot", category="cipher", points=100, difficulty="easy",
        prompt="Two hex digits per character. Turn it into text.",
        data=hexspace("c1ph3r"),
        hint="Use the Hex -> Text tool.", flag="c1ph3r",
    ),
    dict(
        title="Ones and Zeroes", category="cipher", points=100, difficulty="medium",
        prompt="Eight bits per character. Decode the word.",
        data=binspace("PWNED"),
        hint="Binary -> Text. Each group is one letter.", flag="pwned",
    ),
    dict(
        title="Can You Hear Me", category="cipher", points=120, difficulty="medium",
        prompt="Dots, dashes and a slash for the space. Decode it.",
        data=morse("flipper zero"),
        hint="Morse. '/' separates words.", flag="flipper zero",
    ),
    dict(
        title="Through the Looking Glass", category="cipher", points=120, difficulty="medium",
        prompt="The alphabet is mirrored: A<->Z, B<->Y. Decode the word.",
        data=atbash("wizard"),
        hint="That is Atbash.", flag="wizard",
    ),
    dict(
        title="Badge Dump", category="rfid", points=150, difficulty="easy",
        prompt="A 13.56 MHz card's data block spells a word in hex. Read it back.",
        data=hexspace("keycard"),
        hint="Hex -> Text again. RFID blocks are just bytes.", flag="keycard",
    ),
    dict(
        title="Wiegand Unwrapped", category="rfid", points=200, difficulty="hard",
        prompt="A 26-bit Wiegand read. Ignore the first and last (parity) bits. "
               "The next 8 bits are the facility code, then 16 bits are the card ID. "
               "Give the facility code in DECIMAL.",
        data="0 00101010 0000010100111001 0",
        hint="00101010 in binary is the number you want.", flag="42",
    ),
    dict(
        title="Hidden Remote", category="ir", points=150, difficulty="easy",
        prompt="An IR remote's secret label was captured as hex. Decode it.",
        data=hexspace("power"),
        hint="Hex -> Text. It is a button name.", flag="power",
    ),
    dict(
        title="NEC Frame", category="ir", points=200, difficulty="medium",
        prompt="A captured NEC infrared frame carries Address 0x00 and Command 0x45. "
               "Give the command byte in DECIMAL.",
        data="Protocol NEC   Addr 0x00   Cmd 0x45",
        hint="0x45 is 69 in decimal.", flag="69",
    ),
]


def build_pack():
    lines = []
    lines.append("# Gauntlet starter pack - generated by tools_gen_pack.py")
    lines.append("# Edit this file (or drop your own .pack next to it) to add challenges.")
    lines.append("# Keys: PACK AUTHOR DESC | CHALLENGE CATEGORY POINTS DIFFICULTY")
    lines.append("#       PROMPT DATA HINT FLAG/ANSWER EXACT")
    lines.append("")
    lines.append("PACK Gauntlet Starter")
    lines.append("AUTHOR at0m-b0mb")
    lines.append("DESC A tour of every category: cipher, RFID and IR. Start here.")
    lines.append("")
    for c in CH:
        lines.append(f"CHALLENGE {c['title']}")
        lines.append(f"CATEGORY {c['category']}")
        lines.append(f"POINTS {c['points']}")
        lines.append(f"DIFFICULTY {c['difficulty']}")
        lines.append(f"PROMPT {c['prompt']}")
        if c["data"]:
            lines.append(f"DATA {c['data']}")
        if c.get("hint"):
            lines.append(f"HINT {c['hint']}")
        if c.get("exact"):
            lines.append("EXACT")
        if c.get("plain_flag"):
            # demonstrate the easy authoring path: a plaintext FLAG
            lines.append(f"FLAG {c['flag']}")
        else:
            # demonstrate the secret path: a precomputed ANSWER hash
            lines.append(f"ANSWER {answer_hash(c['flag'], c.get('exact', False)):08x}")
        lines.append("")
    return "\n".join(lines) + "\n"


def c_escape(text):
    out = []
    for ch in text:
        if ch == "\\":
            out.append("\\\\")
        elif ch == '"':
            out.append('\\"')
        elif ch == "\n":
            out.append('\\n"\n    "')
        else:
            out.append(ch)
    return "".join(out)


def main():
    pack = build_pack()

    packs_dir = os.path.join(HERE, "packs")
    os.makedirs(packs_dir, exist_ok=True)
    with open(os.path.join(packs_dir, "starter.pack"), "w") as f:
        f.write(pack)
    print("wrote packs/starter.pack", f"({len(pack)} bytes)")

    header = (
        "/* Generated by tools_gen_pack.py - do not edit by hand.\n"
        " * The Gauntlet starter pack, embedded so it can be written to the SD\n"
        " * card on first run when no packs are present. */\n"
        "#pragma once\n\n"
        'static const char CTF_STARTER_PACK[] =\n    "'
        + c_escape(pack)
        + '";\n'
    )
    with open(os.path.join(HERE, "helpers", "ctf_starter.h"), "w") as f:
        f.write(header)
    print("wrote helpers/ctf_starter.h")

    print("\nAnswer hashes:")
    for c in CH:
        tag = "FLAG" if c.get("plain_flag") else "ANSWER"
        print(f"  {c['title']:28s} {tag:6s} {answer_hash(c['flag'], c.get('exact', False)):08x}  ({c['flag']!r})")


if __name__ == "__main__":
    main()
