// Host tool: renders the 3D model (same code as the board) for pairs of "CALLSIGN TYPE" into a PPM
// strip, one 360x240 row per pair. Used by scripts/compare_liveries.py to put renders next to real
// photos (references/photos/). Build: see scripts/compare_liveries.py.
#include <initializer_list>
#include "airline.h"
#include "livery.h"
#include "model3d.h"
#include <stdio.h>
#include <algorithm>
using namespace jf;
static const int CW = 360, CH = 240, COLS = 1; static int ROWS = 1, W = CW, H = CH;
static unsigned char img[60*CH][CW][3];
static void tri(int x0,int y0,int x1,int y1,int x2,int y2,Rgb c){
  int minx=std::max(0,std::min({x0,x1,x2})),maxx=std::min(W-1,std::max({x0,x1,x2}));
  int miny=std::max(0,std::min({y0,y1,y2})),maxy=std::min(H-1,std::max({y0,y1,y2}));
  auto e=[](int ax,int ay,int bx,int by,int px,int py){return (bx-ax)*(py-ay)-(by-ay)*(px-ax);};
  int area=e(x0,y0,x1,y1,x2,y2); if(!area)return;
  for(int y=miny;y<=maxy;y++)for(int x=minx;x<=maxx;x++){int a=e(x0,y0,x1,y1,x,y),b=e(x1,y1,x2,y2,x,y),d=e(x2,y2,x0,y0,x,y);
    if((area>0&&a>=0&&b>=0&&d>=0)||(area<0&&a<=0&&b<=0&&d<=0)){img[y][x][0]=c.r;img[y][x][1]=c.g;img[y][x][2]=c.b;}}}
int main(int argc, char** argv){
  const int N=(argc-2)/2; ROWS=N; H=N*CH;
  struct P{const char*cs;const char*ty;}; P ps[60]; for(int i=0;i<N;i++){ps[i]={argv[2+2*i],argv[3+2*i]};}
  for(int y=0;y<H;y++)for(int x=0;x<W;x++){img[y][x][0]=244;img[y][x][1]=234;img[y][x][2]=208;}
  static ScreenTri tr[kMaxTris];
  for(int k=0;k<N;k++){
    const Airline*al=airlineFor(ps[k].cs); const Livery lv=liveryFor(al);
    Palette pal={{252,250,242},{222,222,228},{120,130,150},{90,95,105},{150,190,220},{44,32,22},{255,255,255},{205,205,210},{120,130,150},lv.nose_red,0,lv.tail,lv.body,{120,130,150}};
    if(al){pal.primary={al->r1,al->g1,al->b1};pal.secondary={al->r2,al->g2,al->b2};pal.tip=pal.primary;}
    if(lv.tail==kTailTap)pal.tip=pal.secondary;
    if(lv.tail==kTailFlag)pal.tip={250,250,250};
    switch(lv.fin_base){case kFinWhite:pal.fin={250,250,250};break;case kFinSecondary:pal.fin=pal.secondary;break;default:pal.fin=pal.primary;}
    pal.motif=lv.fin_base!=kFinPrimary?pal.primary:lv.white_motif?Rgb{255,255,255}:pal.secondary;
    if(lv.engines==1)pal.engine=pal.primary; if(lv.engines==2)pal.engine=pal.secondary;
    switch(lv.hull){case kHullPearl:pal.hull={214,214,212};break;case kHullChampagne:pal.hull={240,230,206};break;case kHullSky:pal.hull={150,200,235};break;default:break;}
    pal.belly={(uint8_t)(pal.hull.r*0.88f),(uint8_t)(pal.hull.g*0.88f),(uint8_t)(pal.hull.b*0.9f)};
    const Mesh&m=buildAircraft(ps[k].ty,pal);
    const int cx=CW/2, cy=CH/2+CH*k;
    const bool right=true;
    size_t n=projectMesh(m,right,cx,cy,330,215,tr,kMaxTris);
    for(int o=0;o<4;o++){int dx=(o==0)-(o==1),dy=(o==2)-(o==3);for(size_t i=0;i<n;i++)tri(tr[i].x[0]+dx,tr[i].y[0]+dy,tr[i].x[1]+dx,tr[i].y[1]+dy,tr[i].x[2]+dx,tr[i].y[2]+dy,{20,24,32});}
    for(size_t i=0;i<n;i++)tri(tr[i].x[0],tr[i].y[0],tr[i].x[1],tr[i].y[1],tr[i].x[2],tr[i].y[2],tr[i].color);
  }
  FILE*f=fopen(argv[1],"wb");fprintf(f,"P6\n%d %d\n255\n",W,H);fwrite(img,1,(size_t)W*H*3,f);fclose(f);
  printf("%zu airlines\n", airlineCount());
}
