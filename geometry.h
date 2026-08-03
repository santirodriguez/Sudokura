#ifndef SUDOKURA_GEOMETRY_H
#define SUDOKURA_GEOMETRY_H
#include <stdbool.h>
typedef struct {int x,y,w,h;} GeoRect;
typedef struct {GeoRect board,side,buttons[9],palette[9];int button_count,palette_count;bool side_by_side;} AppGeometry;
bool geometry_compute(int width,int height,AppGeometry*out);
bool geometry_contains(GeoRect r,int x,int y);
bool geometry_valid(const AppGeometry*g,int width,int height);
#endif
