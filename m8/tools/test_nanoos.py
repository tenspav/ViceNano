#!/usr/bin/env python3
"""Reuse NanoOS's QMP boot helpers, with all outputs confined to M8."""
import os
from pathlib import Path
import subprocess
root = Path(__file__).resolve().parents[1]
nanoos = Path(os.environ.get('NANOOS', '/home/pavliukov/ipodwork/NanoRework/nanoos'))
rework = nanoos.parent
build = Path(os.environ.get('M8_BUILD', str(root / 'build-nanoos')))
# The app normally overlays Blocks.hbapp (20,708 B). A larger build goes
# into the Game Boy.hbapp slot (31,180 B, same home page, chainloader-full's
# slot) and the boot helper taps that icon instead.
# Past 31,180 B it goes into NanoBtFix.hbapp (268,848 B, same page, bottom
# right).
size = (build/'m8-nanoos.hbapp').stat().st_size
big = size > 20708
huge = size > 31180
slot = '/Apps/Executables/' + ('NanoBtFix' if huge else 'Game Boy' if big else 'Blocks') + '.hbapp'
if big:
    print('m8-nanoos.hbapp exceeds the Blocks slot; using', slot, flush=True)
slots = [slot+'='+str(build/'m8-nanoos.hbapp')]
subprocess.run(['python3', str(rework/'emulator/tools/disk_patch.py'),
                str(rework/'dumps/ipod-disk.dd'), *slots,
                '-o', str(build/'chainloader.patch')], check=True)
s = (nanoos/'test_nanoemu.py').read_text()
s = s[:s.index('for i, name in enumerate(profiles):')]
if huge:
    s = s.replace('touch(8700, 17500, ', 'touch(24600, 28000, ')
elif big:
    s = s.replace('touch(8700, 17500, ', 'touch(24600, 17500, ')
s = s.replace('ROOT = Path(__file__).resolve().parents[1]', f'ROOT = Path({str(rework)!r})')
# NanoEmu's BT gamepad (default peer) connects when mode 4 starts the BT host
# (bt.c); its default input script would turn the camera and press Start
# (= leave) mid-test: keep it neutral, the pad step below drives it by keys
s = s.replace("'emulator/run.sh',", "'emulator/run.sh', '--bt-pad', 'none',")
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
    # diagnostics are yellow; read_text only sees white ink
    title.putdata([(255,255,255) if c == (255,255,0) else c for c in title.getdata()])
    for y, text in [(8,'GPU init called: yes'),(78,'MBX available'),(184,'ViceNano M8 / NanoOS'),
                    (224,'Home or Vol- + Vol+ : back')]:
        x = 8 if y < 100 else (432-len(text)*8)//2
        assert read_text(title, x, y, len(text)) == text, (y, text)
    if cycle == 0:
        key('right')
        pause(2)
        triangle = shot('m8_mbx_triangle', landscape=True)
        assert read_text(triangle, (432-12*8)//2, 16, 12) == 'MBX triangle'
        assert triangle.getpixel((20,150)) == (13,20,64), 'GPU clear missing'  # below the diag lines
        r,g,b = triangle.getpixel((216,100))
        assert g>r and g>b and g>60, ('triangle interpolation',r,g,b)
        key('right')
        pause(2)
        quad = shot('m8_mbx_quad', landscape=True)
        assert read_text(quad,(432-16*8)//2,16,16)=='MBX texture test'
        for point,color in [((144,104),(255,0,0)),((288,104),(0,255,0)),((144,168),(0,0,255)),((288,168),(255,255,0))]:
            assert quad.getpixel(point)==color, ('textured quad texel',point,quad.getpixel(point),color)
        key('right')
        pause(2)
        world = shot('m8_mbx_world', landscape=True)
        if os.environ.get('M8_EXPECT_NO_VC'):
            # image built without vcworld.pak: modes 3 and 4 ask for it
            msg = 'no world pack: install vcworld.pak'
            assert read_text(world, (432-len(msg)*8)//2, 110, len(msg)) == msg, 'no-pack screen'
            for _ in range(3):
                key('left')
                pause(.5)
            key('h')
            pause(.5)
            q('quit')
            p.wait(timeout=10)
            log_file.close()
            print('PASS: no-pack build, world modes show the install message')
            raise SystemExit(0)
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
        # m8_gpu_diag after the world frame: every target pixel must come
        # from geometry (none left as CPU sentinel or ISP background).
        address = next(line.split()[0] for line in symbols.splitlines() if line.endswith(' m8_gpu_diag'))
        diag=q('human-monitor-command', {'command-line': 'xp /27wx 0x'+address})['return']
        d=[int(v,16) for row in diag.splitlines() if ':' in row for v in re.findall(r'0x[0-9a-fA-F]+', row.split(':',1)[1])]
        assert len(d)==27 and d[1]==1 and d[10]==1 and d[11]==1, ('diag init/TA/ISP', d)
        assert d[22]==240*432 and d[23]==0 and d[24]==0, ('diag pixel classes', d)
        print('MBX diag: '+' '.join('%08x'%v for v in d), flush=True)
        # Mode 4: the Vice City street corner from the NPAK pack appended to
        # the kernel image (vcworld.bin: North Beach shop + road, real textures)
        key('right')
        pause(3)
        vc = shot('m8_vc_world', landscape=True)
        assert read_text(vc, (432-22*8)//2, 16, 22) == 'Vice City: North Beach', 'VC pack not loaded'
        pixels=[vc.getpixel((x,y)) for y in range(35,210) for x in range(20,412)]
        assert sum(c != (13,20,64) for c in pixels)>8000, 'VC geometry missing'
        assert len(set(pixels))>64, 'VC world has too few texel colors'
        # drawCalls counts up while a frame is submitted: keep the largest of
        # several reads (one frame is ~50 ms here)
        best=None
        for attempt in range(30):
            model_status=q('human-monitor-command',{'command-line':'xp /7wx 0x'+model_address})['return']
            model_values=[int(v,16) for row in model_status.splitlines() if ':' in row for v in re.findall(r'0x[0-9a-fA-F]+',row.split(':',1)[1])]
            if len(model_values)==7 and (best is None or model_values[4]>best[4]):
                best=model_values
            pause(.05)
        model_values=best
        # 32x32 RGB565 textures: packed == resident == 2 KB each; whole 8 m
        # chunks, 148 drawn (nanoos_model.inc VC_DRAW) unless chunks within
        # VC_NEAR_M take more, never past the 320-triangle queue
        assert model_values[0]==model_values[1] and model_values[0]%2048==0 and model_values[0]>=6*2048, model_values
        assert model_values[3]>26 and 60<=model_values[4]<320, model_values
        print('VC model counters (max draws): '+' '.join('%08x'%v for v in model_values),flush=True)
        address = next(line.split()[0] for line in symbols.splitlines() if line.endswith(' m8_gpu_status'))
        status=q('human-monitor-command', {'command-line': 'xp /5wx 0x'+address})['return']
        values=[int(v,16) for row in status.splitlines() if ':' in row for v in re.findall(r'0x[0-9a-fA-F]+', row.split(':',1)[1])]
        assert len(values)==5 and values[2:4]==[0,0], values
        print('VC MBX counters: '+status, flush=True)
        # Walkable camera (nano.cpp m8_walk): HUD "WALK x 35 z 50 hdg 180 fps
        # n.n t N" (metres, N drawn); Vol+ walks along the heading, Home cycles
        # walk/turn/scene.
        def hud(im, want):
            text = read_text(im, 8, 206, 44)
            m = re.match(r'(WALK|TURN|SCENE) x (-?\d+) z (-?\d+) hdg (\d+) fps \d+\.\d', text)
            assert m and m.group(1) == want, ('HUD', text, want)
            print('HUD: '+text, flush=True)
            return [int(v) for v in m.groups()[1:]]
        assert hud(vc, 'WALK') == [35, 50, 180]
        key('right')
        pause(2)
        walk = shot('m8_vc_walk', landscape=True)
        x, z, h = hud(walk, 'WALK')
        # heading 180: forward is -z (GTA north)
        assert x == 35 and z < 50 and h == 180, ('walk forward', x, z, h)
        assert walk.tobytes() != vc.tobytes(), 'view did not move'
        key('h')
        pause(1.5)
        key('right')
        pause(2)
        turn = shot('m8_vc_turn', landscape=True)
        assert hud(turn, 'TURN')[2] > 180, 'turn right'
        key('h')
        pause(1.5)
        x0, z0, _ = hud(shot('m8_vc_scene', landscape=True), 'SCENE')
        # BT gamepad (nanoos bt.c bt_pad_poll -> nano.cpp m8_pad), connected
        # since mode 4 started: F8 = NanoEmu's keyboard drives it, W = left
        # stick up walks in any ctl (Start/PS would leave: not pressed here)
        q('human-monitor-command', {'command-line': 'sendkey f8'})
        pause(.5)
        q('human-monitor-command', {'command-line': 'sendkey w 1500'})
        pause(2)
        q('human-monitor-command', {'command-line': 'sendkey f8'})
        pause(.5)
        x1, z1, _ = hud(shot('m8_vc_pad', landscape=True), 'SCENE')
        assert (x1, z1) != (x0, z0), ('BT pad stick did not walk', x0, z0, x1, z1)
        # scene control: Volume changes the mode again, back to the title
        for _ in range(4):
            key('left')
            pause(.5)
    key('h')
    pause(.5)
    back = shot('m8_back_' + str(cycle), landscape=True)
    assert back.tobytes() == selected.tobytes(), 'Home did not return to launcher'
q('quit')
p.wait(timeout=10)
log_file.close()
print('PASS: NanoOS title, MBX clear/triangle/textured quad/model, VC street corner, Home return and re-entry')
'''
exec(compile(s, str(nanoos/'test_nanoemu.py'), 'exec'))
