#!/usr/bin/env python3
"""Add compiler-generated GL depth-range variants to a private shader fixture.

Keep the original shader source and reflection intact. Only exact matching
SPIR-V inputs are accepted; never patch guest shaders or generated source text.
"""
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
import argparse
import hashlib
import json
import subprocess
import zipfile
from shader_fixture import MSL_FLAGS, metal_arguments


def build(archive, output, roots, cross="spirv-cross", jobs=8):
    temporary = output.with_suffix(output.suffix + ".partial")
    try:
        with zipfile.ZipFile(archive) as old:
            manifest = json.loads(old.read("manifest.json"))
            if manifest["msl_flags"] != MSL_FLAGS:
                raise ValueError("compiler options differ from the base fixture")
            rows = [row for row in manifest["shaders"] if row["stage"] == 1]
            inputs = []
            for row in rows:
                key = row["key"]
                metadata = json.loads(old.read(key + ".reflection.json"))
                expected = metadata.get("spirv_sha256")
                source = next((p for root in roots for p in [root / "msl" / (key + ".spv")]
                               if p.is_file() and hashlib.sha256(p.read_bytes()).hexdigest() == expected), None)
                if source is None:
                    raise ValueError("exact original SPIR-V unavailable: " + key)
                inputs.append((row, source, metadata))

            def convert(item):
                row, source, metadata = item
                converted = subprocess.check_output([cross, str(source), *MSL_FLAGS, "--fixup-clipspace"])
                converted = converted.decode("utf-8").replace("\r\n", "\n")
                if metal_arguments(converted) != metadata["metal_arguments"]:
                    raise ValueError("depth conversion changed resource ABI: " + row["key"])
                return row, converted.encode("utf-8")

            with zipfile.ZipFile(temporary, "w", zipfile.ZIP_DEFLATED) as target:
                modified = {}
                with ThreadPoolExecutor(max_workers=jobs) as workers:
                    for count, (row, source) in enumerate(workers.map(convert, inputs), 1):
                        key = row["key"]
                        name = key + ".clip.metal"
                        row["depth_clip_sha256"] = hashlib.sha256(source).hexdigest()
                        target.writestr(name, source)
                        for suffix in (".reflection.json", ".bindings.json"):
                            data = json.loads(old.read(key + suffix))
                            data["depth_clip_source"] = name
                            modified[key + suffix] = json.dumps(data, sort_keys=True).encode()
                        if count % 1000 == 0:
                            print(f"depth_variants={count}/{len(rows)}", flush=True)
                for name in old.namelist():
                    if name == "manifest.json" or name.endswith(".clip.metal"):
                        continue
                    target.writestr(name, modified.get(name, old.read(name)))
                target.writestr("manifest.json", json.dumps(manifest, sort_keys=True))
        temporary.replace(output)
    finally:
        temporary.unlink(missing_ok=True)
    print(f"depth_variants={len(rows)} zip_bytes={output.stat().st_size} "
          f"sha256={hashlib.sha256(output.read_bytes()).hexdigest()}", flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("archive", type=Path)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--spv-root", type=Path, nargs="+", required=True)
    parser.add_argument("--spirv-cross", default="spirv-cross")
    parser.add_argument("--jobs", type=int, default=8)
    args = parser.parse_args()
    build(args.archive, args.out, args.spv_root, args.spirv_cross, args.jobs)
