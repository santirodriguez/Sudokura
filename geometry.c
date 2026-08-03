#include "geometry.h"
static bool overlap(GeoRect a,GeoRect b){return a.x<b.x+b.w&&b.x<a.x+a.w&&a.y<b.y+b.h&&b.y<a.y+a.h;}
bool geometry_contains(GeoRect r,int x,int y){return r.w>0&&r.h>0&&x>=r.x&&y>=r.y&&x<r.x+r.w&&y<r.y+r.h;}
bool geometry_compute(int W,int H,AppGeometry*g){
  if(!g||W<640||H<480) return false;
  const int m=12,gap=12;g->button_count=g->palette_count=9;g->side_by_side=W>=800;
  if(g->side_by_side){int sw=W>=1024?270:220;int board=H-2*m;if(board>W-sw-3*m)board=W-sw-3*m;board-=board%9;g->board=(GeoRect){m,(H-board)/2,board,board};g->side=(GeoRect){g->board.x+board+gap,m,W-(g->board.x+board+gap)-m,H-2*m};}
  else {int control_h=156;int board=H-control_h-3*m;if(board>W-2*m)board=W-2*m;board-=board%9;g->board=(GeoRect){(W-board)/2,m,board,board};g->side=(GeoRect){m,g->board.y+board+gap,W-2*m,H-(g->board.y+board+gap)-m};}
  int cols=g->side_by_side?2:6;int rows=(9+cols-1)/cols;int pg=5;int info=32;int available=g->side.h-info-pg;int action_h=(available*3)/5;int bh=(action_h-(rows-1)*pg)/rows;if(bh>36)bh=36;if(bh<20)return false;int bw=(g->side.w-(cols-1)*pg)/cols;
  for(int i=0;i<9;i++)g->buttons[i]=(GeoRect){g->side.x+(i%cols)*(bw+pg),g->side.y+info+(i/cols)*(bh+pg),bw,bh};
  int py=g->buttons[8].y+bh+pg;int ph=(g->side.y+g->side.h-py-2*pg)/3;int pw=(g->side.w-2*pg)/3;for(int i=0;i<9;i++)g->palette[i]=(GeoRect){g->side.x+(i%3)*(pw+pg),py+(i/3)*(ph+pg),pw,ph};return geometry_valid(g,W,H);
}
bool geometry_valid(const AppGeometry*g,int W,int H){GeoRect all[19];all[0]=g->board;for(int i=0;i<9;i++){all[1+i]=g->buttons[i];all[10+i]=g->palette[i];}for(int i=0;i<19;i++){GeoRect r=all[i];if(r.w<=0||r.h<=0||r.x<0||r.y<0||r.x+r.w>W||r.y+r.h>H)return false;for(int j=i+1;j<19;j++)if(overlap(r,all[j]))return false;}return true;}
