#!/usr/bin/env python3
"""Fetch pinned OFL font substitutes for the guest shared-font service.

These are not Nintendo fonts. Nintendo-specific glyphs may be missing.
Existing original fonts can instead be supplied through SMO_FONT_DIR.
"""
import argparse
import concurrent.futures
import hashlib
import json
import pathlib
import time
import urllib.request

CJK = "https://raw.githubusercontent.com/notofonts/noto-cjk/f8d157532fbfaeda587e826d4cd5b21a49186f7c/"
NOTO = "https://raw.githubusercontent.com/notofonts/noto-fonts/ffebf8c1ee449e544955a7e813c54f9b73848eac/"
FILES = {
    "standard.ttf": CJK + "Sans/OTF/Japanese/NotoSansCJKjp-Regular.otf",
    "simplified.ttf": CJK + "Sans/OTF/SimplifiedChinese/NotoSansCJKsc-Regular.otf",
    "traditional.ttf": CJK + "Sans/OTF/TraditionalChinese/NotoSansCJKtc-Regular.otf",
    "korean.ttf": CJK + "Sans/OTF/Korean/NotoSansCJKkr-Regular.otf",
    "symbols.ttf": NOTO + "hinted/ttf/NotoSansSymbols2/NotoSansSymbols2-Regular.ttf",
    "OFL-CJK.txt": CJK + "Sans/LICENSE",
    "OFL-Symbols.txt": NOTO + "LICENSE",
}
HASHES = {
    "standard.ttf": "68a3fc98800b2a27b371f2fb79991daf3633bd89309d4ffaa6946fd587f375b5",
    "simplified.ttf": "2c76254f6fc379fddfce0a7e84fb5385bb135d3e399294f6eeb6680d0365b74b",
    "traditional.ttf": "dce08bd4fd91aa8aa76ed8fea4b694c2dfb8550f67871e326843212ddbeb88b4",
    "korean.ttf": "6bcb2a0703aa137e874fc2dffa85f6c21ba9a67fa329e81b8c801663af7e992a",
    "symbols.ttf": "630846d528dbe4c4981370a4d0a9475a1fd1491a129bb411f8e157cdb5de13c6",
    "OFL-CJK.txt": "6a73f9541c2de74158c0e7cf6b0a58ef774f5a780bf191f2d7ec9cc53efe2bf2",
    "OFL-Symbols.txt": "0dab92d0544f7b233403f14b84a663bdbfa746982eda629e7f4f9ffe1b036feb",
}


def fetch(item, destination):
    name, url = item
    path = destination / name
    for attempt in range(3):
        try:
            data = path.read_bytes() if path.is_file() else b''
            if hashlib.sha256(data).hexdigest() != HASHES[name]:
                data = urllib.request.urlopen(url, timeout=120).read()
            if hashlib.sha256(data).hexdigest() != HASHES[name]:
                raise ValueError(f"{name}: SHA256 mismatch")
            if name.endswith('.ttf') and data[:4] not in (b'OTTO', b'\0\1\0\0', b'ttcf'):
                raise ValueError(f"{name}: not an SFNT font")
            path.write_bytes(data)
            return name, dict(bytes=len(data), sha256=hashlib.sha256(data).hexdigest(), source=url)
        except Exception:
            if attempt == 2:
                raise
            time.sleep(2)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("destination", type=pathlib.Path)
    args = parser.parse_args()
    args.destination.mkdir(parents=True, exist_ok=True)
    with concurrent.futures.ThreadPoolExecutor(max_workers=4) as executor:
        results = dict(executor.map(lambda item: fetch(item, args.destination), FILES.items()))
    # The open SC font also substitutes for the extended SC type. This does not
    # claim that its glyph coverage equals Nintendo's shared-font extensions.
    (args.destination / "simplified-extension.ttf").write_bytes((args.destination / "simplified.ttf").read_bytes())
    (args.destination / "font-manifest.json").write_text(json.dumps(results, indent=2), encoding="utf-8")
    print(f"Prepared OFL shared-font substitutes in {args.destination} (Nintendo glyphs not guaranteed)")


if __name__ == "__main__":
    main()
