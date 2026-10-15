#!/usr/bin/env python3
"""Copy disk, replace Blocks on that copy, resume RetailOS and check live GL frames through QMP pipes.
Requires Python Pillow, the existing NanoEmu build and local snap6 dumps.
All generated paths are below this file's directory; original inputs open read-only.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import struct
import subprocess
import time
from PIL import Image, ImageChops

HERE = Path(__file__).resolve().parent
ap = argparse.ArgumentParser(description=__doc__)
ap.add_argument('--nanorework', type=Path, default=Path('/home/pavliukov/ipodwork/NanoRework'))
ap.add_argument('--app', type=Path, required=True)
ap.add_argument('--output', type=Path, required=True)
ap.add_argument('--prefix', default='frame')
ap.add_argument('--vnc', help='QEMU VNC display, e.g. 127.0.0.1:5')
ap.add_argument('--keep-awake', action='store_true', help='Send periodic harmless canvas touches to prevent RetailOS idle sleep')
ap.add_argument('--warmup', type=float, default=20)
ap.add_argument('--samples', type=int, default=8)
ap.add_argument('--page', type=int, default=3)
ap.add_argument('--tap-x', type=int, default=176)
ap.add_argument('--debug', action='store_true')
ap.add_argument('--dump-memory', action='store_true')
ap.add_argument('--audio-out', action='store_true', help='Capture the existing I2S sink to output/audio.wav')
a = ap.parse_args()
root = a.nanorework.resolve()
emu = root / 'emulator'
run = a.output.resolve()
app = a.app.resolve()
workspace = HERE.parent.resolve()
if not run.is_relative_to(workspace) or run.is_symlink():
    raise SystemExit('Output must be inside ViceNano')
run.mkdir(exist_ok=True)
for d in ('state', 'screens'):
    (run / d).mkdir(exist_ok=True)
source = root / 'dumps/ipod-disk.dd'
# The emulator opens nand-disk O_RDONLY; replacements and guest writes stay in memory.
# Reuse the existing Mario Kart executable slot without changing filesystem metadata.
source_stat = source.stat()
patch = run / 'app.patch'
subprocess.run(['python3', str(HERE / 'patch_app.py'), str(source), str(app), str(patch)], check=True)
original_blocks = []
with patch.open('rb') as f, source.open('rb') as original:
    assert f.read(8) == b'N7PATCH1'
    while raw := f.read(8):
        lba, = struct.unpack('<Q', raw)
        offset = (lba - 49216) * 4096
        assert len(f.read(4096)) == 4096
        original.seek(offset)
        original_blocks.append((offset, hashlib.sha256(original.read(4096)).hexdigest()))
machine = ','.join([
    'nano7', 'mmu=0x08b28000', 'torn-ignore-pc=0x093d2a80:0x093d4100', 'pcsample-ms=1000', 'bootrom=' + str(root / 'dumps/bootrom-vrom.bin'),
    'os-image=' + str(root / 'dumps/snap6/ram-full.bin'),
    'iram-image=' + str(root / 'dumps/snap6/iram.bin'),
    'ctx=' + str(emu / 'fixups/snap6-ctx.bin'),
    'overlay=' + str(root / 'dumps/snap6/overlay.bin'),
    'os-poke-file=' + str(emu / 'fixups/snap6-resume.poke'),
    'torn-map=' + str(root / 'dumps/snap6/torn-map.bin'),
    'nand-miss=oracle', 'nand-patch=' + str(patch), 'nand-disk=' + str(source), 'nand-disk-offset=49216',
    'nand-rsrc=' + str(emu / 'rsrc/rsrc-1.1.2.img1'),
    'buttons=8:home:150', 'state-dir=' + str(run / 'state'), 'screens-dir=' + str(run / 'screens'),
])
if a.audio_out: machine += ',audio-out=' + str(run / 'audio.wav')
cmd = [str(emu / 'build/qemu-system-arm'), '-M', machine, '-display', 'none',
       '-serial', 'none', '-monitor', 'none', '-qmp', 'stdio']
if a.vnc: cmd += ['-vnc', a.vnc]
if a.debug: cmd += ['-d', 'int', '-D', str(run / 'cpu-debug.log')]
(run / 'command.json').write_text(json.dumps(cmd, indent=2) + '\n')

with (run / 'emulator.log').open('w') as log, (run / 'qmp.log').open('w') as qlog:
    proc = subprocess.Popen(cmd, cwd=run, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                            stderr=log, text=True, bufsize=1,
                            env={**os.environ, 'NANO7_MBXR_STATS': '1'})
    def receive():
        while True:
            line = proc.stdout.readline()
            if not line:
                raise RuntimeError('NanoEmu exited; see emulator.log')
            qlog.write(line)
            qlog.flush()
            msg = json.loads(line)
            if 'event' not in msg:
                return msg
    def command(name, args=None):
        proc.stdin.write(json.dumps({'execute': name, 'arguments': args or {}}) + '\n')
        proc.stdin.flush()
        result = receive()
        if 'error' in result:
            raise RuntimeError(result)
        return result
    def pointer(down, x, y):
        command('input-send-event', {'events': [
            {'type': 'abs', 'data': {'axis': 'x', 'value': x * 32767 // 239}},
            {'type': 'abs', 'data': {'axis': 'y', 'value': y * 32767 // 431}},
            {'type': 'btn', 'data': {'button': 'left', 'down': down}},
        ]})
    def idle(seconds):
        remaining = seconds
        while remaining > 0:
            delay = min(5, remaining) if a.keep_awake else remaining
            time.sleep(delay)
            remaining -= delay
            if a.keep_awake:
                pointer(True, 5, 300)
                pointer(False, 5, 300)
    def shot(name):
        ppm = run / (name + '.ppm')
        command('screendump', {'filename': str(ppm)})
        with Image.open(ppm) as im:
            im.save(run / (name + '.png'))
    try:
        receive()  # QMP greeting
        command('qmp_capabilities')
        # Resume the capture app, then return Home using a full 150 ms press.
        for _ in range(3):
            time.sleep(5)
            if proc.poll() is not None:
                raise RuntimeError('NanoEmu exited during resume')
        shot('home')
        # snap6 may return to any home page. Locate the bright selected dot.
        with Image.open(run / 'home.png') as im:
            selected = max(range(3), key=lambda i: sum(im.convert('RGB').getpixel((106+14*i, 421))))
        if selected != a.page-1:
            start, end = (215, 25) if selected < a.page-1 else (25, 215)
            pointer(True, start, 150)
            for i in range(1, 26):
                time.sleep(.02)
                pointer(True, start + (end-start) * i // 25, 150)
            pointer(False, end, 150)
            time.sleep(2)
        shot('launch-page')
        pointer(False, a.tap_x, 215)
        time.sleep(.05)
        pointer(True, a.tap_x, 215)
        time.sleep(.15)
        pointer(False, a.tap_x, 215)
        idle(a.warmup)
        for i in range(a.samples):
            shot(f'{a.prefix}-{i}')
            idle(2)
        if a.dump_memory: command('pmemsave', {'val': 0x08000000, 'size': 0x04000000, 'filename': str(run / 'memory.bin')})
    finally:
        proc.terminate()
        try:
            proc.wait(timeout=10)
        except subprocess.TimeoutExpired:
            proc.kill()
            proc.wait()
# Check framebuffer contents, not merely that a screenshot file exists.
frames = [Image.open(run / f'{a.prefix}-{i}.png').convert('RGB') for i in range(a.samples)]
checks = []
for im in frames:
    pixels = list(im.getdata())
    background = sum(abs(r-13) <= 5 and abs(g-20) <= 5 and abs(b-64) <= 5 for r,g,b in pixels)
    colored = sum(max(r,g,b)-min(r,g,b) > 45 and max(r,g,b) > 90 for r,g,b in pixels)
    checks.append({'background_pixels': background, 'colored_pixels': colored})
changes = [sum(p != (0,0,0) for p in ImageChops.difference(frames[0], f).getdata()) for f in frames[1:]]
unchanged = source.stat().st_size == source_stat.st_size and source.stat().st_mtime_ns == source_stat.st_mtime_ns
with source.open('rb') as f:
    for offset, digest in original_blocks:
        f.seek(offset)
        unchanged &= hashlib.sha256(f.read(4096)).hexdigest() == digest
passed = unchanged and all(c['background_pixels'] > 30000 and c['colored_pixels'] > 1000 for c in checks) and max(changes) > 1000
result = {'pass': passed, 'screenshots': checks, 'changed_pixels': changes,
          'source_size_mtime_and_replaced_block_hashes_unchanged': unchanged,
          'app_sha256': hashlib.sha256(app.read_bytes()).hexdigest()}
(run / 'check.json').write_text(json.dumps(result, indent=2) + '\n')
print(json.dumps(result, indent=2))
raise SystemExit(0 if passed else 'Screenshot check failed; inspect run/*.png and emulator.log')
