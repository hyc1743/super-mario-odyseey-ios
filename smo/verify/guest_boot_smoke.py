#!/usr/bin/env python3
"""Run the IPA's embedded ARM64 program before packaging.

This is a limited startup gate, not a rendering/gameplay test. A private RomFS
fixture can be provided for startup resources. Time alone never counts
as success: nnMain, a guest thread and 60 presentation requests are required.
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
    parser.add_argument("--romfs", type=pathlib.Path, help="private startup resource fixture directory")
    parser.add_argument("--output", type=pathlib.Path, default=pathlib.Path("work/boot-smoke"))
    args = parser.parse_args()
    if platform.system() != "Darwin" or platform.machine() != "arm64":
        parser.error("actual guest execution requires an ARM64 macOS runner")
    args.output.mkdir(parents=True, exist_ok=True)
    executable = str(args.executable.resolve())
    if args.romfs and not args.romfs.is_dir():
        parser.error("RomFS fixture directory does not exist")
    env = dict(os.environ)
    with tempfile.TemporaryDirectory(prefix="smo-boot-") as temporary:
        for name in ("romfs", "save", "temp"):
            directory = pathlib.Path(temporary, name)
            directory.mkdir()
            env["SMO_" + name.upper()] = str(directory)
        if args.romfs:
            env["SMO_ROMFS"] = str(args.romfs.resolve())
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
        frames = "GUEST_FRAME_PRESENT count=60" in data
        fatal = "FATAL" in data or "【红】" in data
        passed = timed_out and entry and thread and frames and not fatal
        result = dict(passed=passed, scope="nnMain + guest thread + 60 presentation requests; no rendering validation",
                      romfs_fixture=bool(args.romfs),
                      seconds=args.seconds, timed_out=timed_out, exit_code=code,
                      entered_nnmain=entry, started_guest_thread=thread, presented_60_frames=frames, error_log=fatal,
                      host_page_size=os.sysconf("SC_PAGE_SIZE"))
        (args.output / "result.json").write_text(json.dumps(result, indent=2), encoding="utf-8")
        print(json.dumps(result, ensure_ascii=False), flush=True)
        # Host text logs contain names/addresses, never guest bytes or memory dumps.
        print("\n".join(data.splitlines()[:45] + data.splitlines()[-55:]), flush=True)
        failures = [line for line in data.splitlines() if "失败" in line or "【红】" in line or "UNIMPL" in line]
        if failures:
            print("Startup dependency/error evidence:\n" + "\n".join(failures[-60:]), flush=True)
        if not passed:
            # Interrupt a stalled replay too: an absent crash is not proof of boot.
            interrupt = ("script import threading; "
                         f"smo_timer = threading.Timer({args.seconds}, "
                         "lambda target=lldb.debugger.GetSelectedTarget(): target.GetProcess().SendAsyncInterrupt()); "
                         "smo_timer.daemon = True; smo_timer.start()")
            inspect = ["thread backtrace -c 40", "thread backtrace all -c 12", "register read pc x0 x1 x2 x8 x21 x23 x24",
                       "expression -f x -- (unsigned long long)g_mod[0].base",
                       "expression -f x -- (unsigned long long)g_mod[1].base",
                       "expression -f x -- (unsigned long long)g_mod[2].base"]
            # svcBreak exits with code 42, so LLDB's crash commands never run.
            # Stop before that exit to retain guest registers and call stacks.
            commands = ["xcrun", "lldb", "-b", "-o", interrupt,
                        "-o", "breakpoint set -n dm_guest_break_exit",
                        "-o", "run"]
            for flag in ("-o", "-k"):
                for command in inspect:
                    commands.extend([flag, command])
            commands.extend(["--", executable])
            with (args.output / "backtrace.log").open("wb") as output:
                try:
                    subprocess.run(commands, env=env, stdout=output, stderr=subprocess.STDOUT,
                                   timeout=args.seconds + 15, check=False)
                except subprocess.TimeoutExpired:
                    print("Debugger replay timed out; original failed startup remains a failure.")
            trace = (args.output / "backtrace.log").read_text(encoding="utf-8", errors="replace")
            # Keep debugger setup/condition errors visible too.
            print("\n".join(trace.splitlines()[:25]))
            start = trace.find("(lldb) thread backtrace")
            lines = trace[start:].splitlines() if start >= 0 else trace.splitlines()[-85:]
            print("\n".join(lines[:140] + (lines[-20:] if len(lines) > 140 else [])))
        return 0 if passed else 1


if __name__ == "__main__":
    raise SystemExit(main())
