/*
 * graphics.h
 * 2D Graphics Editor - Data Structures and Function Prototypes
 *
 * Canvas is a 2D array of characters. Shapes are stored as a list of
 * Shape structs. To render, we clear the grid and re-draw all active shapes.
 * Drawing characters: '*' for borders/lines, '_' for fill backgrounds.
 */

#ifndef GRAPHICS_H
#define GRAPHICS_H

#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ─── Canvas Dimensions ────────────────────────────────────────────── */
#define CANVAS_WIDTH   80
#define CANVAS_HEIGHT  40
#define MAX_SHAPES     128
#define BG_CHAR        '_'
#define DRAW_CHAR_LINE '*'
#define DRAW_CHAR_FILL '_'

/* ─── Shape Types ───────────────────────────────────────────────────── */
typedef enum {
    SHAPE_LINE,
    SHAPE_RECTANGLE,
    SHAPE_CIRCLE,
    SHAPE_TRIANGLE
} ShapeType;

/* ─── Point ─────────────────────────────────────────────────────────── */
typedef struct {
    int x; /* column */
    int y; /* row    */
} Point;

/* ─── Shape Parameter Unions ─────────────────────────────────────────── */
typedef struct { Point start; Point end; }                      LineParams;
typedef struct { Point top_left; int width; int height; }       RectParams;
typedef struct { Point center;   int radius; }                  CircleParams;
typedef struct { Point p1; Point p2; Point p3; }               TriParams;

/* ─── Shape ──────────────────────────────────────────────────────────── */
typedef struct {
    int       id;
    ShapeType type;
    char      draw_char;
    int       is_active; /* 0 = deleted, 1 = active */

    union {
        LineParams   line;
        RectParams   rect;
        CircleParams circle;
        TriParams    triangle;
    } data;
} Shape;

/* ─── Canvas ─────────────────────────────────────────────────────────── */
typedef struct {
    char   grid[CANVAS_HEIGHT][CANVAS_WIDTH + 1]; /* +1 for null terminator */
    Shape  shapes[MAX_SHAPES];
    int    shape_count;   /* total slots used (including deleted) */
    int    next_id;       /* monotonically increasing id counter  */
} Canvas;

/* ─── Canvas Management ──────────────────────────────────────────────── */
void canvas_init(Canvas *c);
void canvas_clear(Canvas *c);   /* fill grid with BG_CHAR */
void canvas_render(Canvas *c);  /* draw all active shapes onto grid */
void canvas_display(Canvas *c); /* print grid to stdout */

/* ─── Drawing Primitives (operate on grid) ───────────────────────────── */
void draw_point(Canvas *c, int x, int y, char ch);
void draw_line(Canvas *c, int x1, int y1, int x2, int y2, char ch);
void draw_rectangle(Canvas *c, int x, int y, int w, int h, char ch);
void draw_circle(Canvas *c, int cx, int cy, int r, char ch);
void draw_triangle(Canvas *c, int x1, int y1,
                              int x2, int y2,
                              int x3, int y3, char ch);

/* ─── Shape Management ───────────────────────────────────────────────── */
int  shape_add_line(Canvas *c, int x1, int y1, int x2, int y2, char ch);
int  shape_add_rectangle(Canvas *c, int x, int y, int w, int h, char ch);
int  shape_add_circle(Canvas *c, int cx, int cy, int r, char ch);
int  shape_add_triangle(Canvas *c, int x1, int y1,
                                   int x2, int y2,
                                   int x3, int y3, char ch);

int  shape_delete(Canvas *c, int id);
int  shape_modify_line(Canvas *c, int id, int x1, int y1, int x2, int y2);
int  shape_modify_rectangle(Canvas *c, int id, int x, int y, int w, int h);
int  shape_modify_circle(Canvas *c, int id, int cx, int cy, int r);
int  shape_modify_triangle(Canvas *c, int id,
                                      int x1, int y1,
                                      int x2, int y2,
                                      int x3, int y3);
int  shape_modify_char(Canvas *c, int id, char ch);

/* ─── Utility ────────────────────────────────────────────────────────── */
Shape *shape_find(Canvas *c, int id);
void   shape_type_name(ShapeType t, char *buf, int buflen);
void   shape_describe(Shape *s, char *buf, int buflen);

#endif /* GRAPHICS_H */
