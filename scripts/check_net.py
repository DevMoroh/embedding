import xml.etree.ElementTree as ET, sys
r = ET.parse(sys.argv[1]).getroot()
got = {}
for n in r.find('nets'):
    got[frozenset(f"{x.get('ref')}.{x.get('pin')}" for x in n)] = n.get('name')
want = {
 '+3V3': 'R10.1 R13.1 U1.4 C3.1 C4.1 J1.1',
 'GND':  'R14.2 R15.2 R15.3 C1.2 C2.2 R21.2 J1.3 U1.11 C3.2 C4.2 U1.12',
 'a1':   'R10.2 R11.1 C1.1 R17.?',
 'a2':   'R13.2 R9.1 C2.1 R16.?',
 'sens': 'R11.2 R11.3 R12.1',
 'r12':  'R12.2 R14.1',
 'r9':   'R9.2 R15.1',
 'p_a':  'R17.? U1.3',
 'p_b':  'R16.? U1.5',
 'n_a':  'U1.2 R22.2 R25.1',
 'gain': 'R25.2 R23.1',
 'kp':   'R23.2 R23.3 R24.1 U1.6',
 'out1': 'U1.1 R22.1 R20.?',
 'out2': 'U1.7 R24.2 R18.?',
 'p_c':  'R20.? R21.1 U1.10',
 'n_c':  'R18.? U1.9 R19.?',
 'out3': 'U1.8 R19.? J1.2',
 'n_d':  'U1.13 U1.14',
}
# Розвʼязуємо '?' (неважливий номер виводу резистора) за фактичним netlist
ok = True
gotsets = list(got)
for name, spec in want.items():
    items = spec.split()
    match = [g for g in gotsets if len(g) == len(items) and all(
        (i in g) if not i.endswith('?') else any(x.startswith(i[:-1]) for x in g) for i in items)]
    if len(match) != 1:
        ok = False; print('MISMATCH', name, spec)
    else:
        print('ok', name, '->', got[match[0]], sorted(match[0]))
got = {k: v for k, v in got.items() if not k <= {'H1.', 'H2.', 'H3.', 'H4.'}}
print('nets total', len(got), 'expected', len(want))
print('ALL OK' if ok and len(got) == len(want) else 'FAIL')
