#!/usr/bin/env python3

import os
import sys
import cmd
import re
import shutil


def receive_text(prompt: str | None = "Ready to receive data!") -> list[str]:
    """Reads lines of text from stdin until we receive a blank line"""
    if prompt:
        print(prompt)
    text = []
    while True:
        line = sys.stdin.readline().strip()
        if not line:
            break
        text.append(line)
    return text


def valid_binary_text(text: list[str]) -> bool:
    """Validates that the text represents hex bytes"""
    return all(
        len(line) % 2 == 0
        and all(h in "0123456789ABCDEF" for h in line.upper())
        for line in text
    )


def save_text(text: list[str], filename: str) -> None:
    """Save text list to filename as ASCII text"""
    with open(filename, "w") as file:
        file.writelines(f"{line}\n" for line in text)


def save_binary(text: list[str], filename: str) -> None:
    """Save binary bytes to filename after hex conversion"""
    with open(filename, "wb") as file:
        for line in text:
            file.write(bytes.fromhex(line))


def receive_anim_and_save(name: str) -> None:
    """
    Receives PNG frames and saves as individual files in a new directory.
    Contents of the directory are purged
    """

    # Eliminate the directory and its contents first
    shutil.rmtree(name, ignore_errors=True)
    # Re-create new directory
    os.makedirs(name, exist_ok=True)

    # 1 or more binary frames. We'll keep track of frame number and generate filenames
    frame = 0
    while True:
        text = receive_text(None)
        if not text:
            print(f"Something went wrong at frame: {frame}")
            print("The directory may contain a partial animation")
            break
        # If we receive file text of just "frame_rate", then we're done with the frames
        if text[0] == "frame_rate":
            frame_rate = receive_text(None)
            if not frame_rate:
                frame_rate = ["1"]  # default to 1 fps
            save_text(frame_rate, f"{name}/frame_rate")
            break
        save_binary(text, f"{name}/A_{name}_{frame}.png")
        frame += 1
    if frame > 0:
        print(f"Saved {frame} frames into directory {name}")


class Cli(cmd.Cmd):
    prompt = ">> "
    intro = (
        "Welcome to IconEdit Image Receive. Type ? or help for available commands.\n"
        "Visit https://github.com/rdefeo/iconedit for more information"
    )
    doc_header = "Available commands (type help <command> for more information)"

    def __init__(self) -> None:
        super().__init__()
        self.empty_line_count = 0

    def help_receive(self) -> None:
        print("Receives one or more files from the IconEdit app")
        print()
        print("  receive text filename")
        print("    - Use to save .C source files, whether single-frame or animation")
        print("      This won't send the filename (you will not be prompted) thus")
        print("      it is a required argument. Consider sending .C source files")
        print("      directly to your IDE.")
        print()
        print("  receive bin [filename]")
        print("    - Use to save binary image formats like PNG and BMX")
        print()
        print("  receive anim [dir name]")
        print("    - Use to save multiple PNG frames of an animation. A new directory")
        print("      named 'dir name' will be created in the CWD to store the files")
        print("      If the directory already exists, it will be purged of files!")
        print()
        print("* If a filename/dir name is not supplied, you will be prompted")
        print("  to send a name from the IconEdit app on the Flipper")
        print("* If you do supply a filename/dir name, then press Right to skip")
        print("  sending when you are prompted to send a filename in the app")

    def do_receive(self, line: str) -> None:
        args = line.split()
        filename = ""
        if len(args) > 1:
            filename = args[1]
        if not args or args[0] not in ("text", "bin", "anim"):
            self.help_receive()
            return

        if not filename:
            name_data = receive_text("Ready to receive filename!")
            filename = name_data[0] if name_data else ""
        if not filename:
            print("ERROR! No filename or dir name provided!")
            return
        if not re.fullmatch(r"[\w\-.]+", filename):
            print("ERROR! Filename contains invalid characters")
            return

        match args[0]:
            case "text":
                text = receive_text()
                if text:
                    save_text(text, filename)
            case "bin":
                text = receive_text()
                if not text:
                    print("No data received!")
                elif valid_binary_text(text):
                    save_binary(text, filename)
                else:
                    print("Malformed binary data!")
            case "anim":
                receive_anim_and_save(filename)

    def do_rt(self, line: str) -> None:
        """Shortcut for 'receive text'. See: help receive"""
        self.do_receive("text " + line)

    def do_rb(self, line: str) -> None:
        """Shortcut for 'receive bin'. See: help receive"""
        self.do_receive("bin " + line)

    def do_ra(self, line: str) -> None:
        """Shortcut for 'receive anim'. See: help receive"""
        self.do_receive("anim " + line)

    def do_quit(self, line: str) -> bool:
        """Exit"""
        return True

    def do_EOF(self, line: str) -> bool:
        """Exit on Ctrl-D"""
        print()
        return True

    def emptyline(self) -> None:
        self.empty_line_count += 1
        if self.empty_line_count == 3:
            print("Try typing 'help' to get started!")
            self.empty_line_count = 0

    def default(self, line: str) -> None:
        print(f"Unknown command: {line}. Type 'help' to list available commands")


if __name__ == "__main__":
    Cli().cmdloop()
