#ifndef GRAPHICS_H
#define GRAPHICS_H

#define ROWS 20
#define COLS 50

extern char canvas[ROWS][COLS];

void clearCanvas();
void displayCanvas();
void drawRectangle(int row, int col, int width, int height);

#endif