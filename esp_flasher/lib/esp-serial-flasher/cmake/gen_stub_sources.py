import base64
import json
import os
import sys
import urllib.request
from datetime import datetime

"""
This python module generates esp_stubs.c/h files from a given version and url, with
<stub_download_url>/v<stub_version>/esp32xx.json as the required format.

It is also possible to override stub generation to use a local folder for testing purposes.
"""

# Paths
stub_version = sys.argv[1]
stub_download_url = sys.argv[2]
root_path = sys.argv[3]
stub_override_path = sys.argv[4] if len(sys.argv) == 5 else None

template_path = os.path.join(root_path, "cmake")
priv_inc_path = os.path.join(root_path, "private_include")
src_path = os.path.join(root_path, "src")
h_template_path = os.path.join(template_path, "esp_stubs.h.template")
c_template_path = os.path.join(template_path, "esp_stubs.c.template")
hfile_path = os.path.join(priv_inc_path, "esp_stubs.h")
cfile_path = os.path.join(src_path, "esp_stubs.c")
current_year = datetime.now().year

# Matches order of target_chip_t enumeration
files_to_download = [
    None,  # ESP8266_CHIP
    "esp32.json",  # ESP32_CHIP
    "esp32s2.json",  # ESP32S2_CHIP
    "esp32c3.json",  # ESP32C3_CHIP
    "esp32s3.json",  # ESP32S3_CHIP
    "esp32c2.json",  # ESP32C2_CHIP
    None,  # ESP32C5_CHIP (stub not included for now)
    "esp32h2.json",  # ESP32H2_CHIP
    "esp32c6.json",  # ESP32C6_CHIP
    None,  # ESP32P4_CHIP (stub not included for now)
]


def read_stub_json(json_file):
    stub = json.load(json_file)
    entry = stub["entry"]
    text = base64.b64decode(stub["text"])
    text_start = stub["text_start"]
    try:
        data = base64.b64decode(stub["data"])
        data_start = stub["data_start"]
    except KeyError:
        data = None
        data_start = None

    text_str = ", ".join([hex(b) for b in text])
    text_size = len(text)
    data_str = "" if data is None else ", ".join([hex(b) for b in data])
    data_size = 0 if data is None else len(data)
    data_start = 0 if data_start is None else data_start

    stub_data = f"""    // {file_to_download}
    {{
        .header = {{
            .entrypoint = {entry},
        }},
        .segments = {{
            {{
                .addr = {text_start},
                .size = {text_size},
                .data = (uint8_t[]){{{text_str}}},
            }},
            {{
                .addr = {data_start},
                .size = {data_size},
                .data = (uint8_t[]){{{data_str}}},
            }},
        }},
    }},

"""
    return stub_data


if __name__ == "__main__":
    # .h and .c file templates
    with (
        open(h_template_path, "r") as h_template_file,
        open(c_template_path, "r") as c_template_file,
    ):
        h_template = h_template_file.read()
        c_template = c_template_file.read()

    # .h and .c file to generate
    with open(hfile_path, "w") as hfile, open(cfile_path, "w") as cfile:
        hfile.write(
            h_template.format(
                current_year=current_year,
                stub_version=stub_version,
            )
        )

        cfile.write(
            c_template.format(
                current_year=current_year,
                stub_version=stub_version,
                max_chip_number=len(files_to_download),
            )
        )

        for file_to_download in files_to_download:
            if file_to_download is None:
                cfile.write("    // placeholder\n" "    {},\n" "\n")
            else:
                if stub_override_path:
                    with open(f"{stub_override_path}/{file_to_download}") as file_path:
                        cfile.write(read_stub_json(file_path))
                else:
                    with urllib.request.urlopen(
                        f"{stub_download_url}/v{stub_version}/{file_to_download}"
                    ) as url:
                        cfile.write(read_stub_json(url))

        cfile.write("};\n" "\n" "#endif\n")
