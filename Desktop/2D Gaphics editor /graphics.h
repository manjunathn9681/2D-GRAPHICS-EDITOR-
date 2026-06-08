#ifndef GRAPHICS_H
#define GRAPHICS_H

#define ROWS 20
#define COLS 50
#define MAX_OBJECTS 100

typedef enum
{
    SHAPE_RECTANGLE,
    SHAPE_CIRCLE,
    SHAPE_LINE,
    SHAPE_TRIANGLE
} ShapeType;

typedef struct
{
    int id;
    ShapeType type;
    int row;
    int col;
    int param1;
    int param2;
    int row2;
    int col2;
} GraphicObject;

extern char canvas[ROWS][COLS];
extern GraphicObject objects[MAX_OBJECTS];
extern int objectCount;

void clearCanvas(void);
void displayCanvas(void);
void redrawCanvas(void);
int addObject(GraphicObject object);
int deleteObject(int id);
void listObjects(void);

void drawRectangle(int row, int col, int width, int height);
void drawCircle(int row, int col, int radius);
void drawLine(int row1, int col1, int row2, int col2);
void drawTriangle(int row, int col, int base, int height);
void drawObject(const GraphicObject *object);

#endif