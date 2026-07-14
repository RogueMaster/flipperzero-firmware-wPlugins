import struct
import os

# --- Configuration ---
CSV_FILENAME = "database.csv"
INDEX_FILENAME = "database.idx"


def create_index():
    """
    Reads the CSV file in BINARY MODE and creates a binary index for fast lookups.
    This is a more robust method that avoids text encoding or newline issues.
    """
    print(f"Opening '{CSV_FILENAME}' to generate robust index...")

    if not os.path.exists(CSV_FILENAME):
        print(f"Error: '{CSV_FILENAME}' not found.")
        return

    byte_offset = 0
    entry_count = 0

    try:
        # Open the source CSV in binary read mode ('rb')
        with open(CSV_FILENAME, "rb") as csv_file, open(
            INDEX_FILENAME, "wb"
        ) as index_file:

            # Read the file line by line, honoring the exact byte content
            for line_bytes in csv_file:
                # Decode for parsing, but use original bytes for length
                line_str = line_bytes.decode("utf-8", errors="ignore")

                first_comma_pos = line_str.find(",")
                if first_comma_pos == -1:
                    # Move to the next line's offset
                    byte_offset += len(line_bytes)
                    continue

                try:
                    node_id_str = line_str[:first_comma_pos]
                    node_id = int(node_id_str)

                    # Pack the ID and the current offset
                    packed_data = struct.pack("<II", node_id, byte_offset)
                    index_file.write(packed_data)
                    entry_count += 1

                except (ValueError, struct.error) as e:
                    print(
                        f"Warning: Could not process line at offset {byte_offset}. Content: '{line_str.strip()}'\nError: {e}"
                    )

                # IMPORTANT: Increment offset by the length of the raw bytes of the line
                byte_offset += len(line_bytes)

        index_size = os.path.getsize(INDEX_FILENAME)
        print("-" * 30)
        print("Robust index creation successful!")
        print(f"  + Indexed {entry_count} entries.")
        print(f"  + Output file: '{INDEX_FILENAME}' ({index_size} bytes)")
        print("-" * 30)

    except IOError as e:
        print(f"An error occurred: {e}")


if __name__ == "__main__":
    create_index()
