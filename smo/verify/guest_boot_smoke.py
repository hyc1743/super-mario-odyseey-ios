#!/usr/bin/env python3
"""Run the IPA's embedded ARM64 program before packaging.

This is a limited startup gate, not a rendering/gameplay test. An empty RomFS
allows mount setup but cannot validate resource loading. Time alone never counts
as success: nnMain and at least one started guest thread must both be observed.
"""
import argparse
import json
import os
import pathlib
import platform
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("executable", type=pathlib.Path)
    parser.add_argument("--seconds", type=int, default=30)
    parser.add_argument("--output", type=pathlib.Path, default=pathlib.Path("work/boot-smoke"))
    args = parser.parse_args()
    if platform.system() != "Darwin" or platform.machine() != "arm64":
        parser.error("actual guest execution requires an ARM64 macOS runner")
    args.output.mkdir(parents=True, exist_ok=True)
    executable = str(args.executable.resolve())
    env = dict(os.environ)
    with tempfile.TemporaryDirectory(prefix="smo-boot-") as temporary:
        for name in ("romfs", "save", "temp"):
            directory = pathlib.Path(temporary, name)
            directory.mkdir()
            env["SMO_" + name.upper()] = str(directory)
        log = args.output / "guest.log"
        with log.open("wb") as output:
            process = subprocess.Popen([executable], stdout=output, stderr=subprocess.STDOUT, env=env)
            try:
                code = process.wait(timeout=args.seconds)
                timed_out = False
            except subprocess.TimeoutExpired:
                timed_out = True
                process.kill()
                process.wait()
                code = None
        data = log.read_text(encoding="utf-8", errors="replace")
        entry = "== 进入客体入口 nnMain" in data
        thread = "GUEST_THREAD_STARTED" in data
        fatal = "FATAL" in data or "【红】" in data
        passed = timed_out and entry and thread and not fatal
        result = dict(passed=passed, scope="nnMain + guest thread startup; empty RomFS; no rendering validation",
                      seconds=args.seconds, timed_out=timed_out, exit_code=code,
                      entered_nnmain=entry, started_guest_thread=thread, error_log=fatal,
                      host_page_size=os.sysconf("SC_PAGE_SIZE"))
        (args.output / "result.json").write_text(json.dumps(result, indent=2), encoding="utf-8")
        print(json.dumps(result, ensure_ascii=False), flush=True)
        # Host text logs contain names/addresses, never guest bytes or memory dumps.
        print("\n".join(data.splitlines()[-55:]), flush=True)
        if code is not None and code != 0:
            commands = ["xcrun", "lldb", "-b", "-o", "run", "-o", "thread backtrace all", "--", executable]
            with (args.output / "backtrace.log").open("wb") as output:
                try:
                    subprocess.run(commands, env=env, stdout=output, stderr=subprocess.STDOUT,
                                   timeout=args.seconds, check=False)
                except subprocess.TimeoutExpired:
                    print("Debugger replay timed out; original crash remains a failure.")
            print("\n".join((args.output / "backtrace.log").read_text(encoding="utf-8", errors="replace").splitlines()[-85:]))
        return 0 if passed else 1


if __name__ == "__main__":
    raise SystemExit(main())
