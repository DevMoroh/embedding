import sys, os, json, xml.etree.ElementTree as ET
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import pcbnew
from place import *

OUT = sys.argv[1]            # каталог проєкту
ROUTES = sys.argv[2] if len(sys.argv) > 2 else None
FPDIR = '/Applications/KiCad/KiCad.app/Contents/SharedSupport/footprints/'
OX, OY = 100.0, 100.0        # зсув плати на аркуші
mm = pcbnew.FromMM
def P(x, y): return pcbnew.VECTOR2I(mm(x + OX), mm(y + OY))

net_xml = ET.parse(os.path.join(OUT, 'net.xml')).getroot()
board = pcbnew.NewBoard(os.path.join(OUT, 'Amplifier.kicad_pcb'))

ds = board.GetDesignSettings()
ds.m_MinClearance = mm(0.2)
ds.m_TrackMinWidth = mm(0.25)
ds.m_ViasMinSize = mm(0.6)
ds.m_MinThroughDrill = mm(0.3)
ds.m_CopperEdgeClearance = mm(0.5)
nc = ds.m_NetSettings.GetDefaultNetclass()
nc.SetClearance(mm(0.25)); nc.SetTrackWidth(mm(0.3)); nc.SetViaDiameter(mm(0.8)); nc.SetViaDrill(mm(0.4))

# Контур плати із заокругленими кутами
R = 3.0
def edge_line(a, b):
    s = pcbnew.PCB_SHAPE(board); s.SetShape(pcbnew.SHAPE_T_SEGMENT)
    s.SetStart(P(*a)); s.SetEnd(P(*b)); s.SetLayer(pcbnew.Edge_Cuts); s.SetWidth(mm(0.1)); board.Add(s)
def edge_arc(c, start, angle):
    s = pcbnew.PCB_SHAPE(board); s.SetShape(pcbnew.SHAPE_T_ARC)
    s.SetCenter(P(*c)); s.SetStart(P(*start)); s.SetArcAngleAndEnd(pcbnew.EDA_ANGLE(angle, pcbnew.DEGREES_T), True)
    s.SetLayer(pcbnew.Edge_Cuts); s.SetWidth(mm(0.1)); board.Add(s)
W, H = BOARD_W, BOARD_H
edge_line((R, 0), (W - R, 0)); edge_line((W, R), (W, H - R)); edge_line((W - R, H), (R, H)); edge_line((0, H - R), (0, R))
edge_arc((W - R, R), (W - R, 0), 90); edge_arc((W - R, H - R), (W, H - R), 90)
edge_arc((R, H - R), (R, H), 90); edge_arc((R, R), (0, R), 90)

nets = {}
for n in net_xml.find('nets'):
    ni = pcbnew.NETINFO_ITEM(board, n.get('name')); board.Add(ni); nets[n.get('name')] = ni
pad_net = {}
for n in net_xml.find('nets'):
    for node in n: pad_net[(node.get('ref'), node.get('pin'))] = n.get('name')

def add_fp(libname, ref, value, x, y, rot, path=None):
    lib, name = libname.split(':')
    fp = pcbnew.FootprintLoad(FPDIR + lib + '.pretty', name)
    fp.SetReference(ref); fp.SetValue(value)
    fp.SetFPIDAsString(libname)
    board.Add(fp)
    fp.SetOrientationDegrees(rot)
    fp.SetPosition(P(x, y))
    if path: fp.SetPath(pcbnew.KIID_PATH(path))
    for pad in fp.Pads():
        key = (ref, pad.GetNumber())
        if key in pad_net: pad.SetNet(nets[pad_net[key]])
    return fp

root_uuid = None
for c in net_xml.find('components'):
    ref = c.get('ref')
    ts = c.find('tstamps').text.split()[0] if c.find('tstamps') is not None else ''
    sp = c.find('sheetpath').get('tstamps')
    x, y, rot = PLACE[ref]
    fp = add_fp(c.findtext('footprint'), ref, c.findtext('value'), x, y, rot, sp + ts)
    fp.Value().SetVisible(False)
    if ref.startswith('H'): fp.Reference().SetVisible(False)


def silk(text, x, y, size=1.0, layer=pcbnew.F_SilkS, rot=0):
    t = pcbnew.PCB_TEXT(board); t.SetText(text); t.SetPosition(P(x, y)); t.SetLayer(layer)
    t.SetTextSize(pcbnew.VECTOR2I(mm(size), mm(size))); t.SetTextThickness(mm(size * 0.15))
    t.SetTextAngleDegrees(rot); board.Add(t)

if ROUTES:
    data = json.load(open(ROUTES))
    for name, segs in data['tracks'].items():
        for (x1, y1, x2, y2, layer, w) in segs:
            t = pcbnew.PCB_TRACK(board); t.SetStart(P(x1, y1)); t.SetEnd(P(x2, y2))
            t.SetWidth(mm(w)); t.SetLayer(pcbnew.F_Cu if layer == 0 else pcbnew.B_Cu); t.SetNet(nets[name]); board.Add(t)
    for name, vias in data['vias'].items():
        for (x, y) in vias:
            v = pcbnew.PCB_VIA(board); v.SetPosition(P(x, y)); v.SetWidth(mm(0.8)); v.SetDrill(mm(0.4))
            v.SetLayerPair(pcbnew.F_Cu, pcbnew.B_Cu); v.SetNet(nets[name]); board.Add(v)

# Полігони GND на обох шарах: нижній — суцільна земля, верхній заповнює вільні місця
def gnd_zone(layer, priority):
    z = pcbnew.ZONE(board); z.SetLayer(layer); z.SetNet(nets['GND']); z.SetAssignedPriority(priority)
    z.SetLocalClearance(mm(0.3)); z.SetMinThickness(mm(0.25))
    z.SetThermalReliefGap(mm(0.5)); z.SetThermalReliefSpokeWidth(mm(0.5))
    z.SetPadConnection(pcbnew.ZONE_CONNECTION_THERMAL)
    ol = z.Outline(); ol.NewOutline()
    for (x, y) in [(0.3, 0.3), (W - 0.3, 0.3), (W - 0.3, H - 0.3), (0.3, H - 0.3)]:
        ol.Append(mm(x + OX), mm(y + OY))
    board.Add(z)
if ROUTES:
    gnd_zone(pcbnew.B_Cu, 0); gnd_zone(pcbnew.F_Cu, 0)   # заливку робить kicad-cli --refill-zones

silk('Нуль', PLACE['R15'][0] + 3.6, PLACE['R15'][1] + 2.5, rot=90)
silk('Кп', PLACE['R23'][0] + 3.85, PLACE['R23'][1] + 2.54, 0.9, rot=90)
for k, t in enumerate(('+3V3', 'OUT', 'GND')):
    silk(t, PLACE['J1'][0] - 3.3, PLACE['J1'][1] + 2.54 * k, 0.8)
silk('Bridge amplifier MCP6004', 30, H - 3.0, 1.2)
silk('Beetroot EMB 4', 30, H - 1.4, 0.9)

pcbnew.SaveBoard(os.path.join(OUT, 'Amplifier.kicad_pcb'), board)

# Геометрія виводів для маршрутизатора
pads = []
for fp in board.GetFootprints():
    for pad in fp.Pads():
        pos = pad.GetPosition(); size = pad.GetSize(pcbnew.F_Cu)
        rot = fp.GetOrientationDegrees() % 180
        sx, sy = pcbnew.ToMM(size.x), pcbnew.ToMM(size.y)
        if rot == 90: sx, sy = sy, sx
        shape = 'rect' if pad.GetShape(pcbnew.F_Cu) in (pcbnew.PAD_SHAPE_RECT, pcbnew.PAD_SHAPE_ROUNDRECT) else 'round'
        pads.append(dict(ref=fp.GetReference(), num=pad.GetNumber(), net=pad.GetNetname(),
                         x=pcbnew.ToMM(pos.x) - OX, y=pcbnew.ToMM(pos.y) - OY, sx=sx, sy=sy, shape=shape,
                         drill=pcbnew.ToMM(pad.GetDrillSize().x)))
json.dump(dict(w=BOARD_W, h=BOARD_H, pads=pads), open(os.path.join(OUT, 'pads.json'), 'w'), indent=0)
print('footprints', len(board.GetFootprints()), 'pads', len(pads))
