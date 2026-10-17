#!/usr/bin/env python3
"""Run the IPA's embedded ARM64 program before packaging.

This is a limited startup gate, not a rendering/gameplay test. A private RomFS
fixture can be provided for startup resources. Time alone never counts
as success: nnMain, a guest thread, real GPU submission/completion and 60
GPU-completed offscreen frame deliveries with fresh guest draws are required.
Request counts alone cannot prove rendering. Physical iOS display delivery
is verified separately; image correctness requires inspection before packaging.
"""
import argparse
import json
import os
import pathlib
import platform
import re
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("executable", type=pathlib.Path)
    parser.add_argument("--seconds", type=int, default=30)
    parser.add_argument("--press-a-after", type=int, help="press A once after this many seconds from nnMain")
    parser.add_argument("--press-a-count", type=int, default=1, help="A presses spaced 10 seconds apart")
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
    env["SMO_TRACE_FS_MISSING"] = "1"
    env.pop("SMO_TEST_PRESS_A_AFTER", None)
    env.pop("SMO_TEST_PRESS_A_COUNT", None)
    if args.press_a_after is not None:
        if not 1 <= args.press_a_count <= 8:
            parser.error("input count must be between 1 and 8")
        if args.press_a_after < 0 or args.press_a_after + args.press_a_count*10 >= args.seconds:
            parser.error("input test needs at least 10 seconds after the A press")
        env["SMO_TEST_PRESS_A_AFTER"] = str(args.press_a_after)
        env["SMO_TEST_PRESS_A_COUNT"] = str(args.press_a_count)
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
        gpu_submit = "NVN_GPU_SUBMITTED" in data
        gpu_complete = "NVN_GPU_COMPLETED" in data
        gpu_frames = "GUEST_GPU_FRAME_COMPLETED count=60" in data
        api_gaps = "NVN_API_UNIMPLEMENTED" in data
        mapping_gap = "NVN_VIRTUAL_POOL_MAPPING_UNSUPPORTED" in data
        audio_counts = [int(n) for n in re.findall(r"AUDIO_DEVICE_PCM frames=(\d+)",data)]
        audio_ticks = [int(n) for n in re.findall(r"AUDIO_RENDERER_EVENT_TICKS count=(\d+)",data)]
        audio_mix = [int(n) for n in re.findall(r"AUDIO_RENDERER_MIX_FRAMES count=(\d+)",data)]
        audio_updates = [int(n) for n in re.findall(r"AUDIO_RENDERER_UPDATE_CALLS count=(\d+)",data)]
        audio_pcm = bool(audio_counts)
        audio_min_frames = max(20,args.seconds*30)
        audio_clock = (audio_pcm and max(audio_counts)<=args.seconds*220+200 and
                       max(audio_counts,default=0)>=audio_min_frames and
                       max(audio_mix,default=0)>=audio_min_frames and
                       max(audio_ticks,default=0)>=audio_min_frames)
        audio_gap = "AUDIO_DEVICE_SINK_UNSUPPORTED" in data
        after_input = data.rpartition("TEST_INPUT_A pressed=0")[2]
        input_progress = args.press_a_after is None or (
            data.count("TEST_INPUT_A pressed=0") == args.press_a_count and bool(
            re.search(r"GUEST_GPU_FRAME_COMPLETED count=\d+", after_input)))
        fatal = "FATAL" in data or "【红】" in data or api_gaps
        passed = timed_out and entry and thread and frames and gpu_submit and gpu_complete and gpu_frames and audio_clock and input_progress and not audio_gap and not fatal and not mapping_gap
        result = dict(passed=passed, scope="nnMain + guest thread + real GPU submit/completion + 60 fresh-draw offscreen frame deliveries; image correctness and physical iOS display require separate verification",
                      romfs_fixture=bool(args.romfs),
                      seconds=args.seconds, timed_out=timed_out, exit_code=code,
                      entered_nnmain=entry, started_guest_thread=thread, presented_60_frames=frames, error_log=fatal,
                      fence_wait_observed="NVN_FENCE_WAIT" in data,
                      gpu_submitted=gpu_submit, gpu_completed=gpu_complete, gpu_presented_60_frames=gpu_frames,
                      unimplemented_api_observed=api_gaps,
                      unsupported_mapping_observed=mapping_gap,
                      audio_device_pcm_observed=audio_pcm,
                      audio_frame_clock_bounded=audio_clock,
                      audio_max_reported_frames=max(audio_counts,default=0),
                      audio_mix_frames_observed=max(audio_mix,default=0),
                      audio_event_ticks_observed=max(audio_ticks,default=0),
                      audio_update_calls_observed=max(audio_updates,default=0),
                      unsupported_audio_sink_observed=audio_gap,
                      test_press_a_after=args.press_a_after,
                      test_press_a_count=args.press_a_count if args.press_a_after is not None else 0,
                      gpu_frames_after_input=input_progress if args.press_a_after is not None else None,
                      audio_nonzero_samples_observed=bool(re.search(r"AUDIO_DEVICE_PCM .*audible=[1-9]\d*", data)),
                      host_page_size=os.sysconf("SC_PAGE_SIZE"))
        (args.output / "result.json").write_text(json.dumps(result, indent=2), encoding="utf-8")
        print(json.dumps(result, ensure_ascii=False), flush=True)
        # Host text logs contain names/addresses, never guest bytes or memory dumps.
        print("\n".join(data.splitlines()[:45] + data.splitlines()[-55:]), flush=True)
        failures = [line for line in data.splitlines() if "失败" in line or "【红】" in line or "UNIMPL" in line or "NVN_FENCE_WAIT" in line]
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
