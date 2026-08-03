#include "game.h"
#include "geometry.h"
#include "i18n.h"
#include "version.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static void test_generation(void){for(unsigned seed=1;seed<=12;seed++){Game g;game_new(&g,seed);assert(game_board_valid(g.solution));assert(game_solution_count(g.puzzle,2)==1);int clues=game_clue_count(&g);assert(clues>=32&&clues<=38);for(int i=0;i<81;i++)assert((g.fixed[i]!=0)==(g.puzzle[i]!=0));}}
static void test_actions(void){Game g;game_new(&g,42);int i=0;while(g.fixed[i])i++;int r=i/9,c=i%9,v=g.solution[i];assert(game_toggle_note(&g,r,c,v));assert(g.notes[i]&(1u<<v));assert(game_place(&g,r,c,v,false));assert(!g.notes[i]);assert(game_place(&g,r,c,0,false));assert(game_hint(&g,r,c));assert(g.puzzle[i]==v);assert(!game_hint(&g,r,c));}
static void test_conflicts_and_end(void){Game g;game_new(&g,77);int a=-1,b=-1;for(int r=0;r<9&&a<0;r++)for(int c=0;c<9;c++)if(!g.fixed[r*9+c]){if(a<0)a=r*9+c;else if(a/9==r){b=r*9+c;break;}}assert(a>=0&&b>=0);g.puzzle[a]=g.puzzle[b]=1;assert(game_has_conflict(&g,a/9,a%9));assert(game_conflict_count(&g)>=2);memcpy(g.puzzle,g.solution,sizeof g.puzzle);assert(game_is_solved(&g));assert(!game_mode_lost(MODE_CLASSIC,99,3,999,10));assert(game_mode_lost(MODE_STRIKES,3,3,0,0));assert(game_mode_lost(MODE_TIME,0,3,601,600));}
static void test_geometry(void){const int sizes[][2]={{640,480},{800,600},{1024,720},{1280,720},{1366,768},{1920,1080}};for(unsigned i=0;i<sizeof sizes/sizeof sizes[0];i++){AppGeometry g;assert(geometry_compute(sizes[i][0],sizes[i][1],&g));assert(geometry_valid(&g,sizes[i][0],sizes[i][1]));assert(g.board.w%9==0&&g.board.w>=9);}}
static void test_i18n(void){for(int l=0;l<LANG_COUNT;l++)for(int k=0;k<T_COUNT;k++)assert(tr((Language)l,(TextKey)k)[0]);assert(!strcmp(SUDOKURA_VERSION,"1.1.0"));}
int main(void){test_generation();test_actions();test_conflicts_and_end();test_geometry();test_i18n();puts("all tests passed (12 fixed generation seeds; 6 viewports; 3 languages)");return 0;}
