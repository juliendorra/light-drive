"""Original 128x128 Light Drive menu cover, using the game's indexed art."""
from pathlib import Path
import sys
from PIL import Image,ImageDraw

from chgame_env import chgame_root
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(chgame_root() / 'tools'))
from pixkit import FONT35,glyph35

# Deliberately placed native title pixels; no enlarged low-resolution font.
LETTERS={
'L':"""###......
###......
###......
###......
###......
###......
###......
###......
###......
###......
###......
#########
#########""",
'I':"""#########
#########
...###...
...###...
...###...
...###...
...###...
...###...
...###...
...###...
...###...
#########
#########""",
'G':"""..######.
.########
###....##
###......
###......
###......
###..####
###..####
###...###
###...###
###...###
.########
..######.""",
'H':"""###...###
###...###
###...###
###...###
###...###
#########
#########
###...###
###...###
###...###
###...###
###...###
###...###""",
'T':"""#########
#########
...###...
...###...
...###...
...###...
...###...
...###...
...###...
...###...
...###...
...###...
...###...""",
'D':"""#######..
########.
###...###
###...###
###...###
###...###
###...###
###...###
###...###
###...###
###...###
########.
#######..""",
'R':"""#######..
########.
###...###
###...###
###...###
########.
#######..
###.###..
###..###.
###..###.
###...###
###...###
###...###""",
'V':"""###...###
###...###
###...###
###...###
###...###
###...###
###...###
.##...##.
.###.###.
.###.###.
..#####..
..#####..
...###...""",
'E':"""#########
#########
###......
###......
###......
#######..
#######..
###......
###......
###......
###......
#########
#########"""}

def draw():
    colors=['000000','220000','771100','ee4433','ffbbaa','44ffdd',
            '115544','002211','ff55aa','551122','101015',
            'fff4d6','808080','000000','d62020','ff00ff']
    im=Image.new('P',(128,128),0)
    im.putpalette([v for c in colors for v in bytes.fromhex(c)]+[0]*720)
    d=ImageDraw.Draw(im)
    def sprite(name,x,foot,height=None,mirror=False):
        source=Image.open(ROOT/f'art/png/{name}.png')
        if height:source=source.resize((source.width*height//source.height,height),Image.Resampling.NEAREST)
        if mirror:source=source.transpose(Image.Transpose.FLIP_LEFT_RIGHT)
        mask=source.point(lambda c:0 if c==15 else 255,'1')
        im.paste(source,(x,foot-source.height),mask)
    def small(x,y,text,ink):
        for ch in text:
            idx=glyph35(ch)
            if idx>=0:
                for cx,bits in enumerate(FONT35[idx]):
                    for cy in range(5):
                        if bits&(1<<cy):d.point((x+cx,y+cy),fill=ink)
            x+=4

    # Sparse rain and a receding, gently bending wet road.
    for x,y in [(9,42),(34,52),(76,16),(120,31),(84,42),(45,57),(7,77),(118,86)]:
        d.line((x,y,x-1,y+3),fill=1)
    d.line([(0,62),(21,60),(47,62),(67,61),(87,63),(127,61)],fill=1)
    d.line([(64,62),(53,71),(39,85),(7,106)],fill=3)
    d.line([(66,62),(77,71),(91,85),(127,106)],fill=3)
    for y,w,h in [(65,1,2),(72,1,3),(82,2,5),(96,3,8)]:
        x=65+(y-62)//9;d.rectangle((x,y,x+w-1,y+h-1),fill=4)
    sprite('birch-1',10,80)
    sprite('pine-1',-7,94)
    sprite('pine-2',71,67,27,True)
    sprite('pine-0',117,86,38,True)

    # A real turnout and diner, with the lit sign angled on two poles.
    d.polygon([(85,99),(127,101),(127,77),(105,79)],fill=1)
    sprite('diner-0',91,96,18)
    d.line((96,67,96,94),fill=2);d.line((119,65,119,91),fill=2)
    d.line((97,67,97,94),fill=1);d.line((120,65,120,91),fill=1)
    source=Image.open(ROOT/'art/png/sign-open.png')
    def sign(shift,back=False):
        for x in range(88,124):
            u=x-88;top=51-u*4//36;bottom=72-u*2//36
            for y in range(top,bottom):
                ink=source.getpixel((u*source.width//36,(y-top)*source.height//(bottom-top)))
                if ink==15:continue
                color=(2 if ink else 1) if back else 5 if 5<=ink<=8 else 6 if ink==10 else ink
                im.putpixel((x+shift,y+(1 if back else 0)),color)
    sign(2,True);sign(0)
    for y,width in [(81,3),(89,5),(100,7)]:
        x=73+(y-75)//3
        d.line((x-width,y,x,y-2,x+width,y),fill=5)
        d.line((x,y-1,x,y+3),fill=6)
    d.line((102,97,120,97),fill=6)

    # Letter-shaped outlines leave the complete roadside scene intact.
    for word,y,ink in [('LIGHT',7,4),('DRIVE',25,3)]:
        points=[(8+i*11+x,y+j) for i,ch in enumerate(word)
                for j,row in enumerate(LETTERS[ch].splitlines()) for x,c in enumerate(row) if c=='#']
        for x,yy in points:
            for dx,dy in [(-1,0),(1,0),(0,-1),(0,1),(1,2)]:d.point((x+dx,yy+dy),fill=0)
        for x,yy in points:d.point((x+1,yy+1),fill=2)
        for x,yy in points:d.point((x,yy),fill=11 if word=='LIGHT' and yy-y<2 else ink)
    small(9,43,'AN ENDLESS NIGHT',2)
    # Cockpit rim and the cropped wheel; launcher's install band stays clear.
    d.polygon([(0,107),(41,102),(83,105),(127,106),(127,127),(0,127)],fill=0)
    d.line([(0,107),(41,102),(83,105),(127,106)],fill=3)
    d.arc((1,105,47,151),184,352,fill=2)
    d.rectangle((0,110,127,119),fill=0)
    small(26,122,'KEEP A LITTLE LIGHT',4)
    return im

if __name__=='__main__':
    import boxart
    boxart.save(draw(),ROOT/'docs/cart.png')
