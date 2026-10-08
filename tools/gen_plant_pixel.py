# v2: pixel-art 32x32 legible. Cambios: tallos en verde medio (visible sobre negro),
# hojas separadas con 1-2px de fondo, contorno oscuro + relleno M + brillo L puntual.
from PIL import Image, ImageDraw
S=32
PAL = {0:(0,0,0,0), 1:(11,61,46,255), 2:(52,211,153,255), 3:(165,243,207,255), 4:(16,26,22,255)}
BG=(9,12,20,255)
def new():
    im=Image.new("RGBA",(S,S),(0,0,0,0)); return im,ImageDraw.Draw(im)
def px(im,x,y,c):
    if 0<=x<S and 0<=y<S and c!=0: im.putpixel((x,y),PAL[c])
def rect(im,x0,y0,x1,y1,c):
    for y in range(y0,y1+1):
        for x in range(x0,x1+1): px(im,x,y,c)
def disc(im,cx,cy,r,c):
    for y in range(cy-r,cy+r+1):
        for x in range(cx-r,cx+r+1):
            if (x-cx)**2+(y-cy)**2<=r*r: px(im,x,y,c)
def ell(im,cx,cy,w,h,c):
    for y in range(cy-h//2,cy+h//2+1):
        for x in range(cx-w//2,cx+w//2+1):
            if ((x-cx)/(w/2))**2+((y-cy)/(h/2))**2<=1: px(im,x,y,c)
def clear(im,x,y):
    if 0<=x<S and 0<=y<S: im.putpixel((x,y),(0,0,0,0))
def pot(im):
    rect(im,10,25,21,26,2)   # labio verde fino
    rect(im,11,27,20,30,4)   # cuerpo
    rect(im,11,27,20,27,1)   # sombra sup cuerpo
    px(im,10,25,3); px(im,21,25,3)
    for x in range(12,20): px(im,x,30,1)
def stem(im,x0,y0,x1,y1):
    # linea 1px verde medio
    dx=abs(x1-x0); dy=abs(y1-y0); n=max(dx,dy,1)
    for i in range(n+1):
        px(im,round(x0+(x1-x0)*i/n),round(y0+(y1-y0)*i/n),2)
def heart(im,cx,cy,s):
    # corazon compacto con escote visible
    r=s//2
    disc(im,cx-2,cy-1,2,2); disc(im,cx+2,cy-1,2,2)
    for i in range(s-2):
        w=max(1,int((r+1)*(1-i/(s-1))))
        for x in range(cx-w,cx+w+1): px(im,x,cy+i,2)
    px(im,cx-3,cy-2,3); px(im,cx+1,cy-2,3)
    clear(im,cx,cy-3); clear(im,cx,cy-4)  # escote
def oleaf(im,cx,cy,w,h,ang=0):
    ell(im,cx,cy,w,h,1); ell(im,cx,cy,w-2,h-2,2)
    px(im,cx-1,cy-1,3)
def sword(im,x,top,h,edge=True):
    for y in range(top,25):
        px(im,x,y,2); px(im,x-1,y,1) if y>top+1 else None
        if edge and y>top+1: px(im,x+1,y,3)
    px(im,x,top,3); px(im,x,top+1,2)
    for k,y in enumerate(range(top+5,24,5)): px(im,x,y-1,1)
def monstera_leaf(im,cx,cy,R):
    # hoja cordada: mas ancha arriba, punta abajo + cortes diagonales finos 1px
    for y in range(cy-R,cy+R+1):
        t=(y-(cy-R))/(2*R)  # 0 arriba -> 1 abajo
        w=int(R*(1.05-0.55*t))  # estrecha hacia abajo
        for x in range(cx-w,cx+w+1):
            if (x-cx)**2/((w+0.5)**2)+((y-cy)/(R+0.5))**2<=1: px(im,x,y,2)
    # contorno inferior en punta
    for x in range(cx-3,cx+4): px(im,x,cy+R-1,2)
    # cortes diagonales desde el borde hacia dentro (1px, en diagonal)
    for side in (-1,1):
        for k,dy in enumerate((-5,-1,3)):
            for i in range(5):
                clear(im,cx+side*(R-1-i),cy+dy-i//2 if side<0 else cy+dy+i//2-1)
    clear(im,cx-2,cy-1); clear(im,cx+2,cy+1)
    px(im,cx-3,cy-5,3); px(im,cx-2,cy-5,3)
def cala_leaf(im,cx,cy,w,h):
    ell(im,cx,cy,w,h,1); ell(im,cx,cy,w-2,h-2,2)
    for y in range(cy-h//2+2,cy+h//2-1,2): px(im,cx,y,3)
def cactus_col(im,x0,w,top,bot):
    rect(im,x0,top,x0+w-1,bot,1); rect(im,x0+1,top,x0+w-2,bot,2)
    for y in range(top+1,bot,2): px(im,x0+w-1,y,3)
    for y in range(top+3,bot,4): px(im,x0,y,3)

def m_poto():
    im,_=new()
    stem(im,16,25,10,12); stem(im,16,25,22,11); stem(im,16,25,16,16)
    heart(im,9,9,7); heart(im,23,8,8); heart(im,16,15,7)
    pot(im); return im
def m_monstera():
    im,_=new()
    stem(im,16,25,16,20)
    monstera_leaf(im,15,12,9)
    oleaf(im,24,18,6,7)  # hoja joven sin cortes
    pot(im); return im
def m_sanse():
    im,_=new()
    sword(im,8,13,12); sword(im,12,9,16); sword(im,16,6,19); sword(im,20,10,15); sword(im,24,14,11)
    pot(im); return im
def m_ficus():
    im,_=new()
    for y in range(10,25): px(im,16,y,2)
    px(im,15,14,1)
    oleaf(im,16,6,9,6); oleaf(im,10,11,7,5); oleaf(im,22,11,7,5)
    oleaf(im,8,17,6,5); oleaf(im,24,17,6,5)
    pot(im); return im
def m_calathea():
    im,_=new()
    stem(im,16,25,12,16); stem(im,16,25,20,16); stem(im,16,25,16,14)
    cala_leaf(im,11,13,8,10); cala_leaf(im,21,13,8,10); cala_leaf(im,16,9,9,9)
    pot(im); return im
def m_cactus():
    im,_=new()
    cactus_col(im,14,5,5,24)
    disc(im,16,5,2,2)
    # brazos separados con 1px de aire
    cactus_col(im,9,4,13,24); disc(im,11,13,2,2)
    cactus_col(im,20,4,15,24); disc(im,22,15,2,2)
    pot(im); return im

import os
out="docs/media/plants_pixel"; os.makedirs(out,exist_ok=True)
makers=[("poto",m_poto),("monstera",m_monstera),("sansevieria",m_sanse),
        ("ficus",m_ficus),("calathea",m_calathea),("cactus",m_cactus)]
for name,fn in makers:
    im=fn(); im.save(f"{out}/{name}_32.png")
    bg=Image.new("RGBA",(256,256),BG); bg.alpha_composite(im.resize((256,256),Image.NEAREST))
    bg.convert("RGB").save(f"{out}/{name}_preview.png")
print("v2 ok")
# header 4px/byte no: 2px/byte nibble (0-4)
def quant(im):
    rev={v:k for k,v in PAL.items()}
    return [rev.get(tuple(p),0) for p in im.getdata()]
with open("firmware/kt1-deskpet/plants_icons.h","w") as f:
    f.write("// Pixel-art 32x32 negro+verde v2 (ver docs/media/plants_pixel/*_preview.png)\n#pragma once\n#define PLANT_ICON_S 32\n")
    for name,fn in makers:
        q=quant(fn())
        f.write(f"static const uint8_t ICON_{name.upper()}[] = {{\n")
        for y in range(32):
            row=q[y*32:(y+1)*32]; byts=[(row[x]<<4)|row[x+1] for x in range(0,32,2)]
            f.write("  "+",".join(f"0x{b:02X}" for b in byts)+",\n")
        f.write("};\n")
    f.write("static const uint8_t* const PLANT_ICONS[] = { ICON_POTO, ICON_MONSTERA, ICON_SANSEVIERIA, ICON_FICUS, ICON_CALATHEA, ICON_CACTUS };\n")
print("header ok")
