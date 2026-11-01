"""Check claimed bytes, canonical control pointers, and emitted relocations."""

import argparse
import json
import struct
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument("--readelf", required=True)
parser.add_argument("--image", required=True)
parser.add_argument("--no-merge", action="store_true")
parser.add_argument("--shared", action="store_true")
args = parser.parse_args()

info = json.loads(subprocess.check_output([
    args.readelf, "--elf-output-style=JSON", "--sections", "--relocations",
    args.image], text=True))[0]
sections = {s["Section"]["Name"]["Name"]: s["Section"]
            for s in info["Sections"]}
indexed = {s["Index"]: s for s in sections.values()}
with open(args.image, "rb") as image:
    contents = image.read()
width = 8 if info["FileSummary"]["AddressSize"] == "64bit" else 4


def data(section):
    return contents[section["Offset"]:section["Offset"] + section["Size"]]


claimed = sections[".claimed"]
control = sections[".control"]
assert claimed["Size"] == control["Size"] == 4 * width, (claimed, control)
fmt = "<4Q" if width == 8 else "<4I"
assert struct.unpack(fmt, data(claimed)) == (
    0x12345678, 0x23456789, 0x12345678, 0x23456789)
values = list(struct.unpack(fmt, data(control)))
dynamic = {}
static_controls = []
for group in info["Relocations"]:
    section = indexed[group["SectionIndex"]]
    relocs = [r["Relocation"] for r in group["Relocs"]]
    if section["Info"] == claimed["Index"]:
        assert not relocs, relocs
    if section["Info"] == control["Index"]:
        static_controls.extend(relocs)
    if section["Name"]["Name"] not in (".rel.dyn", ".rela.dyn"):
        continue
    for reloc in relocs:
        address = reloc["Offset"]
        assert not claimed["Address"] <= address < (
            claimed["Address"] + claimed["Size"]), reloc
        if control["Address"] <= address < control["Address"] + control["Size"]:
            assert "RELATIVE" in reloc["Type"]["Name"], reloc
            assert address not in dynamic, reloc
            dynamic[address] = reloc
            if section["Type"]["Name"] == "SHT_RELA":
                values[(address - control["Address"]) // width] = reloc["Addend"]
assert len(static_controls) == (0 if args.shared else 4), static_controls
if not args.shared:
    assert sorted(r["Offset"] for r in static_controls) == list(range(
        control["Address"], control["Address"] + control["Size"], width))
assert len(dynamic) == (4 if args.shared else 0), dynamic

strings = sections[".merge.str"]
constants = sections[".merge.cst"]
copies = 4 if args.no_merge else 1
assert data(strings).count(b"shared\0") == copies, data(strings)
# ELD merges strings, not constant pools.
assert data(constants).count(b"\x44\x33\x22\x11") == 4, data(constants)
for index, value in enumerate(values):
    section = strings if index % 2 == 0 else constants
    expected = b"ared\0" if index % 2 == 0 else b"\x33\x22\x11"
    offset = value - section["Address"]
    assert 0 <= offset <= section["Size"] - len(expected), (value, section)
    assert data(section)[offset:offset + len(expected)] == expected, (index, value)
assert (values[0] == values[2]) == (not args.no_merge), values
assert values[1] != values[3], values
