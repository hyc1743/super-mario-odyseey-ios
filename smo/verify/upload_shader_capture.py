"""Store bounded runtime shader evidence in the configured private repository.

Never attach these raw captures to public CI artifacts or print their contents.
Only hashes, sizes and the resulting private commit ID are logged.
"""
import base64
import hashlib
import io
import json
import os
from pathlib import Path
import urllib.request
import zipfile


def archive_captures(directory):
    files = sorted(directory.glob("shader-*.bin"))
    if not files:
        return None
    if len(files) > 24 or any(p.stat().st_size > 65536 for p in files):
        raise ValueError("shader capture exceeds bounded diagnostic limits")
    output = io.BytesIO()
    with zipfile.ZipFile(output, "w", zipfile.ZIP_DEFLATED) as archive:
        for path in files:
            archive.writestr(path.name, path.read_bytes())
    return output.getvalue()


def main():
    data = archive_captures(Path(os.environ.get("SMO_SHADER_CAPTURE_DIR", "/tmp/smo-private-shader-capture")))
    if data is None:
        print("No unmatched shader captures to upload")
        return
    repo = os.environ["PRIVATE_REPO"]
    run = os.environ["GITHUB_RUN_ID"]
    if not run.isdecimal() or len(repo.split("/")) != 2:
        raise ValueError("invalid private diagnostic destination")
    token = os.environ["SMO_PRIVATE_TOKEN"]
    headers = {"Authorization": "Bearer " + token, "Accept": "application/vnd.github+json",
               "Content-Type": "application/json"}

    def api(path, body=None):
        request = urllib.request.Request("https://api.github.com/repos/" + repo + path,
                                         headers=headers, data=None if body is None else json.dumps(body).encode(),
                                         method="GET" if body is None else "PUT")
        with urllib.request.urlopen(request, timeout=45) as response:
            return json.load(response)

    if api("").get("private") is not True:
        raise ValueError("raw shader captures require a private repository")
    result = api("/contents/diagnostics/shaders-" + run + ".zip", {
        "message": "Capture bounded unmatched runtime shaders for run " + run,
        "branch": "embed-input", "content": base64.b64encode(data).decode("ascii"),
    })
    print("Private shader diagnostic uploaded: bytes=" + str(len(data)) +
          " sha256=" + hashlib.sha256(data).hexdigest() + " commit=" + result["commit"]["sha"])


if __name__ == "__main__":
    main()
