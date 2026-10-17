"""Copy verified private shader runtime files into an embedded iOS app."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import zipfile


def prepare(archive, destination):
    with zipfile.ZipFile(archive) as source:
        manifest = json.loads(source.read("manifest.json"))
        if manifest.get("version") != 1 or not manifest.get("shaders"):
            raise ValueError("incomplete shader fixture")
        files = {"manifest.json": source.read("manifest.json"), "shaders.idx": source.read("shaders.idx")}
        if not files["shaders.idx"].strip():
            raise ValueError("empty shader index")
        variants = 0
        for row in manifest["shaders"]:
            key, stage = row["key"], row["stage"]
            if not re.fullmatch(r"[0-9a-f]{16}", key) or stage not in (1, 5):
                raise ValueError("invalid shader identity")
            name = key + ".metal"
            data = source.read(name)
            if hashlib.sha256(data).hexdigest() != row["msl_sha256"]:
                raise ValueError("shader source digest mismatch")
            files[name] = data
            name = key + ".bindings.json"
            data = source.read(name)
            bindings = json.loads(data)
            if bindings.get("key") != key or bindings.get("stage") != stage or bindings.get("version") != 1:
                raise ValueError("shader binding identity mismatch")
            files[name] = data
            if stage == 1:
                name = key + ".clip.metal"
                if bindings.get("depth_clip_source") != name:
                    raise ValueError("required depth variant not bound")
                data = source.read(name)
                if hashlib.sha256(data).hexdigest() != row.get("depth_clip_sha256"):
                    raise ValueError("depth variant digest mismatch")
                files[name] = data
                variants += 1
        for line in files["shaders.idx"].decode("utf8").splitlines():
            if not line or line.startswith("#"):
                continue
            fields = line.split("\t")
            if len(fields) < 3:
                raise ValueError("invalid shader index row")
            # Preserve unimplemented stages in the exact original index. They
            # remain explicit runtime failures if requested, as in native CI.
            if int(fields[1]) not in (1, 5):
                continue
            if fields[2] not in files or not fields[2].endswith(".metal"):
                raise ValueError("shader index references unavailable source")
        # Validate every input before writing the app. Only flat runtime paths
        # selected above are copied; reflection and unrelated archive files stay out.
        destination.mkdir(parents=True, exist_ok=False)
        for name, data in files.items():
            (destination / name).write_bytes(data)
    print(f"IOS_RUNTIME_SHADERS shaders={len(manifest['shaders'])} depth_variants={variants}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("archive", type=Path)
    parser.add_argument("destination", type=Path)
    args = parser.parse_args()
    prepare(args.archive, args.destination)
