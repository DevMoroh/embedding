# Генерує Amplifier.kicad_sch: схема зі скріну (міст + інструментальний підсилювач на MCP6004)
import sys, uuid, math, json
sys.path.insert(0, '.')
from sexp import *

LIB = '/Applications/KiCad/KiCad.app/Contents/SharedSupport/symbols/'
G = 2.54
PROJECT = 'Amplifier'
ROOT_UUID = str(uuid.uuid5(uuid.NAMESPACE_URL, 'amplifier-root'))
_n = [0]
def uid():
    _n[0] += 1
    return str(uuid.uuid5(uuid.NAMESPACE_URL, 'amp-%d' % _n[0]))

# ---------- бібліотечні символи ----------
libs = {}
def libsym(lib, name):
    if lib not in libs:
        libs[lib] = {s[1]: s for s in find(parse(open(LIB + lib + '.kicad_sym').read()), 'symbol')}
    return libs[lib][name]

def flatten(lib, name):
    s = libsym(lib, name)
    ext = find1(s, 'extends')
    if ext:
        base = libsym(lib, ext[1])
        out = [Sym('symbol'), lib + ':' + name]
        props = {p[1]: p for p in find(s, 'property')}
        for x in base[2:]:
            if isinstance(x, list) and x[0] == 'property':
                out.append(props.get(x[1], x))
            elif isinstance(x, list) and x[0] == 'symbol':
                y = list(x); y[1] = x[1].replace(ext[1], name, 1); out.append(y)
            else:
                out.append(x)
        for p in find(s, 'property'):
            if p[1] not in [x[1] for x in find(base, 'property')]:
                out.append(p)
        return out
    out = list(s); out[1] = lib + ':' + name
    return out

def pins_of(lib_id):
    lib, name = lib_id.split(':')
    s = flatten(lib, name)
    res = {}
    for u in find(s, 'symbol'):
        unit = int(u[1].split('_')[-2])
        for p in find(u, 'pin'):
            at = find1(p, 'at')
            res.setdefault(unit, {})[find1(p, 'number')[1]] = (float(at[1]), float(at[2]))
    common = res.pop(0, {})
    if not res: res[1] = {}
    for u in res.values(): u.update(common)
    return res

FOOT = {
    'R': 'Resistor_THT:R_Axial_DIN0207_L6.3mm_D2.5mm_P7.62mm_Horizontal',
    'POT': 'Potentiometer_THT:Potentiometer_Bourns_3296W_Vertical',
    'C': 'Capacitor_THT:C_Disc_D5.0mm_W2.5mm_P5.00mm',
    'CP': 'Capacitor_THT:CP_Radial_D5.0mm_P2.00mm',
    'U': 'Package_DIP:DIP-14_W7.62mm',
    'J': 'Connector_PinHeader_2.54mm:PinHeader_1x03_P2.54mm_Vertical',
    'H': 'MountingHole:MountingHole_3.2mm_M3',
}
KIND = {
    'R': ('Device:R', 'R'), 'POT': ('Device:R_Potentiometer', 'POT'),
    'C': ('Device:C', 'C'), 'CP': ('Device:C_Polarized', 'CP'),
    'U': ('Amplifier_Operational:MCP6004', 'U'), 'J': ('Connector_Generic:Conn_01x03', 'J'),
    'H': ('Mechanical:MountingHole', 'H'),
}

symbols = []   # (lib_id, ref, value, x, y, rot, unit, footprint, desc)
wires = []     # [(x1,y1),(x2,y2)] у мм
labels = []    # (name, x, y)
texts = []     # (text, x, y, size)
pin_points = []  # усі кінці пінів: (x, y)
pwr_count = [0]

def xf(x, y, rot):
    vx, vy = x, -y
    a = math.radians(rot)
    return (vx * math.cos(a) + vy * math.sin(a), -vx * math.sin(a) + vy * math.cos(a))

MIRROR = set()
def place(kind, ref, value, gx, gy, rot=0, unit=1, desc=None, mirror=False):
    lib_id, fk = KIND[kind]
    x, y = gx * G, gy * G
    symbols.append((lib_id, ref, value, x, y, rot, unit, FOOT[fk], desc))
    if mirror: MIRROR.add(len(symbols) - 1)
    pins = {}
    for num, (px, py) in pins_of(lib_id)[unit].items():
        dx, dy = xf(px, -py if mirror else py, rot)
        pins[num] = (round((x + dx) / G, 3), round((y + dy) / G, 3))
        pin_points.append((round(x + dx, 3), round(y + dy, 3)))
    return pins

def power(name, gx, gy, rot=0):
    pwr_count[0] += 1
    symbols.append(('power:' + name, '#PWR%02d' % pwr_count[0], name, gx * G, gy * G, rot, 1, '', None))
    pin_points.append((round(gx * G, 3), round(gy * G, 3)))

def flag(gx, gy):
    pwr_count[0] += 1
    symbols.append(('power:PWR_FLAG', '#FLG%02d' % pwr_count[0], 'PWR_FLAG', gx * G, gy * G, 0, 1, '', None))
    pin_points.append((round(gx * G, 3), round(gy * G, 3)))

def wire(*pts):
    for a, b in zip(pts, pts[1:]):
        wires.append([(round(a[0] * G, 3), round(a[1] * G, 3)), (round(b[0] * G, 3), round(b[1] * G, 3))])

def label(name, gx, gy):
    labels.append((name, round(gx * G, 3), round(gy * G, 3)))

def text(t, gx, gy, size=1.778):
    texts.append((t, gx * G, gy * G, size))

# ---------- схема ----------
# Міст: ліве плече R10 / (R11 + R12 + R14), праве плече R13 / (R9 + R15)
power('+3V3', 8, 14)
r10 = place('R', 'R10', '1.5k', 8, 16.5)
r11 = place('POT', 'R11', '500', 8, 21.5, desc='Імітація датчика')
r12 = place('R', 'R12', '82', 8, 25.5)
r14 = place('R', 'R14', '1.5k', 8, 29.5)
power('GND', 8, 32)
wire((8, 14), r10['1'])
wire(r10['2'], (8, 19), r11['1'])
wire(r11['2'], (r11['2'][0], r11['3'][1]), r11['3'])     # повзунок R11 замкнений на вивід 3
wire(r11['3'], r12['1'])
wire(r12['2'], r14['1'])
wire(r14['2'], (8, 32))
wire((8, 19), (11, 19)); label('a1', 11, 19)

power('+3V3', 16, 14)
r13 = place('R', 'R13', '1.5k', 16, 16.5)
r9 = place('R', 'R9', '1.5k', 16, 21.5)
r15 = place('POT', 'R15', '1k', 16, 25.5, desc='Нуль')
power('GND', 16, 28)
wire((16, 14), r13['1'])
wire(r13['2'], (16, 19), r9['1'])
wire(r9['2'], r15['1'])
wire(r15['2'], (r15['2'][0], r15['3'][1]), r15['3'])
wire(r15['3'], (16, 28))
wire((16, 19), (19, 19)); label('a2', 19, 19)
text('Нуль', 19, 27.8)

# Вхідні повторювачі з підсиленням: U1A (a1) і U1B (a2)
ua = place('U', 'U1', 'MCP6004', 34, 10, unit=1)
c1 = place('C', 'C1', '0.1u', 24, 11.5)
r17 = place('R', 'R17', '1k', 27.5, 9, rot=90)
power('GND', 24, 14)
label('a1', 22, 9)
wire((22, 9), (24, 9), r17['2'] if r17['2'][0] < r17['1'][0] else r17['1'])
r17_in, r17_out = sorted([r17['1'], r17['2']])
wire((24, 9), c1['1'])
wire(c1['2'], (24, 14))
wire(r17_out, ua['3'])

r22 = place('R', 'R22', '10k', 39, 13.5)
r25 = place('R', 'R25', '1.5k', 39, 17.5)
r23 = place('POT', 'R23', '1k', 39, 21.5, desc='Кп')
r24 = place('R', 'R24', '10k', 39, 26.5)
text('Кп', 35, 22)
wire(ua['1'], (39, 10), r22['1'])
wire(ua['2'], (30, 11), (30, 15), (39, 15))
wire(r22['2'], (39, 15), r25['1'])
wire(r25['2'], r23['1'])
wire(r23['2'], (r23['2'][0], r23['3'][1]), r23['3'])
wire(r23['3'], (39, 24), r24['1'])

ub = place('U', 'U1', 'MCP6004', 34, 30, unit=2, mirror=True)  # «−» зверху, щоб не перетинати дроти
c2 = place('C', 'C2', '0.1u', 24, 32.5)
r16 = place('R', 'R16', '1k', 27.5, 31, rot=90)
power('GND', 24, 35)
label('a2', 22, 31)
r16_in, r16_out = sorted([r16['1'], r16['2']])
wire((22, 31), (24, 31), r16_in)
wire((24, 31), c2['1'])
wire(c2['2'], (24, 35))
wire(r16_out, ub['5'])
wire((39, 24), (30, 24), (30, 29), ub['6'])
wire(ub['7'], (39, 30), r24['2'])

# Диференційний каскад U1C, Кус = R19/R18 = 10
uc = place('U', 'U1', 'MCP6004', 52, 20, unit=3)
r20 = place('R', 'R20', '1k', 45.5, 19, rot=90)
r18 = place('R', 'R18', '1k', 45.5, 21, rot=90)
r21 = place('R', 'R21', '10k', 48, 24.5)
r19 = place('R', 'R19', '10k', 51.5, 29, rot=90)
power('GND', 48, 27)
r20_in, r20_out = sorted([r20['1'], r20['2']])
r18_in, r18_out = sorted([r18['1'], r18['2']])
r19_in, r19_out = sorted([r19['1'], r19['2']])
wire((39, 10), (43, 10), (43, 19), r20_in)
label('out1', 41, 10)
wire((39, 30), (43, 30), (43, 21), r18_in)
label('out2', 41, 30)
wire(r20_out, (48, 19), uc['10'])
wire((48, 19), r21['1'])
wire(r21['2'], (48, 27))
wire(r18_out, uc['9'])
wire(r18_out, (47, 29), r19_in)
wire(uc['8'], (56, 20), (60, 20))
wire(r19_out, (56, 29), (56, 20))
label('out3', 58, 20)

# Роз'єм: 1 = +3V3, 2 = OUT3, 3 = GND
j1 = place('J', 'J1', 'Conn_01x03', 62, 20, desc='1: +3V3, 2: OUT3, 3: GND')
power('+3V3', 59, 17)
wire(j1['1'], (59, 19), (59, 17))
text('1: +3V3\n2: OUT\n3: GND', 64.5, 18.5, 1.27)
flag(57, 18); wire((59, 18), (57, 18))
power('GND', 59, 23)
wire(j1['3'], (59, 21), (59, 23))
flag(57, 22); wire((59, 22), (57, 22))

# Невикористаний U1D: повторювач із входом на GND
ud = place('U', 'U1', 'MCP6004', 68, 10, unit=4)
power('GND', 63, 10)
wire(ud['12'], (63, 9), (63, 10))
wire(ud['13'], (64, 11), (64, 13), (72, 13), (72, 10), ud['14'])

# Живлення U1 і блокувальні конденсатори
ue = place('U', 'U1', 'MCP6004', 68, 30, unit=5)
power('+3V3', 67, 26); wire((67, 26), ue['4'])
power('GND', 67, 34); wire(ue['11'], (67, 34))
c3 = place('C', 'C3', '0.1u', 71, 30.5)
power('+3V3', 71, 28); wire((71, 28), c3['1'])
power('GND', 71, 33); wire(c3['2'], (71, 33))
c4 = place('CP', 'C4', '10u', 74, 30.5)
power('+3V3', 74, 28); wire((74, 28), c4['1'])
power('GND', 74, 33); wire(c4['2'], (74, 33))

for i in range(4):
    place('H', 'H%d' % (i + 1), 'MountingHole', 64 + 4 * i, 40)

text('Мостовий підсилювач: міст R9-R15, інструментальний підсилювач U1A/U1B, диференційний каскад U1C', 8, 6, 2.0)
text('Vout = (a1 - a2) * (1 + 2*10k / (R25 + R23)) * 10', 30, 38)

# ---------- розбиваємо дроти в точках дотику та ставимо вузли ----------
def on_seg(p, a, b):
    if p == a or p == b: return False
    if a[0] == b[0] == p[0]: return min(a[1], b[1]) < p[1] < max(a[1], b[1])
    if a[1] == b[1] == p[1]: return min(a[0], b[0]) < p[0] < max(a[0], b[0])
    return False

wires = [w for w in wires if w[0] != w[1]]
for w in wires:
    assert w[0][0] == w[1][0] or w[0][1] == w[1][1], w
special = set(pin_points) | {p for w in wires for p in w} | {(l[1], l[2]) for l in labels}
changed = True
while changed:
    changed = False
    for i, w in enumerate(wires):
        for p in special:
            if on_seg(p, w[0], w[1]):
                wires[i:i + 1] = [[w[0], p], [p, w[1]]]
                changed = True
                break
        if changed: break
count = {}
for w in wires:
    for p in w: count[p] = count.get(p, 0) + 1
for p in pin_points: count[p] = count.get(p, 0) + 1
junctions = [p for p, c in count.items() if c >= 3]

# ---------- запис ----------
def font(size=1.27):
    return [Sym('effects'), [Sym('font'), [Sym('size'), size, size]]]

def prop(name, val, x, y, hide=False, rot=0):
    p = [Sym('property'), name, val, [Sym('at'), x, y, rot]]
    if hide: p.append([Sym('hide'), Sym('yes')])
    p.append(font())
    return p

BELOW = {'R18', 'R19'}
lib_needed = sorted({s[0] for s in symbols})
doc = [Sym('kicad_sch'), [Sym('version'), 20260306], [Sym('generator'), 'eeschema'],
       [Sym('generator_version'), '10.0'], [Sym('uuid'), ROOT_UUID], [Sym('paper'), 'A4'],
       [Sym('title_block'), [Sym('title'), 'Bridge amplifier'], [Sym('company'), 'Beetroot EMB 4']],
       [Sym('lib_symbols')] + [flatten(*l.split(':')) for l in lib_needed]]
for p in junctions:
    doc.append([Sym('junction'), [Sym('at'), p[0], p[1]], [Sym('diameter'), 0], [Sym('color'), 0, 0, 0, 0], [Sym('uuid'), uid()]])
for a, b in wires:
    doc.append([Sym('wire'), [Sym('pts'), [Sym('xy'), a[0], a[1]], [Sym('xy'), b[0], b[1]]],
                [Sym('stroke'), [Sym('width'), 0], [Sym('type'), Sym('default')]], [Sym('uuid'), uid()]])
for name, x, y in labels:
    doc.append([Sym('label'), name, [Sym('at'), x, y, 0],
                [Sym('effects'), [Sym('font'), [Sym('size'), 1.27, 1.27]], [Sym('justify'), Sym('left'), Sym('bottom')]],
                [Sym('uuid'), uid()]])
for t, x, y, size in texts:
    doc.append([Sym('text'), t, [Sym('exclude_from_sim'), Sym('no')], [Sym('at'), x, y, 0],
                [Sym('effects'), [Sym('font'), [Sym('size'), size, size]], [Sym('justify'), Sym('left'), Sym('bottom')]],
                [Sym('uuid'), uid()]])
for idx, (lib_id, ref, value, x, y, rot, unit, fp, desc) in enumerate(symbols):
    is_pwr = ref.startswith('#')
    no_bom = is_pwr or ref.startswith('H')
    s = [Sym('symbol'), [Sym('lib_id'), lib_id], [Sym('at'), x, y, rot], [Sym('unit'), unit],
         ] + ([[Sym('mirror'), Sym('x')]] if idx in MIRROR else []) + [[Sym('exclude_from_sim'), Sym('no')], [Sym('in_bom'), Sym('no' if no_bom else 'yes')],
         [Sym('on_board'), Sym('no' if is_pwr else 'yes')], [Sym('dnp'), Sym('no')], [Sym('uuid'), uid()]]
    if is_pwr:
        flagged = lib_id.endswith('PWR_FLAG')
        dy = (-3.81 if lib_id.endswith('+3V3') or flagged else 5.08)
        s.append(prop('Reference', ref, x, y + 6.35, hide=True))
        s.append(prop('Value', value, x, y + dy, hide=flagged))
    elif lib_id.startswith('Amplifier'):
        s.append(prop('Reference', ref, x + 2.54, y - 5.08))
        s.append(prop('Value', value, x + 2.54, y + 5.08, hide=unit != 1))
    elif lib_id.startswith('Mechanical'):
        s.append(prop('Reference', ref, x + 2.54, y - 1.27))
        s.append(prop('Value', value, x + 2.54, y + 1.27, hide=True))
    elif lib_id.startswith('Connector'):
        s.append(prop('Reference', ref, x, y - 5.08))
        s.append(prop('Value', value, x, y + 5.08))
    elif rot == 90:
        dy = 2.54 if ref in BELOW else -2.54
        s.append(prop('Reference', ref, x - 2.54, dy + y, rot=90))
        s.append(prop('Value', value, x + 2.54, dy + y, rot=90))
    else:
        off = 6.35 if lib_id.endswith('Potentiometer') or lib_id.endswith('Polarized') else 3.175
        s.append(prop('Reference', ref, x + off, y - 1.27))
        s.append(prop('Value', value, x + off, y + 1.27))
    s.append(prop('Footprint', fp, x, y, hide=True))
    s.append(prop('Datasheet', '', x, y, hide=True))
    lib, name = lib_id.split(':')
    for num in sorted({n for u in pins_of(lib_id).values() for n in u}, key=lambda n: (len(n), n)):
        if num in pins_of(lib_id)[unit]:
            s.append([Sym('pin'), num, [Sym('uuid'), uid()]])
    s.append([Sym('instances'), [Sym('project'), PROJECT, [Sym('path'), '/' + ROOT_UUID,
              [Sym('reference'), ref], [Sym('unit'), unit]]]])
    doc.append(s)
doc.append([Sym('sheet_instances'), [Sym('path'), '/', [Sym('page'), '1']]])
doc.append([Sym('embedded_fonts'), Sym('no')])
open(sys.argv[1], 'w').write(dump(doc) + '\n')
print('symbols', len(symbols), 'wires', len(wires), 'junctions', len(junctions))
