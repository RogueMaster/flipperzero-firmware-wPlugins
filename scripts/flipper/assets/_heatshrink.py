"""Build-time optimal token selection for the existing icon Heatshrink format."""


def encode_heatshrink_8_4(data):
    """Encode bytes with the firmware's 256-byte window and 16-byte lookahead.

    The decoder starts with a zero-filled window and permits overlapping
    back-references. A literal costs nine bits and a back-reference thirteen.
    Dynamic programming chooses the shortest stream without changing that
    format, the decoder, or the decoded pixels.
    """
    data = bytes(data)
    size = len(data)
    history = b"\x00" * 256 + data
    costs = [0] * (size + 1)
    lengths = bytearray(size)
    offsets = bytearray(size)

    for index in range(size - 1, -1, -1):
        position = index + 256
        costs[index] = 9 + costs[index + 1]
        lengths[index] = 1

        # Search in C rather than comparing every window byte in Python.
        # Extending the search past position allows legal overlapping matches;
        # the end excludes a match starting at position itself.
        for maximum in range(min(16, size - index), 1, -1):
            match = history.rfind(
                history[position : position + maximum],
                position - 256,
                position + maximum - 1,
            )
            if match >= 0:
                offset = position - match - 1
                for length in range(2, maximum + 1):
                    cost = 13 + costs[index + length]
                    if cost < costs[index]:
                        costs[index] = cost
                        lengths[index] = length
                        offsets[index] = offset
                break

    encoded = bytearray()
    accumulator = 0
    bit_count = 0
    index = 0
    while index < size:
        length = lengths[index]
        if length == 1:
            value = 0x100 | data[index]
            width = 9
        else:
            value = (offsets[index] << 4) | (length - 1)
            width = 13
        accumulator = (accumulator << width) | value
        bit_count += width
        while bit_count >= 8:
            bit_count -= 8
            encoded.append((accumulator >> bit_count) & 0xFF)
        accumulator &= (1 << bit_count) - 1
        index += length

    if bit_count:
        encoded.append(accumulator << (8 - bit_count))
    return bytes(encoded)
