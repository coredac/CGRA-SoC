from pathlib import Path
import re, sys
from fontTools.ttLib import TTFont
from fontTools.pens.boundsPen import BoundsPen
from fontTools.pens.basePen import BasePen
from fontTools.pens.transformPen import TransformPen
from fontTools.misc.transform import Transform

w=Path(sys.argv[1])
font=TTFont(w/'Arial.TTF')
glyphs=font.getGlyphSet()
cmap=font.getBestCmap()
units=font['head'].unitsPerEm
class PathPen(BasePen):
    def __init__(self):
        super().__init__(glyphs)
        self.ops=[]
    def add(self, op, pts):
        self.ops.append(' '.join(f'{v:.4f}' for p in pts for v in p)+' '+op)
    def _moveTo(self,p): self.add('m',[p])
    def _lineTo(self,p): self.add('l',[p])
    def _curveToOne(self,*pts): self.add('c',pts)
    def _closePath(self): self.ops.append('h')
    def _endPath(self): pass

def draw(pen,text,size,x=0,y=0):
    for ch in text:
        glyph=glyphs[cmap[ord(ch)]]
        k=size/units
        glyph.draw(TransformPen(pen,Transform(k,0,0,k,x,y)))
        x+=glyph.width*k

def midpoint(text,size):
    pen=BoundsPen(glyphs)
    draw(pen,text,size)
    a,b,c,d=pen.bounds
    return (a+c)/2,(b+d)/2

def label(cx,cy,lines):
    size=26 if lines==['16 KiB','SRAM'] else 32
    ops=['q /A1 gs 1 J 1 j 3 w 1 G 0 g']
    for row,text in enumerate(lines):
        mx,my=midpoint(text,size)
        x=cx-mx
        y=cy-my+((len(lines)-1)/2-row)*size
        pen=PathPen()
        draw(pen,text,size,x,y)
        ops.extend(pen.ops+['S',f'BT /F1 {size} Tf 1 0 0 1 {x:.5f} {y:.5f} Tm ({text}) Tj ET'])
    return '\n'.join(ops)+'\nQ\n'

stream=(Path(__file__).with_name('base.stream')).read_bytes().decode('latin-1')
groups=iter([(77.75,128.06,['Rocket','CPU']),(98.62,301.45,['Gemmini']),(272,280.58,['CGRA']),(207.81,160.72,['Pool']),(308.18,107.19,['16 KiB','SRAM'])])
stream,n=re.subn(r'q /A2 gs 1 g [^\n]+ re f /A1 gs 0 g\n(?:BT /F1 32 Tf[^\n]+\n)+Q\n',lambda m:label(*next(groups)),stream)
if n!=5:sys.exit(f'unexpected module count {n}')
mx,my=midpoint('2.493 mm',16)
dims=iter([f'1 0 0 1 {178.13-mx:.5f} {13.3-my:.5f}',f'0 -1 1 0 {378.0-my:.5f} {211.74+mx:.5f}'])
stream,n=re.subn(r'q\n(?:1 0 -0 1|-0 -1 1 -0) [\d. -]+ cm\nBT\n/F1 29.333333 Tf.*?ET\nQ',lambda m:f'q 0 g BT /F1 16 Tf {next(dims)} Tm (2.493 mm) Tj ET Q',stream,flags=re.S)
if n!=2:sys.exit(f'unexpected dimension count {n}')
(w/'contents').write_bytes(stream.encode('latin-1'))
