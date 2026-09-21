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




# Embedded PNG fixtures: four asymmetric quadrants expose UV flips.
import binascii
import math
import zlib


def png_chunk(kind, data):
    return struct.pack(">I", len(data)) + kind + data + struct.pack(
        ">I", binascii.crc32(kind + data) & 0xffffffff)


def png(width, height, rows, color_type=6, bit_depth=8):
    return (b"\x89PNG\r\n\x1a\n"
            + png_chunk(b"IHDR", struct.pack(">IIBBBBB", width, height,
                                             bit_depth, color_type, 0, 0, 0))
            + png_chunk(b"IDAT", zlib.compress(rows))
            + png_chunk(b"IEND", b""))


QUADRANTS = png(2, 2, bytes([0, 128, 0, 0, 0, 0, 128, 0, 255,
                            0, 0, 0, 128, 255, 128, 128, 128, 255]))
(ROOT / "quadrants.png").write_bytes(QUADRANTS)


def textured(image=QUADRANTS):
    binary = bytearray()
    document = {
        "asset": {"version": "2.0"}, "buffers": [{"byteLength": 0}],
        "bufferViews": [], "accessors": [],
        "images": [{"bufferView": 4, "mimeType": "image/png"}],
        "samplers": [{"minFilter": 9728, "magFilter": 9728,
                      "wrapS": 10497, "wrapT": 10497}],
        "textures": [{"source": 0, "sampler": 0}],
        "materials": [{"pbrMetallicRoughness": {
            "baseColorFactor": [1, 1, 1, 0], "baseColorTexture": {"index": 0}}}],
        "meshes": [{"primitives": [{"attributes": {
            "POSITION": 0, "NORMAL": 1, "TEXCOORD_0": 2},
            "indices": 3, "material": 0}]}],
        "nodes": [{"mesh": 0}], "scenes": [{"nodes": [0]}], "scene": 0,
    }
    normal = tuple(v / math.sqrt(1.33) for v in (0.4, 0.9, 0.6))
    for payload, component, count, kind in (
        (struct.pack("<12f", -1, 1, 0, 1, 1, 0, -1, -1, 0, 1, -1, 0), 5126, 4, "VEC3"),
        (struct.pack("<12f", *(normal * 4)), 5126, 4, "VEC3"),
        (struct.pack("<8f", 0, 0, 1, 0, 0, 1, 1, 1), 5126, 4, "VEC2"),
        (struct.pack("<6H", 0, 2, 1, 1, 2, 3), 5123, 6, "SCALAR"),
    ):
        index = len(document["bufferViews"])
        document["bufferViews"].append({"buffer": 0, "byteOffset": len(binary), "byteLength": len(payload)})
        document["accessors"].append({"bufferView": index, "componentType": component, "count": count, "type": kind})
        binary += payload
    document["accessors"][0].update(min=[-1, -1, 0], max=[1, 1, 0])
    document["bufferViews"].append({"buffer": 0, "byteOffset": len(binary), "byteLength": len(image)})
    binary += image
    document["buffers"][0]["byteLength"] = len(binary)
    return document, binary


doc, data = textured()
write("texture_quadrants", doc, data)
doc["samplers"].append({"minFilter": 9987, "magFilter": 9729,
                        "wrapS": 33071, "wrapT": 33648})
doc["textures"].append({"source": 0, "sampler": 1})
doc["materials"].append(copy.deepcopy(doc["materials"][0]))
doc["materials"][1]["pbrMetallicRoughness"]["baseColorTexture"]["index"] = 1
doc["meshes"][0]["primitives"].append(copy.deepcopy(doc["meshes"][0]["primitives"][0]))
doc["meshes"][0]["primitives"][1]["material"] = 1
write("texture_shared", doc, data)

doc, data = textured()
del doc["textures"][0]["sampler"]
write("texture_defaults", doc, data)
# High-frequency black/white requires linear-space mipmap averaging (~188).
checker = b"".join(b"\0" + b"".join(
    bytes([255 if (x+y) % 2 else 0] * 3 + [255]) for x in range(64))
    for y in range(64))
doc, data = textured(png(64, 64, checker))
doc["samplers"][0]["minFilter"] = 9987
offset = doc["bufferViews"][2]["byteOffset"]
struct.pack_into("<8f", data, offset, 0, 0, 128, 0, 0, 128, 128, 128)
write("texture_mipmap", doc, data)

texture_cases = {
    "texture_index": lambda d: d["materials"][0]["pbrMetallicRoughness"]["baseColorTexture"].update(index=99),
    "texture_source": lambda d: d["textures"][0].update(source=99),
    "texture_sampler": lambda d: d["textures"][0].update(sampler=99),
    "texture_filter": lambda d: d["samplers"][0].update(minFilter=4),
    "texture_mag": lambda d: d["samplers"][0].update(magFilter=9987),
    "texture_wrap": lambda d: d["samplers"][0].update(wrapS=5),
    "texture_uv1": lambda d: d["materials"][0]["pbrMetallicRoughness"]["baseColorTexture"].update(texCoord=1),
    "texture_uv_missing": lambda d: d["meshes"][0]["primitives"][0]["attributes"].pop("TEXCOORD_0"),
    "texture_transform": lambda d: d["materials"][0]["pbrMetallicRoughness"]["baseColorTexture"].update(extensions={"KHR_texture_transform": {"offset": [1, 0]}}),
    "texture_extension": lambda d: d["textures"][0].update(extensions={"KHR_texture_basisu": {"source": 0}}),
    "texture_jpeg": lambda d: d["images"][0].update(mimeType="image/jpeg"),
    "texture_data_uri": lambda d: d["images"][0].update(uri="data:image/png;base64,eA=="),
    "texture_view": lambda d: d["images"][0].update(bufferView=99),
    "texture_range": lambda d: d["bufferViews"][4].update(byteLength=9999999),
    "texture_stride": lambda d: d["bufferViews"][4].update(byteStride=4),
}
for name, change in texture_cases.items():
    doc, data = textured()
    change(doc)
    write("reject_" + name, doc, data)
for name, image in (
    ("png_signature", b"x" + QUADRANTS[1:]),
    ("png_crc", QUADRANTS[:-1] + bytes([QUADRANTS[-1] ^ 1])),
    ("png_truncated", QUADRANTS[:-12]),
    ("png_trailing", QUADRANTS + b"x"),
    ("png_dimension", png(4097, 1, b"\0\0\0\0\xff")),
    ("png_zero", png(0, 2, b"\0")),
    ("png_zlib", QUADRANTS[:33] + png_chunk(b"IDAT", b"broken") + png_chunk(b"IEND", b"")),
):
    doc, data = textured(image)
    write("reject_" + name, doc, data)
# Ancillary metadata CRC must also be checked even though metadata is ignored.
corrupt = bytearray(png_chunk(b"tEXt", b"key\0value"))
corrupt[-1] ^= 1
doc, data = textured(QUADRANTS[:-12] + corrupt + QUADRANTS[-12:])
write("reject_png_metadata_crc", doc, data)


for name, image in (
    ("rgb", png(1, 1, bytes([0, 128, 64, 32]), color_type=2)),
    ("gray", png(1, 1, bytes([0, 128]), color_type=0)),
    ("palette", png(1, 1, bytes([0, 0]), color_type=3)),
):
    if name == "palette":
        image = image[:33] + png_chunk(b"PLTE", bytes([128, 64, 32])) + image[33:]
    doc, data = textured(image)
    write("texture_" + name, doc, data)
# Small compressed file expands beyond the decoder allocation cap.
compressor = zlib.compressobj()
packed = b"".join(compressor.compress(bytes(1024 * 1024)) for _ in range(200))
packed += compressor.flush()
bomb = (QUADRANTS[:33] + png_chunk(b"IDAT", packed) + png_chunk(b"IEND", b""))
doc, data = textured(bomb)
write("reject_png_inflate_limit", doc, data)
