#!/bin/zsh
# Повна збірка: схема -> netlist -> розміщення -> трасування -> плата -> ERC/DRC
set -e
cd ${0:a:h}
PY=/Applications/KiCad/KiCad.app/Contents/Frameworks/Python.framework/Versions/3.9/bin/python3-intel64
K=/Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli
O=${1:-build}          # проміжні файли (netlist, координати виводів, траси)
mkdir -p $O
$PY gen_sch.py $O/Amplifier.kicad_sch
(cd $O && $K sch export netlist --format kicadxml -o net.xml Amplifier.kicad_sch >/dev/null 2>&1)
python3 check_net.py $O/net.xml | tail -1
python3 -c "import sys,json; sys.path.insert(0,'.'); from place import HOLES; json.dump(HOLES, open('$O/holes.json','w'))"
rm -f $O/Amplifier.kicad_pcb $O/Amplifier.kicad_pro $O/Amplifier.kicad_prl
$PY gen_pcb.py $O 2>&1 | grep -v assert
python3 router.py $O/pads.json $O/routes.json $O/holes.json
$PY gen_pcb.py $O $O/routes.json 2>&1 | grep -v assert
./render.sh $O/Amplifier.kicad_pcb ../docs/pcb_tracks.png >/dev/null
(cd $O && $K sch erc -o erc.rpt Amplifier.kicad_sch 2>&1 | grep -v Fontc | head -1)
(cd $O && $K pcb drc --schematic-parity --refill-zones --save-board -o drc.rpt Amplifier.kicad_pcb 2>&1 | grep -v Fontc | head -3)
grep '^\[' $O/drc.rpt | sed 's/:.*//' | sort | uniq -c || true
./render.sh $O/Amplifier.kicad_pcb ../docs/pcb.png >/dev/null
# готовий проєкт KiCad — у корінь репозиторію
cp $O/Amplifier.kicad_sch $O/Amplifier.kicad_pcb $O/Amplifier.kicad_pro ..
(cd .. && $K sch export pdf -o docs/schematic.pdf Amplifier.kicad_sch >/dev/null 2>&1)
# PNG схеми для README
rm -rf $O/sch_svg && $K sch export svg --exclude-drawing-sheet -o $O/sch_svg ../Amplifier.kicad_sch >/dev/null 2>&1
SVG=$(ls $O/sch_svg/*.svg | head -1)
cat > $O/sch.html <<H
<html><body style="margin:0;background:#fff"><img src="${SVG:a}" style="width:2400px;display:block"></body></html>
H
"/Applications/Google Chrome.app/Contents/MacOS/Google Chrome" --headless=new --disable-gpu --hide-scrollbars --window-size=1720,900 --screenshot=${PWD}/../docs/schematic.png file://${PWD}/$O/sch.html >/dev/null 2>&1
