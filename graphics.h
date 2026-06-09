#ifndef GRAPHICS_H
#define GRAPHICS_H

#define ROWS       22
#define COLS       60
#define MAX_OBJECTS 100

/* ------------------------------------------------------------------ */
/*  Shape types                                                         */
/* ------------------------------------------------------------------ */
typedef enum
{
    SHAPE_RECTANGLE,
    SHAPE_CIRCLE,
    SHAPE_LINE,
    SHAPE_TRIANGLE
} ShapeType;

/* ------------------------------------------------------------------ */
/*  A simple 2-D point                                                  */
/* ------------------------------------------------------------------ */
typedef struct { int row; int col; } Point;

/* ------------------------------------------------------------------ */
/*  Stored object descriptor                                            */
/* ------------------------------------------------------------------ */
typedef struct
{
    int       id;
    ShapeType type;
    /* For rectangle  : p[0] top-left corner,  p[1] bottom-right corner  */
    /* For circle     : p[0] centre,           p[1] edge point            */
    /* For line       : p[0] start,            p[1] end                   */
    /* For triangle   : p[0], p[1], p[2]                                  */
    Point     p[4];
} GraphicObject;

/* ------------------------------------------------------------------ */
/*  Globals                                                             */
/* ------------------------------------------------------------------ */
extern char          canvas[ROWS][COLS];
extern GraphicObject objects[MAX_OBJECTS];
extern int           objectCount;

/* ------------------------------------------------------------------ */
/*  Canvas helpers                                                      */
/* ------------------------------------------------------------------ */
void clearCanvas(void);
void displayCanvas(void);
void redrawCanvas(void);
int  addObject(GraphicObject object);
int  deleteObject(int id);
void listObjects(void);

/* ------------------------------------------------------------------ */
/*  Primitive drawing (coordinates, not objects)                        */
/* ------------------------------------------------------------------ */
void drawRectangleCorners(Point tl, Point br);
void drawCircleCP(Point centre, int radius);
void drawLine(Point a, Point b);
void drawTrianglePts(Point p1, Point p2, Point p3);
void drawObject(const GraphicObject *object);

/* ------------------------------------------------------------------ */
/*  Interactive (cursor-based) shape entry — returns object id or -1   */
/* ------------------------------------------------------------------ */
int interactiveRectangle(void);
int interactiveCircle(void);
int interactiveLine(void);
int interactiveTriangle(void);

/* ------------------------------------------------------------------ */
/*  Canvas Snapshot — display + save to snapshot.txt                   */
/* ------------------------------------------------------------------ */
void canvasSnapshot(void);

#endif /* GRAPHICS_H */