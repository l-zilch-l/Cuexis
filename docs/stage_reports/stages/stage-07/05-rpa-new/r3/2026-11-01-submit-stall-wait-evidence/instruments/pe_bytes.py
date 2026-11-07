"""Dump bytes at RVAs of a PE image and decode the syscall number of an ntdll stub."""

import struct
import sys


def sections(path):
    data = open(path, "rb").read()
    pe = struct.unpack_from("<I", data, 0x3C)[0]
    assert data[pe:pe + 4] == b"PE\0\0", "not a PE image"
    coff = pe + 4
    count = struct.unpack_from("<H", data, coff + 2)[0]
    optional_size = struct.unpack_from("<H", data, coff + 16)[0]
    optional = coff + 20
    magic = struct.unpack_from("<H", data, optional)[0]
    assert magic == 0x20B, "not PE32+"
    image_base = struct.unpack_from("<Q", data, optional + 24)[0]
    table = optional + optional_size
    found = []
    for index in range(count):
        offset = table + index * 40
        name = data[offset:offset + 8].rstrip(b"\0").decode("ascii", "replace")
        virtual_size, virtual_address, raw_size, raw_pointer = struct.unpack_from(
            "<IIII", data, offset + 8)
        found.append({"name": name, "va": virtual_address, "vsize": virtual_size,
                      "raw": raw_pointer, "raw_size": raw_size})
    return data, image_base, found


def dump(path, rva, length=40):
    data, _base, found = sections(path)
    for section in found:
        span = max(section["vsize"], section["raw_size"])
        if section["va"] <= rva < section["va"] + span:
            offset = section["raw"] + (rva - section["va"])
            return section["name"], data[offset:offset + length]
    return None, b""


def export_rvas(path):
    """AddressOfFunctions, name-string RVAs and name ordinals of the export directory."""
    data, found = sections(path)[0::2][:2]
    pe = struct.unpack_from("<I", data, 0x3C)[0]
    optional_size = struct.unpack_from("<H", data, pe + 4 + 16)[0]
    optional = pe + 4 + 20
    export_rva = struct.unpack_from("<I", data, optional + 112)[0]
    assert export_rva, "no export directory"

    def offset_of(rva):
        for section in found:
            span = max(section["vsize"], section["raw_size"])
            if section["va"] <= rva < section["va"] + span:
                return section["raw"] + (rva - section["va"])
        return None

    directory = offset_of(export_rva)
    number_of_functions, number_of_names, functions_rva, names_rva, ordinals_rva = \
        struct.unpack_from("<IIIII", data, directory + 20)
    functions = [struct.unpack_from("<I", data, offset_of(functions_rva) + index * 4)[0]
                 for index in range(number_of_functions)]
    names = []
    ordinals_offset = offset_of(ordinals_rva)
    for index in range(number_of_names):
        position = offset_of(struct.unpack_from("<I", data, offset_of(names_rva) + index * 4)[0])
        end = data.index(b"\0", position)
        ordinal = struct.unpack_from("<H", data, ordinals_offset + index * 2)[0]
        names.append((data[position:end].decode("ascii", "replace"), ordinal))
    return data, found, functions, names


def imports(path):
    """IAT slot RVA -> "dll!symbol" for every imported function."""
    data, _base, found = sections(path)
    pe = struct.unpack_from("<I", data, 0x3C)[0]
    optional = pe + 4 + 20
    import_rva = struct.unpack_from("<I", data, optional + 120)[0]
    if not import_rva:
        return {}

    def offset_of(rva):
        for section in found:
            span = max(section["vsize"], section["raw_size"])
            if section["va"] <= rva < section["va"] + span:
                return section["raw"] + (rva - section["va"])
        return None

    position = offset_of(import_rva)
    table = {}
    while True:
        thunk, _stamp, _chain, name_rva, slots = struct.unpack_from("<IIIII", data, position)
        if name_rva == 0:
            break
        start = offset_of(name_rva)
        dll = data[start:data.index(b"\0", start)].decode("ascii", "replace")
        index = 0
        while True:
            value = struct.unpack_from("<Q", data, offset_of((thunk or slots) + index * 8))[0]
            if value == 0:
                break
            if not value >> 63:
                hint = offset_of(value)
                symbol = data[hint + 2:data.index(b"\0", hint + 2)].decode("ascii", "replace")
                table[slots + index * 8] = "{}!{}".format(dll, symbol)
            index += 1
        position += 20
    return table


def stub_map(path):
    """SSN -> exported names for every stub that starts with `mov r10, rcx; mov eax, imm32`."""
    data, found, functions, names = export_rvas(path)

    def offset_of(rva):
        for section in found:
            span = max(section["vsize"], section["raw_size"])
            if section["va"] <= rva < section["va"] + span:
                return section["raw"] + (rva - section["va"])
        return None

    stubs = {}
    for name, ordinal in names:
        position = offset_of(functions[ordinal])
        if position is None:
            continue
        if data[position:position + 4] == b"\x4c\x8b\xd1\xb8":
            number = struct.unpack_from("<I", data, position + 4)[0]
            stubs.setdefault(number, []).append(name)
    return stubs


def stub_table(path):
    """Sorted (rva, ssn, name) of every ntdll system call stub, for address to syscall lookup."""
    data, found, functions, names = export_rvas(path)

    def offset_of(rva):
        for section in found:
            span = max(section["vsize"], section["raw_size"])
            if section["va"] <= rva < section["va"] + span:
                return section["raw"] + (rva - section["va"])
        return None

    rows = {}
    for name, ordinal in names:
        position = offset_of(functions[ordinal])
        if position is None:
            continue
        if data[position:position + 4] == b"\x4c\x8b\xd1\xb8":
            number = struct.unpack_from("<I", data, position + 4)[0]
            rva = functions[ordinal]
            preferred = name[2:] if name.startswith("Zw") else name
            if rva not in rows:
                rows[rva] = [number, preferred]
    return sorted((rva, number, name) for rva, (number, name) in rows.items())


def resolve(table, rva, span=32):
    """Nearest stub at or before `rva` within one stub body, as (name, ssn, offset)."""
    low, high = 0, len(table) - 1
    best = None
    while low <= high:
        middle = (low + high) // 2
        if table[middle][0] <= rva:
            best = table[middle]
            low = middle + 1
        else:
            high = middle - 1
    if best is None:
        return None
    start, number, name = best
    offset = rva - start
    if offset >= span:
        return None
    return name, number, offset


if __name__ == "__main__":
    if sys.argv[1] == "--map":
        table = stub_map(sys.argv[2])
        for text in sys.argv[3:]:
            number = int(text)
            print("{:5d} 0x{:03x} {}".format(number, number, table.get(number)))
    else:
        path = sys.argv[1]
        for text in sys.argv[2:]:
            rva = int(text, 16)
            name, blob = dump(path, rva)
            numbers = []
            for index in range(len(blob) - 5):
                if blob[index] == 0xB8:
                    numbers.append((rva + index, struct.unpack_from("<I", blob, index + 1)[0]))
            print("{:08x} section={} bytes={}".format(rva, name, blob.hex(" ")))
            print("   mov eax sites: {}".format(
                ", ".join("+{}->{}".format(site - rva, number) for site, number in numbers)))
