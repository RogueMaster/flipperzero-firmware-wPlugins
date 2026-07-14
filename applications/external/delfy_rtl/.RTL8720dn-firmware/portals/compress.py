import sys
import os
import gzip
from pathlib import Path


def generate_header(input_file):
    input_path = Path(input_file)
    if not input_path.is_file():
        print(f"Error: '{input_file}' no es un archivo válido.")
        return

    # Nombre del archivo sin extensión
    var_name = input_path.stem
    # Nombre del archivo de salida .h
    header_file = input_path.with_suffix(".h")

    # Leer y comprimir el archivo
    with open(input_file, "rb") as f:
        data = f.read()
    compressed_data = gzip.compress(data)

    # Generar contenido en formato C
    lines = []
    lines.append(f"#define {var_name}_len {len(compressed_data)}")
    lines.append(f"const uint8_t {var_name}[] PROGMEM = {{")

    # Dividir los bytes en líneas de 16
    for i in range(0, len(compressed_data), 16):
        chunk = compressed_data[i : i + 16]
        line = ", ".join(f"0x{byte:02X}" for byte in chunk)
        lines.append(f"  {line},")

    # Quitar la coma final de la última línea
    if lines[-1].endswith(","):
        lines[-1] = lines[-1][:-1]

    lines.append("};")

    # Escribir a archivo .h
    with open(header_file, "w") as f:
        f.write("\n".join(lines))

    print(f"Archivo '{header_file}' generado correctamente.")


if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Uso: python script.py <archivo>")
        sys.exit(1)

    generate_header(sys.argv[1])
