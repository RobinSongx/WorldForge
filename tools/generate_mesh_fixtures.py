"""Generate deterministic glTF 2.0 fixtures; no external model dependencies."""
import copy
import json
from pathlib import Path
import struct
import sys

ROOT = Path(sys.argv[1])
ROOT.mkdir(parents=True, exist_ok=True)


def write(name, document, binary):
    text = json.dumps(document, separators=(",", ":")).encode()
    text += b" " * (-len(text) % 4)
    binary += b"\0" * (-len(binary) % 4)
    blob = struct.pack("<III", 0x46546C67, 2, 28 + len(text) + len(binary))
    blob += struct.pack("<II", len(text), 0x4E4F534A) + text
    blob += struct.pack("<II", len(binary), 0x004E4942) + binary
    (ROOT / (name + ".glb")).write_bytes(blob)


def fixture(index_type=5123, indexed=True, normals=True, uv_type=5126):
    # Offset 4 within a view at offset 4; stride includes deliberate padding.
    binary = bytearray(b"\0" * 8)
    positions = [(-1, 0, 0), (1, 0, 0), (0, 2, 0)]
    for point in positions:
        binary += struct.pack("<3fI", *point, 0)
    document = {
        "asset": {"version": "2.0"},
        "buffers": [{"byteLength": 0}],
        "bufferViews": [{"buffer": 0, "byteOffset": 4, "byteLength": 52, "byteStride": 16}],
        "accessors": [{"bufferView": 0, "byteOffset": 4, "componentType": 5126,
                       "count": 3, "type": "VEC3", "min": [-1, 0, 0], "max": [1, 2, 0]}],
        "materials": [{"pbrMetallicRoughness": {"baseColorFactor": [0.8, 0.45, 0.2, 1]}}],
        "meshes": [{"primitives": [{"attributes": {"POSITION": 0}, "material": 0}]}],
        "nodes": [{"name": "Parent", "children": [1], "translation": [2, 0, 0]},
                  {"name": "First instance", "mesh": 0, "translation": [0, 1, 0]},
                  {"name": "Matrix instance", "mesh": 0,
                   "matrix": [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, -2, 0, 0, 1]}],
        "scenes": [{"nodes": [0, 2]}],
        "scene": 0,
    }
    primitive = document["meshes"][0]["primitives"][0]

    def append_accessor(data, component, kind, normalized=False):
        binary.extend(b"\0" * (-len(binary) % 4))
        index = len(document["bufferViews"])
        document["bufferViews"].append({"buffer": 0, "byteOffset": len(binary), "byteLength": len(data)})
        binary.extend(data)
        document["accessors"].append({"bufferView": index, "componentType": component,
                                      "count": 3, "type": kind, "normalized": normalized})
        return len(document["accessors"]) - 1

    if normals:
        primitive["attributes"]["NORMAL"] = append_accessor(struct.pack("<9f", *([0, 0, 1] * 3)), 5126, "VEC3")
    if uv_type == 5126:
        uv = struct.pack("<6f", 0, 0, 1, 0, 0.5, 1)
    else:
        scale, code = (255, "B") if uv_type == 5121 else (65535, "H")
        uv = struct.pack("<6" + code, 0, 0, scale, 0, scale // 2, scale)
    primitive["attributes"]["TEXCOORD_0"] = append_accessor(uv, uv_type, "VEC2", uv_type != 5126)
    if indexed:
        code = {5121: "B", 5123: "H", 5125: "I"}[index_type]
        primitive["indices"] = append_accessor(struct.pack("<3" + code, 0, 1, 2), index_type, "SCALAR")
    document["buffers"][0]["byteLength"] = len(binary)
    return document, binary


for width in (5121, 5123, 5125):
    doc, data = fixture(width, uv_type={5121: 5121, 5123: 5123, 5125: 5126}[width])
    write("indices_" + str(width), doc, data)
doc, data = fixture(indexed=False, normals=False)
write("nonindexed_flat", doc, data)
doc, data = fixture()
doc["meshes"][0]["primitives"].append(copy.deepcopy(doc["meshes"][0]["primitives"][0]))
write("multiple_primitives", doc, data)

doc, data = fixture()
del doc["scene"]
write("first_scene", doc, data)

cases = {
    "cycle": lambda d: d["nodes"][1].update(children=[0]),
    "multiple_parents": lambda d: d["nodes"][2].update(children=[1]),
    "bad_child": lambda d: d["nodes"][0].update(children=[99]),
    "bad_scene": lambda d: d.update(scene=99),
    "no_scenes": lambda d: d.update(scenes=[]),
    "empty_scene": lambda d: d.update(scenes=[{"nodes": []}]),
    "singular": lambda d: d["nodes"][0].update(scale=[0, 1, 1]),
    "bad_quaternion": lambda d: d["nodes"][0].update(rotation=[0, 0, 0, 0]),
    "required_extension": lambda d: d.update(extensionsRequired=["KHR_draco_mesh_compression"]),
    "compressed": lambda d: d["meshes"][0]["primitives"][0].update(extensions={"KHR_draco_mesh_compression": {}}),
    "lines": lambda d: d["meshes"][0]["primitives"][0].update(mode=1),
    "morph": lambda d: d["meshes"][0]["primitives"][0].update(targets=[{"POSITION": 0}]),
    "animation": lambda d: d.update(animations=[{"channels": [], "samplers": []}]),
    "skin": lambda d: d.update(skins=[{"joints": [0]}]),
    "sparse": lambda d: d["accessors"][0].update(sparse={"count": 1, "indices": {"bufferView": 0, "componentType": 5121}, "values": {"bufferView": 0}}),
    "transparent": lambda d: d["materials"][0].update(alphaMode="BLEND"),
    "external_buffer": lambda d: d["buffers"][0].update(uri="external.bin"),
    "external_image": lambda d: d.update(images=[{"uri": "external.png"}]),
    "bad_offset": lambda d: d["accessors"][0].update(byteOffset=90000000),
    "bad_stride": lambda d: d["bufferViews"][0].update(byteStride=8),
    "huge_count": lambda d: d["accessors"][0].update(count=4294967295),
    "quantized": lambda d: d["accessors"][0].update(componentType=5123),
    "bad_normal_count": lambda d: d["accessors"][1].update(count=2),
    "duplicate_root": lambda d: d["scenes"][0].update(nodes=[0, 0]),
}
for name, edit in cases.items():
    doc, data = fixture()
    edit(doc)
    write("reject_" + name, doc, data)
doc, data = fixture()
offset = doc["bufferViews"][-1]["byteOffset"]
struct.pack_into("<H", data, offset, 3)
write("reject_index", doc, data)
doc, data = fixture()
struct.pack_into("<f", data, 8, float("nan"))
write("reject_nan", doc, data)
doc, data = fixture()
struct.pack_into("<3f", data, 24, -1, 0, 0)
write("reject_degenerate", doc, data)
(ROOT / "reject_truncated.glb").write_bytes(b"glTF" * 12)

doc, data = fixture()
write("container_base", doc, data)
base = (ROOT / "container_base.glb").read_bytes()
json_length = struct.unpack_from("<I", base, 12)[0]
json_end = 20 + json_length
for name, payload in (
    ("duplicate_json", base + base[12:json_end]),
    ("trailing_bytes", base + b"x"),
    ("bin_first", base[:12] + base[json_end:] + base[12:json_end]),
):
    payload = bytearray(payload)
    struct.pack_into("<I", payload, 8, len(payload))
    (ROOT / ("reject_" + name + ".glb")).write_bytes(payload)
