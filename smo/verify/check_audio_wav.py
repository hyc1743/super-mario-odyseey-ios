#!/usr/bin/env python3
"""Validate the host audio self-test WAV without requiring a sound device."""
import argparse
import struct
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument("wav", type=Path)
args = parser.parse_args()
data = args.wav.read_bytes()
if len(data) < 44 or data[:4] != b"RIFF" or data[8:12] != b"WAVE":
    raise SystemExit("invalid WAV container")
fmt = struct.unpack_from("<IHHIIHH", data, 16)
if data[12:16] != b"fmt " or fmt != (16, 1, 2, 48000, 192000, 4, 16):
    raise SystemExit("unexpected WAV format; expected PCM16 stereo 48 kHz")
if data[36:40] != b"data":
    raise SystemExit("missing WAV data chunk")
size = struct.unpack_from("<I", data, 40)[0]
payload = data[44:]
if size != len(payload) or size < 480 * 4 or size % 4:
    raise SystemExit("invalid WAV data length")
samples = struct.unpack("<" + "h" * (len(payload) // 2), payload)
peak = max(abs(sample) for sample in samples)
if peak == 0:
    raise SystemExit("WAV contains silence")
print(f"audio self-test: {len(samples)//2} stereo frames, peak={peak}")
