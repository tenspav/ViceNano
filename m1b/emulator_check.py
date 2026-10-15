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
ap.add_argument('--reuse-copy', action='store_true', help='reuse and verify an already patched run/disk-copy.dd')
a = ap.parse_args()
root = a.nanorework.resolve()
emu = root / 'emulator'
run = HERE / 'run'
run.mkdir(exist_ok=True)
for d in ('state', 'screens'):
    (run / d).mkdir(exist_ok=True)
source = root / 'dumps/ipod-disk.dd'
disk = run / 'disk-copy.dd'
# Never follow an existing output symlink or accept an alias of the source.
for p in (disk, run):
    if p.is_symlink():
        raise SystemExit('Refusing symlink output: ' + str(p))
if disk.exists() and not a.reuse_copy:
    raise SystemExit('run/disk-copy.dd already exists; use --reuse-copy or archive run/')
source_stat = source.stat()
patch = run / 'cube.patch'
if not disk.exists():
    subprocess.run(['cp', '--reflink=auto', '--sparse=always', '--', str(source), str(disk)], check=True)
    subprocess.run(['python3', str(emu / 'tools/disk_patch.py'), str(disk),
                    '/Apps/Executables/Blocks.hbapp=' + str(HERE / 'cube.hbapp'),
                    '-o', str(patch)], check=True)
    with patch.open('rb') as f, disk.open('r+b') as out:
        assert f.read(8) == b'N7PATCH1'
        while raw := f.read(8):
            lba, = struct.unpack('<Q', raw)
            offset = (lba - 49216) * 4096
            data = f.read(4096)
            assert offset >= 0 and offset + 4096 <= source_stat.st_size and len(data) == 4096
            out.seek(offset)
            out.write(data)
    os.chmod(disk, 0o444)
# Regenerate expected patch against read-only source, also catches stale app bytes.
subprocess.run(['python3', str(emu / 'tools/disk_patch.py'), str(source),
                '/Apps/Executables/Blocks.hbapp=' + str(HERE / 'cube.hbapp'),
                '-o', str(run / 'expected.patch')], check=True)
original_blocks = []
with (run / 'expected.patch').open('rb') as f, disk.open('rb') as copied, source.open('rb') as original:
    assert f.read(8) == b'N7PATCH1'
    while raw := f.read(8):
        lba, = struct.unpack('<Q', raw)
        offset = (lba - 49216) * 4096
        data = f.read(4096)
        copied.seek(offset)
        assert copied.read(4096) == data, 'Existing disk copy does not contain this app'
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
    'nand-miss=oracle', 'nand-disk=' + str(disk), 'nand-disk-offset=49216',
    'nand-rsrc=' + str(emu / 'rsrc/rsrc-1.1.2.img1'),
    'buttons=8:home:150', 'state-dir=' + str(run / 'state'), 'screens-dir=' + str(run / 'screens'),
])
cmd = [str(emu / 'build/qemu-system-arm'), '-M', machine, '-display', 'none',
       '-serial', 'none', '-monitor', 'none', '-qmp', 'stdio']
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
        if selected != 1:
            start, end = (215, 25) if selected == 0 else (25, 215)
            pointer(True, start, 150)
            for i in range(1, 26):
                time.sleep(.02)
                pointer(True, start + (end-start) * i // 25, 150)
            pointer(False, end, 150)
            time.sleep(2)
        shot('page2')
        pointer(False, 64, 215)
        time.sleep(.05)
        pointer(True, 64, 215)
        time.sleep(.15)
        pointer(False, 64, 215)
        time.sleep(20)
        for i in range(8):
            shot(f'cube-{i}')
            time.sleep(2)
    finally:
        proc.terminate()
        try:
            proc.wait(timeout=10)
        except subprocess.TimeoutExpired:
            proc.kill()
            proc.wait()
# Check framebuffer contents, not merely that a screenshot file exists.
frames = [Image.open(run / f'cube-{i}.png').convert('RGB') for i in range(8)]
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
passed = unchanged and all(c['background_pixels'] > 50000 and c['colored_pixels'] > 1000 for c in checks) and max(changes) > 1000
result = {'pass': passed, 'screenshots': checks, 'changed_pixels': changes,
          'source_size_mtime_and_replaced_block_hashes_unchanged': unchanged,
          'app_sha256': hashlib.sha256((HERE / 'cube.hbapp').read_bytes()).hexdigest()}
(run / 'check.json').write_text(json.dumps(result, indent=2) + '\n')
print(json.dumps(result, indent=2))
raise SystemExit(0 if passed else 'Screenshot check failed; inspect run/*.png and emulator.log')
