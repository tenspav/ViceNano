#!/usr/bin/env python3
"""M8 read-only IMG extraction, IDE/IPL join, and world-coordinate reduced packs with downscaled textures and batched draw calls."""
import argparse, hashlib, json, math, re, struct, subprocess, sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent / 'tools'))
import txdconv
ap = argparse.ArgumentParser()
ap.add_argument('--assets', type=Path, default=Path('/home/pavliukov/ipodwork/NanoRework/games/vc-files'))
a = ap.parse_args()

out = HERE / 'generated'
out.mkdir(exist_ok=True)

# Build host converter
subprocess.run(['c++', '-std=c++11', '-I' + str(HERE / '../vendor/librw'), str(HERE / 'converter.cpp'), str(HERE / '../m4/host-build/src/librw.a'), '-lm', '-o', str(HERE / 'converter')], check=True)

ide = a.assets / 'data/maps/nbeach/nbeach.ide'
ipl = a.assets / 'data/maps/nbeach/nbeach.ipl'
names = ['nbdecoshop01', 'nbdecoshop02', 'nbdecoshop03']

def records(path):
    for line in path.read_text().splitlines():
        cols = [v.strip() for v in line.split(',')]
        if len(cols) > 2 and cols[0].isdigit():
            yield cols, line

objects = {c[1]: (c, line) for c, line in records(ide) if c[1] in names}
placements = {c[1]: (c, line) for c, line in records(ipl) if c[1] in names}

raw = (a.assets / 'models/gta3.dir').read_bytes()
entries = {}
for off in range(0, len(raw), 32):
    sector, count, name = struct.unpack_from('<II24s', raw, off)
    entries[name.split(b'\0')[0].decode().lower()] = (sector, count)

report = {
    'ide_sha256': hashlib.sha256(ide.read_bytes()).hexdigest(),
    'ipl_sha256': hashlib.sha256(ipl.read_bytes()).hexdigest(),
    'buildings': []
}

headers = ['#pragma once\n#include "model.h"\n']

with (a.assets / 'models/gta3.img').open('rb') as img:
    for i, name in enumerate(names):
        obj, ide_line = objects[name]
        inst, ipl_line = placements[name]
        assert obj[0] == inst[0]
        extracted = {}
        for entry in (name + '.dff', obj[2] + '.txd'):
            sector, count = entries[entry]
            img.seek(sector * 2048)
            data = img.read(count * 2048)
            assert len(data) == count * 2048
            chunk, size, version = struct.unpack_from('<III', data)
            data = data[:12 + size]
            (out / entry).write_bytes(data)
            extracted[entry] = {'sector': sector, 'sectors': count, 'bytes': len(data), 'sha256': hashlib.sha256(data).hexdigest()}

        header = out / (name + '.h')
        subprocess.run([str(HERE / 'converter'), str(out / (name + '.dff')), str(out / (obj[2] + '.txd')), str(header)], check=True)
        text = header.read_text()
        # Share the txdconv encoder and NanTex writer. Preserve the converter's
        # sampling and 16-bit colours so only PAL8 quantization changes pixels.
        raw_textures = {
            int(n): struct.pack('<%dH' % len(values), *values)
            for n, body in re.findall(r'static const uint16_t tex(\d+)\[\]=\{([^}]+)\};', text)
            for values in [[int(v) for v in body.split(',') if v]]
        }
        packer = txdconv.NanTexPacker()
        packed_textures = {}
        def texture_entry(m):
            w, h, fmt, n = map(int, m.group(1, 2, 3, 4))
            texname = m.group(5)
            payload = txdconv.encode_pal8_16(raw_textures[n], fmt == 2)
            packed_textures[n] = payload
            target = txdconv.NANTEX_FMT_PAL8_RGBA4444 if fmt == 2 else txdconv.NANTEX_FMT_PAL8_RGB565
            packer.add_texture(texname, w, h, target, 0, [payload[512:]], payload[:512])
            return '{%d,%d,%d,tex%d,"%s",%d,0x%x},' % (w, h, fmt, n, texname, len(payload), 0x8b98 if fmt == 2 else 0x8b97)
        text = re.sub(r'\{(\d+),(\d+),([12]),tex(\d+),"([^"]+)"\},', texture_entry, text)
        text = re.sub(r'static const uint16_t tex(\d+)\[\]=\{[^}]+\};',
                      lambda m: 'alignas(4) static const uint8_t tex%s[]={%s};' %
                      (m.group(1), ','.join(map(str, packed_textures[int(m.group(1))]))), text)
        packer.write(out / (name + '.nantex'))
        txdconv.validate_nantex(out / (name + '.nantex'))
        header.write_text(text)
        raw_bytes = sum(map(len, raw_textures.values()))
        packed_bytes = sum(map(len, packed_textures.values()))
        pos = list(map(float, inst[3:6]))
        scale = list(map(float, inst[6:9]))
        qx, qy, qz, qw = map(float, inst[9:13])
        assert abs(qx*qx + qy*qy + qz*qz + qw*qw - 1) < 1e-5

        # Converter has baked DFF frame transforms and mapped x,y,z -> x,z,-y.
        def placed(m):
            vals = m.group(1).split(',')
            x, z, ny = map(lambda s: float(s.rstrip('f')), vals[:3])
            y = -ny
            x *= scale[0]
            y *= scale[1]
            z *= scale[2]
            tx = 2 * (qy * z - qz * y)
            ty = 2 * (qz * x - qx * z)
            tz = 2 * (qx * y - qy * x)
            xx = x + qw * tx + qy * tz - qz * ty + pos[0]
            yy = y + qw * ty + qz * tx - qx * tz + pos[1]
            zz = z + qw * tz + qx * ty - qy * tx + pos[2]
            return '{' + ','.join([f'{xx:.9e}f', f'{zz:.9e}f', f'{-yy:.9e}f'] + vals[3:]) + '},'

        text = re.sub(r'\{([^{}]+)\},', lambda m: placed(m) if len(m.group(1).split(',')) == 9 else m.group(0), text)
        text = text.replace('#pragma once', '').replace('#include "model.h"', '')
        headers.append('namespace block%d {\n' % i + text + '}\n')

        sizes = [list(map(int, m)) for m in re.findall(r'\{(\d+),(\d+),(-?\d+),verts', text)]
        texture_bytes = raw_bytes # Budget the driver's expanded storage, not PAL8 bytes.
        vertex_bytes = sum(v * 24 for v, t, tex in sizes)
        index_bytes = sum(t * 6 for v, t, tex in sizes)
        reservation = vertex_bytes * 8 + index_bytes * 4 + texture_bytes * 3 + len(sizes) * 4096 + 65536
        report['buildings'].append({
            'model': name,
            'id': int(obj[0]),
            'txd': obj[2],
            'ide_line': ide_line,
            'ipl_line': ipl_line,
            'position': pos,
            'cell': [math.floor(pos[0] / 100), math.floor(-pos[1] / 100)],
            'entries': extracted,
            'packed_bytes': vertex_bytes + index_bytes + packed_bytes,
            'texture_raw_bytes': raw_bytes,
            'texture_pal8_bytes': packed_bytes,
            'texture_count': len(packed_textures),
            'reservation': reservation
        })

headers.append('static const NanPack worldPacks[]={\n')
for i, b in enumerate(report['buildings']):
    x, y, z = b['position']
    bx, bz = b['cell']
    headers.append('{block%d::modelTextures,block%d::modelParts,sizeof(block%d::modelTextures)/sizeof(NanTexture),sizeof(block%d::modelParts)/sizeof(NanPart),%d,%.9ef,%.9ef,%.9ef,%d,%d},\n' % (i, i, i, i, b['reservation'], x, z, -y, bx, bz))
headers.append('};\n')

(out / 'world.h').write_text(''.join(headers))
(HERE / 'asset-provenance.json').write_text(json.dumps(report, indent=2) + '\n')
print("Successfully generated world.h and asset-provenance.json")
