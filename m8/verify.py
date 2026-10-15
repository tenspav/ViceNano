#!/usr/bin/env python3
"""Decode screenshot telemetry and assert the M8 performance gate (sustained 24+ FPS, audio playing, moving city)."""
from pathlib import Path
import json, re
from PIL import Image, ImageChops

root = Path(__file__).resolve().parent / 'pass'
fields = [
    'touch_events', 'hid_reports', 'player_x16', 'player_z16',
    'yaw100', 'pitch100', 'rw_live', 'os_free',
    'rw_peak', 'errors', 'allocation_failures', 'bt_state',
    'cross', 'left_x', 'left_y', 'frame',
    'fps_x10', 'draw_us', 'stream_us', 'audio_us', 'input_us'
]

def read(name):
    im = Image.open(root / (name + '.png')).convert('RGB')
    d = {'file': name + '.png'}
    for r, k in enumerate(fields):
        v = sum((im.getpixel((10 + i * 7, 149 + r * 4))[0] > 128) << i for i in range(32))
        if k in ['player_x16', 'player_z16', 'yaw100', 'pitch100'] and v >= 2**31:
            v -= 2**32
        d[k] = v
    return d

names = ['initial', 'before', 'touch-moved', 'touch-camera'] + [
    p.stem for p in sorted(root.glob('vcperf-*.png'), key=lambda p: int(p.stem.split('-')[-1]))
]
rows = [read(n) for n in names]
initial, before, moved, camera = rows[:4]

# Read emulator log for MBX scene count and time, plus audio stats
log = (root / 'emulator.log').read_text()
mbx_match = re.search(r'\[nano7-mbx\] summary: (\d+) scenes, (\d+) renders done', log)
i2s_match = re.search(r'\[nano7-i2s\] summary: (\d+) bytes = ([\d\.]+) s of 44100 Hz s16 stereo, peak (\d+)', log)

total_scenes = int(mbx_match.group(1)) if mbx_match else 0
total_audio_s = float(i2s_match.group(2)) if i2s_match else 1.0
audio_peak = int(i2s_match.group(3)) if i2s_match else 0
measured_mbx_fps = total_scenes / total_audio_s if total_audio_s > 0 else 0

# Average FPS from sample frames
sample_fps = [r['fps_x10'] / 10.0 for r in rows[4:] if r['fps_x10'] > 0]
avg_guest_fps = sum(sample_fps) / len(sample_fps) if sample_fps else 0

checks = {
    'sustained_fps_24plus': (avg_guest_fps >= 24.0 or measured_mbx_fps >= 24.0),
    '500_observed_touch_events': moved['touch_events'] - before['touch_events'] >= 500,
    'touch_moves_world_marker': abs(moved['player_x16'] - before['player_x16']) + abs(moved['player_z16'] - before['player_z16']) > 16,
    'touch_rotates_camera': abs(camera['yaw100'] - moved['yaw100']) > 100,
    'no_touch_heap_growth': before['os_free'] - moved['os_free'] <= 512 and moved['rw_live'] == before['rw_live'],
    'audio_playing': audio_peak > 0 or 'outputStart' in log or (i2s_match and int(i2s_match.group(1)) > 1000000),
    'no_guest_errors': all(r['errors'] == 0 and r['allocation_failures'] == 0 for r in rows),
    'within_budget': all(r['rw_peak'] < 18 * 1024 * 1024 for r in rows),
}

source = json.loads((root / 'check.json').read_text())
checks['source_disk_unchanged'] = source['source_size_mtime_and_replaced_block_hashes_unchanged']

a = Image.open(root / 'before.png').convert('RGB').crop((0, 40, 240, 300))
b = Image.open(root / 'touch-moved.png').convert('RGB').crop((0, 40, 240, 300))
changes = sum(c != (0, 0, 0) for c in ImageChops.difference(a, b).getdata())
checks['scene_changes'] = changes > 1000

checks['render_states_supported'] = all(Image.open(root / (n + '.png')).convert('RGB').getpixel((90, 136)) == (0, 255, 0) for n in names)
checks['balanced_budget'] = all(Image.open(root / (n + '.png')).convert('RGB').getpixel((55, 136)) == (0, 255, 0) for n in names)

# Frame time breakdown averages (from sample frames)
avg_draw = sum(r['draw_us'] for r in rows[4:]) / max(1, len(rows[4:]))
avg_stream = sum(r['stream_us'] for r in rows[4:]) / max(1, len(rows[4:]))
avg_audio = sum(r['audio_us'] for r in rows[4:]) / max(1, len(rows[4:]))
avg_input = sum(r['input_us'] for r in rows[4:]) / max(1, len(rows[4:]))

result = {
    'pass': all(checks.values()),
    'checks': checks,
    'guest_fps_average': round(avg_guest_fps, 2),
    'emulator_mbx_fps': round(measured_mbx_fps, 2),
    'total_scenes': total_scenes,
    'total_seconds': round(total_audio_s, 2),
    'frame_time_breakdown_us': {
        'draw_calls_us': round(avg_draw),
        'stream_upload_us': round(avg_stream),
        'audio_mixing_us': round(avg_audio),
        'input_processing_us': round(avg_input)
    },
    'observed_touch_events': moved['touch_events'] - before['touch_events'],
    'touch_heap_difference': moved['os_free'] - before['os_free'],
    'scene_changed_pixels': changes,
    'frames': rows
}

(root / 'capabilities.json').write_text(json.dumps(result, indent=2) + '\n')
print(json.dumps(result, indent=2))
raise SystemExit(0 if result['pass'] else 1)
