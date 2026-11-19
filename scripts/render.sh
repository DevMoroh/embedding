#!/bin/zsh
# render.sh <pcb> <png> [layers]
K=/Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli
L=${3:-B.Cu,F.Cu,F.SilkS,Edge.Cuts}
D=$(mktemp -d)
$K pcb export svg --mode-single --layers $L --page-size-mode 2 --exclude-drawing-sheet --drill-shape-opt 2 -o $D/b.svg $1 >/dev/null 2>&1
W=$(grep -o 'width="[0-9.]*mm"' $D/b.svg | head -1 | grep -o '[0-9.]*'); H=$(grep -o 'height="[0-9.]*mm"' $D/b.svg | head -1 | grep -o '[0-9.]*')
PW=$(printf '%.0f' $((W*20))); PH=$(printf '%.0f' $((H*20)))
cat > $D/b.html <<H
<html><body style="margin:0;background:#1a1a1a"><img src="b.svg" style="width:${PW}px;height:${PH}px;display:block"></body></html>
H
"/Applications/Google Chrome.app/Contents/MacOS/Google Chrome" --headless=new --disable-gpu --hide-scrollbars --window-size=$PW,$PH --screenshot=$2 file://$D/b.html >/dev/null 2>&1
echo $W x $H mm
