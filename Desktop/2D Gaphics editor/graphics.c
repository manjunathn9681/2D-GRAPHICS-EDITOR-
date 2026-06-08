/*
 * graphics.c
 * 2D Graphics Editor - Drawing Algorithms and Canvas Management
 *
 * Drawing primitives:
 *   - Line      : Bresenham's Line Algorithm
 *   - Circle    : Midpoint Circle Algorithm
 *   - Rectangle : Horizontal + Vertical edge lines
 *   - Triangle  : Three lines connecting three points
 *
 * Canvas uses a 2D char array [CANVAS_HEIGHT][CANVAS_WIDTH+1].
 * Shapes are stored as structs; canvas_render() replays them.
 */

#include "graphics.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* ═══════════════════════════════════════════════════════════════════
 *  Canvas Management
 * ═══════════════════════════════════════════════════════════════════ */

void canvas_init(Canvas *c) {
    c->shape_count = 0;
    c->next_id     = 1;
    canvas_clear(c);
}

void canvas_clear(Canvas *c) {
    for (int row = 0; row < CANVAS_HEIGHT; row++) {
        for (int col = 0; col < CANVAS_WIDTH; col++) {
            c->grid[row][col] = BG_CHAR;
        }
        c->grid[row][CANVAS_WIDTH] = '\0';
    }
}

void canvas_render(Canvas *c) {
    canvas_clear(c);
    for (int i = 0; i < c->shape_count; i++) {
        Shape *s = &c->shapes[i];
        if (!s->is_active) continue;

        switch (s->type) {
            case SHAPE_LINE:
                draw_line(c,
                    s->data.line.start.x, s->data.line.start.y,
                    s->data.line.end.x,   s->data.line.end.y,
                    s->draw_char);
                break;
            case SHAPE_RECTANGLE:
                draw_rectangle(c,
                    s->data.rect.top_left.x, s->data.rect.top_left.y,
                    s->data.rect.width,       s->data.rect.height,
                    s->draw_char);
                break;
            case SHAPE_CIRCLE:
                draw_circle(c,
                    s->data.circle.center.x, s->data.circle.center.y,
                    s->data.circle.radius,
                    s->draw_char);
                break;
            case SHAPE_TRIANGLE:
                draw_triangle(c,
                    s->data.triangle.p1.x, s->data.triangle.p1.y,
                    s->data.triangle.p2.x, s->data.triangle.p2.y,
                    s->data.triangle.p3.x, s->data.triangle.p3.y,
                    s->draw_char);
                break;
        }
    }
}

void canvas_display(Canvas *c) {
    /* Top border */
    printf("+");
    for (int col = 0; col < CANVAS_WIDTH; col++) printf("-");
    printf("+\n");

    for (int row = 0; row < CANVAS_HEIGHT; row++) {
        printf("|%s|\n", c->grid[row]);
    }

    /* Bottom border */
    printf("+");
    for (int col = 0; col < CANVAS_WIDTH; col++) printf("-");
    printf("+\n");
}

/* ═══════════════════════════════════════════════════════════════════
 *  Drawing Primitives
 * ═══════════════════════════════════════════════════════════════════ */

void draw_point(Canvas *c, int x, int y, char ch) {
    if (x >= 0 && x < CANVAS_WIDTH && y >= 0 && y < CANVAS_HEIGHT)
        c->grid[y][x] = ch;
}

/* Bresenham's Line Algorithm */
void draw_line(Canvas *c, int x1, int y1, int x2, int y2, char ch) {
    int dx  =  abs(x2 - x1);
    int dy  = -abs(y2 - y1);
    int sx  = (x1 < x2) ? 1 : -1;
    int sy  = (y1 < y2) ? 1 : -1;
    int err = dx + dy;

    for (;;) {
        draw_point(c, x1, y1, ch);
        if (x1 == x2 && y1 == y2) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x1 += sx; }
        if (e2 <= dx) { err += dx; y1 += sy; }
    }
}

/* Midpoint Circle Algorithm – plots 8-way symmetric points */
static void circle_points(Canvas *c, int cx, int cy, int px, int py, char ch) {
    draw_point(c, cx + px, cy + py, ch);
    draw_point(c, cx - px, cy + py, ch);
    draw_point(c, cx + px, cy - py, ch);
    draw_point(c, cx - px, cy - py, ch);
    draw_point(c, cx + py, cy + px, ch);
    draw_point(c, cx - py, cy + px, ch);
    draw_point(c, cx + py, cy - px, ch);
    draw_point(c, cx - py, cy - px, ch);
}

void draw_circle(Canvas *c, int cx, int cy, int r, char ch) {
    int x = 0, y = r;
    int d = 1 - r;   /* decision parameter */

    circle_points(c, cx, cy, x, y, ch);
    while (x < y) {
        if (d < 0) {
            d += 2 * x + 3;
        } else {
            d += 2 * (x - y) + 5;
            y--;
        }
        x++;
        circle_points(c, cx, cy, x, y, ch);
    }
}

/* Rectangle – outline only using '*' for corners/borders */
void draw_rectangle(Canvas *c, int x, int y, int w, int h, char ch) {
    /* top & bottom edges */
    draw_line(c, x,         y,         x + w - 1, y,         ch);
    draw_line(c, x,         y + h - 1, x + w - 1, y + h - 1, ch);
    /* left & right edges */
    draw_line(c, x,         y,         x,         y + h - 1, ch);
    draw_line(c, x + w - 1, y,         x + w - 1, y + h - 1, ch);
}

/* Triangle – three lines connecting three vertices */
void draw_triangle(Canvas *c,
                   int x1, int y1,
                   int x2, int y2,
                   int x3, int y3, char ch) {
    draw_line(c, x1, y1, x2, y2, ch);
    draw_line(c, x2, y2, x3, y3, ch);
    draw_line(c, x3, y3, x1, y1, ch);
}

/* ═══════════════════════════════════════════════════════════════════
 *  Shape Management
 * ═══════════════════════════════════════════════════════════════════ */

static int alloc_shape(Canvas *c, ShapeType type, char ch) {
    if (c->shape_count >= MAX_SHAPES) return -1;
    Shape *s      = &c->shapes[c->shape_count];
    s->id         = c->next_id++;
    s->type       = type;
    s->draw_char  = ch ? ch : '*';
    s->is_active  = 1;
    c->shape_count++;
    return s->id;
}

int shape_add_line(Canvas *c, int x1, int y1, int x2, int y2, char ch) {
    int id = alloc_shape(c, SHAPE_LINE, ch);
    if (id < 0) return -1;
    Shape *s = shape_find(c, id);
    s->data.line.start = (Point){x1, y1};
    s->data.line.end   = (Point){x2, y2};
    return id;
}

int shape_add_rectangle(Canvas *c, int x, int y, int w, int h, char ch) {
    int id = alloc_shape(c, SHAPE_RECTANGLE, ch);
    if (id < 0) return -1;
    Shape *s = shape_find(c, id);
    s->data.rect.top_left = (Point){x, y};
    s->data.rect.width    = w;
    s->data.rect.height   = h;
    return id;
}

int shape_add_circle(Canvas *c, int cx, int cy, int r, char ch) {
    int id = alloc_shape(c, SHAPE_CIRCLE, ch);
    if (id < 0) return -1;
    Shape *s = shape_find(c, id);
    s->data.circle.center = (Point){cx, cy};
    s->data.circle.radius = r;
    return id;
}

int shape_add_triangle(Canvas *c,
                        int x1, int y1,
                        int x2, int y2,
                        int x3, int y3, char ch) {
    int id = alloc_shape(c, SHAPE_TRIANGLE, ch);
    if (id < 0) return -1;
    Shape *s = shape_find(c, id);
    s->data.triangle.p1 = (Point){x1, y1};
    s->data.triangle.p2 = (Point){x2, y2};
    s->data.triangle.p3 = (Point){x3, y3};
    return id;
}

int shape_delete(Canvas *c, int id) {
    Shape *s = shape_find(c, id);
    if (!s) return -1;
    s->is_active = 0;
    return 0;
}

/* ─── Modify helpers ─────────────────────────────────────────────── */

int shape_modify_line(Canvas *c, int id, int x1, int y1, int x2, int y2) {
    Shape *s = shape_find(c, id);
    if (!s || s->type != SHAPE_LINE) return -1;
    s->data.line.start = (Point){x1, y1};
    s->data.line.end   = (Point){x2, y2};
    return 0;
}

int shape_modify_rectangle(Canvas *c, int id, int x, int y, int w, int h) {
    Shape *s = shape_find(c, id);
    if (!s || s->type != SHAPE_RECTANGLE) return -1;
    s->data.rect.top_left = (Point){x, y};
    s->data.rect.width    = w;
    s->data.rect.height   = h;
    return 0;
}

int shape_modify_circle(Canvas *c, int id, int cx, int cy, int r) {
    Shape *s = shape_find(c, id);
    if (!s || s->type != SHAPE_CIRCLE) return -1;
    s->data.circle.center = (Point){cx, cy};
    s->data.circle.radius = r;
    return 0;
}

int shape_modify_triangle(Canvas *c, int id,
                           int x1, int y1,
                           int x2, int y2,
                           int x3, int y3) {
    Shape *s = shape_find(c, id);
    if (!s || s->type != SHAPE_TRIANGLE) return -1;
    s->data.triangle.p1 = (Point){x1, y1};
    s->data.triangle.p2 = (Point){x2, y2};
    s->data.triangle.p3 = (Point){x3, y3};
    return 0;
}

int shape_modify_char(Canvas *c, int id, char ch) {
    Shape *s = shape_find(c, id);
    if (!s) return -1;
    s->draw_char = ch;
    return 0;
}

/* ─── Utility ────────────────────────────────────────────────────── */

Shape *shape_find(Canvas *c, int id) {
    for (int i = 0; i < c->shape_count; i++)
        if (c->shapes[i].id == id)
            return &c->shapes[i];
    return NULL;
}

void shape_type_name(ShapeType t, char *buf, int buflen) {
    switch (t) {
        case SHAPE_LINE:      snprintf(buf, buflen, "Line");      break;
        case SHAPE_RECTANGLE: snprintf(buf, buflen, "Rectangle"); break;
        case SHAPE_CIRCLE:    snprintf(buf, buflen, "Circle");    break;
        case SHAPE_TRIANGLE:  snprintf(buf, buflen, "Triangle");  break;
        default:              snprintf(buf, buflen, "Unknown");   break;
    }
}

void shape_describe(Shape *s, char *buf, int buflen) {
    char tname[16];
    shape_type_name(s->type, tname, sizeof(tname));
    switch (s->type) {
        case SHAPE_LINE:
            snprintf(buf, buflen, "[%d] Line  (%d,%d)->(%d,%d)  ch='%c'",
                s->id,
                s->data.line.start.x, s->data.line.start.y,
                s->data.line.end.x,   s->data.line.end.y,
                s->draw_char);
            break;
        case SHAPE_RECTANGLE:
            snprintf(buf, buflen, "[%d] Rect  (%d,%d) %dx%d  ch='%c'",
                s->id,
                s->data.rect.top_left.x, s->data.rect.top_left.y,
                s->data.rect.width,       s->data.rect.height,
                s->draw_char);
            break;
        case SHAPE_CIRCLE:
            snprintf(buf, buflen, "[%d] Circle(%d,%d) r=%d  ch='%c'",
                s->id,
                s->data.circle.center.x, s->data.circle.center.y,
                s->data.circle.radius,
                s->draw_char);
            break;
        case SHAPE_TRIANGLE:
            snprintf(buf, buflen,
                "[%d] Tri   (%d,%d) (%d,%d) (%d,%d)  ch='%c'",
                s->id,
                s->data.triangle.p1.x, s->data.triangle.p1.y,
                s->data.triangle.p2.x, s->data.triangle.p2.y,
                s->data.triangle.p3.x, s->data.triangle.p3.y,
                s->draw_char);
            break;
    }
}
