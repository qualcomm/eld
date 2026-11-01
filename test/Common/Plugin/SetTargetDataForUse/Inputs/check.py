import json
import re
import struct
import sys


with open(sys.argv[1], "rb") as stream:
    data = stream.read()
width = len(data) // 8192
assert width in (4, 8), len(data)
assert len(data) == 8192 * width, len(data)
expected = struct.pack("<I" if width == 4 else "<Q", 0x12345678) * 8192
assert data == expected, "incorrect replacement bytes"

if len(sys.argv) > 2:
    with open(sys.argv[2]) as stream:
        text = stream.read()
    offsets = re.findall(
        r"Section: \.replacement_data\tOffset: 0x([0-9a-fA-F]+)", text
    )
    assert len(offsets) == 8192, len(offsets)
    assert {int(offset, 16) for offset in offsets} == set(range(0, len(data), width))
    assert text.count("Updated: 0x12345678") == 8192

if len(sys.argv) > 3:
    with open(sys.argv[3]) as stream:
        activity = json.load(stream)
    assert isinstance(activity["PluginActivities"], list)
