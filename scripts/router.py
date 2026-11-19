# Простий сітковий трасувальник для 2-шарової плати: A* з переходами між шарами,
# 8 напрямків руху, перевірка зазорів до чужих виводів, доріжок і перехідних отворів.
import json, sys, math, heapq

G = 0.3175          # крок сітки, мм
CLR = 0.25          # зазор між міддю різних кіл
EPS = 0.03          # запас на діагональні відрізки
EDGE = 0.5          # зазор до краю плати
VIA_R = 0.4         # радіус перехідного отвору (0.8 мм)
WIDTH = {'+3V3': 0.5, 'GND': 0.5}
SIG_W = 0.3
VIA_COST = 10.0
B_COST = 2.5        # нижній шар — суцільна земля, тому доріжки там дорожчі
BEND = 0.5
POUR_NETS = {'GND'}  # з'єднується полігоном, не трасується

data = json.load(open(sys.argv[1]))
BW, BH = data['w'], data['h']
NX, NY = int(BW / G) + 1, int(BH / G) + 1
HOLES = [tuple(h) for h in json.load(open(sys.argv[3]))] if len(sys.argv) > 3 else []
pads = data['pads']
net_names = sorted({p['net'] for p in pads if p['net']})
NID = {n: i for i, n in enumerate(net_names)}

def hw_of(net): return WIDTH.get(net, SIG_W) / 2
CLASSES = sorted({hw_of(n) for n in net_names} | {VIA_R})

# Карти зайнятості: для кожного класу півширини і кожного шару — власник клітинки
FREE, BLOCK = -1, -2
grids = {hw: [[FREE] * (NX * NY) for _ in range(2)] for hw in CLASSES}

def dist_item(px, py, item):
    kind = item[0]
    if kind == 'circle':
        _, cx, cy, r = item
        return math.hypot(px - cx, py - cy) - r
    if kind == 'rect':
        _, cx, cy, hx, hy = item
        ax, ay = abs(px - cx) - hx, abs(py - cy) - hy
        if ax <= 0 and ay <= 0: return max(ax, ay)
        return math.hypot(max(ax, 0), max(ay, 0))
    if kind == 'seg':
        _, x1, y1, x2, y2, r = item
        vx, vy = x2 - x1, y2 - y1; L2 = vx * vx + vy * vy
        t = 0 if L2 == 0 else max(0, min(1, ((px - x1) * vx + (py - y1) * vy) / L2))
        return math.hypot(px - x1 - t * vx, py - y1 - t * vy) - r

def item_bbox(item):
    k = item[0]
    if k == 'circle': return item[1] - item[3], item[2] - item[3], item[1] + item[3], item[2] + item[3]
    if k == 'rect': return item[1] - item[3], item[2] - item[4], item[1] + item[3], item[2] + item[4]
    _, x1, y1, x2, y2, r = item
    return min(x1, x2) - r, min(y1, y2) - r, max(x1, x2) + r, max(y1, y2) + r

def mark(item, owner, layers):
    x0, y0, x1, y1 = item_bbox(item)
    for hw in CLASSES:
        inf = CLR + hw + EPS
        i0 = max(0, int((x0 - inf) / G)); i1 = min(NX - 1, int((x1 + inf) / G) + 1)
        j0 = max(0, int((y0 - inf) / G)); j1 = min(NY - 1, int((y1 + inf) / G) + 1)
        for L in layers:
            g = grids[hw][L]
            for j in range(j0, j1 + 1):
                py = j * G; base = j * NX
                for i in range(i0, i1 + 1):
                    if dist_item(i * G, py, item) < inf:
                        v = g[base + i]
                        if v == FREE: g[base + i] = owner
                        elif v != owner: g[base + i] = BLOCK

def pad_item(p):
    if p['shape'] == 'rect': return ('rect', p['x'], p['y'], p['sx'] / 2, p['sy'] / 2)
    return ('circle', p['x'], p['y'], max(p['sx'], p['sy']) / 2)

# Край плати і кріпильні отвори
for hw in CLASSES:
    m = EDGE + hw
    for L in range(2):
        g = grids[hw][L]
        for j in range(NY):
            for i in range(NX):
                x, y = i * G, j * G
                if x < m or y < m or x > BW - m or y > BH - m: g[j * NX + i] = BLOCK
for (hx, hy) in HOLES:
    mark(('circle', hx, hy, 1.6), BLOCK, (0, 1))
for p in pads:
    mark(pad_item(p), NID[p['net']] if p['net'] else BLOCK, (0, 1))

def pad_cells(p, hw):
    it = pad_item(p); cells = []
    x0, y0, x1, y1 = item_bbox(it)
    for j in range(int(y0 / G), int(y1 / G) + 2):
        for i in range(int(x0 / G), int(x1 / G) + 2):
            if 0 <= i < NX and 0 <= j < NY and dist_item(i * G, j * G, it) <= -0.05:
                cells.append((i, j))
    return cells

DIRS = [(1, 0), (-1, 0), (0, 1), (0, -1), (1, 1), (1, -1), (-1, 1), (-1, -1)]

def astar(net, sources, targets, hw):
    nid = NID[net]; g = grids[hw]; gv = grids[VIA_R]
    tset = set(targets)
    tx = [t[1] for t in targets]; ty = [t[2] for t in targets]
    cx, cy = sum(tx) / len(tx), sum(ty) / len(ty)
    def h(i, j):
        return min(max(abs(i - a), abs(j - b)) + 0.414 * min(abs(i - a), abs(j - b)) for a, b in zip(tx, ty))
    best = {}; prev = {}; pq = []
    for s in sources:
        L, i, j = s
        if g[L][j * NX + i] in (FREE, nid):
            best[(s, -1)] = 0; heapq.heappush(pq, (h(i, j), 0, s, -1))
    while pq:
        f, c, s, d = heapq.heappop(pq)
        if best.get((s, d), 1e18) < c: continue
        if s in tset:
            path = [s]; k = (s, d)
            while k in prev:
                k = prev[k]; path.append(k[0])
            return path[::-1]
        L, i, j = s
        for di, (dx, dy) in enumerate(DIRS):
            ni, nj = i + dx, j + dy
            if not (0 <= ni < NX and 0 <= nj < NY): continue
            if g[L][nj * NX + ni] not in (FREE, nid): continue
            if dx and dy:
                # діагональ не повинна зрізати кут заблокованої клітинки
                if g[L][j * NX + ni] not in (FREE, nid) or g[L][nj * NX + i] not in (FREE, nid): continue
                step = 1.414
            else:
                step = 1.0
            if L == 1: step *= B_COST
            if d >= 0 and d != di: step += BEND
            nc = c + step; ns = (L, ni, nj)
            if nc < best.get((ns, di), 1e18):
                best[(ns, di)] = nc; prev[(ns, di)] = (s, d)
                heapq.heappush(pq, (nc + h(ni, nj), nc, ns, di))
        # перехід на інший шар
        if gv[0][j * NX + i] in (FREE, nid) and gv[1][j * NX + i] in (FREE, nid):
            ns = (1 - L, i, j); nc = c + VIA_COST
            if nc < best.get((ns, -1), 1e18):
                best[(ns, -1)] = nc; prev[(ns, -1)] = (s, d)
                heapq.heappush(pq, (nc + h(i, j), nc, ns, -1))
    return None

def route_all(order):
    # знімок початкових карт
    saved = {hw: [list(l) for l in grids[hw]] for hw in CLASSES}
    tracks, vias, failed = {}, {}, []
    for net in order:
        hw = hw_of(net); nid = NID[net]
        npads = [p for p in pads if p['net'] == net]
        # починаємо з найлівішого виводу, далі приєднуємо найближчий до вже з'єднаного дерева
        npads.sort(key=lambda p: (p['x'], p['y']))
        tree = set()
        for L in (0, 1):
            for c in pad_cells(npads[0], hw): tree.add((L,) + c)
        segs, vs = [], []
        remaining = npads[1:]
        while remaining:
            # найближчий вивід до дерева
            remaining.sort(key=lambda p: min(abs(p['x'] / G - t[1]) + abs(p['y'] / G - t[2]) for t in tree))
            p = remaining.pop(0)
            targets = [(L,) + c for L in (0, 1) for c in pad_cells(p, hw)]
            path = astar(net, list(tree), targets, hw)
            if path is None:
                failed.append(net); break
            # перетворюємо шлях у відрізки
            pts = [(c[0], c[1] * G, c[2] * G) for c in path]
            # підводимо кінці до центрів виводів
            def pad_at(pt):
                for q in npads:
                    if dist_item(pt[1], pt[2], pad_item(q)) <= 0: return q
            for end in (0, -1):
                q = pad_at(pts[end])
                if q and (abs(q['x'] - pts[end][1]) > 1e-6 or abs(q['y'] - pts[end][2]) > 1e-6):
                    stub = (pts[end][0], q['x'], q['y'])
                    if end == 0: pts.insert(0, stub)
                    else: pts.append(stub)
            for a, b in zip(pts, pts[1:]):
                if a[0] != b[0]:
                    vs.append((a[1], a[2]))
                    mark(('circle', a[1], a[2], VIA_R), nid, (0, 1))
                elif (a[1], a[2]) != (b[1], b[2]):
                    segs.append([a[1], a[2], b[1], b[2], a[0], hw * 2])
                    mark(('seg', a[1], a[2], b[1], b[2], hw), nid, (a[0],))
            for c in path: tree.add(c)
            for L in (0, 1):
                for c in pad_cells(p, hw): tree.add((L,) + c)
        tracks[net] = merge(segs); vias[net] = vs
    if failed:
        for hw in CLASSES: grids[hw] = saved[hw]
    return tracks, vias, failed

def merge(segs):
    # зливаємо послідовні колінеарні відрізки
    out = []
    for s in segs:
        if out:
            p = out[-1]
            if p[4] == s[4] and p[2] == s[0] and p[3] == s[1]:
                d1 = (p[2] - p[0], p[3] - p[1]); d2 = (s[2] - s[0], s[3] - s[1])
                if abs(d1[0] * d2[1] - d1[1] * d2[0]) < 1e-9 and d1[0] * d2[0] + d1[1] * d2[1] > 0:
                    p[2], p[3] = s[2], s[3]; continue
        out.append(list(s))
    return [[round(v, 4) if isinstance(v, float) else v for v in s] for s in out]

def span(net):
    ps = [p for p in pads if p['net'] == net]
    return (max(p['x'] for p in ps) - min(p['x'] for p in ps)) + (max(p['y'] for p in ps) - min(p['y'] for p in ps))

routable = [n for n in net_names if n not in POUR_NETS and sum(1 for p in pads if p['net'] == n) > 1]
power = [n for n in routable if n in WIDTH]
order = sorted([n for n in routable if n not in WIDTH], key=span) + sorted(power, key=span)
for attempt in range(30):
    tracks, vias, failed = route_all(order)
    if not failed: break
    print('attempt', attempt, 'failed:', failed, file=sys.stderr)
    for f in failed:
        order.remove(f); order.insert(0, f)
else:
    sys.exit('routing failed')
json.dump(dict(tracks=tracks, vias=vias), open(sys.argv[2], 'w'))
print('routed', len(order), 'nets,', sum(len(v) for v in tracks.values()), 'segments,', sum(len(v) for v in vias.values()), 'vias')
