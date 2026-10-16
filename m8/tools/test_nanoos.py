#!/usr/bin/env python3
"""Reuse NanoOS's QMP boot helpers, with all outputs confined to M8."""
import os
from pathlib import Path
import subprocess
root = Path(__file__).resolve().parents[1]
nanoos = Path(os.environ.get('NANOOS', '/home/pavliukov/ipodwork/NanoRework/nanoos'))
rework = nanoos.parent
build = Path(os.environ.get('M8_BUILD', str(root / 'build-nanoos')))
subprocess.run(['python3', str(rework/'emulator/tools/disk_patch.py'),
                str(rework/'dumps/ipod-disk.dd'),
                '/Apps/Executables/Blocks.hbapp='+str(build/'m8-nanoos.hbapp'),
                '-o', str(build/'chainloader.patch')], check=True)
s = (nanoos/'test_nanoemu.py').read_text()
s = s[:s.index('for i, name in enumerate(profiles):')]
s = s.replace('ROOT = Path(__file__).resolve().parents[1]', f'ROOT = Path({str(rework)!r})')
s = s.replace("ROOT / 'nanoos/build/screens'", f'Path({str(build / "screens")!r})')
s = s.replace("ROOT / 'nanoos/build/chainloader.patch'", f'Path({str(build / "chainloader.patch")!r})')
s = s.replace("ROOT / 'nanoos/build/emu'", f'Path({str(build / "emu")!r})')
s = s.replace("ROOT / f'nanoos/build/nanoemu-{tag}.log'", f'Path({str(build)!r}) / f"nanoemu-{{tag}}.log"')
s += '''
# The shared boot helper uses a fixed delay after tapping Blocks. Under host
# load RetailOS can still be visible then; confirm takeover before sending
# keys (Enter on StockOS would otherwise reset straight back to RetailOS).
for attempt in range(30):
    if read_text(selector, 48, 100, 5) == 'STOCK':
        break
    pause(1)
    selector = shot('04_clock_STOCK', landscape=True)
else:
    raise AssertionError('NanoOS clock selector did not appear; see ' + str(log_path))
key('ret')
pause(.5)
key('right')
pause(.5)
selected = shot('m8_selected', landscape=True)
assert read_text(selected, (432-8*8)//2, 180, 8) == 'ViceNano'
for cycle in range(2):
    key('ret')
    pause(1)
    title = shot('m8_title_' + str(cycle), landscape=True)
    for y, text in [(48,'ViceNano'),(80,'M8 / NanoOS'),(120,'Loading'),
                    (184,'Title screen port'),(208,'Home to return')]:
        assert read_text(title, (432-len(text)*8)//2, y, len(text)) == text
    if cycle == 0:
        key('right')
        pause(2)
        triangle = shot('m8_mbx_triangle', landscape=True)
        assert read_text(triangle, (432-12*8)//2, 16, 12) == 'MBX triangle'
        assert triangle.getpixel((20,100)) == (13,20,64), 'GPU clear missing'
        r,g,b = triangle.getpixel((216,100))
        assert g>r and g>b and g>60, ('triangle interpolation',r,g,b)
        key('right')
        pause(2)
        quad = shot('m8_mbx_quad', landscape=True)
        assert read_text(quad,(432-16*8)//2,16,16)=='MBX texture test'
        for point,color in [((144,88),(255,0,0)),((288,88),(0,255,0)),((144,168),(0,0,255)),((288,168),(255,255,0))]:
            assert quad.getpixel(point)==color, ('textured quad texel',point,quad.getpixel(point),color)
        key('right')
        pause(2)
        world = shot('m8_mbx_world', landscape=True)
        assert read_text(world, (432-17*8)//2, 16, 17) == 'MBX world preview'
        pixels=[world.getpixel((x,y)) for y in range(35,210) for x in range(20,412)]
        assert sum(c != (13,20,64) for c in pixels)>1000, 'world geometry missing'
        assert len(set(pixels))>32, 'textured world has too few texel colors'
        symbols = subprocess.check_output(['arm-none-eabi-nm', str(build/'m8-nanoos.elf')], text=True)
        model_address=next(line.split()[0] for line in symbols.splitlines() if line.endswith(' m8Stats'))
        # Per-frame draw counters are reset before projection/sorting. A QMP
        # read can land in that interval even when the displayed frame is valid.
        for attempt in range(50):
            model_status=q('human-monitor-command',{'command-line':'xp /7wx 0x'+model_address})['return']
            model_values=[int(v,16) for row in model_status.splitlines() if ':' in row for v in re.findall(r'0x[0-9a-fA-F]+',row.split(':',1)[1])]
            if len(model_values)==7 and model_values[4]>0:
                break
            pause(.1)
        assert model_values[1]==26*128 and model_values[3]==26 and model_values[4]>0, model_values
        print('Model counters: '+model_status,flush=True)
        address = next(line.split()[0] for line in symbols.splitlines() if line.endswith(' m8_gpu_status'))
        status=q('human-monitor-command', {'command-line': 'xp /5wx 0x'+address})['return']
        values=[int(v,16) for row in status.splitlines() if ':' in row for v in re.findall(r'0x[0-9a-fA-F]+', row.split(':',1)[1])]
        assert len(values)==5 and values[0]>1 and values[1]>128 and values[2:4]==[0,0], values
        print('MBX counters: '+status, flush=True)
        key('left')
        pause(.5)
        key('left')
        pause(.5)
        key('left')
        pause(.5)
    key('h')
    pause(.5)
    back = shot('m8_back_' + str(cycle), landscape=True)
    assert back.tobytes() == selected.tobytes(), 'Home did not return to launcher'
q('quit')
p.wait(timeout=10)
log_file.close()
print('PASS: NanoOS title, MBX clear/triangle/textured quad/model, Home return and re-entry')
'''
exec(compile(s, str(nanoos/'test_nanoemu.py'), 'exec'))
