#include "game.h"
#include <stdlib.h>
#include <string.h>

#define IDX(r,c) ((r)*9+(c))
static bool valid_cell(int r, int c) { return r >= 0 && r < 9 && c >= 0 && c < 9; }
static bool allowed(const int*b,int r,int c,int v){
  for(int i=0;i<9;i++) if((i!=c&&b[IDX(r,i)]==v)||(i!=r&&b[IDX(i,c)]==v)) return false;
  int br=r-r%3,bc=c-c%3; for(int y=br;y<br+3;y++) for(int x=bc;x<bc+3;x++) if((y!=r||x!=c)&&b[IDX(y,x)]==v) return false;
  return true;
}
bool game_board_valid(const int b[81]){ if(!b)return false;for(int i=0;i<81;i++) if(b[i]<1||b[i]>9||!allowed(b,i/9,i%9,b[i])) return false; return true; }
static int mrv(const int*b){ int best=-1,nmin=10; for(int i=0;i<81;i++) if(!b[i]){ int n=0; for(int v=1;v<=9;v++) n+=allowed(b,i/9,i%9,v); if(n<nmin){best=i;nmin=n;} } return best; }
static int solve_count(int*b,int limit){ int i=mrv(b); if(i<0)return 1; int n=0; for(int v=1;v<=9&&n<limit;v++) if(allowed(b,i/9,i%9,v)){b[i]=v;n+=solve_count(b,limit-n);b[i]=0;} return n; }
int game_solution_count(const int p[81],int limit){if(!p||limit<=0)return 0;int b[81];for(int i=0;i<81;i++){if(p[i]<0||p[i]>9)return 0;if(p[i]&&!allowed(p,i/9,i%9,p[i]))return 0;}memcpy(b,p,sizeof b);return solve_count(b,limit);}
static bool solve_one(int*b){int i=mrv(b);if(i<0)return true;for(int v=1;v<=9;v++)if(allowed(b,i/9,i%9,v)){b[i]=v;if(solve_one(b))return true;b[i]=0;}return false;}
static void shuffle(int*a,int n){for(int i=n-1;i;i--){int j=rand()%(i+1),t=a[i];a[i]=a[j];a[j]=t;}}
void game_new(Game*g,unsigned seed){
  if (!g) return;
  srand(seed); int rows[9]={0,1,2,3,4,5,6,7,8},cols[9]={0,1,2,3,4,5,6,7,8},nums[9]={1,2,3,4,5,6,7,8,9};
  for(int band=0;band<3;band++) shuffle(rows+band*3,3);
  for(int stack=0;stack<3;stack++) shuffle(cols+stack*3,3);
  shuffle(nums,9);
  for(int r=0;r<9;r++)for(int c=0;c<9;c++)g->solution[IDX(r,c)]=nums[(rows[r]*3+rows[r]/3+cols[c])%9];
  memcpy(g->puzzle,g->solution,sizeof g->puzzle);int pos[81];for(int i=0;i<81;i++)pos[i]=i;shuffle(pos,81);int clues=81;
  for(int k=0;k<81&&clues>36;k++){int i=pos[k],old=g->puzzle[i];g->puzzle[i]=0;if(game_solution_count(g->puzzle,2)==1)clues--;else g->puzzle[i]=old;}
  int solved[81];memcpy(solved,g->puzzle,sizeof solved);solve_one(solved);memcpy(g->solution,solved,sizeof solved);
  for(int i=0;i<81;i++){g->fixed[i]=(unsigned char)(g->puzzle[i]!=0);g->notes[i]=0;}
}
int game_clue_count(const Game*g){if(!g)return 0;int n=0;for(int i=0;i<81;i++)n+=g->fixed[i]!=0;return n;}
bool game_place(Game*g,int r,int c,int v,bool strict){if(!g||!valid_cell(r,c)||v<0||v>9)return false;int i=IDX(r,c);if(g->fixed[i])return false;if(v&&strict&&!allowed(g->puzzle,r,c,v))return false;g->puzzle[i]=v;g->notes[i]=0;return true;}
bool game_toggle_note(Game*g,int r,int c,int v){if(!g||!valid_cell(r,c)||v<1||v>9)return false;int i=IDX(r,c);if(g->fixed[i]||g->puzzle[i])return false;g->notes[i]^=(uint16_t)(1u<<v);return true;}
bool game_hint(Game*g,int r,int c){if(!g||!valid_cell(r,c))return false;int i=IDX(r,c);if(g->fixed[i]||g->puzzle[i]==g->solution[i])return false;g->puzzle[i]=g->solution[i];g->notes[i]=0;return true;}
bool game_has_conflict(const Game*g,int r,int c){if(!g||!valid_cell(r,c))return false;int v=g->puzzle[IDX(r,c)];return v>=1&&v<=9&&!allowed(g->puzzle,r,c,v);}
int game_conflict_count(const Game*g){if(!g)return 0;int n=0;for(int i=0;i<81;i++)n+=game_has_conflict(g,i/9,i%9);return n;}
bool game_is_solved(const Game*g){return g&&game_board_valid(g->puzzle)&&memcmp(g->puzzle,g->solution,sizeof g->puzzle)==0;}
bool game_mode_lost(GameMode m,int s,int max,double elapsed,double limit){return(m==MODE_STRIKES&&s>=max)||(m==MODE_TIME&&limit>0&&elapsed>limit);}
