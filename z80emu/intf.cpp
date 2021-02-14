#include "stdafx.h"
#include "gfx.h"
#include "intf.h"
extern Gfx theGfx;

Interface::Interface()
{
    regs_enabled=0;
    mem_enabled=0;
    out0cacheidx=out0max=0;
}

Interface::~Interface()
{
    for (vector<InterfaceObject*>::iterator i=ifObjects.begin(); i!=ifObjects.end(); i++)
    {
        delete *i;
    }
}

void Interface::add(InterfaceObject *i)
{
    ifObjects.push_back(i);
}

void Interface::report(char *rep,char *mem)
{
    //theGfx.Line(200,0,200,300,Gfx::Black);
    //theGfx.Line(0,100,200,100,Gfx::Black);
    if (regs_enabled)
        theGfx.Text(regx/*5*/,regy/*110*/,regx+190,regx+80,rep,Gfx::Black,1);
    if (mem_enabled)
        theGfx.Text(memx/*210*/,memy/*10*/,memx+250,memy+310,mem,Gfx::Black,2);
}

void Interface::handle_out(uchar port,uchar byte)
{
    if (port==0)
    {
        if (!out0max)
        {
            switch (byte)
            {
            case 1: out0max=4; break; // panel size takes 4 bytes

            case 2:
            case 3: out0max=7; break; // 7-seg and light button take 7 bytes
            }
            if (out0max)
            {
                cache[out0cacheidx++]=byte;
            }
        }
        else
        {
            cache[out0cacheidx++]=byte;
            if (out0cacheidx>out0max)
            {
                switch (cache[0])
                {
                case 1: // set panel size
                    theGfx.setClient(cache[1]+256*cache[2], cache[3]+256*cache[4]);
                    break;
                }
                out0cacheidx=out0max=0;
            }
        }
    }
}

ioSevenSeg::ioSevenSeg(int _x,int _y,int wid)
{
    x=_x;
    y=_y;
    cx=wid;
    cy=2*wid;
}

void ioSevenSeg::set(char n)
{
    if (n>='0' && n<='9')
        n-='0';
    else if (n>='a' && n<='f') 
        n=n+10-'a';
    else if (n>='A' && n<='F')
        n=n+10-'A';
    
    value=n;
    dirty=1;
}

void ioSevenSeg::redraw()
{
    /*
    ...0...
    .\xxx/.
    .x...x.
    1x...x2
    .>x3x<.
    4x...x5
    .x...x.
    ./xxx\.
    ...6...
    */
    theGfx.Rect(x,y,cx,cy,Gfx::Black);
    char *bits=".......";

    switch (value)
    { //            0123456
    case  0: bits="xxx.xxx"; break;
    case  1: bits="..x..x."; break;
    case  2: bits="x.xxx.x"; break;
    case  3: bits="x.xx.xx"; break;
    case  4: bits=".xxx.x."; break;
    case  5: bits="xx.x.xx"; break;
    case  6: bits="xx.xxxx"; break;
    case  7: bits="x.x..x."; break;
    case  8: bits="xxxxxxx"; break;
    case  9: bits="xxxx.xx"; break;
    case 10: bits="xxxxxx."; break;
    case 11: bits=".x.xxxx"; break;
    case 12: bits="xx..x.x"; break;
    case 13: bits="..xxxxx"; break;
    case 14: bits="xx.xx.x"; break;
    case 15: bits="xx.xx.."; break;
    case '-': bits="...x..."; break;
    case ' ':
    default:  bits="......."; break;
    }
    int T=y+2;
    int M=y+cx;
    int B=y+cy-3;
    int L=x+2;
    int R=x+cx-3;

    int SegOn=Gfx::LtGreen;
    int SegOff=Gfx::DkGreen;

    // Seg 0
    theGfx.Line(L+1,T  , R  ,T  , bits[0]=='x' ? SegOn : SegOff);
    theGfx.Line(L+2,T+1, R-1,T+1, bits[0]=='x' ? SegOn : SegOff);

    // Seg 1
    theGfx.Line(L  ,T+1, L  ,M  , bits[1]=='x' ? SegOn : SegOff);
    theGfx.Line(L+1,T+2, L+1,M-1, bits[1]=='x' ? SegOn : SegOff);

    // Seg 2
    theGfx.Line(R  ,T+1, R  ,M  , bits[2]=='x' ? SegOn : SegOff);
    theGfx.Line(R-1,T+2, R-1,M-1, bits[2]=='x' ? SegOn : SegOff);

    // Seg 3
    theGfx.Line(L+2,M-1, R-1,M-1, bits[3]=='x' ? SegOn : SegOff);
    theGfx.Line(L+1,M  , R  ,M  , bits[3]=='x' ? SegOn : SegOff);
    theGfx.Line(L+2,M+1, R-1,M+1, bits[3]=='x' ? SegOn : SegOff);

    // Seg 4
    theGfx.Line(L  ,M+1, L  ,B  , bits[4]=='x' ? SegOn : SegOff);
    theGfx.Line(L+1,M+2, L+1,B-1, bits[4]=='x' ? SegOn : SegOff);

    // Seg 5
    theGfx.Line(R  ,M+1, R  ,B  , bits[5]=='x' ? SegOn : SegOff);
    theGfx.Line(R-1,M+2, R-1,B-1, bits[5]=='x' ? SegOn : SegOff);

    // Seg 6
    theGfx.Line(L+1,B  , R  ,B  , bits[6]=='x' ? SegOn : SegOff);
    theGfx.Line(L+2,B-1, R-1,B-1, bits[6]=='x' ? SegOn : SegOff);
}

ioButton::ioButton(int _x,int _y,int w,int h)
{
    x=_x;
    y=_y;
    cx=w;
    cy=h;
    pressed=0;
}

void ioButton::redraw()
{
    theGfx.Frame(x,y,cx,cy,Gfx::Black);
    theGfx.Text(x,y,cx,cy,label,Gfx::Black,0);
}

